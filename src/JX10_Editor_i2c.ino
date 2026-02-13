#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include <USBHost_t36.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "Button.h"
#include "HWControls.h"
#include "EepromMgr.h"
#include "imxrt.h"
#include <map>

std::map<int, int> voiceAssignment;

#define PARAMETER 0      //The main page for displaying the current patch and control (parameter) changes
#define RECALL 1         //Patches list
#define SAVE 2           //Save patch page
#define REINITIALISE 3   // Reinitialise message
#define PATCH 4          // Show current patch bypassing PARAMETER
#define PATCHNAMING 5    // Patch naming page
#define DELETE 6         //Delete patch page
#define DELETEMSG 7      //Delete patch message page
#define SETTINGS 8       //Settings page
#define SETTINGSVALUE 9  //Settings page

unsigned int state = PARAMETER;

#include "ST7735Display.h"

bool cardStatus = false;

//USB HOST MIDI Class Compliant
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
MIDIDevice midi1(myusb);

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

#include "Settings.h"

/* ============================================================
   SETUP / RUNTIME
   ============================================================ */

void pollAllMCPs();

void initButtons();

void updateUpperToneData();
void updateLowerToneData();

void setup() {
  Serial.begin(115200);
  Serial3.begin(31250, SERIAL_8N1);

  suppressParamAnnounce = true;
  bootInitInProgress = true;

  setupDisplay();
  setUpSettings();
  setupHardware();
  primeMuxBaseline();

  delay(100);

  cardStatus = SD.begin(BUILTIN_SDCARD);
  if (cardStatus) {
    Serial.println("SD card is connected");
    //Get patch numbers and names from SD card
    loadPatches();
    if (patches.size() == 0) {
      //save an initialised patch to SD card
      savePatch("1", INITPATCH);
      loadPatches();
    }
  } else {
    Serial.println("SD card is not connected or unusable");

    //reinitialiseToPanel();
    showPatchPage("No SD", "conn'd / usable");
  }

  // Set PWM frequency to 8 MHz
  analogWriteFrequency(VOICE_CLOCK, 8000000);
  analogWrite(VOICE_CLOCK, 128);  // 50% of default 8-bit range

  Wire.begin();
  Wire.setClock(400000);  // Slow down I2C to 100kHz
  Wire1.begin();
  Wire1.setClock(400000);  // Slow down I2C to 100kHz
  Wire2.begin();
  Wire2.setClock(400000);  // Slow down I2C to 100kHz
  delay(10);

  mcp1.begin(0, Wire1);
  delay(10);
  mcp2.begin(1, Wire1);
  delay(10);
  mcp3.begin(2, Wire);
  delay(10);
  mcp4.begin(3, Wire);
  delay(10);
  mcp5.begin(4, Wire);
  delay(10);
  mcp6.begin(5, Wire);
  delay(10);
  mcp7.begin(6, Wire1);
  delay(10);
  mcp8.begin(4, Wire1);
  delay(10);
  mcp9.begin(5, Wire1);
  delay(10);
  mcp10.begin(3, Wire1);
  delay(10);
  mcp11.begin(2, Wire1);
  delay(10);

  //groupEncoders();
  //initRotaryEncoders();
  initButtons();

  setupMCPoutputs();
  sendInitSequence();

  //Read MIDI In Channel from EEPROM
  midiChannel = getMIDIChannel();

  //USB HOST MIDI Class Compliant
  delay(400);  //Wait to turn on USB Host
  myusb.begin();
  midi1.setHandleControlChange(myControlConvert);
  midi1.setHandleNoteOff(myNoteOff);
  midi1.setHandleNoteOn(myNoteOn);
  midi1.setHandlePitchChange(myPitchBend);
  midi1.setHandleAfterTouchChannel(myAfterTouch);
  Serial.println("USB HOST MIDI Class Compliant Listening");

  // USB MIDI
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandlePitchChange(myPitchBend);
  usbMIDI.setHandleControlChange(myControlConvert);
  usbMIDI.setHandleProgramChange(myProgramChange);
  usbMIDI.setHandleAfterTouchChannel(myAfterTouch);

  MIDI.begin();
  MIDI.setHandleNoteOn(myNoteOn);
  MIDI.setHandleNoteOff(myNoteOff);
  MIDI.setHandlePitchBend(myPitchBend);
  MIDI.setHandleControlChange(myControlConvert);
  MIDI.setHandleProgramChange(myProgramChange);
  MIDI.setHandleAfterTouchChannel(myAfterTouch);
  MIDI.turnThruOn(midi::Thru::Mode::Off);
  Serial.println("MIDI In DIN Listening");

  //Read Encoder Direction from EEPROM
  encCW = getEncoderDir();

  //Read MIDI Out Channel from EEPROM
  midiOutCh = getMIDIOutCh();

  recallPatch(patchNo);
  bootInitInProgress = false;
  suppressParamAnnounce = false;
  startParameterDisplay();
}

void sendInitSequence() {

  digitalWrite(VOICE_RESET, LOW);
  delay(10);
  digitalWrite(VOICE_RESET, HIGH);
  delay(1);
  // 8x F1
  for (int i = 0; i < 16; i++) Serial3.write((uint8_t)0xF1);
  mcp10.digitalWrite(LOWER_SELECT, HIGH);

  delay(100);
  digitalWrite(VOICE_RESET, LOW);
  delay(10);
  digitalWrite(VOICE_RESET, HIGH);
  delay(1);
  // 8x F9
  for (int i = 0; i < 16; i++) Serial3.write((uint8_t)0xF9);
  mcp10.digitalWrite(UPPER_SELECT, HIGH);

  Serial3.flush();  // wait until bytes have left the TX buffer
  delay(100);
}

void sendSerialOffset(uint8_t extra, uint8_t offset) {
  if (upperSW) {
    board = 0xF9;
  } else {
    board = 0xF1;
  }
  
  if (keyMode == 1 || keyMode == 2) {
    board = 0xF4;
  }

  Serial3.write(board);
  Serial3.write(extra);
  Serial3.write(offset);
}

void sendParamValue(uint8_t param, uint8_t value) {
  Serial3.write(param);
  Serial3.write((uint8_t)(value & 0x7F));  // keep 0..127
}

void primeMuxBaseline() {
  for (int ch = 0; ch < MUXCHANNELS; ch++) {
    digitalWriteFast(MUX_0, ch & B0001);
    digitalWriteFast(MUX_1, ch & B0010);
    digitalWriteFast(MUX_2, ch & B0100);
    digitalWriteFast(MUX_3, ch & B1000);
    delayMicroseconds(2);

    mux1ValuesPrev[ch] = adc->adc1->analogRead(MUX1_S);
    mux2ValuesPrev[ch] = adc->adc1->analogRead(MUX2_S);
    mux3ValuesPrev[ch] = adc->adc1->analogRead(MUX3_S);
    mux4ValuesPrev[ch] = adc->adc1->analogRead(MUX4_S);
  }
  muxInput = 0;
}

void startParameterDisplay() {
  updateScreen();

  lastDisplayTriggerTime = millis();
  waitingToUpdate = true;
}

void myAfterTouch(byte channel, byte value) {
  MIDI.sendAfterTouch(value, channel);
}

void myProgramChange(byte channel, byte program) {
  state = PATCH;
  patchNo = program + 1;
  recallPatch(patchNo);
  Serial.print("MIDI Pgm Change:");
  Serial.println(patchNo);
  state = PARAMETER;
}

void myPitchBend(byte channel, int pitchValue) {
  MIDI.sendPitchBend(pitchValue, midiOutCh);
}

void myControlConvert(byte channel, byte control, byte value) {
  switch (control) {

    case 1:
      MIDI.sendControlChange(control, value, midiOutCh);
      break;

    case 2:
      MIDI.sendControlChange(control, value, midiOutCh);
      break;

    case 7:
      MIDI.sendControlChange(control, value, midiOutCh);
      break;

    case 64:
      MIDI.sendControlChange(control, value, midiOutCh);
      break;

    default:
      int newvalue = value;
      myControlChange(channel, control, newvalue);
      break;
  }
}

void myControlChange(byte channel, byte control, int value) {
  switch (control) {

      // case CCmod_lfo:
      //   mod_lfo = value;
      //   updatemod_lfo(1);
      //   break;

      // case CCbend_range:
      //   bend_range = value;
      //   updatebend_range(1);
      //   break;

    case CClfo1_wave:
      if (upperSW) {
        upperData[P_lfo1_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 1) {
          lowerData[P_lfo1_wave] = upperData[P_lfo1_wave];
        }
      } else {
        lowerData[P_lfo1_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 2) {
          upperData[P_lfo1_wave] = lowerData[P_lfo1_wave];
        }
      }
      lfo1_wave_str = value;
      updatelfo1_wave(1);
      break;

    case CClfo1_rate:
      if (upperSW) {
        upperData[P_lfo1_rate] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_rate] = upperData[P_lfo1_rate];
        }
      } else {
        lowerData[P_lfo1_rate] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_rate] = lowerData[P_lfo1_rate];
        }
      }
      lfo1_rate_str = value;
      updatelfo1_rate(1);
      break;

    case CClfo1_delay:
      if (upperSW) {
        upperData[P_lfo1_delay] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_delay] = upperData[P_lfo1_delay];
        }
      } else {
        lowerData[P_lfo1_delay] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_delay] = lowerData[P_lfo1_delay];
        }
      }
      lfo1_delay_str = value;
      updatelfo1_delay(1);
      break;

    case CClfo1_lfo2:
      if (upperSW) {
        upperData[P_lfo1_lfo2] = value;
        if (keyMode == 1) {
          lowerData[P_lfo1_lfo2] = upperData[P_lfo1_lfo2];
        }
      } else {
        lowerData[P_lfo1_lfo2] = value;
        if (keyMode == 2) {
          upperData[P_lfo1_lfo2] = lowerData[P_lfo1_lfo2];
        }
      }
      lfo1_lfo2_str = value;
      updatelfo1_lfo2(1);
      break;

    case CCdco1_PW:
      if (upperSW) {
        upperData[P_dco1_PW] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PW] = upperData[P_dco1_PW];
        }
      } else {
        lowerData[P_dco1_PW] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PW] = lowerData[P_dco1_PW];
        }
      }
      dco1_PW_str = value;
      updatedco1_PW(1);
      break;

    case CCdco1_PWM_env:
      if (upperSW) {
        upperData[P_dco1_PWM_env] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_env] = upperData[P_dco1_PWM_env];
        }
      } else {
        lowerData[P_dco1_PWM_env] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_env] = lowerData[P_dco1_PWM_env];
        }
      }
      dco1_PWM_env_str = value;
      updatedco1_PWM_env(1);
      break;

    case CCdco1_PWM_lfo:
      if (upperSW) {
        upperData[P_dco1_PWM_lfo] = value;
        if (keyMode == 1) {
          lowerData[P_dco1_PWM_lfo] = upperData[P_dco1_PWM_lfo];
        }
      } else {
        lowerData[P_dco1_PWM_lfo] = value;
        if (keyMode == 2) {
          upperData[P_dco1_PWM_lfo] = lowerData[P_dco1_PWM_lfo];
        }
      }
      dco1_PWM_lfo_str = value;
      updatedco1_PWM_lfo(1);
      break;

      // case CCdco1_pitch_env:
      //   dco1_pitch_env = value;
      //   updatedco1_pitch_env(1);
      //   break;

      // case CCdco1_pitch_lfo:
      //   dco1_pitch_lfo = value;
      //   updatedco1_pitch_lfo(1);
      //   break;

      // case CCdco1_wave:
      //   dco1_wave = value;
      //   updatedco1_wave(1);
      //   break;

      // case CCdco1_range:
      //   dco1_range = value;
      //   updatedco1_range(1);
      //   break;

      // case CCdco1_tune:
      //   dco1_tune = value;
      //   updatedco1_tune(1);
      //   break;

      // case CCdco1_mode:
      //   dco1_mode = value;
      //   updatedco1_mode(1);
      //   break;

    case CClfo2_wave:
      if (upperSW) {
        upperData[P_lfo2_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 1) {
          lowerData[P_lfo2_wave] = upperData[P_lfo2_wave];
        }
      } else {
        lowerData[P_lfo2_wave] = map(value, 0, 127, 0, 4);
        if (keyMode == 2) {
          upperData[P_lfo2_wave] = lowerData[P_lfo2_wave];
        }
      }
      lfo2_wave_str = value;
      updatelfo2_wave(1);
      break;

    case CClfo2_rate:
      if (upperSW) {
        upperData[P_lfo2_rate] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_rate] = upperData[P_lfo2_rate];
        }
      } else {
        lowerData[P_lfo2_rate] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_rate] = lowerData[P_lfo2_rate];
        }
      }
      lfo2_rate_str = value;
      updatelfo2_rate(1);
      break;

    case CClfo2_delay:
      if (upperSW) {
        upperData[P_lfo2_delay] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_delay] = upperData[P_lfo2_delay];
        }
      } else {
        lowerData[P_lfo2_delay] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_delay] = lowerData[P_lfo2_delay];
        }
      }
      lfo2_delay_str = value;
      updatelfo2_delay(1);
      break;

    case CClfo2_lfo1:
      if (upperSW) {
        upperData[P_lfo2_lfo1] = value;
        if (keyMode == 1) {
          lowerData[P_lfo2_lfo1] = upperData[P_lfo2_lfo1];
        }
      } else {
        lowerData[P_lfo2_lfo1] = value;
        if (keyMode == 2) {
          upperData[P_lfo2_lfo1] = lowerData[P_lfo2_lfo1];
        }
      }
      lfo2_lfo1_str = value;
      updatelfo2_lfo1(1);
      break;

    case CCdco2_PW:
      if (upperSW) {
        upperData[P_dco2_PW] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PW] = upperData[P_dco2_PW];
        }
      } else {
        lowerData[P_dco2_PW] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PW] = lowerData[P_dco2_PW];
        }
      }
      dco2_PW_str = value;
      updatedco2_PW(1);
      break;

    case CCdco2_PWM_env:
      if (upperSW) {
        upperData[P_dco2_PWM_env] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_env] = upperData[P_dco2_PWM_env];
        }
      } else {
        lowerData[P_dco2_PWM_env] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_env] = lowerData[P_dco2_PWM_env];
        }
      }
      dco2_PWM_env_str = value;
      updatedco2_PWM_env(1);
      break;

    case CCdco2_PWM_lfo:
      if (upperSW) {
        upperData[P_dco2_PWM_lfo] = value;
        if (keyMode == 1) {
          lowerData[P_dco2_PWM_lfo] = upperData[P_dco2_PWM_lfo];
        }
      } else {
        lowerData[P_dco2_PWM_lfo] = value;
        if (keyMode == 2) {
          upperData[P_dco2_PWM_lfo] = lowerData[P_dco2_PWM_lfo];
        }
      }
      dco2_PWM_lfo_str = value;
      updatedco2_PWM_lfo(1);
      break;

      // case CCdco2_pitch_env:
      //   dco2_pitch_env = value;
      //   updatedco2_pitch_env(1);
      //   break;

      // case CCdco2_pitch_lfo:
      //   dco2_pitch_lfo = value;
      //   updatedco2_pitch_lfo(1);
      //   break;

      // case CCdco2_wave:
      //   dco2_wave = value;
      //   updatedco2_wave(1);
      //   break;

      // case CCdco2_range:
      //   dco2_range = value;
      //   updatedco2_range(1);
      //   break;

      // case CCdco2_tune:
      //   dco2_tune = value;
      //   updatedco2_tune(1);
      //   break;

      // case CCdco2_fine:
      //   dco2_fine = value;
      //   updatedco2_fine(1);
      //   break;

      // case CCdco1_level:
      //   dco1_level = value;
      //   updatedco1_level(1);
      //   break;

      // case CCdco2_level:
      //   dco2_level = value;
      //   updatedco2_level(1);
      //   break;

      // case CCdco2_mod:
      //   dco2_mod = value;
      //   updatedco2_mod(1);
      //   break;

      // case CCvcf_hpf:
      //   vcf_hpf = value;
      //   updatevcf_hpf(1);
      //   break;

      // case CCvcf_cutoff:
      //   vcf_cutoff = value;
      //   updatevcf_cutoff(1);
      //   break;

      // case CCvcf_res:
      //   vcf_res = value;
      //   updatevcf_res(1);
      //   break;

      // case CCvcf_kb:
      //   vcf_kb = value;
      //   updatevcf_kb(1);
      //   break;

      // case CCvcf_env:
      //   vcf_env = value;
      //   updatevcf_env(1);
      //   break;

      // case CCvcf_lfo1:
      //   vcf_lfo1 = value;
      //   updatevcf_lfo1(1);
      //   break;

      // case CCvcf_lfo2:
      //   vcf_lfo2 = value;
      //   updatevcf_lfo2(1);
      //   break;

      // case CCvca_mod:
      //   vca_mod = value;
      //   updatevca_mod(1);
      //   break;

      // case CCat_vib:
      //   at_vib = value;
      //   updateat_vib(1);
      //   break;

      // case CCat_lpf:
      //   at_lpf = value;
      //   updateat_lpf(1);
      //   break;

      // case CCat_vol:
      //   at_vol = value;
      //   updateat_vol(1);
      //   break;

      // case CCbalance:
      //   balance = value;
      //   updatebalance(1);
      //   break;

      // case CCvolume:
      //   volume = value;
      //   updatevolume(1);
      //   break;

      // case CCportamento:
      //   portamento = value;
      //   updateportamento(1);
      //   break;

      // case CCtime1:
      //   time1 = value;
      //   updatetime1(1);
      //   break;

      // case CClevel1:
      //   level1 = value;
      //   updatelevel1(1);
      //   break;

      // case CCtime2:
      //   time2 = value;
      //   updatetime2(1);
      //   break;

      // case CClevel2:
      //   level2 = value;
      //   updatelevel2(1);
      //   break;

      // case CCtime3:
      //   time3 = value;
      //   updatetime3(1);
      //   break;

      // case CClevel3:
      //   level3 = value;
      //   updatelevel3(1);
      //   break;

      // case CCtime4:
      //   time4 = value;
      //   updatetime4(1);
      //   break;

      // case CC5stage_mode:
      //   env5stage_mode = value;
      //   updateenv5stage_mode(1);
      //   break;

      // case CC2time1:
      //   env2_time1 = value;
      //   updateenv2_time1(1);
      //   break;

      // case CC2level1:
      //   env2_level1 = value;
      //   updateenv2_level1(1);
      //   break;

      // case CC2time2:
      //   env2_time2 = value;
      //   updateenv2_time2(1);
      //   break;

      // case CC2level2:
      //   env2_level2 = value;
      //   updateenv2_level2(1);
      //   break;

      // case CC2time3:
      //   env2_time3 = value;
      //   updateenv2_time3(1);
      //   break;

      // case CC2level3:
      //   env2_level3 = value;
      //   updateenv2_level3(1);
      //   break;

      // case CC2time4:
      //   env2_time4 = value;
      //   updateenv2_time4(1);
      //   break;

      // case CC25stage_mode:
      //   env2_5stage_mode = value;
      //   updateenv2_env5stage_mode(1);
      //   break;

      // case CCattack:
      //   attack = value;
      //   updateattack(1);
      //   break;

      // case CC4attack:
      //   env4_attack = value;
      //   updateenv4_attack(1);
      //   break;

      // case CCdecay:
      //   decay = value;
      //   updatedecay(1);
      //   break;

      // case CC4decay:
      //   env4_decay = value;
      //   updateenv4_decay(1);
      //   break;

      // case CCsustain:
      //   sustain = value;
      //   updatesustain(1);
      //   break;

      // case CC4sustain:
      //   env4_sustain = value;
      //   updateenv4_sustain(1);
      //   break;

      // case CCrelease:
      //   release = value;
      //   updaterelease(1);
      //   break;

      // case CC4release:
      //   env4_release = value;
      //   updateenv4_release(1);
      //   break;

      // case CCadsr_mode:
      //   adsr_mode = value;
      //   updateadsr_mode(1);
      //   break;

      // case CC4adsr_mode:
      //   env4_adsr_mode = value;
      //   updateenv4_adsr_mode(1);
      //   break;

      // case CCdualdetune:
      //   dualdetune = value;
      //   updatedualdetune(1);
      //   break;

      // case CCunisondetune:
      //   unisondetune = value;
      //   updateunisondetune(1);
      //   break;

      //   // Buttons

      // case CCoctave_down:
      //   updateoctave_down(1);
      //   break;

      // case CCoctave_up:
      //   updateoctave_up(1);
      //   break;

    case CCdual_button:
      updatedual_button(1);
      break;

    case CCsplit_button:
      updatesplit_button(1);
      break;

    case CCsingle_button:
      updatesingle_button(1);
      break;

    case CCspecial_button:
      updatespecial_button(1);
      break;

    case CCpoly_button:
      updatepoly_button(1);
      break;

    case CCmono_button:
      updatemono_button(1);
      break;

    case CCunison_button:
      updateunison_button(1);
      break;

    // case CClfo1_sync:
    //   updatelfo1_sync(1);
    //   break;

    // case CClfo2_sync:
    //   updatelfo2_sync(1);
    //   break;

    // case CCdco1_PWM_dyn:
    //   updatedco1_PWM_dyn(1);
    //   break;

    // case CCdco2_PWM_dyn:
    //   updatedco2_PWM_dyn(1);
    //   break;

    // case CCdco1_PWM_env_source:
    //   updatedco1_PWM_env_source(1);
    //   break;

    // case CCdco2_PWM_env_source:
    //   updatedco2_PWM_env_source(1);
    //   break;

    // case CCdco1_PWM_lfo_source:
    //   updatedco1_PWM_lfo_source(1);
    //   break;

    // case CCdco2_PWM_lfo_source:
    //   updatedco2_PWM_lfo_source(1);
    //   break;

    // case CCdco1_pitch_dyn:
    //   updatedco1_pitch_dyn(1);
    //   break;

    // case CCdco2_pitch_dyn:
    //   updatedco2_pitch_dyn(1);
    //   break;

    // case CCdco1_pitch_lfo_source:
    //   updatedco1_pitch_lfo_source(1);
    //   break;

    // case CCdco2_pitch_lfo_source:
    //   updatedco2_pitch_lfo_source(1);
    //   break;

    // case CCdco1_pitch_env_source:
    //   updatedco1_pitch_env_source(1);
    //   break;

    // case CCdco2_pitch_env_source:
    //   updatedco2_pitch_env_source(1);
    //   break;

    case CCeditMode:
      updateeditMode(1);
      break;

    // case CCdco_mix_env_source:
    //   updatedco_mix_env_source(1);
    //   break;

    // case CCdco_mix_dyn:
    //   updatedco_mix_dyn(1);
    //   break;

    // case CCvcf_env_source:
    //   updatevcf_env_source(1);
    //   break;

    // case CCvcf_dyn:
    //   updatevcf_dyn(1);
    //   break;

    // case CCvca_env_source:
    //   updatevca_env_source(1);
    //   break;

    // case CCvca_dyn:
    //   updatevca_dyn(1);
    //   break;

    // case CCchorus_sw:
    //   updatechorus(1);
    //   break;

    // case CCportamento_sw:
    //   updateportamento_sw(1);
    //   break;

    // case CCenv5stage:
    //   updateenv5stage(1);
    //   break;

    // case CCadsr:
    //   updateadsr(1);
    //   break;
  }
}

void sendOffset(uint8_t offset, uint8_t parameter) {
  if (EXTRA_OFFSET != offset || LAST_PARAM != parameter || keyMode != oldkeyMode || upperSW != oldupperSW) {
    sendSerialOffset(0xFD, offset);
    EXTRA_OFFSET = offset;
    LAST_PARAM = parameter;
    oldkeyMode = keyMode;
    oldupperSW = upperSW;
  }
}

FLASHMEM void updatelfo1_wave(bool announce) {
  sendOffset(0x0C, 0xA1);
  if (announce && !suppressParamAnnounce) {
    lfo1_wave_str = map(lfo1_wave_str, 0, 127, 0, 4);
    displayMode = 0;
    switch (lfo1_wave_str) {
      case 0:
        showCurrentParameterPage("LFO1 WAVEFORM", "RAND");
        break;

      case 1:
        showCurrentParameterPage("LFO1 WAVEFORM", "SQR");
        break;

      case 2:
        showCurrentParameterPage("LFO1 WAVEFORM", "SAW-");
        break;

      case 3:
        showCurrentParameterPage("LFO1 WAVEFORM", "SAW+");
        break;

      case 4:
        showCurrentParameterPage("LFO1 WAVEFORM", "SINE");
        break;
    }
    startParameterDisplay();
  }
  if (upperSW) {
    switch (upperData[P_lfo1_wave]) {
      case 0:
        sendParamValue(0xA1, 0x00);
        break;

      case 1:
        sendParamValue(0xA1, 0x10);
        break;

      case 2:
        sendParamValue(0xA1, 0x20);
        break;

      case 3:
        sendParamValue(0xA1, 0x30);
        break;

      case 4:
        sendParamValue(0xA1, 0x40);
        break;
    }
  } else {
    switch (lowerData[P_lfo1_wave]) {
      case 0:
        sendParamValue(0xA1, 0x00);
        break;

      case 1:
        sendParamValue(0xA1, 0x10);
        break;

      case 2:
        sendParamValue(0xA1, 0x20);
        break;

      case 3:
        sendParamValue(0xA1, 0x30);
        break;

      case 4:
        sendParamValue(0xA1, 0x40);
        break;
    }
  }
}

// FLASHMEM void updatemod_lfo(bool announce) {
//   mod_lfo_str = map(mod_lfo, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("MOD DEPTH", String(mod_lfo_str));
//     startParameterDisplay();
//   }
//   switch (keyMode) {
//     case 0:
//       sendCustomSysEx((midiOutCh - 1), 0x2B, mod_lfo);
//       break;

//     case 1:
//       sendCustomSysEx((midiOutCh - 1), 0x22, mod_lfo);
//       break;

//     case 2:
//       sendCustomSysEx((midiOutCh - 1), 0x22, mod_lfo);
//       delay(20);
//       sendCustomSysEx((midiOutCh - 1), 0x2B, mod_lfo);
//       break;
//   }
// }

// FLASHMEM void updatebend_range(bool announce) {
//   bend_range_str = map(bend_range, 0, 127, 0, 4);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     switch (bend_range_str) {
//       case 0:
//         showCurrentParameterPage("BEND RANGE", "2 Semitones");
//         break;

//       case 1:
//         showCurrentParameterPage("BEND RANGE", "3 Semitones");
//         break;

//       case 2:
//         showCurrentParameterPage("BEND RANGE", "4 Semitones");
//         break;

//       case 3:
//         showCurrentParameterPage("BEND RANGE", "7 Semitones");
//         break;

//       case 4:
//         showCurrentParameterPage("BEND RANGE", "12 Semitones");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (bend_range_str) {
//     case 0:
//       sendCustomSysEx((midiOutCh - 1), 0x17, 0x00);
//       break;

//     case 1:
//       sendCustomSysEx((midiOutCh - 1), 0x17, 0x20);
//       break;

//     case 2:
//       sendCustomSysEx((midiOutCh - 1), 0x17, 0x40);
//       break;

//     case 3:
//       if (set10ctave) {
//         sendCustomSysEx((midiOutCh - 1), 0x34, 0x00);
//         delay(20);
//         set10ctave = false;
//       }
//       sendCustomSysEx((midiOutCh - 1), 0x17, 0x60);
//       break;

//     case 4:
//       sendCustomSysEx((midiOutCh - 1), 0x34, 0x01);
//       set10ctave = true;
//       break;
//   }
// }

FLASHMEM void updatelfo1_rate(bool announce) {
  sendOffset(0x0C, 0xA3);
  lfo1_rate_str = map(lfo1_rate_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("LFO1 RATE", String(lfo1_rate_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0xA3, (uint8_t)upperData[P_lfo1_rate]);
  } else {
    sendParamValue(0xA3, (uint8_t)lowerData[P_lfo1_rate]);
  }
}

FLASHMEM void updatelfo1_delay(bool announce) {
  sendOffset(0x0C, 0xA2);
  lfo1_delay_str = map(lfo1_delay_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("LFO1 DELAY", String(lfo1_delay_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0xA2, (uint8_t)upperData[P_lfo1_delay]);
  } else {
    sendParamValue(0xA2, (uint8_t)lowerData[P_lfo1_delay]);
  }
}

FLASHMEM void updatelfo1_lfo2(bool announce) {
  sendOffset(0x0C, 0xA4);
  lfo1_lfo2_str = map(lfo1_lfo2_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("LFO1 TO LFO2", String(lfo1_lfo2_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0xA4, (uint8_t)upperData[P_lfo1_lfo2]);
  } else {
    sendParamValue(0xA4, (uint8_t)lowerData[P_lfo1_lfo2]);
  }
}

FLASHMEM void updatedco1_PW(bool announce) {
  sendOffset(0x0C, 0x8A);
  dco1_PW_str = map(dco1_PW_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("DCO1 PW", String(dco1_PW_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0x8A, (uint8_t)upperData[P_dco1_PW]);
  } else {
    sendParamValue(0x8A, (uint8_t)lowerData[P_dco1_PW]);
  }
}

FLASHMEM void updatedco1_PWM_env(bool announce) {
  sendOffset(0x0C, 0x8B);
  dco1_PWM_env_str = map(dco1_PWM_env_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("DCO1 PWM ENV", String(dco1_PWM_env_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0x8B, (uint8_t)upperData[P_dco1_PWM_env]);
  } else {
    sendParamValue(0x8B, (uint8_t)lowerData[P_dco1_PWM_env]);
  }
}

FLASHMEM void updatedco1_PWM_lfo(bool announce) {
  sendOffset(0x0C, 0x8C);
  dco1_PWM_lfo_str = map(dco1_PWM_lfo_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("DCO1 PWM LFO", String(dco1_PWM_lfo_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0x8C, (uint8_t)upperData[P_dco1_PWM_lfo]);
  } else {
    sendParamValue(0x8C, (uint8_t)lowerData[P_dco1_PWM_lfo]);
  }
}

// FLASHMEM void updatedco1_pitch_env(bool announce) {
//   dco1_pitch_env_str = map(dco1_pitch_env, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("DCO1 ENV", String(dco1_pitch_env_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco1_pitch_env, dco1_pitch_env);
// }

// FLASHMEM void updatedco1_pitch_lfo(bool announce) {
//   dco1_pitch_lfo_str = map(dco1_pitch_lfo, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("DCO1 LFO", String(dco1_pitch_lfo));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco1_pitch_lfo, dco1_pitch_lfo);
// }

// FLASHMEM void updatedco1_wave(bool announce) {
//   dco1_wave_str = map(dco1_wave, 0, 127, 0, 3);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_wave_str) {
//       case 0:
//         showCurrentParameterPage("DCO1 WAVEFORM", "NOIS");
//         break;

//       case 1:
//         showCurrentParameterPage("DCO1 WAVEFORM", "SQR");
//         break;

//       case 2:
//         showCurrentParameterPage("DCO1 WAVEFORM", "PWM");
//         break;

//       case 3:
//         showCurrentParameterPage("DCO1 WAVEFORM", "SAW");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_wave_str) {
//     case 0:
//       midiCCOut(CCdco1_wave, 0x00);
//       break;

//     case 1:
//       midiCCOut(CCdco1_wave, 0x20);
//       break;

//     case 2:
//       midiCCOut(CCdco1_wave, 0x40);
//       break;

//     case 3:
//       midiCCOut(CCdco1_wave, 0x60);
//       break;
//   }
// }

// FLASHMEM void updatedco1_range(bool announce) {
//   dco1_range_str = map(dco1_range, 0, 127, 0, 3);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_range_str) {
//       case 0:
//         showCurrentParameterPage("DCO1 RANGE", "16");
//         break;

//       case 1:
//         showCurrentParameterPage("DCO1 RANGE", "8");
//         break;

//       case 2:
//         showCurrentParameterPage("DCO1 RANGE", "4");
//         break;

//       case 3:
//         showCurrentParameterPage("DCO1 RANGE", "2");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_range_str) {
//     case 0:
//       midiCCOut(CCdco1_range, 0x00);
//       break;

//     case 1:
//       midiCCOut(CCdco1_range, 0x20);
//       break;

//     case 2:
//       midiCCOut(CCdco1_range, 0x40);
//       break;

//     case 3:
//       midiCCOut(CCdco1_range, 0x60);
//       break;
//   }
// }

// FLASHMEM void updatedco1_tune(bool announce) {
//   dco1_tune_str = map(dco1_tune, 0, 127, -12, 12);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("DCO1 TUNING", String(dco1_tune_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco1_tune, dco1_tune);
// }

// FLASHMEM void updatedco1_mode(bool announce) {
//   dco1_mode_str = map(dco1_mode, 0, 127, 0, 3);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_mode_str) {
//       case 0:
//         showCurrentParameterPage("DCO XMOD", "OFF");
//         break;

//       case 1:
//         showCurrentParameterPage("DCO XMOD", "SYN1");
//         break;

//       case 2:
//         showCurrentParameterPage("DCO XMOD", "SYN2");
//         break;

//       case 3:
//         showCurrentParameterPage("DCO XMOD", "XMOD");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_mode_str) {
//     case 0:
//       midiCCOut(CCdco1_mode, 0x00);
//       break;

//     case 1:
//       midiCCOut(CCdco1_mode, 0x20);
//       break;

//     case 2:
//       midiCCOut(CCdco1_mode, 0x40);
//       break;

//     case 3:
//       midiCCOut(CCdco1_mode, 0x60);
//       break;
//   }
// }

FLASHMEM void updatelfo2_wave(bool announce) {
  sendOffset(0x0D, 0xA1);
  if (announce && !suppressParamAnnounce) {
    lfo2_wave_str = map(lfo2_wave_str, 0, 127, 0, 4);
    displayMode = 0;
    switch (lfo2_wave_str) {
      case 0:
        showCurrentParameterPage("LFO2 WAVEFORM", "RAND");
        break;

      case 1:
        showCurrentParameterPage("LFO2 WAVEFORM", "SQR");
        break;

      case 2:
        showCurrentParameterPage("LFO2 WAVEFORM", "SAW-");
        break;

      case 3:
        showCurrentParameterPage("LFO2 WAVEFORM", "SAW+");
        break;

      case 4:
        showCurrentParameterPage("LFO2 WAVEFORM", "SINE");
        break;
    }
    startParameterDisplay();
  }
  if (upperSW) {
    switch (upperData[P_lfo2_wave]) {
      case 0:
        sendParamValue(0xA1, 0x00);
        break;

      case 1:
        sendParamValue(0xA1, 0x10);
        break;

      case 2:
        sendParamValue(0xA1, 0x20);
        break;

      case 3:
        sendParamValue(0xA1, 0x30);
        break;

      case 4:
        sendParamValue(0xA1, 0x40);
        break;
    }
  } else {
    switch (lowerData[P_lfo2_wave]) {
      case 0:
        sendParamValue(0xA1, 0x00);
        break;

      case 1:
        sendParamValue(0xA1, 0x10);
        break;

      case 2:
        sendParamValue(0xA1, 0x20);
        break;

      case 3:
        sendParamValue(0xA1, 0x30);
        break;

      case 4:
        sendParamValue(0xA1, 0x40);
        break;
    }
  }
}

FLASHMEM void updatelfo2_rate(bool announce) {
  sendOffset(0x0D, 0xA3);
  lfo2_rate_str = map(lfo2_rate_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("LFO2 RATE", String(lfo2_rate_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0xA3, (uint8_t)upperData[P_lfo2_rate]);
  } else {
    sendParamValue(0xA3, (uint8_t)lowerData[P_lfo2_rate]);
  }
}

FLASHMEM void updatelfo2_delay(bool announce) {
  sendOffset(0x0D, 0xA2);
  lfo2_delay_str = map(lfo2_delay_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("LFO2 DELAY", String(lfo2_delay_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0xA2, (uint8_t)upperData[P_lfo2_delay]);
  } else {
    sendParamValue(0xA2, (uint8_t)lowerData[P_lfo2_delay]);
  }
}

FLASHMEM void updatelfo2_lfo1(bool announce) {
  sendOffset(0x0D, 0xA4);
  lfo2_lfo1_str = map(lfo2_lfo1_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("LFO2 TO LFO1", String(lfo2_lfo1_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0xA4, (uint8_t)upperData[P_lfo2_lfo1]);
  } else {
    sendParamValue(0xA4, (uint8_t)lowerData[P_lfo2_lfo1]);
  }
}

FLASHMEM void updatedco2_PW(bool announce) {
  sendOffset(0x0D, 0x8A);
  dco2_PW_str = map(dco2_PW_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("DCO2 PW", String(dco2_PW_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0x8A, (uint8_t)upperData[P_dco2_PW]);
  } else {
    sendParamValue(0x8A, (uint8_t)lowerData[P_dco2_PW]);
  }
}

FLASHMEM void updatedco2_PWM_env(bool announce) {
  sendOffset(0x0D, 0x8B);
  dco2_PWM_env_str = map(dco2_PWM_env_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("DCO2 PWM ENV", String(dco2_PWM_env_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0x8B, (uint8_t)upperData[P_dco2_PWM_env]);
  } else {
    sendParamValue(0x8B, (uint8_t)lowerData[P_dco2_PWM_env]);
  }
}

FLASHMEM void updatedco2_PWM_lfo(bool announce) {
  sendOffset(0x0D, 0x8C);
  dco2_PWM_lfo_str = map(dco2_PWM_lfo_str, 0, 127, 0, 99);
  if (announce && !suppressParamAnnounce) {
    displayMode = 0;
    showCurrentParameterPage("DCO2 PWM LFO", String(dco2_PWM_lfo_str));
    startParameterDisplay();
  }
  if (upperSW) {
    sendParamValue(0x8C, (uint8_t)upperData[P_dco2_PWM_lfo]);
  } else {
    sendParamValue(0x8C, (uint8_t)lowerData[P_dco2_PWM_lfo]);
  }
}

// FLASHMEM void updatedco2_pitch_env(bool announce) {
//   dco2_pitch_env_str = map(dco2_pitch_env, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("DCO2 ENV", String(dco2_pitch_env_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco2_pitch_env, dco2_pitch_env);
// }

// FLASHMEM void updatedco2_pitch_lfo(bool announce) {
//   dco2_pitch_lfo_str = map(dco2_pitch_lfo, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("DCO2 LFO", String(dco2_pitch_lfo_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco2_pitch_lfo, dco2_pitch_lfo);
// }

// FLASHMEM void updatedco2_wave(bool announce) {
//   dco2_wave_str = map(dco2_wave, 0, 127, 0, 3);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_wave_str) {
//       case 0:
//         showCurrentParameterPage("DCO2 WAVEFORM", "NOIS");
//         break;

//       case 1:
//         showCurrentParameterPage("DCO2 WAVEFORM", "SQR");
//         break;

//       case 2:
//         showCurrentParameterPage("DCO2 WAVEFORM", "PWM");
//         break;

//       case 3:
//         showCurrentParameterPage("DCO2 WAVEFORM", "SAW");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_wave_str) {
//     case 0:
//       midiCCOut(CCdco2_wave, 0x00);
//       break;

//     case 1:
//       midiCCOut(CCdco2_wave, 0x20);
//       break;

//     case 2:
//       midiCCOut(CCdco2_wave, 0x40);
//       break;

//     case 3:
//       midiCCOut(CCdco2_wave, 0x60);
//       break;
//   }
// }

// FLASHMEM void updatedco2_range(bool announce) {
//   dco2_range_str = map(dco2_range, 0, 127, 0, 3);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_range_str) {
//       case 0:
//         showCurrentParameterPage("DCO2 RANGE", "16");
//         break;

//       case 1:
//         showCurrentParameterPage("DCO2 RANGE", "8");
//         break;

//       case 2:
//         showCurrentParameterPage("DCO2 RANGE", "4");
//         break;

//       case 3:
//         showCurrentParameterPage("DCO2 RANGE", "2");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_range_str) {
//     case 0:
//       midiCCOut(CCdco2_range, 0x00);
//       break;

//     case 1:
//       midiCCOut(CCdco2_range, 0x20);
//       break;

//     case 2:
//       midiCCOut(CCdco2_range, 0x40);
//       break;

//     case 3:
//       midiCCOut(CCdco2_range, 0x60);
//       break;
//   }
// }

// FLASHMEM void updatedco2_tune(bool announce) {
//   dco2_tune_str = map(dco2_tune, 0, 127, -12, 12);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("DCO2 TUNING", String(dco2_tune_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco2_tune, dco2_tune);
// }

// FLASHMEM void updatedco2_fine(bool announce) {
//   dco2_fine_str = map(dco2_fine, 0, 127, -50, 50);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     if (dco2_fine_str > 0) {
//       showCurrentParameterPage("DCO2 FINE", "+" + String(dco2_fine_str));
//     } else {
//       showCurrentParameterPage("DCO2 FINE", String(dco2_fine_str));
//     }
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco2_fine, dco2_fine);
// }

// FLASHMEM void updatedco1_level(bool announce) {
//   dco1_level_str = map(dco1_level, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("MIX DCO1", String(dco1_level_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco1_level, dco1_level);
// }

// FLASHMEM void updatedco2_level(bool announce) {
//   dco2_level_str = map(dco2_level, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("MIX DCO2", String(dco2_level_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco2_level, dco2_level);
// }

// FLASHMEM void updatedco2_mod(bool announce) {
//   dco2_mod_str = map(dco2_mod, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("MIX ENV", String(dco2_mod_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdco2_mod, dco2_mod);
// }

// FLASHMEM void updatevcf_hpf(bool announce) {
//   vcf_hpf_str = map(vcf_hpf, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF HPF", String(vcf_hpf_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_hpf, vcf_hpf);
// }

// FLASHMEM void updatevcf_cutoff(bool announce) {
//   vcf_cutoff_str = map(vcf_cutoff, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF FREQ", String(vcf_cutoff_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_cutoff, vcf_cutoff);
// }

// FLASHMEM void updatevcf_res(bool announce) {
//   vcf_res_str = map(vcf_res, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF RES", String(vcf_res_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_res, vcf_res);
// }

// FLASHMEM void updatevcf_kb(bool announce) {
//   vcf_kb_str = map(vcf_kb, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF KEY", String(vcf_kb_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_kb, vcf_kb);
// }

// FLASHMEM void updatevcf_env(bool announce) {
//   vcf_env_str = map(vcf_env, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF ENV", String(vcf_env_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_env, vcf_env);
// }

// FLASHMEM void updatevcf_lfo1(bool announce) {
//   vcf_lfo1_str = map(vcf_lfo1, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF LFO1", String(vcf_lfo1_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_lfo1, vcf_lfo1);
// }

// FLASHMEM void updatevcf_lfo2(bool announce) {
//   vcf_lfo2_str = map(vcf_lfo2, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCF LFO2", String(vcf_lfo2_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvcf_lfo2, vcf_lfo2);
// }

// FLASHMEM void updatevca_mod(bool announce) {
//   vca_mod_str = map(vca_mod, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("VCA LEVEL", String(vca_mod_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCvca_mod, vca_mod);
// }

// FLASHMEM void updateat_vib(bool announce) {
//   at_vib_str = map(at_vib, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("AT VIB", String(at_vib_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x1A, at_vib);
// }

// FLASHMEM void updateat_lpf(bool announce) {
//   at_lpf_str = map(at_lpf, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("AT VCF", String(at_lpf_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x1B, at_lpf);
// }

// FLASHMEM void updateat_vol(bool announce) {
//   at_vol_str = map(at_vol, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("AT VOL", String(at_vol_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x1C, at_vol);
// }

// FLASHMEM void updatebalance(bool announce) {
//   balance_str = map(balance, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("BALANCE", String(balance_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x12, balance);
// }

// FLASHMEM void updateportamento(bool announce) {
//   portamento_str = map(portamento, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("PORTAMENTO", String(portamento_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x16, portamento);
// }

// FLASHMEM void updatevolume(bool announce) {
//   volume_str = map(volume, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("VOLUME", String(volume_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x19, volume);
// }

// FLASHMEM void updatetime1(bool announce) {
//   time1_str = map(time1, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 T1", String(time1_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCtime1, time1);
// }

// FLASHMEM void updatelevel1(bool announce) {
//   level1_str = map(level1, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 L1", String(level1_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CClevel1, level1);
// }

// FLASHMEM void updatetime2(bool announce) {
//   time2_str = map(time2, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 T2", String(time2_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCtime2, time2);
// }

// FLASHMEM void updatelevel2(bool announce) {
//   level2_str = map(level2, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 L2", String(level2_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CClevel2, level2);
// }

// FLASHMEM void updatetime3(bool announce) {
//   time3_str = map(time3, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 T3", String(time3_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCtime3, time3);
// }

// FLASHMEM void updatelevel3(bool announce) {
//   level3_str = map(level3, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 L3", String(level3_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CClevel3, level3);
// }

// FLASHMEM void updatetime4(bool announce) {
//   time4_str = map(time4, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV1 T4", String(time4_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCtime4, time4);
// }

// FLASHMEM void updateenv5stage_mode(bool announce) {
//   env5stage_mode_str = map(env5stage_mode, 0, 127, 0, 7);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (env5stage_mode_str) {
//       case 0:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "Off");
//         break;

//       case 1:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "KEY 1");
//         break;

//       case 2:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "KEY 2");
//         break;

//       case 3:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "KEY 3");
//         break;

//       case 4:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "LOOP 0");
//         break;

//       case 5:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "LOOP 1");
//         break;

//       case 6:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "LOOP 2");
//         break;

//       case 7:
//         showCurrentParameterPage("ENV1 KEY FOLLOW.", "LOOP 3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (env5stage_mode_str) {
//     case 0:
//       midiCCOut(CC5stage_mode, 0x00);
//       break;

//     case 1:
//       midiCCOut(CC5stage_mode, 0x10);
//       break;

//     case 2:
//       midiCCOut(CC5stage_mode, 0x20);
//       break;

//     case 3:
//       midiCCOut(CC5stage_mode, 0x30);
//       break;

//     case 4:
//       midiCCOut(CC5stage_mode, 0x40);
//       break;

//     case 5:
//       midiCCOut(CC5stage_mode, 0x50);
//       break;

//     case 6:
//       midiCCOut(CC5stage_mode, 0x60);
//       break;

//     case 7:
//       midiCCOut(CC5stage_mode, 0x70);
//       break;
//   }
// }

// FLASHMEM void updateenv2_time1(bool announce) {
//   env2_time1_str = map(env2_time1, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 T1", String(env2_time1_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2time1, env2_time1);
// }

// FLASHMEM void updateenv2_level1(bool announce) {
//   env2_level1_str = map(env2_level1, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 L1", String(env2_level1_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2level1, env2_level1);
// }

// FLASHMEM void updateenv2_time2(bool announce) {
//   env2_time2_str = map(env2_time2, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 T2", String(env2_time2_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2time2, env2_time2);
// }

// FLASHMEM void updateenv2_level2(bool announce) {
//   env2_level2_str = map(env2_level2, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 L2", String(env2_level2_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2level2, env2_level2);
// }

// FLASHMEM void updateenv2_time3(bool announce) {
//   env2_time3_str = map(env2_time3, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 T3", String(env2_time3_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2time3, env2_time3);
// }

// FLASHMEM void updateenv2_level3(bool announce) {
//   env2_level3_str = map(env2_level3, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 L3", String(env2_level3_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2level3, env2_level3);
// }

// FLASHMEM void updateenv2_time4(bool announce) {
//   env2_time4_str = map(env2_time4, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV2 T4", String(env2_time4_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC2time4, env2_time4);
// }

// FLASHMEM void updateenv2_env5stage_mode(bool announce) {
//   env2_5stage_mode_str = map(env2_5stage_mode, 0, 127, 0, 7);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (env2_5stage_mode_str) {
//       case 0:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "Off");
//         break;

//       case 1:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "KEY 1");
//         break;

//       case 2:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "KEY 2");
//         break;

//       case 3:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "KEY 3");
//         break;

//       case 4:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "LOOP 0");
//         break;

//       case 5:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "LOOP 1");
//         break;

//       case 6:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "LOOP 2");
//         break;

//       case 7:
//         showCurrentParameterPage("ENV2 KEY FOLLOW.", "LOOP 3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (env2_5stage_mode_str) {
//     case 0:
//       midiCCOut(CC25stage_mode, 0x00);
//       break;

//     case 1:
//       midiCCOut(CC25stage_mode, 0x10);
//       break;

//     case 2:
//       midiCCOut(CC25stage_mode, 0x20);
//       break;

//     case 3:
//       midiCCOut(CC25stage_mode, 0x30);
//       break;

//     case 4:
//       midiCCOut(CC25stage_mode, 0x40);
//       break;

//     case 5:
//       midiCCOut(CC25stage_mode, 0x50);
//       break;

//     case 6:
//       midiCCOut(CC25stage_mode, 0x60);
//       break;

//     case 7:
//       midiCCOut(CC25stage_mode, 0x70);
//       break;
//   }
// }

// FLASHMEM void updateattack(bool announce) {
//   attack_str = map(attack, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV3 ATTACK", String(attack_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCattack, attack);
// }

// FLASHMEM void updatedecay(bool announce) {
//   decay_str = map(decay, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV3 DECAY", String(decay_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCdecay, decay);
// }

// FLASHMEM void updatesustain(bool announce) {
//   sustain_str = map(sustain, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV3 SUSTAIN", String(sustain_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCsustain, sustain);
// }

// FLASHMEM void updaterelease(bool announce) {
//   release_str = map(release, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV3 RELEASE", String(release_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CCrelease, release);
// }

// FLASHMEM void updateadsr_mode(bool announce) {
//   adsr_mode_str = map(adsr_mode, 0, 127, 0, 7);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (adsr_mode_str) {
//       case 0:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "Off");
//         break;

//       case 1:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "KEY 1");
//         break;

//       case 2:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "KEY 2");
//         break;

//       case 3:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "KEY 3");
//         break;

//       case 4:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "LOOP 0");
//         break;

//       case 5:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "LOOP 1");
//         break;

//       case 6:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "LOOP 2");
//         break;

//       case 7:
//         showCurrentParameterPage("ENV3 KEY FOLLOW.", "LOOP 3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (adsr_mode_str) {
//     case 0:
//       midiCCOut(CCadsr_mode, 0x00);
//       break;

//     case 1:
//       midiCCOut(CCadsr_mode, 0x10);
//       break;

//     case 2:
//       midiCCOut(CCadsr_mode, 0x20);
//       break;

//     case 3:
//       midiCCOut(CCadsr_mode, 0x30);
//       break;

//     case 4:
//       midiCCOut(CCadsr_mode, 0x40);
//       break;

//     case 5:
//       midiCCOut(CCadsr_mode, 0x50);
//       break;

//     case 6:
//       midiCCOut(CCadsr_mode, 0x60);
//       break;

//     case 7:
//       midiCCOut(CCadsr_mode, 0x70);
//       break;
//   }
// }

// FLASHMEM void updateenv4_attack(bool announce) {
//   env4_attack_str = map(env4_attack, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV4 ATTACK", String(env4_attack_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC4attack, env4_attack);
// }

// FLASHMEM void updateenv4_decay(bool announce) {
//   env4_decay_str = map(env4_decay, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV4 DECAY", String(env4_decay_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC4decay, env4_decay);
// }

// FLASHMEM void updateenv4_sustain(bool announce) {
//   env4_sustain_str = map(env4_sustain, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV4 SUSTAIN", String(env4_sustain_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC4sustain, env4_sustain);
// }

// FLASHMEM void updateenv4_release(bool announce) {
//   env4_release_str = map(env4_release, 0, 127, 0, 99);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     showCurrentParameterPage("ENV4 RELEASE", String(env4_release_str));
//     startParameterDisplay();
//   }
//   midiCCOut(CC4release, env4_release);
// }

// FLASHMEM void updateenv4_adsr_mode(bool announce) {
//   env4_adsr_mode_str = map(env4_adsr_mode, 0, 127, 0, 7);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (env4_adsr_mode_str) {
//       case 0:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "Off");
//         break;

//       case 1:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "KEY 1");
//         break;

//       case 2:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "KEY 2");
//         break;

//       case 3:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "KEY 3");
//         break;

//       case 4:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "LOOP 0");
//         break;

//       case 5:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "LOOP 1");
//         break;

//       case 6:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "LOOP 2");
//         break;

//       case 7:
//         showCurrentParameterPage("ENV4 KEY FOLLOW.", "LOOP 3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (env4_adsr_mode_str) {
//     case 0:
//       midiCCOut(CC4adsr_mode, 0x00);
//       break;

//     case 1:
//       midiCCOut(CC4adsr_mode, 0x10);
//       break;

//     case 2:
//       midiCCOut(CC4adsr_mode, 0x20);
//       break;

//     case 3:
//       midiCCOut(CC4adsr_mode, 0x30);
//       break;

//     case 4:
//       midiCCOut(CC4adsr_mode, 0x40);
//       break;

//     case 5:
//       midiCCOut(CC4adsr_mode, 0x50);
//       break;

//     case 6:
//       midiCCOut(CC4adsr_mode, 0x60);
//       break;

//     case 7:
//       midiCCOut(CC4adsr_mode, 0x70);
//       break;
//   }
// }

// FLASHMEM void updatedualdetune(bool announce) {
//   dualdetune_str = map(dualdetune, 0, 127, -50, 50);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("DUAL DETUNE", String(dualdetune_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x13, dualdetune);
// }

// FLASHMEM void updateunisondetune(bool announce) {
//   unisondetune_str = map(unisondetune, 0, 127, -50, 50);
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     showCurrentParameterPage("UNISON DETUNE", String(unisondetune_str));
//     startParameterDisplay();
//   }
//   sendCustomSysEx((midiOutCh - 1), 0x13, unisondetune);
// }

// // Buttons

// FLASHMEM void updateoctave_down(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     switch (octave_down) {
//       case 0:
//         showCurrentParameterPage("OCTAVE SHIFT", "0 Semitones");
//         break;
//       case 1:
//         showCurrentParameterPage("OCTAVE SHIFT", "-12 Semitones");
//         break;
//       case 2:
//         showCurrentParameterPage("OCTAVE SHIFT", "-24 Semitones");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (octave_down) {
//     case 0:
//       switch (keyMode) {
//         case 0:
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x00);
//           break;

//         case 1:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x00);
//           break;

//         case 2:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x00);
//           delay(20);
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x00);
//           break;
//       }
//       mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
//       break;
//     case 1:
//       switch (keyMode) {
//         case 0:
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x74);
//           break;

//         case 1:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x74);
//           break;

//         case 2:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x74);
//           delay(20);
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x74);
//           break;
//       }
//       mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_DOWN_GREEN, HIGH);
//       mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
//       break;
//     case 2:
//       switch (keyMode) {
//         case 0:
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x68);
//           break;

//         case 1:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x68);
//           break;

//         case 2:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x68);
//           delay(20);
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x68);
//           break;
//       }
//       mcp7.digitalWrite(OCTAVE_DOWN_RED, HIGH);
//       mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
//       break;
//   }
// }

// FLASHMEM void updateoctave_up(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     switch (octave_up) {
//       case 0:
//         showCurrentParameterPage("OCTAVE SHIFT", "0 Semitones");
//         break;
//       case 1:
//         showCurrentParameterPage("OCTAVE SHIFT", "+12 Semitones");
//         break;
//       case 2:
//         showCurrentParameterPage("OCTAVE SHIFT", "+24 Semitones");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (octave_up) {
//     case 0:
//       switch (keyMode) {
//         case 0:
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x00);
//           break;

//         case 1:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x00);
//           break;

//         case 2:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x00);
//           delay(20);
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x00);
//           break;
//       }
//       mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
//       break;
//     case 1:
//       switch (keyMode) {
//         case 0:
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x0C);
//           break;

//         case 1:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x0C);
//           break;

//         case 2:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x0C);
//           delay(20);
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x0C);
//           break;
//       }
//       mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_RED, HIGH);
//       mcp7.digitalWrite(OCTAVE_UP_GREEN, LOW);
//       break;
//     case 2:
//       switch (keyMode) {
//         case 0:
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x18);
//           break;

//         case 1:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x18);
//           break;

//         case 2:
//           sendCustomSysEx((midiOutCh - 1), 0x1E, 0x18);
//           delay(20);
//           sendCustomSysEx((midiOutCh - 1), 0x27, 0x18);
//           break;
//       }
//       mcp7.digitalWrite(OCTAVE_DOWN_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_DOWN_GREEN, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_RED, LOW);
//       mcp7.digitalWrite(OCTAVE_UP_GREEN, HIGH);
//       break;
//   }
// }

// Keymode buttons

FLASHMEM void updateassignMode(bool announce) {
}

FLASHMEM void updatekeyMode(bool announce) {
  updatedual_button(0);
  updatesplit_button(0);
  updatesingle_button(0);
  updatespecial_button(0);
}

FLASHMEM void updatedual_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("KEY MODE", "DUAL");
    startParameterDisplay();
  }
  mcp8.digitalWrite(KEY_DUAL_RED, HIGH);
  mcp8.digitalWrite(KEY_SPLIT_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
  mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
  if (upperSW) {
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
  } else {
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
  }
  keyMode = 0;
}

FLASHMEM void updatesplit_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    showCurrentParameterPage("KEY MODE", "SPLIT");
    startParameterDisplay();
  }
  mcp8.digitalWrite(KEY_DUAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPLIT_RED, HIGH);
  mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
  mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
  if (upperSW) {
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
  } else {
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
  }
  keyMode = 3;
}

FLASHMEM void updatesingle_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!single_button) {
      showCurrentParameterPage("KEY MODE", "SINGLE LOWER");
    } else {
      showCurrentParameterPage("KEY MODE", "SINGLE UPPER");
    }
    startParameterDisplay();
  }
  if (!single_button) {
    mcp10.digitalWrite(KEY_SINGLE_RED, HIGH);
    mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
    for (int i = 1; i <= 100; i++) {
      upperData[i] = lowerData[i];
    }
    keyMode = 1;
    updateUpperToneData();    
  } else {
    mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
    mcp10.digitalWrite(KEY_SINGLE_GREEN, HIGH);
    for (int i = 1; i <= 100; i++) {
      lowerData[i] = upperData[i];
    }
    keyMode = 2;
    updateLowerToneData();
  }
  mcp8.digitalWrite(KEY_DUAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPLIT_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
  mcp10.digitalWrite(LOWER_SELECT, HIGH);
  mcp10.digitalWrite(UPPER_SELECT, HIGH);
}

FLASHMEM void updatespecial_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!special_button) {
      showCurrentParameterPage("KEY MODE", "X-FADE");
    } else {
      showCurrentParameterPage("KEY MODE", "T-VOICE");
    }
    startParameterDisplay();
  }
  if (!special_button) {
    mcp8.digitalWrite(KEY_SPECIAL_RED, LOW);
    mcp8.digitalWrite(KEY_SPECIAL_GREEN, HIGH);
    keyMode = 5;
  } else {
    mcp8.digitalWrite(KEY_SPECIAL_RED, HIGH);
    mcp8.digitalWrite(KEY_SPECIAL_GREEN, LOW);
    keyMode = 4;
  }
  mcp8.digitalWrite(KEY_DUAL_RED, LOW);
  mcp8.digitalWrite(KEY_SPLIT_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_RED, LOW);
  mcp10.digitalWrite(KEY_SINGLE_GREEN, LOW);
  if (upperSW) {
    mcp10.digitalWrite(UPPER_SELECT, HIGH);
    mcp10.digitalWrite(LOWER_SELECT, LOW);
  } else {
    mcp10.digitalWrite(LOWER_SELECT, HIGH);
    mcp10.digitalWrite(UPPER_SELECT, LOW);
  }
}

// Assigner buttons

FLASHMEM void updatepoly_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!poly_button) {
      showCurrentParameterPage("ASSIGN MODE", "POLY 1");
    } else {
      showCurrentParameterPage("ASSIGN MODE", "POLY 2");
    }
    startParameterDisplay();
  }
  if (!poly_button) {
    assignMode = 0;
    mcp8.digitalWrite(ASSIGN_POLY_RED, HIGH);
    mcp8.digitalWrite(ASSIGN_POLY_GREEN, LOW);
  } else {
    assignMode = 1;
    mcp8.digitalWrite(ASSIGN_POLY_RED, LOW);
    mcp8.digitalWrite(ASSIGN_POLY_GREEN, HIGH);
  }
  mcp8.digitalWrite(ASSIGN_MONO_RED, LOW);
  mcp8.digitalWrite(ASSIGN_MONO_GREEN, LOW);
  mcp8.digitalWrite(ASSIGN_UNI_RED, LOW);
  mcp8.digitalWrite(ASSIGN_UNI_GREEN, LOW);
}

FLASHMEM void updatemono_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!mono_button) {
      showCurrentParameterPage("ASSIGN MODE", "MONO 1");
    } else {
      showCurrentParameterPage("ASSIGN MODE", "MONO 2");
    }
    startParameterDisplay();
  }
  if (!mono_button) {
    assignMode = 2;
    mcp8.digitalWrite(ASSIGN_MONO_RED, HIGH);
    mcp8.digitalWrite(ASSIGN_MONO_GREEN, LOW);
  } else {
    assignMode = 3;
    mcp8.digitalWrite(ASSIGN_MONO_RED, LOW);
    mcp8.digitalWrite(ASSIGN_MONO_GREEN, HIGH);
  }
  mcp8.digitalWrite(ASSIGN_POLY_RED, LOW);
  mcp8.digitalWrite(ASSIGN_POLY_GREEN, LOW);
  mcp8.digitalWrite(ASSIGN_UNI_RED, LOW);
  mcp8.digitalWrite(ASSIGN_UNI_GREEN, LOW);
}

FLASHMEM void updateunison_button(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 1;
    if (!unison_button) {
      showCurrentParameterPage("ASSIGN MODE", "UNISON 1");
    } else {
      showCurrentParameterPage("ASSIGN MODE", "UNISON 2");
    }
    startParameterDisplay();
  }
  if (!unison_button) {
    assignMode = 4;
    mcp8.digitalWrite(ASSIGN_UNI_RED, HIGH);
    mcp8.digitalWrite(ASSIGN_UNI_GREEN, LOW);
  } else {
    assignMode = 5;
    mcp8.digitalWrite(ASSIGN_UNI_RED, LOW);
    mcp8.digitalWrite(ASSIGN_UNI_GREEN, HIGH);
  }
  mcp8.digitalWrite(ASSIGN_POLY_RED, LOW);
  mcp8.digitalWrite(ASSIGN_POLY_GREEN, LOW);
  mcp8.digitalWrite(ASSIGN_MONO_RED, LOW);
  mcp8.digitalWrite(ASSIGN_MONO_GREEN, LOW);
}

// FLASHMEM void updatelfo1_sync(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (lfo1_sync) {
//       case 0:
//         showCurrentParameterPage("LFO1 SYNC", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("LFO1 SYNC", "ON");
//         break;
//       case 2:
//         showCurrentParameterPage("LFO1 SYNC", "KEY");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (lfo1_sync) {
//     case 0:
//       midiCCOut(CClfo1_sync, 0x00);
//       mcp1.digitalWrite(LFO1_SYNC_RED, LOW);
//       mcp1.digitalWrite(LFO1_SYNC_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CClfo1_sync, 0x20);
//       mcp1.digitalWrite(LFO1_SYNC_RED, HIGH);
//       mcp1.digitalWrite(LFO1_SYNC_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CClfo1_sync, 0x40);
//       mcp1.digitalWrite(LFO1_SYNC_RED, LOW);
//       mcp1.digitalWrite(LFO1_SYNC_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatelfo2_sync(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (lfo2_sync) {
//       case 0:
//         showCurrentParameterPage("LFO2 SYNC", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("LFO2 SYNC", "ON");
//         break;
//       case 2:
//         showCurrentParameterPage("LFO2 SYNC", "KEY");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (lfo2_sync) {
//     case 0:
//       midiCCOut(CClfo2_sync, 0x00);
//       mcp2.digitalWrite(LFO2_SYNC_RED, LOW);
//       mcp2.digitalWrite(LFO2_SYNC_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CClfo2_sync, 0x20);
//       mcp2.digitalWrite(LFO2_SYNC_RED, HIGH);
//       mcp2.digitalWrite(LFO2_SYNC_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CClfo2_sync, 0x40);
//       mcp2.digitalWrite(LFO2_SYNC_RED, LOW);
//       mcp2.digitalWrite(LFO2_SYNC_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco1_PWM_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_PWM_dyn) {
//       case 0:
//         showCurrentParameterPage("DCO1 PWM", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO1 PWM", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO1 PWM", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO1 PWM", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_PWM_dyn) {
//     case 0:
//       midiCCOut(CCdco1_PWM_dyn, 0x00);
//       mcp1.digitalWrite(DCO1_PWM_DYN_RED, LOW);
//       mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco1_PWM_dyn, 0x20);
//       mcp1.digitalWrite(DCO1_PWM_DYN_RED, HIGH);
//       mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco1_PWM_dyn, 0x40);
//       mcp1.digitalWrite(DCO1_PWM_DYN_RED, LOW);
//       mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco1_PWM_dyn, 0x60);
//       mcp1.digitalWrite(DCO1_PWM_DYN_RED, HIGH);
//       mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco2_PWM_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_PWM_dyn) {
//       case 0:
//         showCurrentParameterPage("DCO2 PWM", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO2 PWM", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO2 PWM", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO2 PWM", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_PWM_dyn) {
//     case 0:
//       midiCCOut(CCdco2_PWM_dyn, 0x00);
//       mcp2.digitalWrite(DCO2_PWM_DYN_RED, LOW);
//       mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco2_PWM_dyn, 0x20);
//       mcp2.digitalWrite(DCO2_PWM_DYN_RED, HIGH);
//       mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco2_PWM_dyn, 0x40);
//       mcp2.digitalWrite(DCO2_PWM_DYN_RED, LOW);
//       mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco2_PWM_dyn, 0x60);
//       mcp2.digitalWrite(DCO2_PWM_DYN_RED, HIGH);
//       mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco1_PWM_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_PWM_env_source) {
//       case 0:
//         showCurrentParameterPage("DCO1 PWM", "ENV1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO1 PWM", "ENV1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO1 PWM", "ENV2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO1 PWM", "ENV2+");
//         break;
//       case 4:
//         showCurrentParameterPage("DCO1 PWM", "ENV3-");
//         break;
//       case 5:
//         showCurrentParameterPage("DCO1 PWM", "ENV3+");
//         break;
//       case 6:
//         showCurrentParameterPage("DCO1 PWM", "ENV4-");
//         break;
//       case 7:
//         showCurrentParameterPage("DCO1 PWM", "ENV4+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_PWM_env_source) {
//     case 0:
//       midiCCOut(CCdco1_PWM_env_source, 0x00);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco1_PWM_env_source, 0x10);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
//       break;
//     case 2:
//       midiCCOut(CCdco1_PWM_env_source, 0x20);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCdco1_PWM_env_source, 0x30);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
//       break;
//     case 4:
//       midiCCOut(CCdco1_PWM_env_source, 0x40);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
//       break;
//     case 5:
//       midiCCOut(CCdco1_PWM_env_source, 0x50);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
//       break;
//     case 6:
//       midiCCOut(CCdco1_PWM_env_source, 0x60);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
//       break;
//     case 7:
//       midiCCOut(CCdco1_PWM_env_source, 0x70);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
//       mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
//       mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco2_PWM_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_PWM_env_source) {
//       case 0:
//         showCurrentParameterPage("DCO2 PWM", "ENV1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO2 PWM", "ENV1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO2 PWM", "ENV2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO2 PWM", "ENV2+");
//         break;
//       case 4:
//         showCurrentParameterPage("DCO2 PWM", "ENV3-");
//         break;
//       case 5:
//         showCurrentParameterPage("DCO2 PWM", "ENV3+");
//         break;
//       case 6:
//         showCurrentParameterPage("DCO2 PWM", "ENV4-");
//         break;
//       case 7:
//         showCurrentParameterPage("DCO2 PWM", "ENV4+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_PWM_env_source) {
//     case 0:
//       midiCCOut(CCdco2_PWM_env_source, 0x00);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco2_PWM_env_source, 0x10);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
//       break;
//     case 2:
//       midiCCOut(CCdco2_PWM_env_source, 0x20);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCdco2_PWM_env_source, 0x30);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
//       break;
//     case 4:
//       midiCCOut(CCdco1_PWM_env_source, 0x40);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
//       break;
//     case 5:
//       midiCCOut(CCdco2_PWM_env_source, 0x50);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
//       break;
//     case 6:
//       midiCCOut(CCdco2_PWM_env_source, 0x60);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
//       break;
//     case 7:
//       midiCCOut(CCdco2_PWM_env_source, 0x70);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
//       mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
//       mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
//       mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco1_PWM_lfo_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_PWM_lfo_source) {
//       case 0:
//         showCurrentParameterPage("DCO1 PWM", "LFO1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO1 PWM", "LFO1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO1 PWM", "LFO2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO1 PWM", "LFO1+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_PWM_lfo_source) {
//     case 0:
//       midiCCOut(CCdco1_PWM_lfo_source, 0x00);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco1_PWM_lfo_source, 0x20);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco1_PWM_lfo_source, 0x40);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco1_PWM_lfo_source, 0x60);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco2_PWM_lfo_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_PWM_lfo_source) {
//       case 0:
//         showCurrentParameterPage("DCO2 PWM", "LFO1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO2 PWM", "LFO1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO2 PWM", "LFO2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO2 PWM", "LFO1+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_PWM_lfo_source) {
//     case 0:
//       midiCCOut(CCdco2_PWM_lfo_source, 0x00);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco2_PWM_lfo_source, 0x20);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco2_PWM_lfo_source, 0x40);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco2_PWM_lfo_source, 0x60);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco1_pitch_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_pitch_dyn) {
//       case 0:
//         showCurrentParameterPage("DCO1 PITCH", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO1 PITCH", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO1 PITCH", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO1 PITCH", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_pitch_dyn) {
//     case 0:
//       midiCCOut(CCdco1_pitch_dyn, 0x00);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco1_pitch_dyn, 0x20);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco1_pitch_dyn, 0x40);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco1_pitch_dyn, 0x60);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco2_pitch_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_pitch_dyn) {
//       case 0:
//         showCurrentParameterPage("DCO2 PITCH", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO2 PITCH", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO2 PITCH", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO2 PITCH", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_pitch_dyn) {
//     case 0:
//       midiCCOut(CCdco2_pitch_dyn, 0x00);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco2_pitch_dyn, 0x20);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco2_pitch_dyn, 0x40);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco2_pitch_dyn, 0x60);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
//       mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco1_pitch_lfo_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_pitch_lfo_source) {
//       case 0:
//         showCurrentParameterPage("DCO1 PITCH", "LFO1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO1 PITCH", "LFO1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO1 PITCH", "LFO2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO1 PITCH", "LFO2+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_pitch_lfo_source) {
//     case 0:
//       midiCCOut(CCdco1_pitch_lfo_source, 0x00);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco1_pitch_lfo_source, 0x20);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco1_pitch_lfo_source, 0x40);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco1_pitch_lfo_source, 0x60);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco2_pitch_lfo_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_pitch_lfo_source) {
//       case 0:
//         showCurrentParameterPage("DCO2 PITCH", "LFO1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO2 PITCH", "LFO1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO2 PITCH", "LFO2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO2 PITCH", "LFO2+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_pitch_lfo_source) {
//     case 0:
//       midiCCOut(CCdco2_pitch_lfo_source, 0x00);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco2_pitch_lfo_source, 0x20);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco2_pitch_lfo_source, 0x40);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco2_pitch_lfo_source, 0x60);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco1_pitch_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco1_pitch_env_source) {
//       case 0:
//         showCurrentParameterPage("DCO1 PITCH", "ENV1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO1 PITCH", "ENV1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO1 PITCH", "ENV2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO1 PITCH", "ENV2+");
//         break;
//       case 4:
//         showCurrentParameterPage("DCO1 PITCH", "ENV3-");
//         break;
//       case 5:
//         showCurrentParameterPage("DCO1 PITCH", "ENV3+");
//         break;
//       case 6:
//         showCurrentParameterPage("DCO1 PITCH", "ENV4-");
//         break;
//       case 7:
//         showCurrentParameterPage("DCO1 PITCH", "ENV4+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco1_pitch_env_source) {
//     case 0:
//       midiCCOut(CCdco1_pitch_env_source, 0x00);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco1_pitch_env_source, 0x10);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//     case 2:
//       midiCCOut(CCdco1_pitch_env_source, 0x20);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCdco1_pitch_env_source, 0x30);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//     case 4:
//       midiCCOut(CCdco1_pitch_env_source, 0x40);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 5:
//       midiCCOut(CCdco1_pitch_env_source, 0x50);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//     case 6:
//       midiCCOut(CCdco1_pitch_env_source, 0x60);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 7:
//       midiCCOut(CCdco1_pitch_env_source, 0x70);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
//       mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco2_pitch_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco2_pitch_env_source) {
//       case 0:
//         showCurrentParameterPage("DCO2 PITCH", "ENV1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO2 PITCH", "ENV1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO2 PITCH", "ENV2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO2 PITCH", "ENV2+");
//         break;
//       case 4:
//         showCurrentParameterPage("DCO2 PITCH", "ENV3-");
//         break;
//       case 5:
//         showCurrentParameterPage("DCO2 PITCH", "ENV3+");
//         break;
//       case 6:
//         showCurrentParameterPage("DCO2 PITCH", "ENV4-");
//         break;
//       case 7:
//         showCurrentParameterPage("DCO2 PITCH", "ENV4+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco2_pitch_env_source) {
//     case 0:
//       midiCCOut(CCdco2_pitch_env_source, 0x00);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco2_pitch_env_source, 0x10);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//     case 2:
//       midiCCOut(CCdco2_pitch_env_source, 0x20);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCdco2_pitch_env_source, 0x30);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//     case 4:
//       midiCCOut(CCdco2_pitch_env_source, 0x40);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 5:
//       midiCCOut(CCdco2_pitch_env_source, 0x50);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//     case 6:
//       midiCCOut(CCdco2_pitch_env_source, 0x60);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
//       break;
//     case 7:
//       midiCCOut(CCdco2_pitch_env_source, 0x70);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
//       mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
//       break;
//   }
// }

FLASHMEM void updateeditMode(bool announce) {
  if (announce && !suppressParamAnnounce) {
    displayMode = 2;
    switch (editMode) {
      case 0:
        showCurrentParameterPage("EDITING", "LOWER TONE");
        break;
      case 1:
        showCurrentParameterPage("EDITING", "UPPER TONE");
        break;
      case 2:
        showCurrentParameterPage("EDITING", "BOTH TONES");
        break;
    }
    startParameterDisplay();
  }
  switch (editMode) {
    case 0:
      upperSW = false;
      mcp10.digitalWrite(LOWER_SELECT, HIGH);
      mcp10.digitalWrite(UPPER_SELECT, LOW);
      break;
    case 1:
      upperSW = true;
      mcp10.digitalWrite(LOWER_SELECT, LOW);
      mcp10.digitalWrite(UPPER_SELECT, HIGH);
      break;
    case 2:
      mcp10.digitalWrite(LOWER_SELECT, HIGH);
      mcp10.digitalWrite(UPPER_SELECT, HIGH);
      break;
  }
}

// FLASHMEM void updatedco_mix_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco_mix_env_source) {
//       case 0:
//         showCurrentParameterPage("DCO MIX", "ENV1-");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO MIX", "ENV1+");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO MIX", "ENV2-");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO MIX", "ENV2+");
//         break;
//       case 4:
//         showCurrentParameterPage("DCO MIX", "ENV3-");
//         break;
//       case 5:
//         showCurrentParameterPage("DCO MIX", "ENV3+");
//         break;
//       case 6:
//         showCurrentParameterPage("DCO MIX", "ENV4-");
//         break;
//       case 7:
//         showCurrentParameterPage("DCO MIX", "ENV4+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco_mix_env_source) {
//     case 0:
//       midiCCOut(CCdco_mix_env_source, 0x00);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco_mix_env_source, 0x10);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
//       break;
//     case 2:
//       midiCCOut(CCdco_mix_env_source, 0x20);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCdco_mix_env_source, 0x30);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
//       break;
//     case 4:
//       midiCCOut(CCdco_mix_env_source, 0x40);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
//       break;
//     case 5:
//       midiCCOut(CCdco_mix_env_source, 0x50);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
//       break;
//     case 6:
//       midiCCOut(CCdco_mix_env_source, 0x60);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
//       break;
//     case 7:
//       midiCCOut(CCdco_mix_env_source, 0x70);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatedco_mix_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (dco_mix_dyn) {
//       case 0:
//         showCurrentParameterPage("DCO MIX", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("DCO MIX", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("DCO MIX", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("DCO MIX", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (dco_mix_dyn) {
//     case 0:
//       midiCCOut(CCdco_mix_dyn, 0x00);
//       mcp5.digitalWrite(DCO_MIX_DYN_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCdco_mix_dyn, 0x20);
//       mcp5.digitalWrite(DCO_MIX_DYN_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCdco_mix_dyn, 0x40);
//       mcp5.digitalWrite(DCO_MIX_DYN_RED, LOW);
//       mcp5.digitalWrite(DCO_MIX_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCdco_mix_dyn, 0x60);
//       mcp5.digitalWrite(DCO_MIX_DYN_RED, HIGH);
//       mcp5.digitalWrite(DCO_MIX_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatevcf_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (vcf_env_source) {
//       case 0:
//         showCurrentParameterPage("VCF EG", "ENV1-");
//         break;
//       case 1:
//         showCurrentParameterPage("VCF EG", "ENV1+");
//         break;
//       case 2:
//         showCurrentParameterPage("VCF EG", "ENV2-");
//         break;
//       case 3:
//         showCurrentParameterPage("VCF EG", "ENV2+");
//         break;
//       case 4:
//         showCurrentParameterPage("VCF EG", "ENV3-");
//         break;
//       case 5:
//         showCurrentParameterPage("VCF EG", "ENV3+");
//         break;
//       case 6:
//         showCurrentParameterPage("VCF EG", "ENV4-");
//         break;
//       case 7:
//         showCurrentParameterPage("VCF EG", "ENV4+");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vcf_env_source) {
//     case 0:
//       midiCCOut(CCvcf_env_source, 0x00);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCvcf_env_source, 0x10);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
//       break;
//     case 2:
//       midiCCOut(CCvcf_env_source, 0x20);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCvcf_env_source, 0x30);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
//       break;
//     case 4:
//       midiCCOut(CCvcf_env_source, 0x40);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
//       break;
//     case 5:
//       midiCCOut(CCvcf_env_source, 0x50);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
//       break;
//     case 6:
//       midiCCOut(CCvcf_env_source, 0x60);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
//       break;
//     case 7:
//       midiCCOut(CCvcf_env_source, 0x70);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
//       mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
//       mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
//       mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatevcf_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (vcf_dyn) {
//       case 0:
//         showCurrentParameterPage("VCF ENV", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("VCF ENV", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("VCF ENV", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("VCF ENV", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vcf_dyn) {
//     case 0:
//       midiCCOut(CCvcf_dyn, 0x00);
//       mcp6.digitalWrite(VCF_DYN_RED, LOW);
//       mcp6.digitalWrite(VCF_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCvcf_dyn, 0x20);
//       mcp6.digitalWrite(VCF_DYN_RED, HIGH);
//       mcp6.digitalWrite(VCF_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCvcf_dyn, 0x40);
//       mcp6.digitalWrite(VCF_DYN_RED, LOW);
//       mcp6.digitalWrite(VCF_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCvcf_dyn, 0x60);
//       mcp6.digitalWrite(VCF_DYN_RED, HIGH);
//       mcp6.digitalWrite(VCF_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatevca_env_source(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (vca_env_source) {
//       case 0:
//         showCurrentParameterPage("VCA EG", "ENV1");
//         break;
//       case 1:
//         showCurrentParameterPage("VCA EG", "ENV2");
//         break;
//       case 2:
//         showCurrentParameterPage("VCA EG", "ENV3");
//         break;
//       case 3:
//         showCurrentParameterPage("VCA _EG", "ENV4");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vca_env_source) {
//     case 0:
//       midiCCOut(CCvca_env_source, 0x00);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_RED, LOW);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCvca_env_source, 0x20);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_RED, LOW);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCvca_env_source, 0x40);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_RED, HIGH);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
//       break;
//     case 3:
//       midiCCOut(CCvca_env_source, 0x60);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_RED, HIGH);
//       mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
//       break;
//   }
// }

// FLASHMEM void updatevca_dyn(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (vca_dyn) {
//       case 0:
//         showCurrentParameterPage("VCA ENV", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("VCA ENV", "DYN1");
//         break;
//       case 2:
//         showCurrentParameterPage("VCA ENV", "DYN2");
//         break;
//       case 3:
//         showCurrentParameterPage("VCA ENV", "DYN3");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vca_dyn) {
//     case 0:
//       midiCCOut(CCvca_dyn, 0x00);
//       mcp6.digitalWrite(VCA_DYN_RED, LOW);
//       mcp6.digitalWrite(VCA_DYN_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCvca_dyn, 0x20);
//       mcp6.digitalWrite(VCA_DYN_RED, HIGH);
//       mcp6.digitalWrite(VCA_DYN_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCvca_dyn, 0x40);
//       mcp6.digitalWrite(VCA_DYN_RED, LOW);
//       mcp6.digitalWrite(VCA_DYN_GREEN, HIGH);
//       break;
//     case 3:
//       midiCCOut(CCvca_dyn, 0x60);
//       mcp6.digitalWrite(VCA_DYN_RED, HIGH);
//       mcp6.digitalWrite(VCA_DYN_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updatechorus(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 0;
//     switch (chorus) {
//       case 0:
//         showCurrentParameterPage("CHORUS", "Off");
//         break;
//       case 1:
//         showCurrentParameterPage("CHORUS", "1");
//         break;
//       case 2:
//         showCurrentParameterPage("CHORUS", "2");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (chorus) {
//     case 0:
//       midiCCOut(CCchorus_sw, 0x00);
//       mcp6.digitalWrite(CHORUS_SELECT_RED, LOW);
//       mcp6.digitalWrite(CHORUS_SELECT_GREEN, LOW);
//       break;
//     case 1:
//       midiCCOut(CCchorus_sw, 0x20);
//       mcp6.digitalWrite(CHORUS_SELECT_RED, HIGH);
//       mcp6.digitalWrite(CHORUS_SELECT_GREEN, LOW);
//       break;
//     case 2:
//       midiCCOut(CCchorus_sw, 0x40);
//       mcp6.digitalWrite(CHORUS_SELECT_RED, LOW);
//       mcp6.digitalWrite(CHORUS_SELECT_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updateportamento_sw(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 1;
//     switch (portamento_sw) {
//       case 0:
//         showCurrentParameterPage("PORTAMENTO", "OFF");
//         break;
//       case 1:
//         showCurrentParameterPage("PORTAMENTO", "LOWER");
//         break;
//       case 2:
//         showCurrentParameterPage("PORTAMENTO", "UPPER");
//         break;
//       case 3:
//         showCurrentParameterPage("PORTAMENTO", "BOTH");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (portamento_sw) {
//     case 0:
//       sendCustomSysEx((midiOutCh - 1), 0x2C, 0x00);
//       delay(20);
//       sendCustomSysEx((midiOutCh - 1), 0x23, 0x00);
//       mcp2.digitalWrite(PORTAMENTO_LOWER_RED, LOW);
//       mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, LOW);
//       break;
//     case 1:
//       sendCustomSysEx((midiOutCh - 1), 0x23, 0x00);
//       delay(20);
//       sendCustomSysEx((midiOutCh - 1), 0x2C, 0x01);
//       mcp2.digitalWrite(PORTAMENTO_LOWER_RED, HIGH);
//       mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, LOW);
//       break;
//     case 2:
//       sendCustomSysEx((midiOutCh - 1), 0x2C, 0x00);
//       delay(20);
//       sendCustomSysEx((midiOutCh - 1), 0x23, 0x01);
//       mcp2.digitalWrite(PORTAMENTO_LOWER_RED, LOW);
//       mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, HIGH);
//       break;
//     case 3:
//       sendCustomSysEx((midiOutCh - 1), 0x23, 0x01);
//       delay(20);
//       sendCustomSysEx((midiOutCh - 1), 0x2C, 0x01);
//       mcp2.digitalWrite(PORTAMENTO_LOWER_RED, HIGH);
//       mcp2.digitalWrite(PORTAMENTO_UPPER_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updateenv5stage(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 2;
//     switch (env5stage) {
//       case 0:
//         showCurrentParameterPage("5 STAGE", "ENVELOPE 1");
//         break;
//       case 1:
//         showCurrentParameterPage("5 STAGE", "ENVELOPE 2");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (env5stage) {
//     case 0:
//       mcp5.digitalWrite(ENV5STAGE_SELECT_RED, HIGH);
//       mcp5.digitalWrite(ENV5STAGE_SELECT_GREEN, LOW);
//       break;
//     case 1:
//       mcp5.digitalWrite(ENV5STAGE_SELECT_RED, LOW);
//       mcp5.digitalWrite(ENV5STAGE_SELECT_GREEN, HIGH);
//       break;
//   }
// }

// FLASHMEM void updateadsr(bool announce) {
//   if (announce && !suppressParamAnnounce) {
//     displayMode = 2;
//     switch (adsr) {
//       case 0:
//         showCurrentParameterPage("ADSR", "ENVELOPE 3");
//         break;
//       case 1:
//         showCurrentParameterPage("ADSR", "ENVELOPE 4");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (adsr) {
//     case 0:
//       mcp6.digitalWrite(ADSR_SELECT_RED, HIGH);
//       mcp6.digitalWrite(ADSR_SELECT_GREEN, LOW);
//       break;
//     case 1:
//       mcp6.digitalWrite(ADSR_SELECT_RED, LOW);
//       mcp6.digitalWrite(ADSR_SELECT_GREEN, HIGH);
//       break;
//   }
// }

void initRotaryEncoders() {
  for (auto &rotaryEncoder : rotaryEncoders) {
    rotaryEncoder.init();
  }
}

void initButtons() {
  for (auto &button : allButtons) {
    button->begin();
  }
}

void pollAllMCPs() {

  for (int j = 0; j < numMCPs; j++) {
    uint16_t gpioAB = allMCPs[j]->readGPIOAB();

    for (auto &button : allButtons) {
      if (button->getMcp() == allMCPs[j]) {
        button->feedInput(gpioAB);
      }
    }
  }
}

int getLowerSplitVoice(byte note) {
  // Try round-robin for a free voice first (Poly1 behaviour)
  for (int i = 0; i < 6; i++) {
    int idx = (lowerSplitVoicePointer + i) % 6;
    if (!voiceOn[idx]) {
      lowerSplitVoicePointer = (idx + 1) % 6;
      return idx;
    }
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  int oldest = oldestVoicePreferNotPhysHeld(0, 5);
  lowerSplitVoicePointer = (oldest + 1) % 6;
  return oldest;
}

int getUpperSplitVoice(byte note) {
  // Try round-robin for a free voice first (Poly1 behaviour)
  for (int i = 0; i < 6; i++) {
    int idx = 6 + (upperSplitVoicePointer + i) % 6;
    if (!voiceOn[idx]) {
      upperSplitVoicePointer = (idx - 6 + 1) % 6;  // pointer is 0..3
      return idx;
    }
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  int oldest = oldestVoicePreferNotPhysHeld(6, 11);
  upperSplitVoicePointer = ((oldest - 6) + 1) % 6;
  return oldest;
}

int getLowerSplitVoicePoly2(byte note) {
  // Poly2: pick the lowest-numbered free voice if any
  for (int i = 0; i < 6; i++) {
    if (!voiceOn[i]) return i;
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  return oldestVoicePreferNotPhysHeld(0, 5);
}

int getUpperSplitVoicePoly2(byte note) {
  // Poly2: pick the lowest-numbered free voice if any
  for (int i = 6; i < 12; i++) {
    if (!voiceOn[i]) return i;
  }

  // No free voice: steal oldest, but prefer not physically held (JP-8 Hold behaviour)
  return oldestVoicePreferNotPhysHeld(6, 11);
}


inline void sendVoiceNoteOn(int voiceIdx, byte note, byte vel) {
  if (voiceIdx < 6) MIDI.sendNoteOn(note, vel, 1);
  else MIDI.sendNoteOn(note, vel, 1);
}

inline void sendVoiceNoteOff(int voiceIdx, byte note) {
  if (voiceIdx < 6) MIDI.sendNoteOn(note, 0, 1);
  else MIDI.sendNoteOn(note, 0, 1);
}

void assignVoice(byte note, byte velocity, int voiceIdx) {
  if (voiceIdx < 0 || voiceIdx >= 12) return;

  // If voice is already sounding a different note, release it properly (state + LED + mappings)
  if (voices[voiceIdx].noteOn && voices[voiceIdx].note >= 0 && voices[voiceIdx].note != note) {
    releaseVoice((byte)voices[voiceIdx].note, voiceIdx);
  }

  voices[voiceIdx].note = note;
  voices[voiceIdx].velocity = velocity;
  voices[voiceIdx].timeOn = millis();
  voices[voiceIdx].noteOn = true;
  voiceOn[voiceIdx] = true;

  sendVoiceNoteOn(voiceIdx, note, velocity);
}

void releaseVoice(byte note, int voiceIdx) {
  if (voiceIdx < 0 || voiceIdx >= 12) return;

  if (voices[voiceIdx].noteOn && voices[voiceIdx].note == note) {
    sendVoiceNoteOff(voiceIdx, note);

    voices[voiceIdx].note = -1;
    voices[voiceIdx].noteOn = false;
    voiceOn[voiceIdx] = false;


    if (voiceIdx < 6) {
      voiceAssignmentLower[note] = -1;
      voiceToNoteLower[voiceIdx] = -1;
    } else {
      voiceAssignmentUpper[note] = -1;
      voiceToNoteUpper[voiceIdx - 6] = -1;
    }
  }
}

int getVoiceNoPoly2(int note) {
  voiceToReturn = -1;       // Initialize to 'null'
  earliestTime = millis();  // Initialize to now

  if (note == -1) {
    // NoteOn() - Get the oldest free voice (recent voices may still be on the release stage)
    if (voices[lastUsedVoice].note == -1) {
      return lastUsedVoice + 1;
    }

    // If the last used voice is not free or doesn't exist, check if the first voice is free
    if (voices[0].note == -1) {
      return 1;
    }

    // Find the lowest available voice for the new note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == -1) {
        return i + 1;
      }
    }

    // If no voice is available, release the oldest note
    int oldestVoice = 0;
    for (int i = 1; i < NO_OF_VOICES; i++) {
      if (voices[i].timeOn < voices[oldestVoice].timeOn) {
        oldestVoice = i;
      }
    }
    return oldestVoice + 1;
  } else {
    // NoteOff() - Get the voice number from the note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == note) {
        return i + 1;
      }
    }
  }

  // Shouldn't get here, return voice 1
  return 1;
}

int getVoiceNo(int note) {
  voiceToReturn = -1;       //Initialise to 'null'
  earliestTime = millis();  //Initialise to now
  if (note == -1) {
    //NoteOn() - Get the oldest free voice (recent voices may be still on release stage)
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == -1) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    if (voiceToReturn == -1) {
      //No free voices, need to steal oldest sounding voice
      earliestTime = millis();  //Reinitialise
      for (int i = 0; i < NO_OF_VOICES; i++) {
        if (voices[i].timeOn < earliestTime) {
          earliestTime = voices[i].timeOn;
          voiceToReturn = i;
        }
      }
    }
    return voiceToReturn + 1;
  } else {
    //NoteOff() - Get voice number from note
    for (int i = 0; i < NO_OF_VOICES; i++) {
      if (voices[i].note == note) {
        return i + 1;
      }
    }
  }
  //Shouldn't get here, return voice 1
  return 1;
}

int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

// Arpeggiator

void serviceArpClockLoss() {

  if (arpMode == ARP_OFF) return;

  // Only relevant for external clock source
  if (arpClockSrc != ARPCLK_EXTERNAL) return;

  // If no arp note is currently sounding, nothing to do
  if (!arpNoteActive) return;

  // If we've never seen a pulse, don't force-off
  if (lastExtPulseUs == 0) return;

  uint32_t nowMs = millis();
  uint32_t lastPulseMs = lastExtPulseUs / 1000u;

  if ((uint32_t)(nowMs - lastPulseMs) > ARP_EXT_CLOCK_LOSS_MS) {
    // Clock stopped: kill the held arp note
    arpStopCurrent();
    arpNoteActive = false;

    // Prevent queued ticks from retriggering later
    arpExtTickCount = 0;

    // Optional: mark not running
    arpRunning = false;
  }
}

inline bool arpNotePresentLower(uint8_t n) {
  return keyDownLower[n] || holdLatchedLower[n];
}

inline bool arpNotePresentUpper(uint8_t n) {
  return keyDownUpper[n] || holdLatchedUpper[n];
}

inline bool arpPatternContains(uint8_t n) {
  for (uint8_t i = 0; i < arpLen; i++)
    if (arpPattern[i] == n) return true;
  return false;
}

void arpClearPattern() {
  arpLen = 0;
  arpPos = -1;
  arpDir = +1;
  arpRunning = false;
}

void arpAddNote(uint8_t n) {
  if (arpLen >= 8) return;
  if (arpPatternContains(n)) return;
  arpPattern[arpLen++] = n;
  // If we were empty and now have notes, start transport cleanly
  if (arpLen == 1) {
    arpPos = -1;
    arpDir = +1;
  }
}

void arpRemoveNote(uint8_t n) {
  for (uint8_t i = 0; i < arpLen; i++) {
    if (arpPattern[i] == n) {
      for (uint8_t j = i; j + 1 < arpLen; j++) arpPattern[j] = arpPattern[j + 1];
      arpLen--;
      if (arpLen == 0) {
        arpPos = -1;
        arpDir = +1;
      } else {
        // keep position in bounds
        int16_t L = (int16_t)arpLen * (int16_t)arpRange;
        if (arpPos >= L) arpPos = -1;
      }
      return;
    }
  }
}

inline int16_t arpUnfoldedLength() {
  return (int16_t)arpLen * (int16_t)arpRange;
}

inline uint8_t arpUnfoldedNoteAt(int16_t p) {
  uint8_t idx = (uint8_t)(p % arpLen);
  uint8_t oct = (uint8_t)(p / arpLen);
  int16_t n = (int16_t)arpPattern[idx] + (int16_t)(12 * oct);
  if (n < 0) n = 0;
  if (n > 127) n = 127;
  return (uint8_t)n;
}

int16_t arpNextPos(int16_t L) {
  if (L <= 1) return 0;

  switch (arpMode) {
    case ARP_UP:
      return (int16_t)((arpPos + 1) % L);

    case ARP_DOWN:
      return (arpPos <= 0) ? (L - 1) : (arpPos - 1);

    case ARP_UPDOWN:
      {
        int16_t np = arpPos + arpDir;
        if (np >= L) {
          arpDir = -1;
          np = L - 2;
        }
        if (np < 0) {
          arpDir = +1;
          np = 1;
        }
        return np;
      }

    case ARP_RANDOM:
      return (int16_t)(random(L));

    default:
      return arpPos;
  }
}

// Release currently sounding arp note (if any)
void arpStopCurrent() {
  if (!arpNoteActive) return;

  // In Split mode, arp assigned to lower only
  if (keyMode == 3 && arpLowerOnlyWhenSplit) {
    int v = voiceAssignmentLower[arpCurrentNote];
    if (v >= 0 && v <= 5) releaseVoice(arpCurrentNote, v);
  } else if (keyMode == 0) {
    // DUAL: release in both engines if present
    int vl = voiceAssignmentLower[arpCurrentNote];
    if (vl >= 0 && vl <= 5) releaseVoice(arpCurrentNote, vl);

    int vu = voiceAssignmentUpper[arpCurrentNote];
    if (vu >= 6 && vu <= 11) releaseVoice(arpCurrentNote, vu);
  } else {
    // WHOLE: release across whatever voice currently has that note
    for (int v = 0; v < 12; v++) {
      if (voices[v].noteOn && voices[v].note == arpCurrentNote) {
        releaseVoice(arpCurrentNote, v);
      }
    }
  }

  arpNoteActive = false;
}

// Play next arp note using your existing allocation rules
void arpPlayNote(uint8_t note, uint8_t vel) {

  // Split: lower only
  if (keyMode == 3 && arpLowerOnlyWhenSplit) {

    switch (lowerData[P_assign]) {
      case 0:
        {
          int v = getLowerSplitVoice(note);
          assignVoice(note, vel, v);
          voiceAssignmentLower[note] = v;
          voiceToNoteLower[v] = note;
        }
        break;

      case 1:
        {
          int v = getLowerSplitVoicePoly2(note);
          // Poly2 behavior: if voice already has a note, release it first
          int old = voiceToNoteLower[v];
          if (old >= 0) {
            releaseVoice(old, v);
            voiceAssignmentLower[old] = -1;
          }
          assignVoice(note, vel, v);
          voiceAssignmentLower[note] = v;
          voiceToNoteLower[v] = note;
        }
        break;

      case 2:
        commandMonoNoteOnLower(note, vel);
        break;

      case 3:
        commandUnisonNoteOnLower(note, vel);
        break;
    }

    return;
  }

  // DUAL: drive both lower and upper simultaneously, per your existing logic
  if (keyMode == 0) {

    // Lower
    if (lowerData[P_assign] == 1) {
      int v = getLowerSplitVoicePoly2(note);
      int old = voiceToNoteLower[v];
      if (old >= 0) {
        releaseVoice(old, v);
        voiceAssignmentLower[old] = -1;
      }
      assignVoice(note, vel, v);
      voiceAssignmentLower[note] = v;
      voiceToNoteLower[v] = note;

    } else if (lowerData[P_assign] == 0) {
      int v = getLowerSplitVoice(note);
      assignVoice(note, vel, v);
      voiceAssignmentLower[note] = v;
      voiceToNoteLower[v] = note;

    } else if (lowerData[P_assign] == 2) {
      commandMonoNoteOnLower(note, vel);
    } else if (lowerData[P_assign] == 3) {
      commandUnisonNoteOnLower(note, vel);
    }

    // Upper
    if (upperData[P_assign] == 1) {
      int v = getUpperSplitVoicePoly2(note);
      int old = voiceToNoteUpper[v - 6];
      if (old >= 0) {
        releaseVoice(old, v);
        voiceAssignmentUpper[old] = -1;
      }
      assignVoice(note, vel, v);
      voiceAssignmentUpper[note] = v;
      voiceToNoteUpper[v - 6] = note;

    } else if (upperData[P_assign] == 0) {
      int v = getUpperSplitVoice(note);
      assignVoice(note, vel, v);
      voiceAssignmentUpper[note] = v;
      voiceToNoteUpper[v - 6] = note;

    } else if (upperData[P_assign] == 2) {
      commandMonoNoteOnUpper(note, vel);
    } else if (upperData[P_assign] == 3) {
      commandUnisonNoteOnUpper(note, vel);
    }

    return;
  }

  // WHOLE: use your whole-mode allocation rules
  if (keyMode == 1) {
    int voiceNum = -1;
    switch (lowerData[P_assign]) {
      case 0:
        voiceNum = getVoiceNo(-1) - 1;
        assignVoice(note, vel, voiceNum);
        break;
      case 1:
        voiceNum = getVoiceNoPoly2(-1) - 1;
        assignVoice(note, vel, voiceNum);
        break;
      case 2:
        commandMonoNoteOn(note, vel);
        break;
      case 3:
        commandUnisonNoteOn(note, vel);
        break;
    }
    return;
  }
}

void arpEngine() {

  if (arpMode == ARP_OFF || arpLen == 0) {
    if (arpNoteActive) arpStopCurrent();
    arpRunning = false;
    return;
  }

  if (!arpShouldStepNow()) return;

  // Tight JP-8 feel: off at step boundary
  if (arpNoteActive) arpStopCurrent();

  int16_t L = arpUnfoldedLength();
  if (L <= 0) return;

  arpPos = arpNextPos(L);
  uint8_t nextNote = arpUnfoldedNoteAt(arpPos);

  arpPlayNote(nextNote, arpCurrentVel);
  arpCurrentNote = nextNote;
  arpNoteActive = true;
  arpRunning = true;
}

bool arpShouldStepNow() {

  if (arpClockSrc == ARPCLK_INTERNAL) {
    return arpShouldStepNow_InternalSmooth();
  }

  if (arpClockSrc == ARPCLK_MIDI) {
    if (!midiClockRunning) return false;
    if (arpTicksPerStep == 0) arpTicksPerStep = 1;
    if (arpClkTickCount >= arpTicksPerStep) {
      arpClkTickCount = 0;
      return true;
    }
    return false;
  }

  // EXTERNAL: 1 pulse = 1 step
  if (arpExtTickCount > 0) {
    arpExtTickCount = 0;
    return true;
  }

  return false;
}

void serviceExternalClockLed() {

  static bool ledOn = false;
  static uint32_t ledOffAtMs = 0;

  // Only show the red LED for external clock mode
  if (arpClockSrc != ARPCLK_EXTERNAL) {
    if (ledOn) {
      //mcp2.digitalWrite(ARP_CLK_LED_RED, LOW);
      ledOn = false;
    }
    return;
  }

  // If ISR requested a pulse, turn LED on and set an off time
  if (extClkLedPulseReq) {
    noInterrupts();
    extClkLedPulseReq = false;
    uint32_t t = extClkLedPulseAtMs;
    interrupts();

    //mcp2.digitalWrite(ARP_CLK_LED_RED, HIGH);
    ledOn = true;
    ledOffAtMs = t + EXT_LED_PULSE_MS;
  }

  // Turn off after pulse width
  if (ledOn && (int32_t)(millis() - ledOffAtMs) >= 0) {
    //mcp2.digitalWrite(ARP_CLK_LED_RED, LOW);
    ledOn = false;
  }
}

inline void setArpMode(ArpMode m) {
  ArpMode prev = arpMode;
  bool wasOff = (prev == ARP_OFF);
  bool nowOff = (m == ARP_OFF);

  // If we are turning arp OFF, stop notes and restore modes
  if (!wasOff && nowOff) {
    arpMode = ARP_OFF;
    arpNextStepUs = 0;
    arpLastSmoothUs = 0;
    if (arpNoteActive) arpStopCurrent();
    arpRestorePoly2Off();

    // Transport reset
    arpPos = -1;
    arpDir = +1;
    arpClkTickCount = 0;
    arpLastStepMs = millis();

    updateArpLEDs();
    return;
  }

  // If we are turning arp ON (OFF -> something)
  if (wasOff && !nowOff) {

    arpRange = lastArpRange;         // already preloaded by patch recall
    arpEverEnabledSinceBoot = true;  // mark as used

    arpForcePoly2On();
  }

  // Switching between arp modes while already on:
  arpMode = m;

  // Reset transport and stop any current arp note for clean switching
  if (arpNoteActive) arpStopCurrent();
  arpPos = -1;
  arpDir = +1;
  arpClkTickCount = 0;
  arpLastStepMs = millis();

  updateArpLEDs();
}

inline void updateArpTicksPerStepFromDiv() {
  switch (arpMidiDivSW) {
    case 0: arpTicksPerStep = 12; break;  // 8th
    case 1: arpTicksPerStep = 8; break;   // 8th triplet
    default: arpTicksPerStep = 6; break;  // 16th
  }
}

void onMidiClockTick() {
  if (arpClockSrc != ARPCLK_MIDI) return;
  if (!midiClockRunning) return;  // only step after Start/Continue

  arpClkTickCount++;
}

void onMidiStart() {
  midiClockRunning = true;
  arpClkTickCount = 0;

  // Reset arp transport phase (JP-8-ish)
  arpPos = -1;
  arpDir = +1;

  // If a note is currently sounding, stop it so first step is clean
  if (arpNoteActive) arpStopCurrent();
}

void onMidiStop() {
  midiClockRunning = false;
  arpClkTickCount = 0;

  // Stop current arp note and suspend stepping
  if (arpNoteActive) arpStopCurrent();
}

void onMidiContinue() {
  midiClockRunning = true;
  // Do NOT clear pattern; do NOT reset arpPos unless you want "restart"
  // Keep tick count as-is or zero it; JP-8 behavior is less defined here.
  // I recommend leaving it as-is for continuity.
}

inline void toggleArpMode(ArpMode m) {
  if (arpMode == m) setArpMode(ARP_OFF);
  else setArpMode(m);
}

inline void setArpRange(uint8_t r) {
  if (r < 1) r = 1;
  if (r > 4) r = 4;

  lastArpRange = r;  // remember for next time
  arpRange = r;

  // Reset transport so unfolding restarts cleanly
  arpPos = -1;
  arpDir = +1;
  arpClkTickCount = 0;
  arpLastStepMs = millis();

  updateArpLEDs();
}

inline void updateArpLEDs() {

  bool arpOn = (arpMode != ARP_OFF);

  // --- Range LEDs ---
  // User request: when arp OFF, range LEDs should be OFF
  //mcp1.digitalWrite(ARP_RANGE1_LED, (arpOn && arpRange == 1) ? HIGH : LOW);
  //mcp1.digitalWrite(ARP_RANGE2_LED, (arpOn && arpRange == 2) ? HIGH : LOW);
  //mcp2.digitalWrite(ARP_RANGE3_LED, (arpOn && arpRange == 3) ? HIGH : LOW);
  //mcp2.digitalWrite(ARP_RANGE4_LED, (arpOn && arpRange == 4) ? HIGH : LOW);

  // --- Mode LEDs (already correct: off when arp OFF) ---
  //mcp2.digitalWrite(ARP_MODE_UP_LED, (arpMode == ARP_UP) ? HIGH : LOW);
  //mcp2.digitalWrite(ARP_MODE_DOWN_LED, (arpMode == ARP_DOWN) ? HIGH : LOW);
  //mcp2.digitalWrite(ARP_MODE_UP_DOWN_LED, (arpMode == ARP_UPDOWN) ? HIGH : LOW);
  //mcp2.digitalWrite(ARP_MODE_RAND_LED, (arpMode == ARP_RANDOM) ? HIGH : LOW);
}

inline void updateArpClockLEDs() {
  switch (arpClockSrc) {
    case ARPCLK_INTERNAL:
      midiClockRunning = false;  // optional; avoids stale running state
      arpClkTickCount = 0;
      showCurrentParameterPage("Arp Clock", "Internal");
      startParameterDisplay();
      //mcp2.digitalWrite(ARP_CLK_LED_RED, LOW);
      //mcp2.digitalWrite(ARP_CLK_LED_GRN, LOW);
      break;

    case ARPCLK_EXTERNAL:
      midiClockRunning = false;  // optional; avoids stale running state
      arpClkTickCount = 0;
      showCurrentParameterPage("Arp Clock", "External");
      startParameterDisplay();
      //mcp2.digitalWrite(ARP_CLK_LED_RED, HIGH);
      //mcp2.digitalWrite(ARP_CLK_LED_GRN, LOW);
      break;

    case ARPCLK_MIDI:
      arpClkTickCount = 0;
      updateArpTicksPerStepFromDiv();
      showCurrentParameterPage("Arp Clock", "MIDI Clock");
      startParameterDisplay();
      //mcp2.digitalWrite(ARP_CLK_LED_RED, LOW);
      //mcp2.digitalWrite(ARP_CLK_LED_GRN, HIGH);
      break;
  }
}

inline float arpHzFromValue(uint8_t v) {
  const float minHz = 1.0f;
  const float maxHz = 20.0f;
  float t = v / 255.0f;
  return minHz * powf(maxHz / minHz, t);
}

inline uint16_t arpStepMsFromRate(uint8_t v) {
  float hz = arpHzFromValue(v);
  return (uint16_t)(1000.0f / hz + 0.5f);  // rounded ms per step
}

inline void setArpClockSrc(ArpClockSrc src) {
  arpClockSrc = src;

  // Reset clock accumulators and transport
  arpClkTickCount = 0;
  arpLastStepMs = millis();
  arpPos = -1;
  arpDir = +1;
  arpExtTickCount = 0;
  lastExtPulseUs = 0;
  extClkLedPulseReq = false;

  // Stop any sounding arp note on clock change
  if (arpNoteActive) arpStopCurrent();

  updateArpClockLEDs();
}

inline void cycleArpClockSrc() {
  switch (arpClockSrc) {
    case ARPCLK_INTERNAL:
      setArpClockSrc(ARPCLK_EXTERNAL);
      break;
    case ARPCLK_EXTERNAL:
      setArpClockSrc(ARPCLK_MIDI);
      break;
    default:
      setArpClockSrc(ARPCLK_INTERNAL);
      break;
  }
}

inline void updateArpTicksPerStep() {
  switch (arpMidiDiv) {
    case ARP_DIV_8TH: arpTicksPerStep = 12; break;
    case ARP_DIV_8TH_TRIP: arpTicksPerStep = 8; break;
    default: arpTicksPerStep = 6; break;
  }
}

inline bool arpKeyPresentLower(uint8_t n) {
  return keyDownLower[n] || holdLatchedLower[n];
}

inline bool arpKeyPresentUpper(uint8_t n) {
  return keyDownUpper[n] || holdLatchedUpper[n];
}

inline bool arpConsumesKey(byte note) {
  if (arpMode == ARP_OFF) return false;
  if (arpInjecting) return false;  // arp-generated notes must still sound

  // JP-8: in Split, arp is assigned to LOWER only
  if (keyMode == 3 && arpLowerOnlyWhenSplit) {
    return (note < splitPoint);
  }

  // Whole and Dual: arp accepts notes over entire keyboard
  // (and you generally don't want the chord to sound directly)
  return true;
}

inline void arpUpdateSmoothHz() {
  uint32_t now = micros();
  if (arpLastSmoothUs == 0) {
    arpLastSmoothUs = now;
    arpHzSmooth = arpHzTarget;
    return;
  }

  float dt = (now - arpLastSmoothUs) * 1e-6f;  // seconds
  arpLastSmoothUs = now;

  // Time constant (seconds). Larger = smoother/slower response.
  const float tau = 0.20f;  // 200ms is a good starting point

  // One-pole coefficient based on dt
  float a = dt / (tau + dt);  // stable even if dt varies
  arpHzSmooth += (arpHzTarget - arpHzSmooth) * a;

  // Safety clamp
  if (arpHzSmooth < 1.0f) arpHzSmooth = 1.0f;
  if (arpHzSmooth > 20.0f) arpHzSmooth = 20.0f;
}

inline bool arpShouldStepNow_InternalSmooth() {
  arpUpdateSmoothHz();

  uint32_t now = micros();

  if (arpNextStepUs == 0) {
    // initialize on first run
    arpNextStepUs = now;
    return true;  // step immediately on start (optional; remove if you don't want immediate)
  }

  // time until next step elapsed?
  if ((int32_t)(now - arpNextStepUs) < 0) return false;

  // schedule next step using current smoothed interval
  float intervalUsF = 1000000.0f / arpHzSmooth;
  uint32_t intervalUs = (uint32_t)(intervalUsF + 0.5f);

  // Advance by one interval (not "now + interval") to reduce jitter
  arpNextStepUs += intervalUs;

  // If we fell behind (e.g. debugger, heavy load), resync gracefully
  if ((int32_t)(now - arpNextStepUs) > (int32_t)intervalUs) {
    arpNextStepUs = now + intervalUs;
  }

  return true;
}

inline ArpMode patchToArpMode(uint8_t v) {
  switch (v) {
    case 1: return ARP_UP;
    case 2: return ARP_DOWN;
    case 3: return ARP_UPDOWN;
    case 4: return ARP_RANDOM;
    default: return ARP_OFF;
  }
}

inline uint8_t arpModeToPatch(ArpMode m) {
  switch (m) {
    case ARP_UP: return 1;
    case ARP_DOWN: return 2;
    case ARP_UPDOWN: return 3;
    case ARP_RANDOM: return 4;
    default: return 0;
  }
}

inline uint8_t patchToArpRange(uint8_t v) {
  // Accept either 0..3 or 1..4
  if (v <= 3) return v + 1;        // 0..3 -> 1..4
  if (v >= 1 && v <= 4) return v;  // 1..4 -> 1..4
  return 4;
}

inline uint8_t arpRangeToPatch(uint8_t r) {
  if (r < 1) r = 1;
  if (r > 4) r = 4;
  return (uint8_t)(r - 1);  // store 0..3
}


void updateArpRange(boolean announce) {

  uint8_t r = patchToArpRange(arpRangeL);

  arpRange = r;
  lastArpRange = r;  // so next ARP enable recalls the stored range

  // Optional: restart unfolding when range changes
  arpPos = -1;
  arpDir = +1;

  if (announce && !suppressParamAnnounce) {
    showCurrentParameterPage("Arp Range", String(r));
    startParameterDisplay();
  }

  updateArpLEDs();
}

void updateArpMode(boolean announce) {

  ArpMode m = patchToArpMode(arpModeL);

  // If patch wants ARP ON, preload lastArpRange from patch so setArpMode() uses it.
  if (m != ARP_OFF) {
    lastArpRange = patchToArpRange(arpRangeL);
  }

  setArpMode(m);

  if (announce && !suppressParamAnnounce) {
    const char *name =
      (m == ARP_UP) ? "Up" : (m == ARP_DOWN)   ? "Down"
                           : (m == ARP_UPDOWN) ? "UpDown"
                           : (m == ARP_RANDOM) ? "Random"
                                               : "Off";

    showCurrentParameterPage("Arp Mode", String(name));
    startParameterDisplay();
  }

  // Ensure LEDs reflect range for ON patches (and range LEDs remain off when OFF)
  updateArpLEDs();
}

// Hold functions

inline bool pedalAffectsLower() {
  return true;
}

inline bool pedalAffectsUpper() {
  return (keyMode != 2);
}

inline bool holdEffectiveLower() {
  if (keyMode == 3) return (holdManualLower || holdPedal);   // SPLIT
  return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL
}

inline bool holdEffectiveUpper() {
  if (keyMode == 3) return holdManualUpper;                  // SPLIT (no pedal)
  return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL
}

void reconcileHoldReleases() {

  // PEDAL: sustain acts as global hold in WHOLE/DUAL, and LOWER-only in SPLIT
  auto holdEffectiveLower = [&]() -> bool {
    if (keyMode == 3) return (holdManualLower || holdPedal);   // SPLIT lower + pedal
    return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL global
  };
  auto holdEffectiveUpper = [&]() -> bool {
    if (keyMode == 3) return (holdManualUpper);                // SPLIT upper (no pedal)
    return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL global
  };

  // -------------------------
  // WHOLE or DUAL: hold is global
  // -------------------------
  if (keyMode != 3) {

    // Only reconcile when hold is effectively OFF (manual + pedal)
    if (!(holdManualLower || holdManualUpper || holdPedal)) {

      for (int n = 0; n < 128; n++) {

        bool latched = holdLatchedLower[n] || holdLatchedUpper[n];
        if (!latched) continue;

        bool phys = keyDownLower[n] || keyDownUpper[n] || keyDownWhole[n];
        if (phys) continue;

        // Release ALL voices currently playing this note
        for (int v = 0; v < 8; v++) {
          if (voices[v].noteOn && voices[v].note == n) {
            releaseVoice((byte)n, v);
          }
        }

        // Clear latch
        holdLatchedLower[n] = false;
        holdLatchedUpper[n] = false;

        // ARP: if note no longer present anywhere, remove from pattern
        if (!arpInjecting && arpMode != ARP_OFF) {
          bool present = arpKeyPresentLower(n) || arpKeyPresentUpper(n);
          if (!present) arpRemoveNote((byte)n);
        }
      }

      // If pattern emptied, stop arp immediately
      if (arpMode != ARP_OFF && arpLen == 0 && arpNoteActive) {
        arpStopCurrent();
      }
    }

    return;
  }

  // -------------------------
  // SPLIT: Lower/Upper independent
  // -------------------------

  // LOWER (manual lower OR pedal)
  if (!holdEffectiveLower()) {
    for (int n = 0; n < 128; n++) {
      if (holdLatchedLower[n] && !keyDownLower[n]) {

        // Safer than voiceAssignmentLower[n] because voice stealing / mono/unison can change it.
        for (int v = 0; v <= 5; v++) {
          if (voices[v].noteOn && voices[v].note == n) {
            releaseVoice((byte)n, v);
          }
        }

        holdLatchedLower[n] = false;

        // ARP: JP-8 uses LOWER only in split (if configured that way)
        if (!arpInjecting && arpMode != ARP_OFF && arpLowerOnlyWhenSplit) {
          if (!arpKeyPresentLower(n)) arpRemoveNote((byte)n);
        }
      }
    }
  }

  // UPPER (manual upper only; pedal does not affect upper in split)
  if (!holdEffectiveUpper()) {
    for (int n = 0; n < 128; n++) {
      if (holdLatchedUpper[n] && !keyDownUpper[n]) {

        for (int v = 6; v <= 11; v++) {
          if (voices[v].noteOn && voices[v].note == n) {
            releaseVoice((byte)n, v);
          }
        }

        holdLatchedUpper[n] = false;

        // Only prune upper if you ever allow arp to consume upper in split
        if (!arpInjecting && arpMode != ARP_OFF && !arpLowerOnlyWhenSplit) {
          if (!arpKeyPresentUpper(n)) arpRemoveNote((byte)n);
        }
      }
    }
  }

  if (arpMode != ARP_OFF && arpLen == 0 && arpNoteActive) {
    arpStopCurrent();
  }
}

inline void arpForcePoly2On() {
  if (arpForcedPoly2) return;

  savedLowerKBMode = lowerData[P_assign];
  savedUpperKBMode = upperData[P_assign];

  // JP-8: when split, arp is assigned to LOWER only
  if (keyMode == 3 && arpLowerOnlyWhenSplit) {
    lowerData[P_assign] = 1;  // Poly2 lower only
  } else {
    // Whole / Dual: force both
    lowerData[P_assign] = 1;
    upperData[P_assign] = 1;
  }
  updateassignMode(0);
  arpForcedPoly2 = true;
}

inline void arpRestorePoly2Off() {
  if (!arpForcedPoly2) return;

  // restore whatever was previously selected
  lowerData[P_assign] = savedLowerKBMode;
  upperData[P_assign] = savedUpperKBMode;

  updateassignMode(0);
  arpForcedPoly2 = false;
}

inline bool isKeyPhysicallyDownForVoice(int voiceIdx) {
  int n = voices[voiceIdx].note;
  if (n < 0 || n > 127) return false;

  if (voiceIdx < 6) return keyDownLower[n];
  else return keyDownUpper[n];
}

int oldestVoicePreferNotPhysHeld(int vStart, int vEndInclusive) {
  int best = vStart;
  unsigned long bestTime = 0;
  bool found = false;

  // 1) oldest voice where key is NOT physically held
  for (int v = vStart; v <= vEndInclusive; v++) {
    if (!voiceOn[v]) continue;
    if (isKeyPhysicallyDownForVoice(v)) continue;
    if (!found || voices[v].timeOn < bestTime) {
      best = v;
      bestTime = voices[v].timeOn;
      found = true;
    }
  }
  if (found) return best;

  // 2) otherwise fall back to oldest regardless
  best = vStart;
  bestTime = voices[vStart].timeOn;
  for (int v = vStart + 1; v <= vEndInclusive; v++) {
    if (voices[v].timeOn < bestTime) {
      best = v;
      bestTime = voices[v].timeOn;
    }
  }
  return best;
}

// Mono lower & uppper

void commandTopNoteLower() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesLower[i]) topNote = i;

  if (topNote >= 0)
    assignVoice(topNote, noteVel, 0);
  else
    releaseVoice(noteMsg, 0);
}

void commandBottomNoteLower() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesLower[i]) bottomNote = i;

  if (bottomNote >= 0)
    assignVoice(bottomNote, noteVel, 0);
  else
    releaseVoice(noteMsg, 0);
}

void commandLastNoteLower() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderLower[mod(orderIndxLower - i, 40)];
    if (notesLower[idx]) {
      assignVoice(idx, noteVel, 0);
      return;
    }
  }
  releaseVoice(noteMsg, 0);
}

void commandTopNoteUpper() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesUpper[i]) topNote = i;

  if (topNote >= 0)
    assignVoice(topNote, noteVel, 4);
  else
    releaseVoice(noteMsg, 4);
}

void commandBottomNoteUpper() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesUpper[i]) bottomNote = i;

  if (bottomNote >= 0)
    assignVoice(bottomNote, noteVel, 4);
  else
    releaseVoice(noteMsg, 4);
}

void commandLastNoteUpper() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderUpper[mod(orderIndxUpper - i, 40)];
    if (notesUpper[idx]) {
      assignVoice(idx, noteVel, 4);
      return;
    }
  }
  releaseVoice(noteMsg, 4);
}

// Unison lower and upper

void commandTopNoteUniLower() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesLower[i]) topNote = i;

  if (topNote >= 0)
    for (int v = 0; v < 4; v++) assignVoice(topNote, noteVel, v);
  else
    for (int v = 0; v < 4; v++) releaseVoice(noteMsg, v);
}

void commandBottomNoteUniLower() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesLower[i]) bottomNote = i;

  if (bottomNote >= 0)
    for (int v = 0; v < 6; v++) assignVoice(bottomNote, noteVel, v);
  else
    for (int v = 0; v < 6; v++) releaseVoice(noteMsg, v);
}

void commandLastNoteUniLower() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderLower[mod(orderIndxLower - i, 40)];
    if (notesLower[idx]) {
      for (int v = 0; v < 6; v++) assignVoice(idx, noteVel, v);
      return;
    }
  }
  for (int v = 0; v < 6; v++) releaseVoice(noteMsg, v);
}

void commandTopNoteUniUpper() {
  int topNote = -1;
  for (int i = 0; i < 128; i++)
    if (notesUpper[i]) topNote = i;

  if (topNote >= 0)
    for (int v = 6; v < 12; v++) assignVoice(topNote, noteVel, v);
  else
    for (int v = 6; v < 12; v++) releaseVoice(noteMsg, v);
}

void commandBottomNoteUniUpper() {
  int bottomNote = -1;
  for (int i = 127; i >= 0; i--)
    if (notesUpper[i]) bottomNote = i;

  if (bottomNote >= 0)
    for (int v = 6; v < 12; v++) assignVoice(bottomNote, noteVel, v);
  else
    for (int v = 6; v < 12; v++) releaseVoice(noteMsg, v);
}

void commandLastNoteUniUpper() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderUpper[mod(orderIndxUpper - i, 40)];
    if (notesUpper[idx]) {
      for (int v = 6; v < 12; v++) assignVoice(idx, noteVel, v);
      return;
    }
  }
  for (int v = 6; v < 12; v++) releaseVoice(noteMsg, v);
}

void commandMonoNoteOnUpper(byte note, byte velocity) {
  notesUpper[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxUpper = (orderIndxUpper + 1) % 40;
  noteOrderUpper[orderIndxUpper] = note;
  commandLastNoteUpper();
}

void commandMonoNoteOffUpper(byte note) {
  notesUpper[note] = false;
  noteMsg = note;
  commandLastNoteUpper();
}

void commandMonoNoteOnLower(byte note, byte velocity) {
  notesLower[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxLower = (orderIndxLower + 1) % 40;
  noteOrderLower[orderIndxLower] = note;
  commandLastNoteLower();
}

void commandMonoNoteOffLower(byte note) {
  notesLower[note] = false;
  noteMsg = note;
  commandLastNoteLower();
}

void commandUnisonNoteOnUpper(byte note, byte velocity) {
  notesUpper[note] = true;
  noteMsg = note;      // explicitly set here
  noteVel = velocity;  // explicitly set here
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderUpper[orderIndxUpper] = note;
  commandLastNoteUniUpper();  // Last note priority
}

void commandUnisonNoteOffUpper(byte note) {
  notesUpper[note] = false;
  noteMsg = note;  // explicitly set here
  commandLastNoteUniUpper();
}

void commandUnisonNoteOnLower(byte note, byte velocity) {
  notesLower[note] = true;
  noteMsg = note;      // explicitly set here
  noteVel = velocity;  // explicitly set here
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderLower[orderIndxLower] = note;
  commandLastNoteUniLower();  // Last note priority
}

void commandUnisonNoteOffLower(byte note) {
  notesLower[note] = false;
  noteMsg = note;  // explicitly set here
  commandLastNoteUniLower();
}

void myNoteOn(byte channel, byte note, byte velocity) {

  prevNote = note;

  // --- JP-8 HOLD: physical key down tracking ---
  if (keyMode == 3) {  // SPLIT
    if (note < splitPoint) {
      keyDownLower[note] = true;
      holdLatchedLower[note] = false;
    } else {
      keyDownUpper[note] = true;
      holdLatchedUpper[note] = false;
    }
  } else if (keyMode == 0) {  // DUAL: note goes to BOTH engines
    keyDownLower[note] = true;
    keyDownUpper[note] = true;
    holdLatchedLower[note] = false;
    holdLatchedUpper[note] = false;
  } else {  // WHOLE
    keyDownWhole[note] = true;
    keyDownLower[note] = true;
    keyDownUpper[note] = true;
    holdLatchedLower[note] = false;
    holdLatchedUpper[note] = false;
  }

  // -------------------- ARP: capture entry order (ignore injected notes) --------------------
  if (!arpInjecting && arpMode != ARP_OFF) {
    if (keyMode == 3 && arpLowerOnlyWhenSplit) {
      if (note < splitPoint) arpAddNote(note);
    } else {
      arpAddNote(note);
    }
    arpCurrentVel = velocity;
  }

  // -------------------- ARP ACTIVE: keys are pattern entry only (no chord sound) --------------------
  if (arpConsumesKey(note)) {
    return;
  }

  int voiceNum = -1;

  switch (keyMode) {

    // WHOLE MODE (No changes needed if currently working)
    case 0:
      switch (lowerData[P_assign]) {
        case 0:
          voiceNum = getVoiceNo(-1) - 1;
          assignVoice(note, velocity, voiceNum);
          break;  // Poly1
        case 1:
          voiceNum = getVoiceNoPoly2(-1) - 1;
          assignVoice(note, velocity, voiceNum);
          break;                                             // Poly2
        case 2: commandMonoNoteOn(note, velocity); break;    // Mono
        case 3: commandUnisonNoteOn(note, velocity); break;  // Unison
      }
      voiceAssignment[note] = voiceNum;
      break;

    // DUAL MODE (Explicitly corrected, place this clearly here):
    case 1:
      {
        // Lower Split
        if (lowerData[P_assign] == 1) {  // Poly2 Lower
          int lowerVoice = getLowerSplitVoicePoly2(note);
          int oldNote = voiceToNoteLower[lowerVoice];
          if (oldNote >= 0) {
            releaseVoice(oldNote, lowerVoice);
            voiceAssignmentLower[oldNote] = -1;
          }
          assignVoice(note, velocity, lowerVoice);
          voiceAssignmentLower[note] = lowerVoice;
          voiceToNoteLower[lowerVoice] = note;
        } else if (lowerData[P_assign] == 0) {  // Poly1 Lower
          int lowerVoice = getLowerSplitVoice(note);
          assignVoice(note, velocity, lowerVoice);
          voiceAssignmentLower[note] = lowerVoice;
          voiceToNoteLower[lowerVoice] = note;
        } else if (lowerData[P_assign] == 2) {
          commandMonoNoteOnLower(note, velocity);
        } else if (lowerData[P_assign] == 3) {
          commandUnisonNoteOnLower(note, velocity);
        }

        // Upper Split
        if (upperData[P_assign] == 1) {  // Poly2 Upper
          int upperVoice = getUpperSplitVoicePoly2(note);
          int oldNote = voiceToNoteUpper[upperVoice - 4];
          if (oldNote >= 0) {
            releaseVoice(oldNote, upperVoice);
            voiceAssignmentUpper[oldNote] = -1;
          }
          assignVoice(note, velocity, upperVoice);
          voiceAssignmentUpper[note] = upperVoice;
          voiceToNoteUpper[upperVoice - 4] = note;
        } else if (upperData[P_assign] == 0) {  // Poly1 Upper
          int upperVoice = getUpperSplitVoice(note);
          assignVoice(note, velocity, upperVoice);
          voiceAssignmentUpper[note] = upperVoice;
          voiceToNoteUpper[upperVoice - 4] = note;
        } else if (upperData[P_assign] == 2) {
          commandMonoNoteOnUpper(note, velocity);
        } else if (upperData[P_assign] == 3) {
          commandUnisonNoteOnUpper(note, velocity);
        }
      }
      break;

      // SPLIT MODE (Also explicitly corrected, place here clearly):
    case 2:  // SPLIT MODE explicitly confirmed (note-on):
      if (note < splitPoint) {
        switch (lowerData[P_assign]) {
          case 0:
            voiceNum = getLowerSplitVoice(note);
            assignVoice(note, velocity, voiceNum);
            voiceAssignmentLower[note] = voiceNum;
            voiceToNoteLower[voiceNum] = note;
            break;
          case 1:
            voiceNum = getLowerSplitVoicePoly2(note);
            assignVoice(note, velocity, voiceNum);
            voiceAssignmentLower[note] = voiceNum;
            voiceToNoteLower[voiceNum] = note;
            break;
          case 2:
            commandMonoNoteOnLower(note, velocity);
            break;
          case 3:
            commandUnisonNoteOnLower(note, velocity);
            break;
        }
      } else {
        switch (upperData[P_assign]) {
          case 0:
            voiceNum = getUpperSplitVoice(note);
            assignVoice(note, velocity, voiceNum);
            voiceAssignmentUpper[note] = voiceNum;
            voiceToNoteUpper[voiceNum - 4] = note;
            break;
          case 1:
            voiceNum = getUpperSplitVoicePoly2(note);
            assignVoice(note, velocity, voiceNum);
            voiceAssignmentUpper[note] = voiceNum;
            voiceToNoteUpper[voiceNum - 4] = note;
            break;
          case 2:
            commandMonoNoteOnUpper(note, velocity);
            break;
          case 3:
            commandUnisonNoteOnUpper(note, velocity);
            break;
        }
      }
      break;
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {

  auto holdEffectiveLower = [&]() -> bool {
    if (keyMode == 2) return (holdManualLower || holdPedal);   // SPLIT: pedal affects LOWER
    return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL global
  };
  auto holdEffectiveUpper = [&]() -> bool {
    if (keyMode == 2) return (holdManualUpper);                // SPLIT: pedal does NOT affect UPPER
    return (holdManualLower || holdManualUpper || holdPedal);  // WHOLE/DUAL global
  };

  // "present" for arp removal rules = physically down OR held-by-hold
  auto arpPresentLower = [&](uint8_t n) -> bool {
    return keyDownLower[n] || holdLatchedLower[n];
  };
  auto arpPresentUpper = [&](uint8_t n) -> bool {
    return keyDownUpper[n] || holdLatchedUpper[n];
  };

  if (keyMode == 2) {
    // SPLIT
    if (note < splitPoint) keyDownLower[note] = false;
    else keyDownUpper[note] = false;

  } else if (keyMode == 1) {
    // DUAL: same key affects both engines
    keyDownLower[note] = false;
    keyDownUpper[note] = false;

  } else {
    // WHOLE
    keyDownWhole[note] = false;
    // Recommended mirroring so "physically held" tests work consistently everywhere
    keyDownLower[note] = false;
    keyDownUpper[note] = false;
  }

  if (keyMode == 2) {
    // SPLIT: latch only the side the note belongs to
    if (note < splitPoint) {
      if (holdEffectiveLower()) {
        holdLatchedLower[note] = true;

        // ARP: do not remove; note remains present via holdLatchedLower
        return;
      }
    } else {
      if (holdEffectiveUpper()) {
        holdLatchedUpper[note] = true;

        // ARP: do not remove; note remains present via holdLatchedUpper
        return;
      }
    }

  } else {
    // WHOLE or DUAL: hold is global
    if (holdEffectiveLower()) {  // same truth for upper in whole/dual
      holdLatchedLower[note] = true;
      holdLatchedUpper[note] = true;

      // ARP: do not remove; note remains present via holdLatched*
      return;
    }
  }

  if (!arpInjecting && arpMode != ARP_OFF) {

    if (keyMode == 2 && arpLowerOnlyWhenSplit) {
      // Split: JP-8 assigns arp to LOWER only
      if (note < splitPoint) {
        if (!arpPresentLower(note)) arpRemoveNote(note);
      }
    } else {
      // Whole/Dual: treat present if in either engine
      bool present = arpPresentLower(note) || arpPresentUpper(note);
      if (!present) arpRemoveNote(note);
    }
  }

  if (arpConsumesKey(note)) {
    return;
  }

  int assignedVoice = voiceAssignment[note];

  switch (keyMode) {

    // WHOLE MODE
    case 0:
      switch (lowerData[P_assign]) {
        case 0:
          assignedVoice = getVoiceNo(note) - 1;
          releaseVoice(note, assignedVoice);
          break;
        case 1:
          assignedVoice = getVoiceNoPoly2(note) - 1;
          releaseVoice(note, assignedVoice);
          break;
        case 2: commandMonoNoteOff(note); break;
        case 3: commandUnisonNoteOff(note); break;
      }
      break;

    // DUAL MODE
    case 1:
      {
        // Lower
        if (lowerData[P_assign] == 2) commandMonoNoteOffLower(note);
        else if (lowerData[P_assign] == 3) commandUnisonNoteOffLower(note);
        else {
          int lowerVoice = voiceAssignmentLower[note];
          if (lowerVoice >= 0 && lowerVoice <= 3 && voiceToNoteLower[lowerVoice] == note) {
            releaseVoice(note, lowerVoice);
            voiceAssignmentLower[note] = -1;
            voiceToNoteLower[lowerVoice] = -1;
          }
        }

        // Upper
        if (upperData[P_assign] == 2) commandMonoNoteOffUpper(note);
        else if (upperData[P_assign] == 3) commandUnisonNoteOffUpper(note);
        else {
          int upperVoice = voiceAssignmentUpper[note];
          if (upperVoice >= 4 && upperVoice <= 7 && voiceToNoteUpper[upperVoice - 4] == note) {
            releaseVoice(note, upperVoice);
            voiceAssignmentUpper[note] = -1;
            voiceToNoteUpper[upperVoice - 4] = -1;
          }
        }
      }
      break;

    // SPLIT MODE
    case 2:
      {
        if (note < splitPoint) {
          if (lowerData[P_assign] == 2) {
            commandMonoNoteOffLower(note);
          } else if (lowerData[P_assign] == 3) {
            commandUnisonNoteOffLower(note);
          } else {
            int lowerVoice = voiceAssignmentLower[note];
            if (lowerVoice >= 0 && lowerVoice <= 3 && voiceToNoteLower[lowerVoice] == note) {
              releaseVoice(note, lowerVoice);
              voiceAssignmentLower[note] = -1;
              voiceToNoteLower[lowerVoice] = -1;
            }
          }
        } else {
          if (upperData[P_assign] == 2) {
            commandMonoNoteOffUpper(note);
          } else if (upperData[P_assign] == 3) {
            commandUnisonNoteOffUpper(note);
          } else {
            int upperVoice = voiceAssignmentUpper[note];
            if (upperVoice >= 4 && upperVoice <= 7 && voiceToNoteUpper[upperVoice - 4] == note) {
              releaseVoice(note, upperVoice);
              voiceAssignmentUpper[note] = -1;
              voiceToNoteUpper[upperVoice - 4] = -1;
            }
          }
        }
      }
      break;
  }
}

void commandMonoNoteOn(byte note, byte velocity) {
  notesWhole[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderWhole[orderIndxWhole] = note;
  commandLastNoteWhole();
}

void commandMonoNoteOff(byte note) {
  notesWhole[note] = false;
  noteMsg = note;
  commandLastNoteWhole();
}

void commandLastNoteWhole() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderWhole[mod(orderIndxWhole - i, 40)];
    if (notesWhole[idx]) {
      assignVoice(idx, noteVel, 0);
      return;
    }
  }
  releaseVoice(noteMsg, 0);
}

void commandUnisonNoteOn(byte note, byte velocity) {
  notesWhole[note] = true;
  noteMsg = note;
  noteVel = velocity;
  orderIndxWhole = (orderIndxWhole + 1) % 40;
  noteOrderWhole[orderIndxWhole] = note;
  commandLastNoteUniWhole();
}

void commandUnisonNoteOff(byte note) {
  notesWhole[note] = false;
  noteMsg = note;
  commandLastNoteUniWhole();
}

void commandLastNoteUniWhole() {
  for (int i = 0; i < 40; i++) {
    int8_t idx = noteOrderWhole[mod(orderIndxWhole - i, 40)];
    if (notesWhole[idx]) {
      for (int v = 0; v < 12; v++) assignVoice(idx, noteVel, v);
      return;
    }
  }
  for (int v = 0; v < 12; v++) releaseVoice(noteMsg, v);
}

void recallPatch(int patchNo) {
  allNotesOff();
  if (!updateParams) {
    MIDI.sendProgramChange(patchNo - 1, midiOutCh);
  }
  delay(50);
  announce = true;
  File patchFile = SD.open(String(patchNo).c_str());
  if (!patchFile) {
    Serial.println("File not found");
  } else {
    String data[NO_OF_PARAMS];  //Array of data read in
    recallPatchData(patchFile, data);
    setCurrentPatchData(data);
    patchFile.close();
  }
  announce = false;
}

void allNotesOff() {

  voices[0].note = -1;
  voiceOn[0] = false;


  voices[1].note = -1;
  voiceOn[1] = false;


  voices[2].note = -1;
  voiceOn[2] = false;


  voices[3].note = -1;
  voiceOn[3] = false;


  voices[4].note = -1;
  voiceOn[4] = false;


  voices[5].note = -1;
  voiceOn[5] = false;


  voices[6].note = -1;
  voiceOn[6] = false;


  voices[7].note = -1;
  voiceOn[7] = false;

  voices[8].note = -1;
  voiceOn[8] = false;


  voices[9].note = -1;
  voiceOn[9] = false;


  voices[10].note = -1;
  voiceOn[10] = false;


  voices[11].note = -1;
  voiceOn[11] = false;
}

String getCurrentPatchData() {
  return patchName + "," + String(upperData[P_lfo1_wave]) + "," + String(lowerData[P_lfo1_wave]) + "," + String(upperData[P_lfo1_rate]) + "," + String(lowerData[P_lfo1_rate]) + "," + String(upperData[P_lfo1_delay]) + "," + String(lowerData[P_lfo1_delay])
         + "," + String(upperData[P_lfo1_lfo2]) + "," + String(lowerData[P_lfo1_lfo2]) + "," + String(upperData[P_lfo1_sync]) + "," + String(lowerData[P_lfo1_sync]) + "," + String(upperData[P_lfo2_wave]) + "," + String(lowerData[P_lfo2_wave])
         + "," + String(upperData[P_lfo2_rate]) + "," + String(lowerData[P_lfo2_rate]) + "," + String(upperData[P_lfo2_delay]) + "," + String(lowerData[P_lfo2_delay]) + "," + String(upperData[P_lfo2_lfo1]) + "," + String(lowerData[P_lfo2_lfo1])
         + "," + String(upperData[P_lfo2_sync]) + "," + String(lowerData[P_lfo2_sync]) + "," + String(upperData[P_dco1_PW]) + "," + String(lowerData[P_dco1_PW]) + "," + String(upperData[P_dco1_PWM_env]) + "," + String(lowerData[P_dco1_PWM_env])
         + "," + String(upperData[P_dco1_PWM_lfo]) + "," + String(lowerData[P_dco1_PWM_lfo]) + "," + String(upperData[P_dco1_PWM_dyn]) + "," + String(lowerData[P_dco1_PWM_dyn]) + "," + String(upperData[P_dco1_PWM_env_source]) + "," + String(lowerData[P_dco1_PWM_env_source])
         + "," + String(upperData[P_dco1_PWM_env_pol]) + "," + String(lowerData[P_dco1_PWM_env_pol]) + "," + String(upperData[P_dco1_PWM_lfo_source]) + "," + String(lowerData[P_dco1_PWM_lfo_source]) + "," + String(upperData[P_dco1_pitch_env]) + "," + String(lowerData[P_dco1_pitch_env])
         + "," + String(upperData[P_dco1_pitch_env_source]) + "," + String(lowerData[P_dco1_pitch_env_source]) + "," + String(upperData[P_dco1_pitch_env_pol]) + "," + String(lowerData[P_dco1_pitch_env_pol]) + "," + String(upperData[P_dco1_pitch_lfo]) + "," + String(lowerData[P_dco1_pitch_lfo])
         + "," + String(upperData[P_dco1_pitch_lfo_source]) + "," + String(lowerData[P_dco1_pitch_lfo_source]) + "," + String(upperData[P_dco1_pitch_dyn]) + "," + String(lowerData[P_dco1_pitch_dyn]) + "," + String(upperData[P_dco1_wave]) + "," + String(lowerData[P_dco1_wave])
         + "," + String(upperData[P_dco1_range]) + "," + String(lowerData[P_dco1_range]) + "," + String(upperData[P_dco1_tune]) + "," + String(lowerData[P_dco1_tune]) + "," + String(upperData[P_dco1_mode]) + "," + String(lowerData[P_dco1_mode])
         + "," + String(upperData[P_dco2_PW]) + "," + String(lowerData[P_dco2_PW]) + "," + String(upperData[P_dco2_PWM_env]) + "," + String(lowerData[P_dco2_PWM_env]) + "," + String(upperData[P_dco2_PWM_lfo]) + "," + String(lowerData[P_dco2_PWM_lfo])
         + "," + String(upperData[P_dco2_PWM_dyn]) + "," + String(lowerData[P_dco2_PWM_dyn]) + "," + String(upperData[P_dco2_PWM_env_source]) + "," + String(lowerData[P_dco2_PWM_env_source]) + "," + String(upperData[P_dco2_PWM_env_pol]) + "," + String(lowerData[P_dco2_PWM_env_pol])
         + "," + String(upperData[P_dco2_PWM_lfo_source]) + "," + String(lowerData[P_dco2_PWM_lfo_source]) + "," + String(upperData[P_dco2_pitch_env]) + "," + String(lowerData[P_dco2_pitch_env]) + "," + String(upperData[P_dco2_pitch_env_source]) + "," + String(lowerData[P_dco2_pitch_env_source])
         + "," + String(upperData[P_dco2_pitch_env_pol]) + "," + String(lowerData[P_dco2_pitch_env_pol]) + "," + String(upperData[P_dco2_pitch_lfo]) + "," + String(lowerData[P_dco2_pitch_lfo]) + "," + String(upperData[P_dco2_pitch_lfo_source]) + "," + String(lowerData[P_dco2_pitch_lfo_source])
         + "," + String(upperData[P_dco2_pitch_dyn]) + "," + String(lowerData[P_dco2_pitch_dyn]) + "," + String(upperData[P_dco2_wave]) + "," + String(lowerData[P_dco2_wave]) + "," + String(upperData[P_dco2_range]) + "," + String(lowerData[P_dco2_range])
         + "," + String(upperData[P_dco2_tune]) + "," + String(lowerData[P_dco2_tune]) + "," + String(upperData[P_dco2_fine]) + "," + String(lowerData[P_dco2_fine]) + "," + String(upperData[P_dco1_level]) + "," + String(lowerData[P_dco1_level])
         + "," + String(upperData[P_dco2_level]) + "," + String(lowerData[P_dco2_level]) + "," + String(upperData[P_dco2_mod]) + "," + String(lowerData[P_dco2_mod]) + "," + String(upperData[P_dco_mix_env_pol]) + "," + String(lowerData[P_dco_mix_env_pol])
         + "," + String(upperData[P_dco_mix_env_source]) + "," + String(lowerData[P_dco_mix_env_source]) + "," + String(upperData[P_dco_mix_dyn]) + "," + String(lowerData[P_dco_mix_dyn]) + "," + String(upperData[P_vcf_hpf]) + "," + String(lowerData[P_vcf_hpf])
         + "," + String(upperData[P_vcf_cutoff]) + "," + String(lowerData[P_vcf_cutoff]) + "," + String(upperData[P_vcf_res]) + "," + String(lowerData[P_vcf_res]) + "," + String(upperData[P_vcf_kb]) + "," + String(lowerData[P_vcf_kb])
         + "," + String(upperData[P_vcf_env]) + "," + String(lowerData[P_vcf_env]) + "," + String(upperData[P_vcf_lfo1]) + "," + String(lowerData[P_vcf_lfo1]) + "," + String(upperData[P_vcf_lfo2]) + "," + String(lowerData[P_vcf_lfo2])
         + "," + String(upperData[P_vcf_env_source]) + "," + String(lowerData[P_vcf_env_source]) + "," + String(upperData[P_vcf_env_pol]) + "," + String(lowerData[P_vcf_env_pol]) + "," + String(upperData[P_vcf_dyn]) + "," + String(lowerData[P_vcf_dyn])
         + "," + String(upperData[P_vca_mod]) + "," + String(lowerData[P_vca_mod]) + "," + String(upperData[P_vca_env_source]) + "," + String(lowerData[P_vca_env_source]) + "," + String(upperData[P_vca_dyn]) + "," + String(lowerData[P_vca_dyn])
         + "," + String(upperData[P_time1]) + "," + String(lowerData[P_time1]) + "," + String(upperData[P_level1]) + "," + String(lowerData[P_level1]) + "," + String(upperData[P_time2]) + "," + String(lowerData[P_time2])
         + "," + String(upperData[P_level2]) + "," + String(lowerData[P_level2]) + "," + String(upperData[P_time3]) + "," + String(lowerData[P_time3]) + "," + String(upperData[P_level3]) + "," + String(lowerData[P_level3])
         + "," + String(upperData[P_time4]) + "," + String(lowerData[P_time4]) + "," + String(upperData[P_env5stage_mode]) + "," + String(lowerData[P_env5stage_mode]) + "," + String(upperData[P_env2_time1]) + "," + String(lowerData[P_env2_time1])
         + "," + String(upperData[P_env2_level1]) + "," + String(lowerData[P_env2_level1]) + "," + String(upperData[P_env2_time2]) + "," + String(lowerData[P_env2_time2]) + "," + String(upperData[P_env2_level2]) + "," + String(lowerData[P_env2_level2])
         + "," + String(upperData[P_env2_time3]) + "," + String(lowerData[P_env2_time3]) + "," + String(upperData[P_env2_level3]) + "," + String(lowerData[P_env2_level3]) + "," + String(upperData[P_env2_time4]) + "," + String(lowerData[P_env2_time4])
         + "," + String(upperData[P_env2_5stage_mode]) + "," + String(lowerData[P_env2_5stage_mode]) + "," + String(upperData[P_attack]) + "," + String(lowerData[P_attack]) + "," + String(upperData[P_decay]) + "," + String(lowerData[P_decay])
         + "," + String(upperData[P_sustain]) + "," + String(lowerData[P_sustain]) + "," + String(upperData[P_release]) + "," + String(lowerData[P_release]) + "," + String(upperData[P_adsr_mode]) + "," + String(lowerData[P_adsr_mode])
         + "," + String(upperData[P_env4_attack]) + "," + String(lowerData[P_env4_attack]) + "," + String(upperData[P_env4_decay]) + "," + String(lowerData[P_env4_decay]) + "," + String(upperData[P_env4_sustain]) + "," + String(lowerData[P_env4_sustain])
         + "," + String(upperData[P_env4_release]) + "," + String(lowerData[P_env4_release]) + "," + String(upperData[P_env4_adsr_mode]) + "," + String(lowerData[P_env4_adsr_mode]) + "," + String(upperData[P_chorus]) + "," + String(lowerData[P_chorus])
         + "," + String(upperData[P_portamento_sw]) + "," + String(lowerData[P_portamento_sw]) + "," + String(upperData[P_octave_down]) + "," + String(lowerData[P_octave_down]) + "," + String(upperData[P_octave_up]) + "," + String(lowerData[P_octave_up])
         + "," + String(upperData[P_mod_lfo]) + "," + String(lowerData[P_mod_lfo]) + "," + String(upperData[P_unisondetune]) + "," + String(lowerData[P_unisondetune]) + "," + String(upperData[P_bend_enable]) + "," + String(lowerData[P_bend_enable])
         + "," + String(upperData[P_assign]) + "," + String(lowerData[P_assign]) + "," + String(toneNameU) + "," + String(toneNameL)
         + "," + String(keyMode) + "," + String(dualdetune) + "," + String(at_vib) + "," + String(at_lpf) + "," + String(at_vol) + "," + String(balance)
         + "," + String(portamento) + "," + String(volume) + "," + String(bend_range) + "," + String(chaseLevel) + "," + String(chaseMode) + "," + String(chaseTime)
         + "," + String(chasePlay);
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];

  upperData[P_lfo1_wave] = data[1].toInt();
  lowerData[P_lfo1_wave] = data[2].toInt();
  upperData[P_lfo1_rate] = data[3].toInt();
  lowerData[P_lfo1_rate] = data[4].toInt();
  upperData[P_lfo1_delay] = data[5].toInt();
  lowerData[P_lfo1_delay] = data[6].toInt();
  upperData[P_lfo1_lfo2] = data[7].toInt();
  lowerData[P_lfo1_lfo2] = data[8].toInt();
  upperData[P_lfo1_sync] = data[9].toInt();
  lowerData[P_lfo1_sync] = data[10].toInt();
  upperData[P_lfo2_wave] = data[11].toInt();
  lowerData[P_lfo2_wave] = data[12].toInt();
  upperData[P_lfo2_rate] = data[13].toInt();
  lowerData[P_lfo2_rate] = data[14].toInt();
  upperData[P_lfo2_delay] = data[15].toInt();
  lowerData[P_lfo2_delay] = data[16].toInt();
  upperData[P_lfo2_lfo1] = data[17].toInt();
  lowerData[P_lfo2_lfo1] = data[18].toInt();
  upperData[P_lfo2_sync] = data[19].toInt();
  lowerData[P_lfo2_sync] = data[20].toInt();

  upperData[P_dco1_PW] = data[21].toInt();
  lowerData[P_dco1_PW] = data[22].toInt();
  upperData[P_dco1_PWM_env] = data[23].toInt();
  lowerData[P_dco1_PWM_env] = data[24].toInt();
  upperData[P_dco1_PWM_lfo] = data[25].toInt();
  lowerData[P_dco1_PWM_lfo] = data[26].toInt();
  upperData[P_dco1_PWM_dyn] = data[27].toInt();
  lowerData[P_dco1_PWM_dyn] = data[28].toInt();
  upperData[P_dco1_PWM_env_source] = data[29].toInt();
  lowerData[P_dco1_PWM_env_source] = data[30].toInt();
  upperData[P_dco1_PWM_env_pol] = data[31].toInt();
  lowerData[P_dco1_PWM_env_pol] = data[32].toInt();
  upperData[P_dco1_PWM_lfo_source] = data[33].toInt();
  lowerData[P_dco1_PWM_lfo_source] = data[34].toInt();
  upperData[P_dco1_pitch_env] = data[35].toInt();
  lowerData[P_dco1_pitch_env] = data[36].toInt();
  upperData[P_dco1_pitch_env_source] = data[37].toInt();
  lowerData[P_dco1_pitch_env_source] = data[38].toInt();
  upperData[P_dco1_pitch_env_pol] = data[39].toInt();
  lowerData[P_dco1_pitch_env_pol] = data[40].toInt();
  upperData[P_dco1_pitch_lfo] = data[41].toInt();
  lowerData[P_dco1_pitch_lfo] = data[42].toInt();
  upperData[P_dco1_pitch_lfo_source] = data[43].toInt();
  lowerData[P_dco1_pitch_lfo_source] = data[44].toInt();
  upperData[P_dco1_pitch_dyn] = data[45].toInt();
  lowerData[P_dco1_pitch_dyn] = data[46].toInt();
  upperData[P_dco1_wave] = data[47].toInt();
  lowerData[P_dco1_wave] = data[48].toInt();
  upperData[P_dco1_range] = data[49].toInt();
  lowerData[P_dco1_range] = data[50].toInt();
  upperData[P_dco1_tune] = data[51].toInt();
  lowerData[P_dco1_tune] = data[52].toInt();
  upperData[P_dco1_mode] = data[53].toInt();
  lowerData[P_dco1_mode] = data[54].toInt();

  upperData[P_dco2_PW] = data[55].toInt();
  lowerData[P_dco2_PW] = data[56].toInt();
  upperData[P_dco2_PWM_env] = data[57].toInt();
  lowerData[P_dco2_PWM_env] = data[58].toInt();
  upperData[P_dco2_PWM_lfo] = data[59].toInt();
  lowerData[P_dco2_PWM_lfo] = data[60].toInt();
  upperData[P_dco2_PWM_dyn] = data[61].toInt();
  lowerData[P_dco2_PWM_dyn] = data[62].toInt();
  upperData[P_dco2_PWM_env_source] = data[63].toInt();
  lowerData[P_dco2_PWM_env_source] = data[64].toInt();
  upperData[P_dco2_PWM_env_pol] = data[65].toInt();
  lowerData[P_dco2_PWM_env_pol] = data[66].toInt();
  upperData[P_dco2_PWM_lfo_source] = data[67].toInt();
  lowerData[P_dco2_PWM_lfo_source] = data[68].toInt();
  upperData[P_dco2_pitch_env] = data[69].toInt();
  lowerData[P_dco2_pitch_env] = data[70].toInt();
  upperData[P_dco2_pitch_env_source] = data[71].toInt();
  lowerData[P_dco2_pitch_env_source] = data[72].toInt();
  upperData[P_dco2_pitch_env_pol] = data[73].toInt();
  lowerData[P_dco2_pitch_env_pol] = data[74].toInt();
  upperData[P_dco2_pitch_lfo] = data[75].toInt();
  lowerData[P_dco2_pitch_lfo] = data[76].toInt();
  upperData[P_dco2_pitch_lfo_source] = data[77].toInt();
  lowerData[P_dco2_pitch_lfo_source] = data[78].toInt();
  upperData[P_dco2_pitch_dyn] = data[79].toInt();
  lowerData[P_dco2_pitch_dyn] = data[80].toInt();
  upperData[P_dco2_wave] = data[81].toInt();
  lowerData[P_dco2_wave] = data[82].toInt();
  upperData[P_dco2_range] = data[83].toInt();
  lowerData[P_dco2_range] = data[84].toInt();
  upperData[P_dco2_tune] = data[85].toInt();
  lowerData[P_dco2_tune] = data[86].toInt();
  upperData[P_dco2_fine] = data[87].toInt();
  lowerData[P_dco2_fine] = data[88].toInt();

  upperData[P_dco1_level] = data[89].toInt();
  lowerData[P_dco1_level] = data[90].toInt();
  upperData[P_dco2_level] = data[91].toInt();
  lowerData[P_dco2_level] = data[92].toInt();
  upperData[P_dco2_mod] = data[93].toInt();
  lowerData[P_dco2_mod] = data[94].toInt();
  upperData[P_dco_mix_env_pol] = data[95].toInt();
  lowerData[P_dco_mix_env_pol] = data[96].toInt();
  upperData[P_dco_mix_env_source] = data[97].toInt();
  lowerData[P_dco_mix_env_source] = data[98].toInt();
  upperData[P_dco_mix_dyn] = data[99].toInt();
  lowerData[P_dco_mix_dyn] = data[100].toInt();

  upperData[P_vcf_hpf] = data[101].toInt();
  lowerData[P_vcf_hpf] = data[102].toInt();
  upperData[P_vcf_cutoff] = data[103].toInt();
  lowerData[P_vcf_cutoff] = data[104].toInt();
  upperData[P_vcf_res] = data[105].toInt();
  lowerData[P_vcf_res] = data[106].toInt();
  upperData[P_vcf_kb] = data[107].toInt();
  lowerData[P_vcf_kb] = data[108].toInt();
  upperData[P_vcf_env] = data[109].toInt();
  lowerData[P_vcf_env] = data[110].toInt();
  upperData[P_vcf_lfo1] = data[111].toInt();
  lowerData[P_vcf_lfo1] = data[112].toInt();
  upperData[P_vcf_lfo2] = data[113].toInt();
  lowerData[P_vcf_lfo2] = data[114].toInt();
  upperData[P_vcf_env_source] = data[115].toInt();
  lowerData[P_vcf_env_source] = data[116].toInt();
  upperData[P_vcf_env_pol] = data[117].toInt();
  lowerData[P_vcf_env_pol] = data[118].toInt();
  upperData[P_vcf_dyn] = data[119].toInt();
  lowerData[P_vcf_dyn] = data[120].toInt();

  upperData[P_vca_mod] = data[121].toInt();
  lowerData[P_vca_mod] = data[122].toInt();
  upperData[P_vca_env_source] = data[123].toInt();
  lowerData[P_vca_env_source] = data[124].toInt();
  upperData[P_vca_dyn] = data[125].toInt();
  lowerData[P_vca_dyn] = data[126].toInt();

  upperData[P_time1] = data[127].toInt();
  lowerData[P_time1] = data[128].toInt();
  upperData[P_level1] = data[129].toInt();
  lowerData[P_level1] = data[130].toInt();
  upperData[P_time2] = data[131].toInt();
  lowerData[P_time2] = data[132].toInt();
  upperData[P_level2] = data[133].toInt();
  lowerData[P_level2] = data[134].toInt();
  upperData[P_time3] = data[135].toInt();
  lowerData[P_time3] = data[136].toInt();
  upperData[P_level3] = data[137].toInt();
  lowerData[P_level3] = data[138].toInt();
  upperData[P_time4] = data[139].toInt();
  lowerData[P_time4] = data[140].toInt();
  upperData[P_env5stage_mode] = data[141].toInt();
  lowerData[P_env5stage_mode] = data[142].toInt();

  upperData[P_env2_time1] = data[143].toInt();
  lowerData[P_env2_time1] = data[144].toInt();
  upperData[P_env2_level1] = data[145].toInt();
  lowerData[P_env2_level1] = data[146].toInt();
  upperData[P_env2_time2] = data[147].toInt();
  lowerData[P_env2_time2] = data[148].toInt();
  upperData[P_env2_level2] = data[149].toInt();
  lowerData[P_env2_level2] = data[150].toInt();
  upperData[P_env2_time3] = data[151].toInt();
  lowerData[P_env2_time3] = data[152].toInt();
  upperData[P_env2_level3] = data[153].toInt();
  lowerData[P_env2_level3] = data[154].toInt();
  upperData[P_env2_time4] = data[155].toInt();
  lowerData[P_env2_time4] = data[156].toInt();
  upperData[P_env2_5stage_mode] = data[157].toInt();
  lowerData[P_env2_5stage_mode] = data[158].toInt();

  upperData[P_attack] = data[159].toInt();
  lowerData[P_attack] = data[160].toInt();
  upperData[P_decay] = data[161].toInt();
  lowerData[P_decay] = data[162].toInt();
  upperData[P_sustain] = data[163].toInt();
  lowerData[P_sustain] = data[164].toInt();
  upperData[P_release] = data[165].toInt();
  lowerData[P_release] = data[166].toInt();
  upperData[P_adsr_mode] = data[167].toInt();
  lowerData[P_adsr_mode] = data[168].toInt();

  upperData[P_env4_attack] = data[169].toInt();
  lowerData[P_env4_attack] = data[170].toInt();
  upperData[P_env4_decay] = data[171].toInt();
  lowerData[P_env4_decay] = data[172].toInt();
  upperData[P_env4_sustain] = data[173].toInt();
  lowerData[P_env4_sustain] = data[174].toInt();
  upperData[P_env4_release] = data[175].toInt();
  lowerData[P_env4_release] = data[176].toInt();
  upperData[P_env4_adsr_mode] = data[177].toInt();
  lowerData[P_env4_adsr_mode] = data[178].toInt();

  upperData[P_chorus] = data[179].toInt();
  lowerData[P_chorus] = data[180].toInt();
  upperData[P_portamento_sw] = data[181].toInt();
  lowerData[P_portamento_sw] = data[182].toInt();
  upperData[P_octave_down] = data[183].toInt();
  lowerData[P_octave_down] = data[184].toInt();
  upperData[P_octave_up] = data[185].toInt();
  lowerData[P_octave_up] = data[186].toInt();
  upperData[P_mod_lfo] = data[187].toInt();
  lowerData[P_mod_lfo] = data[188].toInt();
  upperData[P_unisondetune] = data[189].toInt();
  lowerData[P_unisondetune] = data[190].toInt();
  upperData[P_bend_enable] = data[191].toInt();
  lowerData[P_bend_enable] = data[192].toInt();
  upperData[P_assign] = data[193].toInt();
  lowerData[P_assign] = data[194].toInt();
  toneNameU = data[195];
  toneNameL = data[196];

  keyMode = data[197].toInt();
  dualdetune = data[198].toInt();
  at_vib = data[199].toInt();
  at_lpf = data[200].toInt();
  at_vol = data[201].toInt();
  balance = data[201].toInt();
  portamento = data[203].toInt();
  volume = data[204].toInt();
  bend_range = data[205].toInt();
  chaseLevel = data[206].toInt();
  chaseMode = data[207].toInt();
  chaseTime = data[208].toInt();
  chasePlay = data[209].toInt();

  //Patchname
  updatePatchname();
  updateButtons();
  updateUpperToneData();
  updateLowerToneData();

  Serial.print("Set Patch: ");
  Serial.println(patchName);
}

void updateButtons() {
  updateeditMode(0);
  //updateenv5stage(0);
  //updateadsr(0);
  LAST_PARAM = 0x00;
}

void updateUpperToneData() {
  bool upperSWsafe = upperSW;
  upperSW = true;
  updatelfo1_wave(0);
  updatelfo1_rate(0);
  updatelfo1_delay(0);
  updatelfo1_lfo2(0);
  updatedco1_PW(0);
  updatedco1_PWM_env(0);
  updatedco1_PWM_env(0);

  updatelfo2_wave(0);
  updatelfo2_rate(0);
  updatelfo2_delay(0);
  updatelfo2_lfo1(0);
  updatedco2_PW(0);
  updatedco2_PWM_env(0);
  updatedco2_PWM_env(0);

  LAST_PARAM = 0x00;
  upperSW = upperSWsafe;
}

void updateLowerToneData() {
  bool upperSWsafe = upperSW;
  upperSW = false;
  updatelfo1_wave(0);
  updatelfo1_rate(0);
  updatelfo1_delay(0);
  updatelfo1_lfo2(0);
  updatedco1_PW(0);
  updatedco1_PWM_env(0);
  updatedco1_PWM_env(0);

  updatelfo2_wave(0);
  updatelfo2_rate(0);
  updatelfo2_delay(0);
  updatelfo2_lfo1(0);
  updatedco2_PW(0);
  updatedco2_PWM_env(0);
  updatedco2_PWM_env(0);

  LAST_PARAM = 0x00;
  upperSW = upperSWsafe;
}

void showSettingsPage() {
  showSettingsPage(settings::current_setting(), settings::current_setting_value(), state);
}

void updatePatchname() {
  showPatchPage(String(patchNo), patchName);
}

void checkSwitches() {

  saveButton.update();
  if (saveButton.held()) {
    switch (state) {
      case PARAMETER:
      case PATCH:
        state = DELETE;
        updateScreen();
        break;
    }
  } else if (saveButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        if (patches.size() < PATCHES_LIMIT) {
          resetPatchesOrdering();  //Reset order of patches from first patch
          patches.push({ patches.size() + 1, INITPATCHNAME });
          state = SAVE;
        }
        updateScreen();
        break;
      case SAVE:
        //Save as new patch with INITIALPATCH name or overwrite existing keeping name - bypassing patch renaming
        patchName = patches.last().patchName;
        state = PATCH;
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patches.last().patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        updateScreen();
        break;
      case PATCHNAMING:
        if (renamedPatch.length() > 0) patchName = renamedPatch;  //Prevent empty strings
        state = PATCH;
        savePatch(String(patches.last().patchNo).c_str(), getCurrentPatchData());
        showPatchPage(patches.last().patchNo, patchName);
        patchNo = patches.last().patchNo;
        loadPatches();  //Get rid of pushed patch if it wasn't saved
        setPatchesOrdering(patchNo);
        renamedPatch = "";
        state = PARAMETER;
        updateScreen();
        break;
    }
  }

  settingsButton.update();
  if (settingsButton.held()) {
    //If recall held, set current patch to match current hardware state
    //Reinitialise all hardware values to force them to be re-read if different
    state = REINITIALISE;
    reinitialiseToPanel();
    updateScreen();
  } else if (settingsButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;
      case SETTINGS:
        showSettingsPage();
        updateScreen();
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;
    }
  }

  backButton.update();
  if (backButton.held()) {
    //If Back button held, Panic - all notes off
    allNotesOff();
    updateScreen();
  } else if (backButton.numClicks() == 1) {
    switch (state) {
      case RECALL:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        updateScreen();
        break;
      case SAVE:
        renamedPatch = "";
        state = PARAMETER;
        loadPatches();  //Remove patch that was to be saved
        setPatchesOrdering(patchNo);
        updateScreen();
        break;
      case PATCHNAMING:
        charIndex = 0;
        renamedPatch = "";
        state = SAVE;
        updateScreen();
        break;
      case DELETE:
        setPatchesOrdering(patchNo);
        state = PARAMETER;
        updateScreen();
        break;
      case SETTINGS:
        state = PARAMETER;
        updateScreen();
        break;
      case SETTINGSVALUE:
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;
    }
  }

  //Encoder switch
  recallButton.update();
  if (recallButton.held()) {
    //If Recall button held, return to current patch setting
    //which clears any changes made
    state = PATCH;
    //Recall the current patch
    patchNo = patches.first().patchNo;
    recallPatch(patchNo);
    state = PARAMETER;
    updateScreen();
  } else if (recallButton.numClicks() == 1) {
    switch (state) {
      case PARAMETER:
        state = RECALL;  //show patch list
        updateScreen();
        break;
      case RECALL:
        state = PATCH;
        //Recall the current patch
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        updateScreen();
        break;
      case SAVE:
        showRenamingPage(patches.last().patchName);
        patchName = patches.last().patchName;
        state = PATCHNAMING;
        updateScreen();
        break;
      case PATCHNAMING:
        if (renamedPatch.length() < 13) {
          renamedPatch.concat(String(currentCharacter));
          charIndex = 0;
          currentCharacter = CHARACTERS[charIndex];
          showRenamingPage(renamedPatch);
        }
        updateScreen();
        break;
      case DELETE:
        //Don't delete final patch
        if (patches.size() > 1) {
          state = DELETEMSG;
          patchNo = patches.first().patchNo;     //PatchNo to delete from SD card
          patches.shift();                       //Remove patch from circular buffer
          deletePatch(String(patchNo).c_str());  //Delete from SD card
          loadPatches();                         //Repopulate circular buffer to start from lowest Patch No
          renumberPatchesOnSD();
          loadPatches();                      //Repopulate circular buffer again after delete
          patchNo = patches.first().patchNo;  //Go back to 1
          recallPatch(patchNo);               //Load first patch
        }
        state = PARAMETER;
        updateScreen();
        break;
      case SETTINGS:
        state = SETTINGSVALUE;
        showSettingsPage();
        updateScreen();
        break;
      case SETTINGSVALUE:
        settings::save_current_value();
        state = SETTINGS;
        showSettingsPage();
        updateScreen();
        break;
    }
  }
}

void reinitialiseToPanel() {
  // //This sets the current patch to be the same as the current hardware panel state - all the pots
  // //The four button controls stay the same state
  // //This reinialises the previous hardware values to force a re-read
  // muxInput = 0;
  // for (int i = 0; i < MUXCHANNELS; i++) {
  // }
  // patchName = INITPATCHNAME;
  // showPatchPage("Initial", "Panel Settings");
}

void checkEncoder() {
  //Encoder works with relative inc and dec values
  //Detent encoder goes up in 4 steps, hence +/-3

  long encRead = encoder.read();
  if ((encCW && encRead > encPrevious + 3) || (!encCW && encRead < encPrevious - 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.push(patches.shift());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        updateScreen();
        break;
      case RECALL:
        patches.push(patches.shift());
        updateScreen();
        break;
      case SAVE:
        patches.push(patches.shift());
        updateScreen();
        break;
      case PATCHNAMING:
        if (charIndex == TOTALCHARS) charIndex = 0;  //Wrap around
        currentCharacter = CHARACTERS[charIndex++];
        showRenamingPage(renamedPatch + currentCharacter);
        updateScreen();
        break;
      case DELETE:
        patches.push(patches.shift());
        updateScreen();
        break;
      case SETTINGS:
        settings::increment_setting();
        showSettingsPage();
        updateScreen();
        break;
      case SETTINGSVALUE:
        settings::increment_setting_value();
        showSettingsPage();
        updateScreen();
        break;
    }
    encPrevious = encRead;
  } else if ((encCW && encRead < encPrevious - 3) || (!encCW && encRead > encPrevious + 3)) {
    switch (state) {
      case PARAMETER:
        state = PATCH;
        patches.unshift(patches.pop());
        patchNo = patches.first().patchNo;
        recallPatch(patchNo);
        state = PARAMETER;
        updateScreen();
        break;
      case RECALL:
        patches.unshift(patches.pop());
        updateScreen();
        break;
      case SAVE:
        patches.unshift(patches.pop());
        updateScreen();
        break;
      case PATCHNAMING:
        if (charIndex == -1)
          charIndex = TOTALCHARS - 1;
        currentCharacter = CHARACTERS[charIndex--];
        showRenamingPage(renamedPatch + currentCharacter);
        updateScreen();
        break;
      case DELETE:
        patches.unshift(patches.pop());
        updateScreen();
        break;
      case SETTINGS:
        settings::decrement_setting();
        showSettingsPage();
        updateScreen();
        break;
      case SETTINGSVALUE:
        settings::decrement_setting_value();
        showSettingsPage();
        updateScreen();
        break;
    }
    encPrevious = encRead;
  }
}

void sendCustomSysEx(byte outChannel, byte parameter, byte value) {
  const byte sysexData[] = {
    0xF0,
    0x41,
    0x39,  // Correct IPR opcode
    (byte)(outChannel & 0x0F),
    0x24,
    0x30,
    0x01,
    (byte)(parameter & 0x7F),
    (byte)(value & 0x7F),
    0xF7
  };

  MIDI.sendSysEx(sizeof(sysexData), sysexData, true);
}

void midiCCOut(int CC, int value) {
  switch (keyMode) {
    case 0:
      MIDI.sendControlChange(99, 1, midiOutCh);     // NRPN MSB
      MIDI.sendControlChange(98, CC, midiOutCh);    // NRPN LSB
      MIDI.sendControlChange(6, value, midiOutCh);  // Data Entry MSB
      break;

    case 1:
      MIDI.sendControlChange(99, 0, midiOutCh);     // NRPN MSB
      MIDI.sendControlChange(98, CC, midiOutCh);    // NRPN LSB
      MIDI.sendControlChange(6, value, midiOutCh);  // Data Entry MSB
      break;

    case 2:
      MIDI.sendControlChange(99, 1, midiOutCh);     // NRPN MSB
      MIDI.sendControlChange(98, CC, midiOutCh);    // NRPN LSB
      MIDI.sendControlChange(6, value, midiOutCh);  // Data Entry MSB

      MIDI.sendControlChange(99, 0, midiOutCh);     // NRPN MSB
      MIDI.sendControlChange(98, CC, midiOutCh);    // NRPN LSB
      MIDI.sendControlChange(6, value, midiOutCh);  // Data Entry MSB
      break;
  }
}

void pingPongStep(int &value, bool &goingUp) {
  if (goingUp) {
    value++;
    if (value >= 2) goingUp = false;  // hit 2 → reverse
  } else {
    value--;
    if (value <= 0) goingUp = true;  // hit 0 → reverse
  }
}

void mainButtonChanged(Button *btn, bool released) {

  switch (btn->id) {

    // case OCTAVE_DOWN_BUTTON:
    //   if (!released) {
    //     if (octave_up > 0) {
    //       octave_down = 0;
    //       octave_up = 0;
    //       myControlChange(midiChannel, CCoctave_down, octave_down);
    //     } else {
    //       pingPongStep(octave_down, octave_down_upwards);
    //       octave_up = 0;
    //       octave_up_upwards = true;
    //       myControlChange(midiChannel, CCoctave_down, octave_down);
    //     }
    //   }
    //   break;

    // case OCTAVE_UP_BUTTON:
    //   if (!released) {
    //     if (octave_down > 0) {
    //       octave_up = 0;
    //       octave_down = 0;
    //       myControlChange(midiChannel, CCoctave_up, octave_up);
    //     } else {
    //       pingPongStep(octave_up, octave_up_upwards);
    //       octave_down = 0;
    //       octave_down_upwards = true;
    //       myControlChange(midiChannel, CCoctave_up, octave_up);
    //     }
    //   }
    //   break;

      // Keymode Buttons

    case KEY_DUAL_BUTTON:
      if (!released) {
        dual_button = true;
        myControlChange(midiChannel, CCdual_button, dual_button);
      }
      break;

    case KEY_SPLIT_BUTTON:
      if (!released) {
        split_button = true;
        myControlChange(midiChannel, CCsplit_button, split_button);
      }
      break;

    case KEY_SINGLE_BUTTON:
      if (!released) {
        single_button = !single_button;
        myControlChange(midiChannel, CCsingle_button, single_button);
      }
      break;

    case KEY_SPECIAL_BUTTON:
      if (!released) {
        special_button = !special_button;
        myControlChange(midiChannel, CCspecial_button, special_button);
      }
      break;

      // Asssigner Buttons

    case ASSIGN_POLY_BUTTON:
      if (!released) {
        poly_button = !poly_button;
        myControlChange(midiChannel, CCpoly_button, poly_button);
      }
      break;

    case ASSIGN_UNI_BUTTON:
      if (!released) {
        unison_button = !unison_button;
        myControlChange(midiChannel, CCunison_button, unison_button);
      }
      break;

    case ASSIGN_MONO_BUTTON:
      if (!released) {
        mono_button = !mono_button;
        myControlChange(midiChannel, CCmono_button, mono_button);
      }
      break;

    // case LFO1_SYNC_BUTTON:
    //   if (!released) {
    //     lfo1_sync = lfo1_sync + 1;
    //     if (lfo1_sync > 2) {
    //       lfo1_sync = 0;
    //     }
    //     myControlChange(midiChannel, CClfo1_sync, lfo1_sync);
    //   }
    //   break;

    // case LFO2_SYNC_BUTTON:
    //   if (!released) {
    //     lfo2_sync = lfo2_sync + 1;
    //     if (lfo2_sync > 2) {
    //       lfo2_sync = 0;
    //     }
    //     myControlChange(midiChannel, CClfo2_sync, lfo2_sync);
    //   }
    //   break;

    // case DCO1_PWM_DYN_BUTTON:
    //   if (!released) {
    //     dco1_PWM_dyn = dco1_PWM_dyn + 1;
    //     if (dco1_PWM_dyn > 3) {
    //       dco1_PWM_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCdco1_PWM_dyn, dco1_PWM_dyn);
    //   }
    //   break;

    // case DCO2_PWM_DYN_BUTTON:
    //   if (!released) {
    //     dco2_PWM_dyn = dco2_PWM_dyn + 1;
    //     if (dco2_PWM_dyn > 3) {
    //       dco2_PWM_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCdco2_PWM_dyn, dco2_PWM_dyn);
    //   }
    //   break;

    // case DCO1_PWM_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     dco1_PWM_env_source = dco1_PWM_env_source + 2;
    //     if (dco1_PWM_env_source > 7) {
    //       dco1_PWM_env_source = 0;
    //       dco1_PWM_env_pol = 0;
    //     }
    //     myControlChange(midiChannel, CCdco1_PWM_env_source, dco1_PWM_env_source);
    //   }
    //   break;

    // case DCO2_PWM_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     dco2_PWM_env_source = dco2_PWM_env_source + 2;
    //     if (dco2_PWM_env_source > 7) {
    //       dco2_PWM_env_source = 0;
    //       dco2_PWM_env_pol = 0;
    //     }
    //     myControlChange(midiChannel, CCdco2_PWM_env_source, dco2_PWM_env_source);
    //   }
    //   break;

    // case DCO1_PWM_ENV_POLARITY_BUTTON:
    //   if (!released) {
    //     dco1_PWM_env_pol = !dco1_PWM_env_pol;
    //     if (dco1_PWM_env_pol) {
    //       dco1_PWM_env_source++;
    //     }
    //     if (!dco1_PWM_env_pol) {
    //       dco1_PWM_env_source--;
    //     }
    //     myControlChange(midiChannel, CCdco1_PWM_env_source, dco1_PWM_env_source);
    //   }
    //   break;

    // case DCO2_PWM_ENV_POLARITY_BUTTON:
    //   if (!released) {
    //     dco2_PWM_env_pol = !dco2_PWM_env_pol;
    //     if (dco2_PWM_env_pol) {
    //       dco2_PWM_env_source++;
    //     }
    //     if (!dco2_PWM_env_pol) {
    //       dco2_PWM_env_source--;
    //     }
    //     myControlChange(midiChannel, CCdco2_PWM_env_source, dco2_PWM_env_source);
    //   }
    //   break;

    // case DCO1_PWM_LFO_SOURCE_BUTTON:
    //   if (!released) {
    //     dco1_PWM_lfo_source = dco1_PWM_lfo_source + 1;
    //     if (dco1_PWM_lfo_source > 3) {
    //       dco1_PWM_lfo_source = 0;
    //     }
    //     myControlChange(midiChannel, CCdco1_PWM_lfo_source, dco1_PWM_lfo_source);
    //   }
    //   break;

    // case DCO2_PWM_LFO_SOURCE_BUTTON:
    //   if (!released) {
    //     dco2_PWM_lfo_source = dco2_PWM_lfo_source + 1;
    //     if (dco2_PWM_lfo_source > 3) {
    //       dco2_PWM_lfo_source = 0;
    //     }
    //     myControlChange(midiChannel, CCdco2_PWM_lfo_source, dco2_PWM_lfo_source);
    //   }
    //   break;

    // case DCO1_PITCH_DYN_BUTTON:
    //   if (!released) {
    //     dco1_pitch_dyn = dco1_pitch_dyn + 1;
    //     if (dco1_pitch_dyn > 3) {
    //       dco1_pitch_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCdco1_pitch_dyn, dco1_pitch_dyn);
    //   }
    //   break;

    // case DCO2_PITCH_DYN_BUTTON:
    //   if (!released) {
    //     dco2_pitch_dyn = dco2_pitch_dyn + 1;
    //     if (dco2_pitch_dyn > 3) {
    //       dco2_pitch_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCdco2_pitch_dyn, dco2_pitch_dyn);
    //   }
    //   break;

    // case DCO1_PITCH_LFO_SOURCE_BUTTON:
    //   if (!released) {
    //     dco1_pitch_lfo_source = dco1_pitch_lfo_source + 1;
    //     if (dco1_pitch_lfo_source > 3) {
    //       dco1_pitch_lfo_source = 0;
    //     }
    //     myControlChange(midiChannel, CCdco1_pitch_lfo_source, dco1_pitch_lfo_source);
    //   }
    //   break;

    // case DCO2_PITCH_LFO_SOURCE_BUTTON:
    //   if (!released) {
    //     dco2_pitch_lfo_source = dco2_pitch_lfo_source + 1;
    //     if (dco2_pitch_lfo_source > 3) {
    //       dco2_pitch_lfo_source = 0;
    //     }
    //     myControlChange(midiChannel, CCdco2_pitch_lfo_source, dco2_pitch_lfo_source);
    //   }
    //   break;

    // case DCO1_PITCH_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     dco1_pitch_env_source = dco1_pitch_env_source + 2;
    //     if (dco1_pitch_env_source > 7) {
    //       dco1_pitch_env_source = 0;
    //       dco1_pitch_env_pol = 0;
    //     }
    //     myControlChange(midiChannel, CCdco1_pitch_env_source, dco1_pitch_env_source);
    //   }
    //   break;

    // case DCO2_PITCH_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     dco2_pitch_env_source = dco2_pitch_env_source + 2;
    //     if (dco2_pitch_env_source > 7) {
    //       dco2_pitch_env_source = 0;
    //       dco2_pitch_env_pol = 0;
    //     }
    //     myControlChange(midiChannel, CCdco2_pitch_env_source, dco2_pitch_env_source);
    //   }
    //   break;

    // case DCO1_PITCH_ENV_POLARITY_BUTTON:
    //   if (!released) {
    //     dco1_pitch_env_pol = !dco1_pitch_env_pol;
    //     if (dco1_pitch_env_pol) {
    //       dco1_pitch_env_source++;
    //     }
    //     if (!dco1_pitch_env_pol) {
    //       dco1_pitch_env_source--;
    //     }
    //     myControlChange(midiChannel, CCdco1_pitch_env_source, dco1_pitch_env_pol);
    //   }
    //   break;

    // case DCO2_PITCH_ENV_POLARITY_BUTTON:
    //   if (!released) {
    //     dco2_pitch_env_pol = !dco2_pitch_env_pol;
    //     if (dco2_pitch_env_pol) {
    //       dco2_pitch_env_source++;
    //     }
    //     if (!dco2_pitch_env_pol) {
    //       dco2_pitch_env_source--;
    //     }
    //     myControlChange(midiChannel, CCdco2_pitch_env_source, dco2_pitch_env_pol);
    //   }
    //   break;

    case LOWER_BUTTON:
      if (!released) {
        editMode = 0;
        myControlChange(midiChannel, CCeditMode, editMode);
      }
      break;

    case UPPER_BUTTON:
      if (!released) {
        editMode = 1;
        myControlChange(midiChannel, CCeditMode, editMode);
      }
      break;

    // case DCO_MIX_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     dco_mix_env_source = dco_mix_env_source + 2;
    //     if (dco_mix_env_source > 7) {
    //       dco_mix_env_source = 0;
    //       dco_mix_env_pol = 0;
    //     }
    //     myControlChange(midiChannel, CCdco_mix_env_source, dco_mix_env_source);
    //   }
    //   break;

    // case DCO_MIX_ENV_POLARITY_BUTTON:
    //   if (!released) {
    //     dco_mix_env_pol = !dco_mix_env_pol;
    //     if (dco_mix_env_pol) {
    //       dco_mix_env_source++;
    //     }
    //     if (!dco_mix_env_pol) {
    //       dco_mix_env_source--;
    //     }
    //     myControlChange(midiChannel, CCdco_mix_env_source, dco_mix_env_source);
    //   }
    //   break;

    // case DCO_MIX_DYN_BUTTON:
    //   if (!released) {
    //     dco_mix_dyn = dco_mix_dyn + 1;
    //     if (dco_mix_dyn > 3) {
    //       dco_mix_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCdco_mix_dyn, dco_mix_dyn);
    //   }
    //   break;

    // case VCF_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     vcf_env_source = vcf_env_source + 2;
    //     if (vcf_env_source > 7) {
    //       vcf_env_source = 0;
    //       vcf_env_pol = 0;
    //     }
    //     myControlChange(midiChannel, CCvcf_env_source, vcf_env_source);
    //   }
    //   break;

    // case VCF_ENV_POLARITY_BUTTON:
    //   if (!released) {
    //     vcf_env_pol = !vcf_env_pol;
    //     if (vcf_env_pol) {
    //       vcf_env_source++;
    //     }
    //     if (!vcf_env_pol) {
    //       vcf_env_source--;
    //     }
    //     myControlChange(midiChannel, CCvcf_env_source, vcf_env_source);
    //   }
    //   break;

    // case VCF_DYN_BUTTON:
    //   if (!released) {
    //     vcf_dyn = vcf_dyn + 1;
    //     if (vcf_dyn > 3) {
    //       vcf_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCvcf_dyn, vcf_dyn);
    //   }
    //   break;

    // case VCA_ENV_SOURCE_BUTTON:
    //   if (!released) {
    //     vca_env_source = vca_env_source + 1;
    //     if (vca_env_source > 3) {
    //       vca_env_source = 0;
    //     }
    //     myControlChange(midiChannel, CCvca_env_source, vca_env_source);
    //   }
    //   break;

    // case VCA_DYN_BUTTON:
    //   if (!released) {
    //     vca_dyn = vca_dyn + 1;
    //     if (vca_dyn > 3) {
    //       vca_dyn = 0;
    //     }
    //     myControlChange(midiChannel, CCvca_dyn, vca_dyn);
    //   }
    //   break;

    // case CHORUS_BUTTON:
    //   if (!released) {
    //     chorus = chorus + 1;
    //     if (chorus > 2) {
    //       chorus = 0;
    //     }
    //     myControlChange(midiChannel, CCchorus_sw, chorus);
    //   }
    //   break;

    // case PORTAMENTO_BUTTON:
    //   if (!released) {
    //     portamento_sw = portamento_sw + 1;
    //     if (portamento_sw > 3) {
    //       portamento_sw = 0;
    //     }
    //     myControlChange(midiChannel, CCportamento_sw, portamento_sw);
    //   }
    //   break;

    // case ENV5STAGE_SELECT_BUTTON:
    //   if (!released) {
    //     env5stage = !env5stage;
    //     myControlChange(midiChannel, CCenv5stage, env5stage);
    //   }
    //   break;

    // case ADSR_SELECT_BUTTON:
    //   if (!released) {
    //     adsr = !adsr;
    //     myControlChange(midiChannel, CCadsr, adsr);
    //   }
    //   break;
  }
}

bool anyMuxNeedsReread() {
  for (int i = 0; i < MUXCHANNELS; i++) {
    if (mux1ValuesPrev[i] == RE_READ) return true;
    if (mux2ValuesPrev[i] == RE_READ) return true;
    if (mux3ValuesPrev[i] == RE_READ) return true;
    if (mux4ValuesPrev[i] == RE_READ) return true;
  }
  return false;
}

inline bool isRereadSentinel(int v) {
  return (v == RE_READ);
}

void checkMux() {

  if (bootInitInProgress) {
    muxInput++;
    if (muxInput >= MUXCHANNELS) muxInput = 0;
    return;
  }

  digitalWriteFast(MUX_0, muxInput & B0001);
  digitalWriteFast(MUX_1, muxInput & B0010);
  digitalWriteFast(MUX_2, muxInput & B0100);
  digitalWriteFast(MUX_3, muxInput & B1000);
  delayMicroseconds(2);

  mux1Read = adc->adc1->analogRead(MUX1_S);
  mux2Read = adc->adc1->analogRead(MUX2_S);
  mux3Read = adc->adc1->analogRead(MUX3_S);
  mux4Read = adc->adc1->analogRead(MUX4_S);

  bool reread1 = isRereadSentinel(mux1ValuesPrev[muxInput]);

  if (reread1 || mux1Read > (mux1ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux1Read < (mux1ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux1ValuesPrev[muxInput] = mux1Read;
    mux1Read = (mux1Read >> resolutionFrig);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread1) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX1_MOD_LFO:
        myControlChange(midiChannel, CCmod_lfo, mux1Read);
        break;
      case MUX1_LFO1_RATE:
        myControlChange(midiChannel, CClfo1_rate, mux1Read);
        break;
      case MUX1_LFO1_DELAY:
        myControlChange(midiChannel, CClfo1_delay, mux1Read);
        break;
      case MUX1_LFO1_LFO2_MOD:
        myControlChange(midiChannel, CClfo1_lfo2, mux1Read);
        break;
      case MUX1_DCO1_PW:
        myControlChange(midiChannel, CCdco1_PW, mux1Read);
        break;
      case MUX1_DCO1_PWM_ENV:
        myControlChange(midiChannel, CCdco1_PWM_env, mux1Read);
        break;
      case MUX1_DCO1_PWM_LFO:
        myControlChange(midiChannel, CCdco1_PWM_lfo, mux1Read);
        break;
      case MUX1_DCO1_PITCH_ENV:
        myControlChange(midiChannel, CCdco1_pitch_env, mux1Read);
        break;
      case MUX1_DCO1_PITCH_LFO:
        myControlChange(midiChannel, CCdco1_pitch_lfo, mux1Read);
        break;
      case MUX1_DCO1_WAVE:
        myControlChange(midiChannel, CCdco1_wave, mux1Read);
        break;
      case MUX1_DCO1_RANGE:
        myControlChange(midiChannel, CCdco1_range, mux1Read);
        break;
      case MUX1_DCO1_TUNE:
        myControlChange(midiChannel, CCdco1_tune, mux1Read);
        break;
      case MUX1_PORTAMENTO:
        myControlChange(midiChannel, CCportamento, mux1Read);
        break;
      case MUX1_LFO1_WAVE:
        myControlChange(midiChannel, CClfo1_wave, mux1Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  bool reread2 = isRereadSentinel(mux2ValuesPrev[muxInput]);

  if (reread2 || mux2Read > (mux2ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux2Read < (mux2ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux2ValuesPrev[muxInput] = mux2Read;
    mux2Read = (mux2Read >> resolutionFrig);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread2) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX2_BEND_RANGE:
        myControlChange(midiChannel, CCbend_range, mux2Read);
        break;
      case MUX2_LFO2_RATE:
        myControlChange(midiChannel, CClfo2_rate, mux2Read);
        break;
      case MUX2_LFO2_DELAY:
        myControlChange(midiChannel, CClfo2_delay, mux2Read);
        break;
      case MUX2_LFO2_LFO1_MOD:
        myControlChange(midiChannel, CClfo2_lfo1, mux2Read);
        break;
      case MUX2_DCO2_PW:
        myControlChange(midiChannel, CCdco2_PW, mux2Read);
        break;
      case MUX2_DCO2_PWM_ENV:
        myControlChange(midiChannel, CCdco2_PWM_env, mux2Read);
        break;
      case MUX2_DCO2_PWM_LFO:
        myControlChange(midiChannel, CCdco2_PWM_lfo, mux2Read);
        break;
      case MUX2_DCO2_PITCH_ENV:
        myControlChange(midiChannel, CCdco2_pitch_env, mux2Read);
        break;
      case MUX2_DCO2_PITCH_LFO:
        myControlChange(midiChannel, CCdco2_pitch_lfo, mux2Read);
        break;
      case MUX2_DCO2_WAVE:
        myControlChange(midiChannel, CCdco2_wave, mux2Read);
        break;
      case MUX2_DCO2_RANGE:
        myControlChange(midiChannel, CCdco2_range, mux2Read);
        break;
      case MUX2_DCO2_TUNE:
        myControlChange(midiChannel, CCdco2_tune, mux2Read);
        break;
      case MUX2_DCO2_FINE:
        myControlChange(midiChannel, CCdco2_fine, mux2Read);
        break;
      case MUX2_DCO1_MODE:
        myControlChange(midiChannel, CCdco1_mode, mux2Read);
        break;
      case MUX2_LFO2_WAVE:
        myControlChange(midiChannel, CClfo2_wave, mux2Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  bool reread3 = isRereadSentinel(mux3ValuesPrev[muxInput]);

  if (reread3 || mux3Read > (mux3ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux3Read < (mux3ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux3ValuesPrev[muxInput] = mux3Read;
    mux3Read = (mux3Read >> resolutionFrig);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread3) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX3_DCO1_LEVEL:
        myControlChange(midiChannel, CCdco1_level, mux3Read);
        break;
      case MUX3_DCO2_LEVEL:
        myControlChange(midiChannel, CCdco2_level, mux3Read);
        break;
      case MUX3_DCO2_MOD:
        myControlChange(midiChannel, CCdco2_mod, mux3Read);
        break;
      case MUX3_VCF_HPF:
        myControlChange(midiChannel, CCvcf_hpf, mux3Read);
        break;
      case MUX3_VCF_CUTOFF:
        myControlChange(midiChannel, CCvcf_cutoff, mux3Read);
        break;
      case MUX3_VCF_RES:
        myControlChange(midiChannel, CCvcf_res, mux3Read);
        break;
      case MUX3_VCF_KB:
        myControlChange(midiChannel, CCvcf_kb, mux3Read);
        break;
      case MUX3_VCF_ENV:
        myControlChange(midiChannel, CCvcf_env, mux3Read);
        break;
      case MUX3_VCF_LFO1:
        myControlChange(midiChannel, CCvcf_lfo1, mux3Read);
        break;
      case MUX3_VCF_LFO2:
        myControlChange(midiChannel, CCvcf_lfo2, mux3Read);
        break;
      case MUX3_VCA_MOD:
        myControlChange(midiChannel, CCvca_mod, mux3Read);
        break;
      case MUX3_AT_VIB:
        myControlChange(midiChannel, CCat_vib, mux3Read);
        break;
      case MUX3_AT_LPF:
        myControlChange(midiChannel, CCat_lpf, mux3Read);
        break;
      case MUX3_AT_VOL:
        myControlChange(midiChannel, CCat_vol, mux3Read);
        break;
      case MUX3_BALANCE:
        myControlChange(midiChannel, CCbalance, mux3Read);
        break;
      case MUX3_VOLUME:
        myControlChange(midiChannel, CCvolume, mux3Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  bool reread4 = isRereadSentinel(mux4ValuesPrev[muxInput]);

  if (reread4 || mux4Read > (mux4ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux4Read < (mux4ValuesPrev[muxInput] - QUANTISE_FACTOR)) {

    mux4ValuesPrev[muxInput] = mux4Read;
    mux4Read = (mux4Read >> resolutionFrig);

    // During RE_READ pass: do not announce UI
    bool prevSuppress = suppressParamAnnounce;
    if (reread4) suppressParamAnnounce = true;

    switch (muxInput) {
      case MUX4_T1:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime1, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time1, mux4Read);
        }
        break;
      case MUX4_L1:
        if (!env5stage) {
          myControlChange(midiChannel, CClevel1, mux4Read);
        } else {
          myControlChange(midiChannel, CC2level1, mux4Read);
        }
        break;
      case MUX4_T2:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime2, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time2, mux4Read);
        }
        break;
      case MUX4_L2:
        if (!env5stage) {
          myControlChange(midiChannel, CClevel2, mux4Read);
        } else {
          myControlChange(midiChannel, CC2level2, mux4Read);
        }
        break;
      case MUX4_T3:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime3, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time3, mux4Read);
        }
        break;
      case MUX4_L3:
        if (!env5stage) {
          myControlChange(midiChannel, CClevel3, mux4Read);
        } else {
          myControlChange(midiChannel, CC2level3, mux4Read);
        }
        break;
      case MUX4_T4:
        if (!env5stage) {
          myControlChange(midiChannel, CCtime4, mux4Read);
        } else {
          myControlChange(midiChannel, CC2time4, mux4Read);
        }
        break;
      case MUX4_5STAGE_MODE:
        if (!env5stage) {
          myControlChange(midiChannel, CC5stage_mode, mux4Read);
        } else {
          myControlChange(midiChannel, CC25stage_mode, mux4Read);
        }
        break;
      case MUX4_ATTACK:
        if (!adsr) {
          myControlChange(midiChannel, CCattack, mux4Read);
        } else {
          myControlChange(midiChannel, CC4attack, mux4Read);
        }
        break;
      case MUX4_DECAY:
        if (!adsr) {
          myControlChange(midiChannel, CCdecay, mux4Read);
        } else {
          myControlChange(midiChannel, CC4decay, mux4Read);
        }
        break;
      case MUX4_SUSTAIN:
        if (!adsr) {
          myControlChange(midiChannel, CCsustain, mux4Read);
        } else {
          myControlChange(midiChannel, CC4sustain, mux4Read);
        }
        break;
      case MUX4_RELEASE:
        if (!adsr) {
          myControlChange(midiChannel, CCrelease, mux4Read);
        } else {
          myControlChange(midiChannel, CC4release, mux4Read);
        }
        break;
      case MUX4_ADSR_MODE:
        if (!adsr) {
          myControlChange(midiChannel, CCadsr_mode, mux4Read);
        } else {
          myControlChange(midiChannel, CC4adsr_mode, mux4Read);
        }
        break;
      case MUX4_DUAL_DETUNE:
        myControlChange(midiChannel, CCdualdetune, mux4Read);
        break;
      case MUX4_UNISON_DETUNE:
        myControlChange(midiChannel, CCunisondetune, mux4Read);
        break;
    }
    suppressParamAnnounce = prevSuppress;
  }

  muxInput++;
  if (muxInput >= MUXCHANNELS) {
    muxInput = 0;
  }

  if (manualSyncInProgress && !anyMuxNeedsReread()) {
    manualSyncInProgress = false;
    suppressParamAnnounce = false;

    // Optional: one clean UI update at end
    //showPatchPage("--", "Manual", "--", "Manual");
    //startParameterDisplay();
  }
}

void loop() {

  myusb.Task();
  midi1.read();
  usbMIDI.read(midiChannel);
  MIDI.read(midiChannel);
  checkMux();
  checkEncoder();
  checkSwitches();
  pollAllMCPs();


  if (waitingToUpdate && (millis() - lastDisplayTriggerTime >= displayTimeout)) {
    updateScreen();  // retrigger
    waitingToUpdate = false;
  }
}