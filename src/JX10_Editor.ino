#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>
#include <MIDI.h>
#include "MidiCC.h"
#include "Constants.h"
#include "Parameters.h"
#include "PatchMgr.h"
#include "Button.h"
#include "HWControls.h"
#include "EepromMgr.h"

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

MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

#include "Settings.h"

/* ============================================================
   SETUP / RUNTIME
   ============================================================ */

void pollAllMCPs();

void initButtons();

int getEncoderSpeed(int id);


void setup() {
  Serial.begin(115200);

  SPI.begin();
  SPISettings settings(8000000, MSBFIRST, SPI_MODE0);  // 8 MHz to start

  setupDisplay();
  setUpSettings();
  setupHardware();

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

  Wire.begin();
  Wire.setClock(400000);  // Slow down I2C to 100kHz
  delay(10);

  mcp1.begin(0);
  delay(10);
  mcp2.begin(1);
  delay(10);
  mcp3.begin(2);
  delay(10);
  mcp4.begin(3);
  delay(10);
  mcp5.begin(4);
  delay(10);
  mcp6.begin(5);
  delay(10);

  //groupEncoders();
  //initRotaryEncoders();
  initButtons();

  setupMCPoutputs();

  pitchDirty = true;

  // USB MIDI
  usbMIDI.setHandleNoteOn(myNoteOn);
  usbMIDI.setHandleNoteOff(myNoteOff);
  usbMIDI.setHandlePitchChange(myPitchBend);
  usbMIDI.setHandleControlChange(myControlConvert);
  usbMIDI.setHandleProgramChange(myProgramChange);
  usbMIDI.setHandleAfterTouchChannel(myAfterTouch);

  MIDI.begin(MIDI_CHANNEL_OMNI);
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

  //Read MIDI In Channel from EEPROM
  midiChannel = getMIDIChannel();

  //Read MIDI Out Channel from EEPROM
  midiOutCh = getMIDIOutCh();

  recallPatch(patchNo);
}

void startParameterDisplay() {
  updateScreen();

  lastDisplayTriggerTime = millis();
  waitingToUpdate = true;
}

void myAfterTouch(byte channel, byte value) {
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
}

void myControlConvert(byte channel, byte control, byte value) {
  myControlChange(channel, control, value);
}

void myControlChange(byte channel, byte control, int value) {
  switch (control) {

      // case CCmodWheelinput:
      //   break;

    case CClfo1_wave:
      lfo1_wave = value;
      updatelfo1_wave(1);
      break;

    case CClfo1_rate:
      lfo1_rate = value;
      updatelfo1_rate(1);
      break;

    case CClfo1_delay:
      lfo1_delay = value;
      updatelfo1_delay(1);
      break;

    case CClfo1_lfo2:
      lfo1_lfo2 = value;
      updatelfo1_lfo2(1);
      break;

    case CCdco1_PW:
      dco1_PW = value;
      updatedco1_PW(1);
      break;

    case CCdco1_PWM_env:
      dco1_PWM_env = value;
      updatedco1_PWM_env(1);
      break;

    case CCdco1_PWM_lfo:
      dco1_PWM_lfo = value;
      updatedco1_PWM_lfo(1);
      break;

    case CCdco1_pitch_env:
      dco1_pitch_env = value;
      updatedco1_pitch_env(1);
      break;

    case CCdco1_pitch_lfo:
      dco1_pitch_lfo = value;
      updatedco1_pitch_lfo(1);
      break;

    case CCdco1_wave:
      dco1_wave = value;
      updatedco1_wave(1);
      break;

    case CCdco1_range:
      dco1_range = value;
      updatedco1_range(1);
      break;

    case CCdco1_tune:
      dco1_tune = value;
      updatedco1_tune(1);
      break;

    case CCdco1_mode:
      dco1_mode = value;
      updatedco1_mode(1);
      break;

    case CClfo2_wave:
      lfo2_wave = value;
      updatelfo2_wave(1);
      break;

    case CClfo2_rate:
      lfo2_rate = value;
      updatelfo2_rate(1);
      break;

    case CClfo2_delay:
      lfo2_delay = value;
      updatelfo2_delay(1);
      break;

    case CClfo2_lfo1:
      lfo2_lfo1 = value;
      updatelfo2_lfo1(1);
      break;

    case CCdco2_PW:
      dco2_PW = value;
      updatedco2_PW(1);
      break;

    case CCdco2_PWM_env:
      dco2_PWM_env = value;
      updatedco2_PWM_env(1);
      break;

    case CCdco2_PWM_lfo:
      dco2_PWM_lfo = value;
      updatedco2_PWM_lfo(1);
      break;

    case CCdco2_pitch_env:
      dco2_pitch_env = value;
      updatedco2_pitch_env(1);
      break;

    case CCdco2_pitch_lfo:
      dco2_pitch_lfo = value;
      updatedco2_pitch_lfo(1);
      break;

    case CCdco2_wave:
      dco2_wave = value;
      updatedco2_wave(1);
      break;

    case CCdco2_range:
      dco2_range = value;
      updatedco2_range(1);
      break;

    case CCdco2_tune:
      dco2_tune = value;
      updatedco2_tune(1);
      break;

    case CCdco2_fine:
      dco2_fine = value;
      updatedco2_fine(1);
      break;

    case CCdco1_level:
      dco1_level = value;
      updatedco1_level(1);
      break;

    case CCdco2_level:
      dco2_level = value;
      updatedco2_level(1);
      break;

    case CCdco2_mod:
      dco2_mod = value;
      updatedco2_mod(1);
      break;

    case CCvcf_hpf:
      vcf_hpf = value;
      updatevcf_hpf(1);
      break;

    case CCvcf_cutoff:
      vcf_cutoff = value;
      updatevcf_cutoff(1);
      break;

    case CCvcf_res:
      vcf_res = value;
      updatevcf_res(1);
      break;

    case CCvcf_kb:
      vcf_kb = value;
      updatevcf_kb(1);
      break;

    case CCvcf_env:
      vcf_env = value;
      updatevcf_env(1);
      break;

    case CCvcf_lfo1:
      vcf_lfo1 = value;
      updatevcf_lfo1(1);
      break;

    case CCvcf_lfo2:
      vcf_lfo2 = value;
      updatevcf_lfo2(1);
      break;

    case CCvca_mod:
      vca_mod = value;
      updatevca_mod(1);
      break;

    case CCat_vib:
      at_vib = value;
      updateat_vib(1);
      break;

    case CCat_lpf:
      at_lpf = value;
      updateat_lpf(1);
      break;

    case CCat_vol:
      at_vol = value;
      updateat_vol(1);
      break;

    case CCbalance:
      balance = value;
      updatebalance(1);
      break;

    case CCtime1:
      time1 = value;
      updatetime1(1);
      break;

    case CClevel1:
      level1 = value;
      updatelevel1(1);
      break;

    case CCtime2:
      time2 = value;
      updatetime2(1);
      break;

    case CClevel2:
      level2 = value;
      updatelevel2(1);
      break;

    case CCtime3:
      time3 = value;
      updatetime3(1);
      break;

    case CClevel3:
      level3 = value;
      updatelevel3(1);
      break;

    case CCtime4:
      time4 = value;
      updatetime4(1);
      break;

    case CC5stage_mode:
      env5stage_mode = value;
      updateenv5stage_mode(1);
      break;

    case CC2time1:
      env2_time1 = value;
      updateenv2_time1(1);
      break;

    case CC2level1:
      env2_level1 = value;
      updateenv2_level1(1);
      break;

    case CC2time2:
      env2_time2 = value;
      updateenv2_time2(1);
      break;

    case CC2level2:
      env2_level2 = value;
      updateenv2_level2(1);
      break;

    case CC2time3:
      env2_time3 = value;
      updateenv2_time3(1);
      break;

    case CC2level3:
      env2_level3 = value;
      updateenv2_level3(1);
      break;

    case CC2time4:
      env2_time4 = value;
      updateenv2_time4(1);
      break;

    case CC25stage_mode:
      env2_5stage_mode = value;
      updateenv2_env5stage_mode(1);
      break;

    case CCattack:
      attack = value;
      updateattack(1);
      break;

    case CC4attack:
      env4_attack = value;
      updateenv4_attack(1);
      break;

    case CCdecay:
      decay = value;
      updatedecay(1);
      break;

    case CC4decay:
      env4_decay = value;
      updateenv4_decay(1);
      break;

    case CCsustain:
      sustain = value;
      updatesustain(1);
      break;

    case CC4sustain:
      env4_sustain = value;
      updateenv4_sustain(1);
      break;

    case CCrelease:
      release = value;
      updaterelease(1);
      break;

    case CC4release:
      env4_release = value;
      updateenv4_release(1);
      break;

    case CCadsr_mode:
      adsr_mode = value;
      updateadsr_mode(1);
      break;

    case CC4adsr_mode:
      env4_adsr_mode = value;
      updateenv4_adsr_mode(1);
      break;

    case CCctla:
      ctla = value;
      updatectla(1);
      break;

    case CCctlb:
      ctlb = value;
      updatectlb(1);
      break;

      // Buttons

    case CClfo1_sync:
      updatelfo1_sync(1);
      break;

    case CClfo2_sync:
      updatelfo2_sync(1);
      break;

    case CCdco1_PWM_dyn:
      updatedco1_PWM_dyn(1);
      break;

    case CCdco2_PWM_dyn:
      updatedco2_PWM_dyn(1);
      break;

    case CCdco1_PWM_env_source:
      updatedco1_PWM_env_source(1);
      break;

    case CCdco2_PWM_env_source:
      updatedco2_PWM_env_source(1);
      break;

    case CCdco1_PWM_lfo_source:
      updatedco1_PWM_lfo_source(1);
      break;

    case CCdco2_PWM_lfo_source:
      updatedco2_PWM_lfo_source(1);
      break;

    case CCdco1_pitch_dyn:
      updatedco1_pitch_dyn(1);
      break;

    case CCdco2_pitch_dyn:
      updatedco2_pitch_dyn(1);
      break;

    case CCdco1_pitch_lfo_source:
      updatedco1_pitch_lfo_source(1);
      break;

    case CCdco2_pitch_lfo_source:
      updatedco2_pitch_lfo_source(1);
      break;

    case CCdco1_pitch_env_source:
      updatedco1_pitch_env_source(1);
      break;

    case CCdco2_pitch_env_source:
      updatedco2_pitch_env_source(1);
      break;

    case CCplaymode:
      updateplaymode(1);
      break;

    case CCdco_mix_env_source:
      updatedco_mix_env_source(1);
      break;

    case CCdco_mix_dyn:
      updatedco_mix_dyn(1);
      break;

    case CCvcf_env_source:
      updatevcf_env_source(1);
      break;

    case CCvcf_dyn:
      updatevcf_dyn(1);
      break;

    case CCvca_env_source:
      updatevca_env_source(1);
      break;

    case CCvca_dyn:
      updatevca_dyn(1);
      break;

    case CCchorus_sw:
      updatechorus(1);
      break;

    case CCenv5stage:
      updateenv5stage(1);
      break;

    case CCadsr:
      updateadsr(1);
      break;
  }
}

FLASHMEM void updatelfo1_wave(bool announce) {
  lfo1_wave_str = map(lfo1_wave, 0, 127, 0, 4);
  if (announce) {
    switch (lfo1_wave_str) {
      case 0:
        showCurrentParameterPage("LFO1 Wave", "Random");
        break;

      case 1:
        showCurrentParameterPage("LFO1 Wave", "Square");
        break;

      case 2:
        showCurrentParameterPage("LFO1 Wave", "Saw Neg");
        break;

      case 3:
        showCurrentParameterPage("LFO1 Wave", "Saw Pos");
        break;

      case 4:
        showCurrentParameterPage("LFO1 Wave", "Sinewave");
        break;
    }
    startParameterDisplay();
  }
  switch (lfo1_wave_str) {
    case 0:
      midiCCOut(CClfo1_wave, 0x00);
      break;

    case 1:
      midiCCOut(CClfo1_wave, 0x10);
      break;

    case 2:
      midiCCOut(CClfo1_wave, 0x20);
      break;

    case 3:
      midiCCOut(CClfo1_wave, 0x30);
      break;

    case 4:
      midiCCOut(CClfo1_wave, 0x40);
      break;
  }
}

FLASHMEM void updatelfo1_rate(bool announce) {
  lfo1_rate_str = map(lfo1_rate, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("LFO1 Rate", String(lfo1_rate_str));
    startParameterDisplay();
  }
  midiCCOut(CClfo1_rate, lfo1_rate);
}

FLASHMEM void updatelfo1_delay(bool announce) {
  lfo1_delay_str = map(lfo1_delay, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("LFO1 Delay", String(lfo1_delay_str));
    startParameterDisplay();
  }
  midiCCOut(CClfo1_delay, lfo1_delay);
}

FLASHMEM void updatelfo1_lfo2(bool announce) {
  lfo1_lfo2_str = map(lfo1_lfo2, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("LFO1 to LFO2", String(lfo1_lfo2_str));
    startParameterDisplay();
  }
  midiCCOut(CClfo1_lfo2, lfo1_lfo2);
}

FLASHMEM void updatedco1_PW(bool announce) {
  dco1_PW_str = map(dco1_PW, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("DCO1 PW", String(dco1_PW_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_PW, dco1_PW);
}

FLASHMEM void updatedco1_PWM_env(bool announce) {
  dco1_PWM_env_str = map(dco1_PWM_env, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("PWM1 Env", String(dco1_PWM_env_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_PWM_env, dco1_PWM_env);
}

FLASHMEM void updatedco1_PWM_lfo(bool announce) {
  dco1_PWM_lfo_str = map(dco1_PWM_lfo, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("PWM1 LFO", String(dco1_PWM_lfo));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_PWM_lfo, dco1_PWM_lfo);
}

FLASHMEM void updatedco1_pitch_env(bool announce) {
  dco1_pitch_env_str = map(dco1_pitch_env, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("DCO1 Env", String(dco1_pitch_env_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_pitch_env, dco1_pitch_env);
}

FLASHMEM void updatedco1_pitch_lfo(bool announce) {
  dco1_pitch_lfo_str = map(dco1_pitch_lfo, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("DCO1 LFO", String(dco1_pitch_lfo));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_pitch_lfo, dco1_pitch_lfo);
}

FLASHMEM void updatedco1_wave(bool announce) {
  dco1_wave_str = map(dco1_wave, 0, 127, 0, 3);
  if (announce) {
    switch (dco1_wave_str) {
      case 0:
        showCurrentParameterPage("DCO1 Wave", "Noise");
        break;

      case 1:
        showCurrentParameterPage("DCO1 Wave", "Square");
        break;

      case 2:
        showCurrentParameterPage("DCO1 Wave", "PulseWidth");
        break;

      case 3:
        showCurrentParameterPage("DCO1 Wave", "Sawtooth");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_wave_str) {
    case 0:
      midiCCOut(CCdco1_wave, 0x00);
      break;

    case 1:
      midiCCOut(CCdco1_wave, 0x20);
      break;

    case 2:
      midiCCOut(CCdco1_wave, 0x40);
      break;

    case 3:
      midiCCOut(CCdco1_wave, 0x60);
      break;
  }
}

FLASHMEM void updatedco1_range(bool announce) {
  dco1_range_str = map(dco1_range, 0, 127, 0, 3);
  if (announce) {
    switch (dco1_range_str) {
      case 0:
        showCurrentParameterPage("DCO1 Range", "16 Foot");
        break;

      case 1:
        showCurrentParameterPage("DCO1 Range", "8 Foot");
        break;

      case 2:
        showCurrentParameterPage("DCO1 Range", "4 Foot");
        break;

      case 3:
        showCurrentParameterPage("DCO1 Range", "2 Foot");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_range_str) {
    case 0:
      midiCCOut(CCdco1_range, 0x00);
      break;

    case 1:
      midiCCOut(CCdco1_range, 0x20);
      break;

    case 2:
      midiCCOut(CCdco1_range, 0x40);
      break;

    case 3:
      midiCCOut(CCdco1_range, 0x60);
      break;
  }
}

FLASHMEM void updatedco1_tune(bool announce) {
  dco1_tune_str = map(dco1_tune, 0, 127, -12, 12);
  if (announce) {
    showCurrentParameterPage("DCO1 Tuning", String(dco1_tune_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_tune, dco1_tune);
}

FLASHMEM void updatedco1_mode(bool announce) {
  dco1_mode_str = map(dco1_mode, 0, 127, 0, 3);
  if (announce) {
    switch (dco1_mode_str) {
      case 0:
        showCurrentParameterPage("DCO XMOD", "Off");
        break;

      case 1:
        showCurrentParameterPage("DCO XMOD", "Sync 1");
        break;

      case 2:
        showCurrentParameterPage("DCO XMOD", "Sync 2");
        break;

      case 3:
        showCurrentParameterPage("DCO XMOD", "X Mod");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_mode_str) {
    case 0:
      midiCCOut(CCdco1_mode, 0x00);
      break;

    case 1:
      midiCCOut(CCdco1_mode, 0x20);
      break;

    case 2:
      midiCCOut(CCdco1_mode, 0x40);
      break;

    case 3:
      midiCCOut(CCdco1_mode, 0x60);
      break;
  }
}

FLASHMEM void updatelfo2_wave(bool announce) {
  lfo2_wave_str = map(lfo2_wave, 0, 127, 0, 4);
  if (announce) {
    switch (lfo2_wave_str) {
      case 0:
        showCurrentParameterPage("LFO2 Wave", "Random");
        break;

      case 1:
        showCurrentParameterPage("LFO2 Wave", "Square");
        break;

      case 2:
        showCurrentParameterPage("LFO2 Wave", "Saw Neg");
        break;

      case 3:
        showCurrentParameterPage("LFO2 Wave", "Saw Pos");
        break;

      case 4:
        showCurrentParameterPage("LFO2 Wave", "Sinewave");
        break;
    }
    startParameterDisplay();
  }
  switch (lfo2_wave_str) {
    case 0:
      midiCCOut(CClfo2_wave, 0x00);
      break;

    case 1:
      midiCCOut(CClfo2_wave, 0x10);
      break;

    case 2:
      midiCCOut(CClfo2_wave, 0x20);
      break;

    case 3:
      midiCCOut(CClfo2_wave, 0x30);
      break;

    case 4:
      midiCCOut(CClfo2_wave, 0x40);
      break;
  }
}

FLASHMEM void updatelfo2_rate(bool announce) {
  lfo2_rate_str = map(lfo2_rate, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("LFO2 Rate", String(lfo2_rate_str));
    startParameterDisplay();
  }
  midiCCOut(CClfo2_rate, lfo2_rate);
}

FLASHMEM void updatelfo2_delay(bool announce) {
  lfo2_delay_str = map(lfo2_delay, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("LFO2 Delay", String(lfo2_delay_str));
    startParameterDisplay();
  }
  midiCCOut(CClfo2_delay, lfo2_delay);
}

FLASHMEM void updatelfo2_lfo1(bool announce) {
  lfo2_lfo1_str = map(lfo2_lfo1, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("LFO2 to LFO1", String(lfo2_lfo1_str));
    startParameterDisplay();
  }
  midiCCOut(CClfo2_lfo1, lfo2_lfo1);
}

FLASHMEM void updatedco2_PW(bool announce) {
  dco2_PW_str = map(dco2_PW, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("DCO2 PW", String(dco2_PW_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_PW, dco2_PW);
}

FLASHMEM void updatedco2_PWM_env(bool announce) {
  dco2_PWM_env_str = map(dco2_PWM_env, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("PWM2 Env", String(dco2_PWM_env_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_PWM_env, dco2_PWM_env);
}

FLASHMEM void updatedco2_PWM_lfo(bool announce) {
  dco2_PWM_lfo_str = map(dco2_PWM_lfo, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("PWM2 LFO", String(dco2_PWM_lfo_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_PWM_lfo, dco2_PWM_lfo);
}

FLASHMEM void updatedco2_pitch_env(bool announce) {
  dco2_pitch_env_str = map(dco2_pitch_env, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("DCO2 Env", String(dco2_pitch_env_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_pitch_env, dco2_pitch_env);
}

FLASHMEM void updatedco2_pitch_lfo(bool announce) {
  dco2_pitch_lfo_str = map(dco2_pitch_lfo, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("DCO2 LFO", String(dco2_pitch_lfo_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_pitch_lfo, dco2_pitch_lfo);
}

FLASHMEM void updatedco2_wave(bool announce) {
  dco2_wave_str = map(dco2_wave, 0, 127, 0, 3);
  if (announce) {
    switch (dco2_wave_str) {
      case 0:
        showCurrentParameterPage("DCO2 Wave", "Noise");
        break;

      case 1:
        showCurrentParameterPage("DCO2 Wave", "Square");
        break;

      case 2:
        showCurrentParameterPage("DCO2 Wave", "PulseWidth");
        break;

      case 3:
        showCurrentParameterPage("DCO2 Wave", "Sawtooth");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_wave_str) {
    case 0:
      midiCCOut(CCdco2_wave, 0x00);
      break;

    case 1:
      midiCCOut(CCdco2_wave, 0x20);
      break;

    case 2:
      midiCCOut(CCdco2_wave, 0x40);
      break;

    case 3:
      midiCCOut(CCdco2_wave, 0x60);
      break;
  }
}

FLASHMEM void updatedco2_range(bool announce) {
  dco2_range_str = map(dco2_range, 0, 127, 0, 3);
  if (announce) {
    switch (dco2_range_str) {
      case 0:
        showCurrentParameterPage("DCO2 Range", "16 Foot");
        break;

      case 1:
        showCurrentParameterPage("DCO2 Range", "8 Foot");
        break;

      case 2:
        showCurrentParameterPage("DCO2 Range", "4 Foot");
        break;

      case 3:
        showCurrentParameterPage("DCO2 Range", "2 Foot");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_range_str) {
    case 0:
      midiCCOut(CCdco2_range, 0x00);
      break;

    case 1:
      midiCCOut(CCdco2_range, 0x20);
      break;

    case 2:
      midiCCOut(CCdco2_range, 0x40);
      break;

    case 3:
      midiCCOut(CCdco2_range, 0x60);
      break;
  }
}

FLASHMEM void updatedco2_tune(bool announce) {
  dco2_tune_str = map(dco2_tune, 0, 127, -12, 12);
  if (announce) {
    showCurrentParameterPage("DCO2 Tuning", String(dco2_tune_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_tune, dco2_tune);
}

FLASHMEM void updatedco2_fine(bool announce) {
  dco2_fine_str = map(dco2_fine, 0, 127, -50, 50);
  if (announce) {
    showCurrentParameterPage("DCO2 Fine", String(dco2_fine_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_fine, dco2_fine);
}

FLASHMEM void updatedco1_level(bool announce) {
  dco1_level_str = map(dco1_level, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("MIX DCO1", String(dco1_level_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_level, dco1_level);
}

FLASHMEM void updatedco2_level(bool announce) {
  dco2_level_str = map(dco2_level, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("MIX DCO2", String(dco2_level_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_level, dco2_level);
}

FLASHMEM void updatedco2_mod(bool announce) {
  dco2_mod_str = map(dco2_mod, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("MIX ENV", String(dco2_mod_str));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_mod, dco2_mod);
}

FLASHMEM void updatevcf_hpf(bool announce) {
  vcf_hpf_str = map(vcf_hpf, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF HPF", String(vcf_hpf_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_hpf, vcf_hpf);
}

FLASHMEM void updatevcf_cutoff(bool announce) {
  vcf_cutoff_str = map(vcf_cutoff, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF FREQ", String(vcf_cutoff_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_cutoff, vcf_cutoff);
}

FLASHMEM void updatevcf_res(bool announce) {
  vcf_res_str = map(vcf_res, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF RES", String(vcf_res_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_res, vcf_res);
}

FLASHMEM void updatevcf_kb(bool announce) {
  vcf_kb_str = map(vcf_kb, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF KEY", String(vcf_kb_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_kb, vcf_kb);
}

FLASHMEM void updatevcf_env(bool announce) {
  vcf_env_str = map(vcf_env, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF Env", String(vcf_env_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_env, vcf_env);
}

FLASHMEM void updatevcf_lfo1(bool announce) {
  vcf_lfo1_str = map(vcf_lfo1, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF LFO1", String(vcf_lfo1_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_lfo1, vcf_lfo1);
}

FLASHMEM void updatevcf_lfo2(bool announce) {
  vcf_lfo2_str = map(vcf_lfo2, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCF LFO2", String(vcf_lfo2_str));
    startParameterDisplay();
  }
  midiCCOut(CCvcf_lfo2, vcf_lfo2);
}

FLASHMEM void updatevca_mod(bool announce) {
  vca_mod_str = map(vca_mod, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("VCA LEVEL", String(vca_mod_str));
    startParameterDisplay();
  }
  midiCCOut(CCvca_mod, vca_mod);
}

FLASHMEM void updateat_vib(bool announce) {
  if (announce) {
    showCurrentParameterPage("AT Vibrato", String(at_vib));
    startParameterDisplay();
  }
  midiCCOut(CCat_vib, at_vib);
}

FLASHMEM void updateat_lpf(bool announce) {
  if (announce) {
    showCurrentParameterPage("AT Filter", String(at_lpf));
    startParameterDisplay();
  }
  midiCCOut(CCat_lpf, at_lpf);
}

FLASHMEM void updateat_vol(bool announce) {
  if (announce) {
    showCurrentParameterPage("AT Volume", String(at_vol));
    startParameterDisplay();
  }
  midiCCOut(CCat_vol, at_vol);
}

FLASHMEM void updatebalance(bool announce) {
  if (announce) {
    showCurrentParameterPage("Balance", String(balance));
    startParameterDisplay();
  }
  midiCCOut(CCbalance, balance);
}

FLASHMEM void updatetime1(bool announce) {
  time1_str = map(time1, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 T1", String(time1_str));
    startParameterDisplay();
  }
  midiCCOut(CCtime1, time1);
}

FLASHMEM void updatelevel1(bool announce) {
  level1_str = map(level1, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 L1", String(level1_str));
    startParameterDisplay();
  }
  midiCCOut(CClevel1, level1);
}

FLASHMEM void updatetime2(bool announce) {
  time2_str = map(time2, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 T2", String(time2_str));
    startParameterDisplay();
  }
  midiCCOut(CCtime2, time2);
}

FLASHMEM void updatelevel2(bool announce) {
  level2_str = map(level2, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 L2", String(level2_str));
    startParameterDisplay();
  }
  midiCCOut(CClevel2, level2);
}

FLASHMEM void updatetime3(bool announce) {
  time3_str = map(time3, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 T3", String(time3_str));
    startParameterDisplay();
  }
  midiCCOut(CCtime3, time3);
}

FLASHMEM void updatelevel3(bool announce) {
  level3_str = map(level3, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 L3", String(level3_str));
    startParameterDisplay();
  }
  midiCCOut(CClevel3, level3);
}

FLASHMEM void updatetime4(bool announce) {
  time4_str = map(time4, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env1 T4", String(time4_str));
    startParameterDisplay();
  }
  midiCCOut(CCtime4, time4);
}

FLASHMEM void updateenv5stage_mode(bool announce) {
  env5stage_mode_str = map(env5stage_mode, 0, 127, 0, 7);
  if (announce) {
    switch (env5stage_mode_str) {
      case 0:
        showCurrentParameterPage("Env1 Key F.", "Off");
        break;

      case 1:
        showCurrentParameterPage("Env1 Key F.", "Key 1");
        break;

      case 2:
        showCurrentParameterPage("Env1 Key F.", "Key 2");
        break;

      case 3:
        showCurrentParameterPage("Env1 Key F.", "Key 3");
        break;

      case 4:
        showCurrentParameterPage("Env1 Key F.", "Loop 0");
        break;

      case 5:
        showCurrentParameterPage("Env1 Key F.", "Loop 1");
        break;

      case 6:
        showCurrentParameterPage("Env1 Key F.", "Loop 2");
        break;

      case 7:
        showCurrentParameterPage("Env1 Key F.", "Loop 3");
        break;
    }
    startParameterDisplay();
  }
  switch (env5stage_mode_str) {
    case 0:
      midiCCOut(CC5stage_mode, 0x00);
      break;

    case 1:
      midiCCOut(CC5stage_mode, 0x10);
      break;

    case 2:
      midiCCOut(CC5stage_mode, 0x20);
      break;

    case 3:
      midiCCOut(CC5stage_mode, 0x30);
      break;

    case 4:
      midiCCOut(CC5stage_mode, 0x40);
      break;

    case 5:
      midiCCOut(CC5stage_mode, 0x50);
      break;

    case 6:
      midiCCOut(CC5stage_mode, 0x60);
      break;

    case 7:
      midiCCOut(CC5stage_mode, 0x70);
      break;
  }
}

FLASHMEM void updateenv2_time1(bool announce) {
  env2_time1_str = map(env2_time1, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 T1", String(env2_time1_str));
    startParameterDisplay();
  }
  midiCCOut(CC2time1, env2_time1);
}

FLASHMEM void updateenv2_level1(bool announce) {
  env2_level1_str = map(env2_level1, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 L1", String(env2_level1_str));
    startParameterDisplay();
  }
  midiCCOut(CC2level1, env2_level1);
}

FLASHMEM void updateenv2_time2(bool announce) {
  env2_time2_str = map(env2_time2, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 T2", String(env2_time2_str));
    startParameterDisplay();
  }
  midiCCOut(CC2time2, env2_time2);
}

FLASHMEM void updateenv2_level2(bool announce) {
  env2_level2_str = map(env2_level2, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 L2", String(env2_level2_str));
    startParameterDisplay();
  }
  midiCCOut(CC2level2, env2_level2);
}

FLASHMEM void updateenv2_time3(bool announce) {
  env2_time3_str = map(env2_time3, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 T3", String(env2_time3_str));
    startParameterDisplay();
  }
  midiCCOut(CC2time3, env2_time3);
}

FLASHMEM void updateenv2_level3(bool announce) {
  env2_level3_str = map(env2_level3, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 L3", String(env2_level3_str));
    startParameterDisplay();
  }
  midiCCOut(CC2level3, env2_level3);
}

FLASHMEM void updateenv2_time4(bool announce) {
  env2_time4_str = map(env2_time4, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env2 T4", String(env2_time4_str));
    startParameterDisplay();
  }
  midiCCOut(CC2time4, env2_time4);
}

FLASHMEM void updateenv2_env5stage_mode(bool announce) {
  env2_5stage_mode_str = map(env2_5stage_mode, 0, 127, 0, 7);
  if (announce) {
    switch (env2_5stage_mode_str) {
      case 0:
        showCurrentParameterPage("Env2 Key F.", "Off");
        break;

      case 1:
        showCurrentParameterPage("Env2 Key F.", "Key 1");
        break;

      case 2:
        showCurrentParameterPage("Env2 Key F.", "Key 2");
        break;

      case 3:
        showCurrentParameterPage("Env2 Key F.", "Key 3");
        break;

      case 4:
        showCurrentParameterPage("Env2 Key F.", "Loop 0");
        break;

      case 5:
        showCurrentParameterPage("Env2 Key F.", "Loop 1");
        break;

      case 6:
        showCurrentParameterPage("Env2 Key F.", "Loop 2");
        break;

      case 7:
        showCurrentParameterPage("Env2 Key F.", "Loop 3");
        break;
    }
    startParameterDisplay();
  }
  switch (env2_5stage_mode_str) {
    case 0:
      midiCCOut(CC25stage_mode, 0x00);
      break;

    case 1:
      midiCCOut(CC25stage_mode, 0x10);
      break;

    case 2:
      midiCCOut(CC25stage_mode, 0x20);
      break;

    case 3:
      midiCCOut(CC25stage_mode, 0x30);
      break;

    case 4:
      midiCCOut(CC25stage_mode, 0x40);
      break;

    case 5:
      midiCCOut(CC25stage_mode, 0x50);
      break;

    case 6:
      midiCCOut(CC25stage_mode, 0x60);
      break;

    case 7:
      midiCCOut(CC25stage_mode, 0x70);
      break;
  }
}

FLASHMEM void updateattack(bool announce) {
  attack_str = map(attack, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env3 Attack", String(attack_str));
    startParameterDisplay();
  }
  midiCCOut(CCattack, attack);
}

FLASHMEM void updatedecay(bool announce) {
  decay_str = map(decay, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env3 Decay", String(decay_str));
    startParameterDisplay();
  }
  midiCCOut(CCdecay, decay);
}

FLASHMEM void updatesustain(bool announce) {
  sustain_str = map(sustain, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env3 Sustain", String(sustain_str));
    startParameterDisplay();
  }
  midiCCOut(CCsustain, sustain);
}

FLASHMEM void updaterelease(bool announce) {
  release_str = map(release, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env3 Release", String(release_str));
    startParameterDisplay();
  }
  midiCCOut(CCrelease, release);
}

FLASHMEM void updateadsr_mode(bool announce) {
  adsr_mode_str = map(adsr_mode, 0, 127, 0, 7);
  if (announce) {
    switch (adsr_mode_str) {
      case 0:
        showCurrentParameterPage("Env3 Key F.", "Off");
        break;

      case 1:
        showCurrentParameterPage("Env3 Key F.", "Key 1");
        break;

      case 2:
        showCurrentParameterPage("Env3 Key F.", "Key 2");
        break;

      case 3:
        showCurrentParameterPage("Env3 Key F.", "Key 3");
        break;

      case 4:
        showCurrentParameterPage("Env3 Key F.", "Loop 0");
        break;

      case 5:
        showCurrentParameterPage("Env3 Key F.", "Loop 1");
        break;

      case 6:
        showCurrentParameterPage("Env3 Key F.", "Loop 2");
        break;

      case 7:
        showCurrentParameterPage("Env3 Key F.", "Loop 3");
        break;
    }
    startParameterDisplay();
  }
  switch (adsr_mode_str) {
    case 0:
      midiCCOut(CCadsr_mode, 0x00);
      break;

    case 1:
      midiCCOut(CCadsr_mode, 0x10);
      break;

    case 2:
      midiCCOut(CCadsr_mode, 0x20);
      break;

    case 3:
      midiCCOut(CCadsr_mode, 0x30);
      break;

    case 4:
      midiCCOut(CCadsr_mode, 0x40);
      break;

    case 5:
      midiCCOut(CCadsr_mode, 0x50);
      break;

    case 6:
      midiCCOut(CCadsr_mode, 0x60);
      break;

    case 7:
      midiCCOut(CCadsr_mode, 0x70);
      break;
  }
}

FLASHMEM void updateenv4_attack(bool announce) {
  env4_attack_str = map(env4_attack, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env4 Attack", String(env4_attack_str));
    startParameterDisplay();
  }
  midiCCOut(CC4attack, env4_attack);
}

FLASHMEM void updateenv4_decay(bool announce) {
  env4_decay_str = map(env4_decay, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env4 Decay", String(env4_decay_str));
    startParameterDisplay();
  }
  midiCCOut(CC4decay, env4_decay);
}

FLASHMEM void updateenv4_sustain(bool announce) {
  env4_sustain_str = map(env4_sustain, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env4 Sustain", String(env4_sustain_str));
    startParameterDisplay();
  }
  midiCCOut(CC4sustain, env4_sustain);
}

FLASHMEM void updateenv4_release(bool announce) {
  env4_release_str = map(env4_release, 0, 127, 0, 99);
  if (announce) {
    showCurrentParameterPage("Env4 Release", String(env4_release_str));
    startParameterDisplay();
  }
  midiCCOut(CC4release, env4_release);
}

FLASHMEM void updateenv4_adsr_mode(bool announce) {
  env4_adsr_mode_str = map(env4_adsr_mode, 0, 127, 0, 7);
  if (announce) {
    switch (env4_adsr_mode_str) {
      case 0:
        showCurrentParameterPage("Env4 Key F.", "Off");
        break;

      case 1:
        showCurrentParameterPage("Env4 Key F.", "Key 1");
        break;

      case 2:
        showCurrentParameterPage("Env4 Key F.", "Key 2");
        break;

      case 3:
        showCurrentParameterPage("Env4 Key F.", "Key 3");
        break;

      case 4:
        showCurrentParameterPage("Env4 Key F.", "Loop 0");
        break;

      case 5:
        showCurrentParameterPage("Env4 Key F.", "Loop 1");
        break;

      case 6:
        showCurrentParameterPage("Env4 Key F.", "Loop 2");
        break;

      case 7:
        showCurrentParameterPage("Env4 Key F.", "Loop 3");
        break;
    }
    startParameterDisplay();
  }
  switch (env4_adsr_mode_str) {
    case 0:
      midiCCOut(CC4adsr_mode, 0x00);
      break;

    case 1:
      midiCCOut(CC4adsr_mode, 0x10);
      break;

    case 2:
      midiCCOut(CC4adsr_mode, 0x20);
      break;

    case 3:
      midiCCOut(CC4adsr_mode, 0x30);
      break;

    case 4:
      midiCCOut(CC4adsr_mode, 0x40);
      break;

    case 5:
      midiCCOut(CC4adsr_mode, 0x50);
      break;

    case 6:
      midiCCOut(CC4adsr_mode, 0x60);
      break;

    case 7:
      midiCCOut(CC4adsr_mode, 0x70);
      break;
  }
}

FLASHMEM void updatectla(bool announce) {
  if (announce) {
    showCurrentParameterPage("Control A", String(ctla));
    startParameterDisplay();
  }
  midiCCOut(CCctla, ctla);
}

FLASHMEM void updatectlb(bool announce) {
  if (announce) {
    showCurrentParameterPage("Control B", String(ctlb));
    startParameterDisplay();
  }
  midiCCOut(CCctlb, ctlb);
}

// Buttons

FLASHMEM void updatelfo1_sync(bool announce) {
  if (announce) {
    switch (lfo1_sync) {
      case 0:
        showCurrentParameterPage("LFO1 Sync", "Off");
        break;
      case 1:
        showCurrentParameterPage("LFO1 Sync", "On");
        break;
      case 2:
        showCurrentParameterPage("LFO1 Sync", "Key");
        break;
    }
    startParameterDisplay();
  }
  switch (lfo1_sync) {
    case 0:
      midiCCOut(CClfo1_sync, 0x00);
      mcp1.digitalWrite(LFO1_SYNC_RED, LOW);
      mcp1.digitalWrite(LFO1_SYNC_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CClfo1_sync, 0x20);
      mcp1.digitalWrite(LFO1_SYNC_RED, HIGH);
      mcp1.digitalWrite(LFO1_SYNC_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CClfo1_sync, 0x40);
      mcp1.digitalWrite(LFO1_SYNC_RED, LOW);
      mcp1.digitalWrite(LFO1_SYNC_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatelfo2_sync(bool announce) {
  if (announce) {
    switch (lfo2_sync) {
      case 0:
        showCurrentParameterPage("LFO2 Sync", "Off");
        break;
      case 1:
        showCurrentParameterPage("LFO2 Sync", "On");
        break;
      case 2:
        showCurrentParameterPage("LFO2 Sync", "Key");
        break;
    }
    startParameterDisplay();
  }
  switch (lfo2_sync) {
    case 0:
      midiCCOut(CClfo2_sync, 0x00);
      mcp2.digitalWrite(LFO2_SYNC_RED, LOW);
      mcp2.digitalWrite(LFO2_SYNC_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CClfo2_sync, 0x20);
      mcp2.digitalWrite(LFO2_SYNC_RED, HIGH);
      mcp2.digitalWrite(LFO2_SYNC_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CClfo2_sync, 0x40);
      mcp2.digitalWrite(LFO2_SYNC_RED, LOW);
      mcp2.digitalWrite(LFO2_SYNC_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco1_PWM_dyn(bool announce) {
  if (announce) {
    switch (dco1_PWM_dyn) {
      case 0:
        showCurrentParameterPage("DCO1 PWM", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("DCO1 PWM", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("DCO1 PWM", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("DCO1 PWM", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_PWM_dyn) {
    case 0:
      midiCCOut(CCdco1_PWM_dyn, 0x00);
      mcp1.digitalWrite(DCO1_PWM_DYN_RED, LOW);
      mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco1_PWM_dyn, 0x20);
      mcp1.digitalWrite(DCO1_PWM_DYN_RED, HIGH);
      mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco1_PWM_dyn, 0x40);
      mcp1.digitalWrite(DCO1_PWM_DYN_RED, LOW);
      mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco1_PWM_dyn, 0x60);
      mcp1.digitalWrite(DCO1_PWM_DYN_RED, HIGH);
      mcp1.digitalWrite(DCO1_PWM_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco2_PWM_dyn(bool announce) {
  if (announce) {
    switch (dco2_PWM_dyn) {
      case 0:
        showCurrentParameterPage("DCO2 PWM", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("DCO2 PWM", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("DCO2 PWM", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("DCO2 PWM", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_PWM_dyn) {
    case 0:
      midiCCOut(CCdco2_PWM_dyn, 0x00);
      mcp2.digitalWrite(DCO2_PWM_DYN_RED, LOW);
      mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco2_PWM_dyn, 0x20);
      mcp2.digitalWrite(DCO2_PWM_DYN_RED, HIGH);
      mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco2_PWM_dyn, 0x40);
      mcp2.digitalWrite(DCO2_PWM_DYN_RED, LOW);
      mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco2_PWM_dyn, 0x60);
      mcp2.digitalWrite(DCO2_PWM_DYN_RED, HIGH);
      mcp2.digitalWrite(DCO2_PWM_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco1_PWM_env_source(bool announce) {
  if (announce) {
    switch (dco1_PWM_env_source) {
      case 0:
        showCurrentParameterPage("DCO1 PWM", "Env1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO1 PWM", "Env1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO1 PWM", "Env2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO1 PWM", "Env2 Positive");
        break;
      case 4:
        showCurrentParameterPage("DCO1 PWM", "Env3 Negative");
        break;
      case 5:
        showCurrentParameterPage("DCO1 PWM", "Env3 Positive");
        break;
      case 6:
        showCurrentParameterPage("DCO1 PWM", "Env4 Negative");
        break;
      case 7:
        showCurrentParameterPage("DCO1 PWM", "Env4 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_PWM_env_source) {
    case 0:
      midiCCOut(CCdco1_PWM_env_source, 0x00);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco1_PWM_env_source, 0x10);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
      break;
    case 2:
      midiCCOut(CCdco1_PWM_env_source, 0x20);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCdco1_PWM_env_source, 0x30);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
      break;
    case 4:
      midiCCOut(CCdco1_PWM_env_source, 0x40);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
      break;
    case 5:
      midiCCOut(CCdco1_PWM_env_source, 0x50);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, LOW);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
      break;
    case 6:
      midiCCOut(CCdco1_PWM_env_source, 0x60);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, LOW);
      break;
    case 7:
      midiCCOut(CCdco1_PWM_env_source, 0x70);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_RED, HIGH);
      mcp1.digitalWrite(DCO1_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp1.digitalWrite(DCO1_ENV_POL_RED, LOW);
      mcp1.digitalWrite(DCO1_ENV_POL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco2_PWM_env_source(bool announce) {
  if (announce) {
    switch (dco2_PWM_env_source) {
      case 0:
        showCurrentParameterPage("DCO2 PWM", "Env1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO2 PWM", "Env1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO2 PWM", "Env2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO2 PWM", "Env2 Positive");
        break;
      case 4:
        showCurrentParameterPage("DCO2 PWM", "Env3 Negative");
        break;
      case 5:
        showCurrentParameterPage("DCO2 PWM", "Env3 Positive");
        break;
      case 6:
        showCurrentParameterPage("DCO2 PWM", "Env4 Negative");
        break;
      case 7:
        showCurrentParameterPage("DCO2 PWM", "Env4 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_PWM_env_source) {
    case 0:
      midiCCOut(CCdco2_PWM_env_source, 0x00);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco2_PWM_env_source, 0x10);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
      break;
    case 2:
      midiCCOut(CCdco2_PWM_env_source, 0x20);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCdco2_PWM_env_source, 0x30);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
      break;
    case 4:
      midiCCOut(CCdco1_PWM_env_source, 0x40);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
      break;
    case 5:
      midiCCOut(CCdco2_PWM_env_source, 0x50);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, LOW);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
      break;
    case 6:
      midiCCOut(CCdco2_PWM_env_source, 0x60);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, LOW);
      break;
    case 7:
      midiCCOut(CCdco2_PWM_env_source, 0x70);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_RED, HIGH);
      mcp2.digitalWrite(DCO2_PWM_ENV_SOURCE_GREEN, HIGH);
      mcp2.digitalWrite(DCO2_ENV_POL_RED, LOW);
      mcp2.digitalWrite(DCO2_ENV_POL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco1_PWM_lfo_source(bool announce) {
  if (announce) {
    switch (dco1_PWM_lfo_source) {
      case 0:
        showCurrentParameterPage("DCO1 PWM", "LFO1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO1 PWM", "LFO1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO1 PWM", "LFO2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO1 PWM", "LFO1 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_PWM_lfo_source) {
    case 0:
      midiCCOut(CCdco1_PWM_lfo_source, 0x00);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, LOW);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco1_PWM_lfo_source, 0x20);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco1_PWM_lfo_source, 0x40);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, LOW);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco1_PWM_lfo_source, 0x60);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PWM_LFO_SEL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco2_PWM_lfo_source(bool announce) {
  if (announce) {
    switch (dco2_PWM_lfo_source) {
      case 0:
        showCurrentParameterPage("DCO2 PWM", "LFO1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO2 PWM", "LFO1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO2 PWM", "LFO2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO2 PWM", "LFO1 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_PWM_lfo_source) {
    case 0:
      midiCCOut(CCdco2_PWM_lfo_source, 0x00);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, LOW);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco2_PWM_lfo_source, 0x20);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco2_PWM_lfo_source, 0x40);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, LOW);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco2_PWM_lfo_source, 0x60);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PWM_LFO_SEL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco1_pitch_dyn(bool announce) {
  if (announce) {
    switch (dco1_pitch_dyn) {
      case 0:
        showCurrentParameterPage("DCO1 Pitch", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("DCO1 Pitch", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("DCO1 Pitch", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("DCO1 Pitch", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_pitch_dyn) {
    case 0:
      midiCCOut(CCdco1_pitch_dyn, 0x00);
      mcp4.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco1_pitch_dyn, 0x20);
      mcp4.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco1_pitch_dyn, 0x40);
      mcp4.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco1_pitch_dyn, 0x60);
      mcp4.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco2_pitch_dyn(bool announce) {
  if (announce) {
    switch (dco2_pitch_dyn) {
      case 0:
        showCurrentParameterPage("DCO2 Pitch", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("DCO2 Pitch", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("DCO2 Pitch", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("DCO2 Pitch", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_pitch_dyn) {
    case 0:
      midiCCOut(CCdco2_pitch_dyn, 0x00);
      mcp3.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
      mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco2_pitch_dyn, 0x20);
      mcp3.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
      mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco2_pitch_dyn, 0x40);
      mcp3.digitalWrite(DCO1_PITCH_DYN_RED, LOW);
      mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco2_pitch_dyn, 0x60);
      mcp3.digitalWrite(DCO1_PITCH_DYN_RED, HIGH);
      mcp3.digitalWrite(DCO1_PITCH_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco1_pitch_lfo_source(bool announce) {
  if (announce) {
    switch (dco1_pitch_lfo_source) {
      case 0:
        showCurrentParameterPage("DCO1 Pitch", "LFO1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO1 Pitch", "LFO1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO1 Pitch", "LFO2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO1 Pitch", "LFO2 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_pitch_lfo_source) {
    case 0:
      midiCCOut(CCdco1_pitch_lfo_source, 0x00);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco1_pitch_lfo_source, 0x20);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco1_pitch_lfo_source, 0x40);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco1_pitch_lfo_source, 0x60);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_LFO_SEL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco2_pitch_lfo_source(bool announce) {
  if (announce) {
    switch (dco2_pitch_lfo_source) {
      case 0:
        showCurrentParameterPage("DCO2 Pitch", "LFO1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO2 Pitch", "LFO1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO2 Pitch", "LFO2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO2 Pitch", "LFO2 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_pitch_lfo_source) {
    case 0:
      midiCCOut(CCdco2_pitch_lfo_source, 0x00);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco2_pitch_lfo_source, 0x20);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco2_pitch_lfo_source, 0x40);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco2_pitch_lfo_source, 0x60);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_LFO_SEL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco1_pitch_env_source(bool announce) {
  if (announce) {
    switch (dco1_pitch_env_source) {
      case 0:
        showCurrentParameterPage("DCO1 Pitch", "Env1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO1 Pitch", "Env1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO1 Pitch", "Env2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO1 Pitch", "Env2 Positive");
        break;
      case 4:
        showCurrentParameterPage("DCO1 Pitch", "Env3 Negative");
        break;
      case 5:
        showCurrentParameterPage("DCO1 Pitch", "Env3 Positive");
        break;
      case 6:
        showCurrentParameterPage("DCO1 Pitch", "Env4 Negative");
        break;
      case 7:
        showCurrentParameterPage("DCO1 Pitch", "Env4 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco1_pitch_env_source) {
    case 0:
      midiCCOut(CCdco1_pitch_env_source, 0x00);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco1_pitch_env_source, 0x10);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
      break;
    case 2:
      midiCCOut(CCdco1_pitch_env_source, 0x20);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCdco1_pitch_env_source, 0x30);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
      break;
    case 4:
      midiCCOut(CCdco1_pitch_env_source, 0x40);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 5:
      midiCCOut(CCdco1_pitch_env_source, 0x50);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
      break;
    case 6:
      midiCCOut(CCdco1_pitch_env_source, 0x60);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 7:
      midiCCOut(CCdco1_pitch_env_source, 0x70);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_RED, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_RED, LOW);
      mcp4.digitalWrite(DCO1_PITCH_ENV_POL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco2_pitch_env_source(bool announce) {
  if (announce) {
    switch (dco2_pitch_env_source) {
      case 0:
        showCurrentParameterPage("DCO2 Pitch", "Env1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO2 Pitch", "Env1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO2 Pitch", "Env2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO2 Pitch", "Env2 Positive");
        break;
      case 4:
        showCurrentParameterPage("DCO2 Pitch", "Env3 Negative");
        break;
      case 5:
        showCurrentParameterPage("DCO2 Pitch", "Env3 Positive");
        break;
      case 6:
        showCurrentParameterPage("DCO2 Pitch", "Env4 Negative");
        break;
      case 7:
        showCurrentParameterPage("DCO2 Pitch", "Env4 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco2_pitch_env_source) {
    case 0:
      midiCCOut(CCdco2_pitch_env_source, 0x00);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco2_pitch_env_source, 0x10);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
      break;
    case 2:
      midiCCOut(CCdco2_pitch_env_source, 0x20);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCdco2_pitch_env_source, 0x30);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
      break;
    case 4:
      midiCCOut(CCdco2_pitch_env_source, 0x40);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 5:
      midiCCOut(CCdco2_pitch_env_source, 0x50);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
      break;
    case 6:
      midiCCOut(CCdco2_pitch_env_source, 0x60);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, LOW);
      break;
    case 7:
      midiCCOut(CCdco2_pitch_env_source, 0x70);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_RED, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_SOURCE_GREEN, HIGH);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_RED, LOW);
      mcp3.digitalWrite(DCO2_PITCH_ENV_POL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updateplaymode(bool announce) {
  if (announce) {
    switch (playmode) {
      case 0:
        showCurrentParameterPage("Editing", "Lower");
        break;
      case 1:
        showCurrentParameterPage("Editing", "Upper");
        break;
      case 2:
        showCurrentParameterPage("Editing", "Both");
        break;
    }
    startParameterDisplay();
  }
  switch (playmode) {
    case 0:
      //midiCCOut(CCplaymode, 0x00);
      mcp1.digitalWrite(LOWER_SELECT, HIGH);
      mcp1.digitalWrite(UPPER_SELECT, LOW);
      break;
    case 1:
      //midiCCOut(CCplaymode, 0x20);
      mcp1.digitalWrite(LOWER_SELECT, LOW);
      mcp1.digitalWrite(UPPER_SELECT, HIGH);
      break;
    case 2:
      //midiCCOut(CCplaymode, 0x40);
      mcp1.digitalWrite(LOWER_SELECT, HIGH);
      mcp1.digitalWrite(UPPER_SELECT, HIGH);
      break;
  }
}

FLASHMEM void updatedco_mix_env_source(bool announce) {
  if (announce) {
    switch (dco_mix_env_source) {
      case 0:
        showCurrentParameterPage("DCO Mix", "Env1 Negative");
        break;
      case 1:
        showCurrentParameterPage("DCO Mix", "Env1 Positive");
        break;
      case 2:
        showCurrentParameterPage("DCO MIx", "Env2 Negative");
        break;
      case 3:
        showCurrentParameterPage("DCO Mix", "Env2 Positive");
        break;
      case 4:
        showCurrentParameterPage("DCO Mix", "Env3 Negative");
        break;
      case 5:
        showCurrentParameterPage("DCO Mix", "Env3 Positive");
        break;
      case 6:
        showCurrentParameterPage("DCO Mix", "Env4 Negative");
        break;
      case 7:
        showCurrentParameterPage("DCO Mix", "Env4 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (dco_mix_env_source) {
    case 0:
      midiCCOut(CCdco_mix_env_source, 0x00);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco_mix_env_source, 0x10);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
      break;
    case 2:
      midiCCOut(CCdco_mix_env_source, 0x20);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCdco_mix_env_source, 0x30);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
      break;
    case 4:
      midiCCOut(CCdco_mix_env_source, 0x40);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
      break;
    case 5:
      midiCCOut(CCdco_mix_env_source, 0x50);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
      break;
    case 6:
      midiCCOut(CCdco_mix_env_source, 0x60);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, LOW);
      break;
    case 7:
      midiCCOut(CCdco_mix_env_source, 0x70);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_ENV_POL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatedco_mix_dyn(bool announce) {
  if (announce) {
    switch (dco_mix_dyn) {
      case 0:
        showCurrentParameterPage("DCO Mix", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("DCO Mix", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("DCO Mix", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("DCO Mix", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (dco_mix_dyn) {
    case 0:
      midiCCOut(CCdco_mix_dyn, 0x00);
      mcp5.digitalWrite(DCO_MIX_DYN_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCdco_mix_dyn, 0x20);
      mcp5.digitalWrite(DCO_MIX_DYN_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCdco_mix_dyn, 0x40);
      mcp5.digitalWrite(DCO_MIX_DYN_RED, LOW);
      mcp5.digitalWrite(DCO_MIX_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCdco_mix_dyn, 0x60);
      mcp5.digitalWrite(DCO_MIX_DYN_RED, HIGH);
      mcp5.digitalWrite(DCO_MIX_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatevcf_env_source(bool announce) {
  if (announce) {
    switch (vcf_env_source) {
      case 0:
        showCurrentParameterPage("VCF EG", "Env1 Negative");
        break;
      case 1:
        showCurrentParameterPage("VCF EG", "Env1 Positive");
        break;
      case 2:
        showCurrentParameterPage("VCF EG", "Env2 Negative");
        break;
      case 3:
        showCurrentParameterPage("VCF EG", "Env2 Positive");
        break;
      case 4:
        showCurrentParameterPage("VCF EG", "Env3 Negative");
        break;
      case 5:
        showCurrentParameterPage("VCF EG", "Env3 Positive");
        break;
      case 6:
        showCurrentParameterPage("VCF EG", "Env4 Negative");
        break;
      case 7:
        showCurrentParameterPage("VCF EG", "Env4 Positive");
        break;
    }
    startParameterDisplay();
  }
  switch (vcf_env_source) {
    case 0:
      midiCCOut(CCvcf_env_source, 0x00);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCvcf_env_source, 0x10);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
      break;
    case 2:
      midiCCOut(CCvcf_env_source, 0x20);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCvcf_env_source, 0x30);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
      break;
    case 4:
      midiCCOut(CCvcf_env_source, 0x40);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
      break;
    case 5:
      midiCCOut(CCvcf_env_source, 0x50);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
      break;
    case 6:
      midiCCOut(CCvcf_env_source, 0x60);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, LOW);
      break;
    case 7:
      midiCCOut(CCvcf_env_source, 0x70);
      mcp5.digitalWrite(VCF_ENV_SOURCE_RED, HIGH);
      mcp5.digitalWrite(VCF_ENV_SOURCE_GREEN, HIGH);
      mcp5.digitalWrite(VCF_ENV_POL_RED, LOW);
      mcp5.digitalWrite(VCF_ENV_POL_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatevcf_dyn(bool announce) {
  if (announce) {
    switch (vcf_dyn) {
      case 0:
        showCurrentParameterPage("VCF Env", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("VCF Env", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("VCF Env", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("VCF Env", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (vcf_dyn) {
    case 0:
      midiCCOut(CCvcf_dyn, 0x00);
      mcp6.digitalWrite(VCF_DYN_RED, LOW);
      mcp6.digitalWrite(VCF_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCvcf_dyn, 0x20);
      mcp6.digitalWrite(VCF_DYN_RED, HIGH);
      mcp6.digitalWrite(VCF_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCvcf_dyn, 0x40);
      mcp6.digitalWrite(VCF_DYN_RED, LOW);
      mcp6.digitalWrite(VCF_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCvcf_dyn, 0x60);
      mcp6.digitalWrite(VCF_DYN_RED, HIGH);
      mcp6.digitalWrite(VCF_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatevca_env_source(bool announce) {
  if (announce) {
    switch (vca_env_source) {
      case 0:
        showCurrentParameterPage("VCA EG", "Env1");
        break;
      case 1:
        showCurrentParameterPage("VCA EG", "Env2");
        break;
      case 2:
        showCurrentParameterPage("VCA EG", "Env3");
        break;
      case 3:
        showCurrentParameterPage("VCA _EG", "Env4");
        break;
    }
    startParameterDisplay();
  }
  switch (vca_env_source) {
    case 0:
      midiCCOut(CCvca_env_source, 0x00);
      mcp6.digitalWrite(VCA_ENV_SOURCE_RED, LOW);
      mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCvca_env_source, 0x20);
      mcp6.digitalWrite(VCA_ENV_SOURCE_RED, LOW);
      mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCvca_env_source, 0x40);
      mcp6.digitalWrite(VCA_ENV_SOURCE_RED, HIGH);
      mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
      break;
    case 3:
      midiCCOut(CCvca_env_source, 0x60);
      mcp6.digitalWrite(VCA_ENV_SOURCE_RED, HIGH);
      mcp6.digitalWrite(VCA_ENV_SOURCE_GREEN, LOW);
      break;
  }
}

FLASHMEM void updatevca_dyn(bool announce) {
  if (announce) {
    switch (vca_dyn) {
      case 0:
        showCurrentParameterPage("VCA Env", "Dynamics Off");
        break;
      case 1:
        showCurrentParameterPage("VCA Env", "Dynamics 1");
        break;
      case 2:
        showCurrentParameterPage("VCA Env", "Dynamics 2");
        break;
      case 3:
        showCurrentParameterPage("VCA Env", "Dynamics 3");
        break;
    }
    startParameterDisplay();
  }
  switch (vca_dyn) {
    case 0:
      midiCCOut(CCvca_dyn, 0x00);
      mcp6.digitalWrite(VCA_DYN_RED, LOW);
      mcp6.digitalWrite(VCA_DYN_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCvca_dyn, 0x20);
      mcp6.digitalWrite(VCA_DYN_RED, HIGH);
      mcp6.digitalWrite(VCA_DYN_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCvca_dyn, 0x40);
      mcp6.digitalWrite(VCA_DYN_RED, LOW);
      mcp6.digitalWrite(VCA_DYN_GREEN, HIGH);
      break;
    case 3:
      midiCCOut(CCvca_dyn, 0x60);
      mcp6.digitalWrite(VCA_DYN_RED, HIGH);
      mcp6.digitalWrite(VCA_DYN_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updatechorus(bool announce) {
  if (announce) {
    switch (chorus) {
      case 0:
        showCurrentParameterPage("Chorus", "Off");
        break;
      case 1:
        showCurrentParameterPage("Chorus", "Chorus 1");
        break;
      case 2:
        showCurrentParameterPage("Chorus", "Chorus 2");
        break;
    }
    startParameterDisplay();
  }
  switch (chorus) {
    case 0:
      midiCCOut(CCchorus_sw, 0x00);
      mcp6.digitalWrite(CHORUS_SELECT_RED, LOW);
      mcp6.digitalWrite(CHORUS_SELECT_GREEN, LOW);
      break;
    case 1:
      midiCCOut(CCchorus_sw, 0x20);
      mcp6.digitalWrite(CHORUS_SELECT_RED, HIGH);
      mcp6.digitalWrite(CHORUS_SELECT_GREEN, LOW);
      break;
    case 2:
      midiCCOut(CCchorus_sw, 0x40);
      mcp6.digitalWrite(CHORUS_SELECT_RED, LOW);
      mcp6.digitalWrite(CHORUS_SELECT_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updateenv5stage(bool announce) {
  if (announce) {
    switch (env5stage) {
      case 0:
        showCurrentParameterPage("5 Stage", "Envelope 1");
        break;
      case 1:
        showCurrentParameterPage("5 Stage", "Envelope 2");
        break;
    }
    startParameterDisplay();
  }
  switch (env5stage) {
    case 0:
      mcp5.digitalWrite(ENV5STAGE_SELECT_RED, HIGH);
      mcp5.digitalWrite(ENV5STAGE_SELECT_GREEN, LOW);
      break;
    case 1:
      mcp5.digitalWrite(ENV5STAGE_SELECT_RED, LOW);
      mcp5.digitalWrite(ENV5STAGE_SELECT_GREEN, HIGH);
      break;
  }
}

FLASHMEM void updateadsr(bool announce) {
  if (announce) {
    switch (adsr) {
      case 0:
        showCurrentParameterPage("ADSR", "Envelope 3");
        break;
      case 1:
        showCurrentParameterPage("ADSR", "Envelope 4");
        break;
    }
    startParameterDisplay();
  }
  switch (adsr) {
    case 0:
      mcp6.digitalWrite(ADSR_SELECT_RED, HIGH);
      mcp6.digitalWrite(ADSR_SELECT_GREEN, LOW);
      break;
    case 1:
      mcp6.digitalWrite(ADSR_SELECT_RED, LOW);
      mcp6.digitalWrite(ADSR_SELECT_GREEN, HIGH);
      break;
  }
}

// void RotaryEncoderChanged(bool clockwise, int id) {

//   if (!accelerate) {
//     speed = 1;
//   } else {
//     speed = getEncoderSpeed(id);
//   }

//   if (!clockwise) {
//     speed = -speed;
//   }



//   //rotaryEncoderChanged(id, clockwise, speed);
// }

int getEncoderSpeed(int id) {
  if (id < 1 || id > numEncoders) return 1;

  unsigned long now = millis();
  unsigned long revolutionTime = now - lastTransition[id];

  int speed = 1;
  if (revolutionTime < 50) {
    speed = 10;
  } else if (revolutionTime < 125) {
    speed = 5;
  } else if (revolutionTime < 250) {
    speed = 2;
  }

  lastTransition[id] = now;
  return speed;
}

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

void myNoteOn(byte channel, byte note, byte velocity) {


  if (playModeSW == 0) {
    detune = 1.000;  //POLYPHONIC mode
    if (note < 0 || note > 127) return;
    switch (getVoiceNo(-1)) {
      case 1:
        voices[0].note = note;
        voices[0].velocity = velocity;
        voices[0].timeOn = millis();
        note1freq = note;


        voiceOn[0] = true;
        //Serial.println("Voice 1 On");
        break;

      case 2:
        voices[1].note = note;
        voices[1].velocity = velocity;
        voices[1].timeOn = millis();
        note2freq = note;


        voiceOn[1] = true;
        //Serial.println("Voice 2 On");
        break;

      case 3:
        voices[2].note = note;
        voices[2].velocity = velocity;
        voices[2].timeOn = millis();
        note3freq = note;

        voiceOn[2] = true;
        //Serial.println("Voice 3 On");
        break;

      case 4:
        voices[3].note = note;
        voices[3].velocity = velocity;
        voices[3].timeOn = millis();
        note4freq = note;

        voiceOn[3] = true;
        //Serial.println("Voice 4 On");
        break;

      case 5:
        voices[4].note = note;
        voices[4].velocity = velocity;
        voices[4].timeOn = millis();
        note5freq = note;

        voiceOn[4] = true;
        //Serial.println("Voice 5 On");
        break;

      case 6:
        voices[5].note = note;
        voices[5].velocity = velocity;
        voices[5].timeOn = millis();
        note6freq = note;

        voiceOn[5] = true;
        //Serial.println("Voice 6 On");
        break;

      case 7:
        voices[6].note = note;
        voices[6].velocity = velocity;
        voices[6].timeOn = millis();
        note7freq = note;

        voiceOn[6] = true;
        //Serial.println("Voice 7 On");
        break;

      case 8:
        voices[7].note = note;
        voices[7].velocity = velocity;
        voices[7].timeOn = millis();
        note8freq = note;

        voiceOn[7] = true;
        //Serial.println("Voice 8 On");
        break;
    }
  }

  if (playModeSW == 2) {  //UNISON mode

    noteMsg = note;

    if (velocity == 0) {
      notes[noteMsg] = false;
    } else {
      notes[noteMsg] = true;
    }

    switch (notePrioritySW) {
      case 1:
        commandTopNoteUnison();
        break;

      case 0:
        commandBottomNoteUnison();
        break;

      case 2:
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNoteUnison();
        break;
    }
  }

  if (playModeSW == 1) {
    detune = 1.000;
    noteMsg = note;

    if (velocity == 0) {
      notes[noteMsg] = false;
    } else {
      notes[noteMsg] = true;
    }

    switch (notePrioritySW) {
      case 1:
        commandTopNote();
        break;

      case 0:
        commandBottomNote();
        break;

      case 2:
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNote();
        break;
    }
  }
}

void myNoteOff(byte channel, byte note, byte velocity) {

  numberOfNotes = numberOfNotes - 1;
  oldnumberOfNotes = oldnumberOfNotes - 1;

  if (playModeSW == 0) {  //POLYPHONIC mode
    detune = 1.000;
    switch (getVoiceNo(note)) {
      case 1:

        voices[0].note = -1;
        voiceOn[0] = false;
        //Serial.println("Voice 1 Off");
        break;

      case 2:

        voices[1].note = -1;
        voiceOn[1] = false;
        //Serial.println("Voice 2 Off");
        break;

      case 3:

        voices[2].note = -1;
        voiceOn[2] = false;
        //Serial.println("Voice 3 Off");
        break;

      case 4:

        voices[3].note = -1;
        voiceOn[3] = false;
        //Serial.println("Voice 4 Off");
        break;

      case 5:

        voices[4].note = -1;
        voiceOn[4] = false;
        //Serial.println("Voice 5 Off");
        break;

      case 6:

        voices[5].note = -1;
        voiceOn[5] = false;
        //Serial.println("Voice 6 Off");
        break;

      case 7:

        voices[6].note = -1;
        voiceOn[6] = false;
        //Serial.println("Voice 7 Off");
        break;

      case 8:

        voices[7].note = -1;
        voiceOn[7] = false;
        //Serial.println("Voice 8 Off");
        break;
    }
  }

  if (playModeSW == 2) {  //UNISON

    noteMsg = note;
    notes[noteMsg] = false;

    switch (notePrioritySW) {
      case 1:
        commandTopNoteUnison();
        break;

      case 0:
        commandBottomNoteUnison();
        break;

      case 2:
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNoteUnison();
        break;
    }
  }

  if (playModeSW == 1) {

    noteMsg = note;
    notes[noteMsg] = false;

    switch (notePrioritySW) {
      case 1:
        commandTopNote();
        break;

      case 0:
        commandBottomNote();
        break;

      case 2:
        if (notes[noteMsg]) {  // If note is on and using last note priority, add to ordered list
          orderIndx = (orderIndx + 1) % 40;
          noteOrder[orderIndx] = noteMsg;
        }
        commandLastNote();
        break;
    }
  }
}

int mod(int a, int b) {
  int r = a % b;
  return r < 0 ? r + b : r;
}

void commandTopNote() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 88; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNote(topNote);
  } else {  // All notes are off, turn off gate
  }
}

void commandBottomNote() {

  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 87; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNote(bottomNote);
  } else {  // All notes are off, turn off gate
  }
}

void commandLastNote() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNote(noteIndx);
      return;
    }
  }
}

void commandNote(int note) {

  note1freq = note;
}

void commandTopNoteUnison() {
  int topNote = 0;
  bool noteActive = false;

  for (int i = 0; i < 88; i++) {
    if (notes[i]) {
      topNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUnison(topNote);
  } else {  // All notes are off, turn off gate
  }
}

void commandBottomNoteUnison() {

  int bottomNote = 0;
  bool noteActive = false;

  for (int i = 87; i >= 0; i--) {
    if (notes[i]) {
      bottomNote = i;
      noteActive = true;
    }
  }

  if (noteActive) {
    commandNoteUnison(bottomNote);
  } else {  // All notes are off, turn off gate
  }
}

void commandLastNoteUnison() {

  int8_t noteIndx;

  for (int i = 0; i < 40; i++) {
    noteIndx = noteOrder[mod(orderIndx - i, 40)];
    if (notes[noteIndx]) {
      commandNoteUnison(noteIndx);
      return;
    }
  }
}

void commandNoteUnison(int note) {

  // Limit to available voices
  if (uniNotes > NO_OF_VOICES) uniNotes = NO_OF_VOICES;
  if (uniNotes < 1) uniNotes = 1;

  // Set note frequency base
  for (int i = 0; i < uniNotes; i++) {
    voices[i].note = note;  // Optional bookkeeping
  }

  // Calculate detune spread
  float baseOffset = detune - 1.000;  // e.g. 0.02 for ±2%
  float spread = baseOffset;          // could later be scaled with uniNotes

  // Center index (for symmetry)
  int center = uniNotes / 2;  // integer division works for both even/odd

  // Reset all detunes first
  for (int i = 0; i < NO_OF_VOICES; i++) {
    voiceDetune[i] = 1.000;
  }

  // Assign detunes to active voices
  for (int i = 0; i < uniNotes; i++) {
    int distance = i - center;
    voiceDetune[i] = 1.000 + (distance * spread);
  }

  // Trigger only the voices used by unison
  for (int i = 0; i < uniNotes; i++) {
    switch (i) {
      case 0:
        note1freq = note;

        break;

      case 1:
        note2freq = note;

        break;

      case 2:
        note3freq = note;

        break;

      case 3:
        note4freq = note;

        break;

      case 4:
        note5freq = note;

        break;

      case 5:
        note6freq = note;

        break;

      case 6:
        note7freq = note;

        break;

      case 7:
        note8freq = note;

        break;
    }
  }
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
}

String getCurrentPatchData() {
  return patchName + "," + String(vcoAWave) + "," + String(vcoBWave) + "," + String(vcoCWave) + "," + String(vcoAPW) + "," + String(vcoBPW) + "," + String(vcoCPW)
         + "," + String(vcoAPWM) + "," + String(vcoBPWM) + "," + String(vcoCPWM) + "," + String(vcoBDetune) + "," + String(vcoCDetune)
         + "," + String(vcoAFMDepth) + "," + String(vcoBFMDepth) + "," + String(vcoCFMDepth) + "," + String(vcoALevel) + "," + String(vcoBLevel) + "," + String(vcoCLevel)
         + "," + String(filterCutoff) + "," + String(filterResonance) + "," + String(filterEGDepth) + "," + String(filterKeyTrack) + "," + String(filterLFODepth)
         + "," + String(pitchAttack) + "," + String(pitchDecay) + "," + String(pitchSustain) + "," + String(pitchRelease)
         + "," + String(filterAttack) + "," + String(filterDecay) + "," + String(filterSustain) + "," + String(filterRelease)
         + "," + String(ampAttack) + "," + String(ampDecay) + "," + String(ampSustain) + "," + String(ampRelease)
         + "," + String(LFO1Rate) + "," + String(LFO1Delay) + "," + String(LFO1Wave) + "," + String(LFO2Rate)
         + "," + String(vcoAInterval) + "," + String(vcoBInterval) + "," + String(vcoCInterval)
         + "," + String(vcoAPWMsource) + "," + String(vcoBPWMsource) + "," + String(vcoCPWMsource) + "," + String(vcoAFMsource) + "," + String(vcoBFMsource) + "," + String(vcoCFMsource)
         + "," + String(ampLFODepth) + "," + String(XModDepth) + "," + String(LFO2Wave) + "," + String(noiseLevel)
         + "," + String(effectPot1) + "," + String(effectPot2) + "," + String(effectPot3) + "," + String(effectsMix)
         + "," + String(volumeLevel) + "," + String(MWDepth) + "," + String(PBDepth) + "," + String(ATDepth) + "," + String(filterType) + "," + String(filterPoleSW)
         + "," + String(vcoAOctave) + "," + String(vcoBOctave) + "," + String(vcoCOctave) + "," + String(filterKeyTrackSW) + "," + String(filterVelocitySW) + "," + String(ampVelocitySW)
         + "," + String(multiSW) + "," + String(effectNumberSW) + "," + String(effectBankSW) + "," + String(egInvertSW) + "," + String(vcoATable) + "," + String(vcoBTable) + "," + String(vcoCTable)
         + "," + String(vcoAWaveNumber) + "," + String(vcoBWaveNumber) + "," + String(vcoCWaveNumber) + "," + String(vcoAWaveBank) + "," + String(vcoBWaveBank) + "," + String(vcoCWaveBank)
         + "," + String(playModeSW) + "," + String(notePrioritySW) + "," + String(unidetune) + "," + String(uniNotes);
}

void setCurrentPatchData(String data[]) {
  patchName = data[0];

  vcoAWave = data[1].toInt();
  vcoBWave = data[2].toInt();
  vcoCWave = data[3].toInt();
  vcoAPW = data[4].toFloat();
  vcoBPW = data[5].toFloat();
  vcoCPW = data[6].toFloat();
  vcoAPWM = data[7].toFloat();
  vcoBPWM = data[8].toFloat();
  vcoCPWM = data[9].toFloat();
  vcoBDetune = data[10].toFloat();

  vcoCDetune = data[11].toFloat();
  vcoAFMDepth = data[12].toFloat();
  vcoBFMDepth = data[13].toFloat();
  vcoCFMDepth = data[14].toFloat();
  vcoALevel = data[15].toFloat();
  vcoBLevel = data[16].toFloat();
  vcoCLevel = data[17].toFloat();
  filterCutoff = data[18].toFloat();
  filterResonance = data[19].toFloat();
  filterEGDepth = data[20].toFloat();

  filterKeyTrack = data[21].toFloat();
  filterLFODepth = data[22].toFloat();
  pitchAttack = data[23].toFloat();
  pitchDecay = data[24].toFloat();
  pitchSustain = data[25].toFloat();
  pitchRelease = data[26].toFloat();
  filterAttack = data[27].toFloat();
  filterDecay = data[28].toFloat();
  filterSustain = data[29].toFloat();
  filterRelease = data[30].toFloat();

  ampAttack = data[31].toFloat();
  ampDecay = data[32].toFloat();
  ampSustain = data[33].toFloat();
  ampRelease = data[34].toFloat();
  LFO1Rate = data[35].toFloat();
  LFO1Delay = data[36].toFloat();
  LFO1Wave = data[37].toInt();
  LFO2Rate = data[38].toFloat();
  vcoAInterval = data[39].toInt();
  vcoBInterval = data[40].toInt();

  vcoCInterval = data[41].toInt();
  vcoAPWMsource = data[42].toInt();
  vcoBPWMsource = data[43].toInt();
  vcoCPWMsource = data[44].toInt();
  vcoAFMsource = data[45].toInt();
  vcoBFMsource = data[46].toInt();
  vcoCFMsource = data[47].toInt();
  ampLFODepth = data[48].toFloat();
  XModDepth = data[49].toFloat();
  LFO2Wave = data[50].toInt();

  noiseLevel = data[51].toFloat();
  effectPot1 = data[52].toFloat();
  effectPot2 = data[53].toFloat();
  effectPot3 = data[54].toFloat();
  effectsMix = data[55].toFloat();
  volumeLevel = data[56].toFloat();
  MWDepth = data[57].toInt();
  PBDepth = data[58].toInt();
  ATDepth = data[59].toInt();
  filterType = data[60].toInt();
  filterPoleSW = data[61].toInt();
  vcoAOctave = data[62].toInt();
  vcoBOctave = data[63].toInt();
  vcoCOctave = data[64].toInt();
  filterKeyTrackSW = data[65].toInt();
  filterVelocitySW = data[66].toInt();
  ampVelocitySW = data[67].toInt();
  multiSW = data[68].toInt();
  effectNumberSW = data[69].toInt();
  effectBankSW = data[70].toInt();
  egInvertSW = data[71].toInt();

  vcoATable = data[72].toInt();
  vcoBTable = data[73].toInt();
  vcoCTable = data[74].toInt();
  vcoAWaveNumber = data[75].toInt();
  vcoBWaveNumber = data[76].toInt();
  vcoCWaveNumber = data[77].toInt();
  vcoAWaveBank = data[78].toInt();
  vcoBWaveBank = data[79].toInt();
  vcoCWaveBank = data[80].toInt();
  playModeSW = data[81].toInt();
  notePrioritySW = data[82].toInt();
  unidetune = data[83].toInt();
  uniNotes = data[84].toInt();



  updateplaymode(0);
  updateenv5stage(0);
  updateadsr(0);

  //Patchname
  updatePatchname();

  Serial.print("Set Patch: ");
  Serial.println(patchName);
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

void midiCCOut(int CC, int value) {
  switch (playmode) {
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

void mainButtonChanged(Button *btn, bool released) {

  switch (btn->id) {
    case LFO1_SYNC_BUTTON:
      if (!released) {
        lfo1_sync = lfo1_sync + 1;
        if (lfo1_sync > 2) {
          lfo1_sync = 0;
        }
        myControlChange(midiChannel, CClfo1_sync, lfo1_sync);
      }
      break;

    case LFO2_SYNC_BUTTON:
      if (!released) {
        lfo2_sync = lfo2_sync + 1;
        if (lfo2_sync > 2) {
          lfo2_sync = 0;
        }
        myControlChange(midiChannel, CClfo2_sync, lfo2_sync);
      }
      break;

    case DCO1_PWM_DYN_BUTTON:
      if (!released) {
        dco1_PWM_dyn = dco1_PWM_dyn + 1;
        if (dco1_PWM_dyn > 3) {
          dco1_PWM_dyn = 0;
        }
        myControlChange(midiChannel, CCdco1_PWM_dyn, dco1_PWM_dyn);
      }
      break;

    case DCO2_PWM_DYN_BUTTON:
      if (!released) {
        dco2_PWM_dyn = dco2_PWM_dyn + 1;
        if (dco2_PWM_dyn > 3) {
          dco2_PWM_dyn = 0;
        }
        myControlChange(midiChannel, CCdco2_PWM_dyn, dco2_PWM_dyn);
      }
      break;

    case DCO1_PWM_ENV_SOURCE_BUTTON:
      if (!released) {
        dco1_PWM_env_source = dco1_PWM_env_source + 2;
        if (dco1_PWM_env_source > 7) {
          dco1_PWM_env_source = 0;
          dco1_PWM_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco1_PWM_env_source, dco1_PWM_env_source);
      }
      break;

    case DCO2_PWM_ENV_SOURCE_BUTTON:
      if (!released) {
        dco2_PWM_env_source = dco2_PWM_env_source + 2;
        if (dco2_PWM_env_source > 7) {
          dco2_PWM_env_source = 0;
          dco2_PWM_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco2_PWM_env_source, dco2_PWM_env_source);
      }
      break;

    case DCO1_PWM_ENV_POLARITY_BUTTON:
      if (!released) {
        dco1_PWM_env_pol = !dco1_PWM_env_pol;
        if (dco1_PWM_env_pol) {
          dco1_PWM_env_source++;
        }
        if (!dco1_PWM_env_pol) {
          dco1_PWM_env_source--;
        }
        myControlChange(midiChannel, CCdco1_PWM_env_source, dco1_PWM_env_source);
      }
      break;

    case DCO2_PWM_ENV_POLARITY_BUTTON:
      if (!released) {
        dco2_PWM_env_pol = !dco2_PWM_env_pol;
        if (dco2_PWM_env_pol) {
          dco2_PWM_env_source++;
        }
        if (!dco2_PWM_env_pol) {
          dco2_PWM_env_source--;
        }
        myControlChange(midiChannel, CCdco2_PWM_env_source, dco2_PWM_env_source);
      }
      break;

    case DCO1_PWM_LFO_SOURCE_BUTTON:
      if (!released) {
        dco1_PWM_lfo_source = dco1_PWM_lfo_source + 1;
        if (dco1_PWM_lfo_source > 3) {
          dco1_PWM_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco1_PWM_lfo_source, dco1_PWM_lfo_source);
      }
      break;

    case DCO2_PWM_LFO_SOURCE_BUTTON:
      if (!released) {
        dco2_PWM_lfo_source = dco2_PWM_lfo_source + 1;
        if (dco2_PWM_lfo_source > 3) {
          dco2_PWM_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco2_PWM_lfo_source, dco2_PWM_lfo_source);
      }
      break;

    case DCO1_PITCH_DYN_BUTTON:
      if (!released) {
        dco1_pitch_dyn = dco1_pitch_dyn + 1;
        if (dco1_pitch_dyn > 3) {
          dco1_pitch_dyn = 0;
        }
        myControlChange(midiChannel, CCdco1_pitch_dyn, dco1_pitch_dyn);
      }
      break;

    case DCO2_PITCH_DYN_BUTTON:
      if (!released) {
        dco2_pitch_dyn = dco2_pitch_dyn + 1;
        if (dco2_pitch_dyn > 3) {
          dco2_pitch_dyn = 0;
        }
        myControlChange(midiChannel, CCdco2_pitch_dyn, dco2_pitch_dyn);
      }
      break;

    case DCO1_PITCH_LFO_SOURCE_BUTTON:
      if (!released) {
        dco1_pitch_lfo_source = dco1_pitch_lfo_source + 1;
        if (dco1_pitch_lfo_source > 3) {
          dco1_pitch_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco1_pitch_lfo_source, dco1_pitch_lfo_source);
      }
      break;

    case DCO2_PITCH_LFO_SOURCE_BUTTON:
      if (!released) {
        dco2_pitch_lfo_source = dco2_pitch_lfo_source + 1;
        if (dco2_pitch_lfo_source > 3) {
          dco2_pitch_lfo_source = 0;
        }
        myControlChange(midiChannel, CCdco2_pitch_lfo_source, dco2_pitch_lfo_source);
      }
      break;

    case DCO1_PITCH_ENV_SOURCE_BUTTON:
      if (!released) {
        dco1_pitch_env_source = dco1_pitch_env_source + 2;
        if (dco1_pitch_env_source > 7) {
          dco1_pitch_env_source = 0;
          dco1_pitch_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco1_pitch_env_source, dco1_pitch_env_source);
      }
      break;

    case DCO2_PITCH_ENV_SOURCE_BUTTON:
      if (!released) {
        dco2_pitch_env_source = dco2_pitch_env_source + 2;
        if (dco2_pitch_env_source > 7) {
          dco2_pitch_env_source = 0;
          dco2_pitch_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco2_pitch_env_source, dco2_pitch_env_source);
      }
      break;

    case DCO1_PITCH_ENV_POLARITY_BUTTON:
      if (!released) {
        dco1_pitch_env_pol = !dco1_pitch_env_pol;
        if (dco1_pitch_env_pol) {
          dco1_pitch_env_source++;
        }
        if (!dco1_pitch_env_pol) {
          dco1_pitch_env_source--;
        }
        myControlChange(midiChannel, CCdco1_pitch_env_source, dco1_pitch_env_pol);
      }
      break;

    case DCO2_PITCH_ENV_POLARITY_BUTTON:
      if (!released) {
        dco2_pitch_env_pol = !dco2_pitch_env_pol;
        if (dco2_pitch_env_pol) {
          dco2_pitch_env_source++;
        }
        if (!dco2_pitch_env_pol) {
          dco2_pitch_env_source--;
        }
        myControlChange(midiChannel, CCdco2_pitch_env_source, dco2_pitch_env_pol);
      }
      break;

    case LOWER_UPPER_BUTTON:
      if (!released) {
        playmode = playmode + 1;
        if (playmode > 2) {
          playmode = 0;
        }
        myControlChange(midiChannel, CCplaymode, playmode);
      }
      break;

    case DCO_MIX_ENV_SOURCE_BUTTON:
      if (!released) {
        dco_mix_env_source = dco_mix_env_source + 2;
        if (dco_mix_env_source > 7) {
          dco_mix_env_source = 0;
          dco_mix_env_pol = 0;
        }
        myControlChange(midiChannel, CCdco_mix_env_source, dco_mix_env_source);
      }
      break;

    case DCO_MIX_ENV_POLARITY_BUTTON:
      if (!released) {
        dco_mix_env_pol = !dco_mix_env_pol;
        if (dco_mix_env_pol) {
          dco_mix_env_source++;
        }
        if (!dco_mix_env_pol) {
          dco_mix_env_source--;
        }
        myControlChange(midiChannel, CCdco_mix_env_source, dco_mix_env_source);
      }
      break;

    case DCO_MIX_DYN_BUTTON:
      if (!released) {
        dco_mix_dyn = dco_mix_dyn + 1;
        if (dco_mix_dyn > 3) {
          dco_mix_dyn = 0;
        }
        myControlChange(midiChannel, CCdco_mix_dyn, dco_mix_dyn);
      }
      break;

    case VCF_ENV_SOURCE_BUTTON:
      if (!released) {
        vcf_env_source = vcf_env_source + 2;
        if (vcf_env_source > 7) {
          vcf_env_source = 0;
          vcf_env_pol = 0;
        }
        myControlChange(midiChannel, CCvcf_env_source, vcf_env_source);
      }
      break;

    case VCF_ENV_POLARITY_BUTTON:
      if (!released) {
        vcf_env_pol = !vcf_env_pol;
        if (vcf_env_pol) {
          vcf_env_source++;
        }
        if (!vcf_env_pol) {
          vcf_env_source--;
        }
        myControlChange(midiChannel, CCvcf_env_source, vcf_env_source);
      }
      break;

    case VCF_DYN_BUTTON:
      if (!released) {
        vcf_dyn = vcf_dyn + 1;
        if (vcf_dyn > 3) {
          vcf_dyn = 0;
        }
        myControlChange(midiChannel, CCvcf_dyn, vcf_dyn);
      }
      break;

    case VCA_ENV_SOURCE_BUTTON:
      if (!released) {
        vca_env_source = vca_env_source + 1;
        if (vca_env_source > 3) {
          vca_env_source = 0;
        }
        myControlChange(midiChannel, CCvca_env_source, vca_env_source);
      }
      break;

    case VCA_DYN_BUTTON:
      if (!released) {
        vca_dyn = vca_dyn + 1;
        if (vca_dyn > 3) {
          vca_dyn = 0;
        }
        myControlChange(midiChannel, CCvca_dyn, vca_dyn);
      }
      break;

    case CHORUS_BUTTON:
      if (!released) {
        chorus = chorus + 1;
        if (chorus > 2) {
          chorus = 0;
        }
        myControlChange(midiChannel, CCchorus_sw, chorus);
      }
      break;

    case ENV5STAGE_SELECT_BUTTON:
      if (!released) {
        env5stage = !env5stage;
        myControlChange(midiChannel, CCenv5stage, env5stage);
      }
      break;

    case ADSR_SELECT_BUTTON:
      if (!released) {
        adsr = !adsr;
        myControlChange(midiChannel, CCadsr, adsr);
      }
      break;
  }
}

void checkMux() {

  mux1Read = adc->adc0->analogRead(MUX1_S);
  mux2Read = adc->adc0->analogRead(MUX2_S);
  mux3Read = adc->adc1->analogRead(MUX4_S);
  mux4Read = adc->adc1->analogRead(MUX3_S);

  if (mux1Read > (mux1ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux1Read < (mux1ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux1ValuesPrev[muxInput] = mux1Read;
    mux1Read = (mux1Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX1_LFO1_WAVE:
        myControlChange(midiChannel, CClfo1_wave, mux1Read);
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
      case MUX1_DCO1_MODE:
        myControlChange(midiChannel, CCdco1_mode, mux1Read);
        break;
    }
  }

  if (mux2Read > (mux2ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux2Read < (mux2ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux2ValuesPrev[muxInput] = mux2Read;
    mux2Read = (mux2Read >> resolutionFrig);  // Change range to 0-127

    switch (muxInput) {
      case MUX2_LFO2_WAVE:
        myControlChange(midiChannel, CClfo2_wave, mux2Read);
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
    }
  }

  if (mux3Read > (mux3ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux3Read < (mux3ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux3ValuesPrev[muxInput] = mux3Read;
    mux3Read = (mux3Read >> resolutionFrig);  // Change range to 0-127

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
    }
  }

  if (mux4Read > (mux4ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux4Read < (mux4ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux4ValuesPrev[muxInput] = mux4Read;
    mux4Read = (mux4Read >> resolutionFrig);  // Change range to 0-127

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
          myControlChange(midiChannel, CCrelease, mux4Read);
        }
        break;
      case MUX4_ADSR_MODE:
        if (!adsr) {
          myControlChange(midiChannel, CCadsr_mode, mux4Read);
        } else {
          myControlChange(midiChannel, CC4adsr_mode, mux4Read);
        }
        break;
      case MUX4_CTLA:
        myControlChange(midiChannel, CCctla, mux4Read);
        break;
      case MUX4_CTLB:
        myControlChange(midiChannel, CCctlb, mux4Read);
        break;
    }
  }

  muxInput++;
  if (muxInput >= MUXCHANNELS) {
    muxInput = 0;
  }

  digitalWriteFast(MUX_0, muxInput & B0001);
  digitalWriteFast(MUX_1, muxInput & B0010);
  digitalWriteFast(MUX_2, muxInput & B0100);
  digitalWriteFast(MUX_3, muxInput & B1000);
  delayMicroseconds(75);
}

void loop() {

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