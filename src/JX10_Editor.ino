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
#include "Wavetables.h"

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

EXTMEM int16_t wavetablePSRAM[BANKS][MAX_TABLES_PER_BANK][TABLE_SIZE];
uint16_t stagingBuffer[TABLE_SIZE];  // stays in RAM
uint16_t tablesInBank[BANKS];        // track how many tables per bank

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
      lfo1_wave_str = map(value, 0, 127, 0, 4);
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
      dco1_wave_str = map(value, 0, 127, 0, 3);
      dco1_wave = value;
      updatedco1_wave(1);
      break;

    case CCdco1_range:
      dco1_range_str = map(value, 0, 127, 0, 3);
      dco1_range = value;
      updatedco1_range(1);
      break;

    case CCdco1_tune:
      dco1_tune = value;
      updatedco1_tune(1);
      break;

    case CCdco1_mode:
      dco1_mode_str = map(value, 0, 127, 0, 3);
      dco1_mode = value;
      updatedco1_mode(1);
      break;

    case CClfo2_wave:
      lfo2_wave = value;
      lfo2_wave_str = map(value, 0, 127, 0, 4);
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
      dco2_wave_str = map(value, 0, 127, 0, 3);
      dco2_wave = value;
      updatedco2_wave(1);
      break;

    case CCdco2_range:
      dco2_range_str = map(value, 0, 127, 0, 3);
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

      // case CCvcoCFMDepth:
      //   vcoCFMDepth = map(value, 0, 127, 0, 255);
      //   updatevcoCFMDepth(1);
      //   break;

      // case CCvcoBDetune:
      //   vcoBDetune = value;
      //   updatevcoBDetune(1);
      //   break;

      // case CCvcoCDetune:
      //   vcoCDetune = value;
      //   updatevcoCDetune(1);
      //   break;

      // case CCfilterLFODepth:
      //   filterLFODepth = map(value, 0, 127, -127, 127);
      //   updatefilterLFODepth(1);
      //   break;

      // case CCeffectsMix:
      //   effectsMix = map(value, 0, 127, -127, 127);
      //   updateeffectsMix(1);
      //   break;

      // case CCvcoALevel:
      //   vcoALevel = map(value, 0, 127, 0, 255);
      //   updatevcoALevel(1);
      //   break;

      // case CCvcoBLevel:
      //   vcoBLevel = map(value, 0, 127, 0, 255);
      //   updatevcoBLevel(1);
      //   break;

      // case CCvcoCLevel:
      //   vcoCLevel = map(value, 0, 127, 0, 255);
      //   updatevcoCLevel(1);
      //   break;

      // case CCvcoAPW:

      //   break;

      // case CCvcoBPW:

      //   break;

      // case CCvcoCPW:

      //   break;

      // case CCvcoAWave:

      //   break;

      // case CCvcoBWave:

      //   break;

      // case CCvcoCWave:

      //   break;

      // case CCvcoAInterval:
      //   vcoAInterval = map(value, 0, 127, -12, 12);
      //   updatevcoAInterval(1);
      //   break;

      // case CCvcoBInterval:
      //   vcoBInterval = map(value, 0, 127, -12, 12);
      //   updatevcoBInterval(1);
      //   break;

      // case CCvcoCInterval:
      //   vcoCInterval = map(value, 0, 127, -12, 12);
      //   updatevcoCInterval(1);
      //   break;

      // case CCXModDepth:
      //   XModDepth = map(value, 0, 127, 0, 255);
      //   updateXModDepth(1);
      //   break;

      //   // Buttons

      // case CCvcoATable:
      //   updatevcoAWave(1);
      //   break;

      // case CCvcoBTable:
      //   updatevcoBWave(1);
      //   break;

      // case CCvcoCTable:
      //   updatevcoCWave(1);
      //   break;

      // case CCvcoAOctave:
      //   vcoAOctave = value;
      //   updatevcoAOctave(1);
      //   break;

      // case CCvcoBOctave:
      //   vcoBOctave = value;
      //   updatevcoBOctave(1);
      //   break;

      // case CCvcoCOctave:
      //   vcoCOctave = value;
      //   updatevcoCOctave(1);
      //   break;

      // case CCLFO1Wave:
      //   LFO1Wave = value;
      //   updateLFO1Wave(1);
      //   break;

      // case CCLFO2Wave:
      //   LFO2Wave = value;
      //   updateLFO2Wave(1);
      //   break;

      // case CCfilterLFODepthSW:
      //   updatefilterLFODepth(1);
      //   break;

      // case CCampLFODepthSW:
      //   updateampLFODepth(1);
      //   break;

      // case CCfilterEGDepthSW:
      //   updatefilterEGDepth(1);
      //   break;

      // case CCnoiseLevelSW:
      //   updatenoiseLevel(1);
      //   break;

      // case CCeffectsMixSW:
      //   updateeffectsMix(1);
      //   break;

      // case CCeffects3SW:
      //   effectsPot3SW = map(value, 0, 127, 0, 1);
      //   updateeffectsPot3SW(1);
      //   break;

      // case CCfilterKeyTrackZeroSW:
      //   updatefilterKeyTrack(1);
      //   break;

      // case CCfilterType:
      //   filterType = value;
      //   updatefilterType(1);
      //   break;

      // case CCfilterPoleSW:
      //   filterPoleSW = value;
      //   updatefilterPoleSwitch(1);
      //   break;

      // case CCegInvertSW:
      //   egInvertSW = value;
      //   updateegInvertSwitch(1);
      //   break;

      // case CCfilterKeyTrackSW:
      //   filterKeyTrackSW = value;
      //   updatefilterKeyTrackSwitch(1);
      //   break;

      // case CCfilterVelocitySW:
      //   filterVelocitySW = value;
      //   updatefilterVelocitySwitch(1);
      //   break;

      // case CCampVelocitySW:
      //   ampVelocitySW = value;
      //   updateampVelocitySwitch(1);
      //   break;

      // case CCFMSyncSW:
      //   updateFMSyncSwitch(1);
      //   break;

      // case CCPWSyncSW:
      //   updatePWSyncSwitch(1);
      //   break;

      // case CCPWMSyncSW:
      //   updatePWMSyncSwitch(1);
      //   break;

      // case CCmultiSW:
      //   multiSW = value;
      //   updatemultiSwitch(1);
      //   break;

      // case CCplayModeSW:
      //   updateplayModeSW(1);
      //   break;

      // case CCnotePrioritySW:
      //   updatenotePrioritySW(1);
      //   break;
  }
}

FLASHMEM void updatelfo1_wave(bool announce) {
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
  midiCCOut(CClfo1_wave, lfo1_wave);
}

FLASHMEM void updatelfo1_rate(bool announce) {
  if (announce) {
    showCurrentParameterPage("LFO1 Rate", String(lfo1_rate));
    startParameterDisplay();
  }
  midiCCOut(CClfo1_rate, lfo1_rate);
}

FLASHMEM void updatelfo1_delay(bool announce) {
  if (announce) {
    showCurrentParameterPage("LFO1 Delay", String(lfo1_delay));
    startParameterDisplay();
  }
  midiCCOut(CClfo1_delay, lfo1_delay);
}

FLASHMEM void updatelfo1_lfo2(bool announce) {
  if (announce) {
    showCurrentParameterPage("LFO1 to LFO2", String(lfo1_lfo2));
    startParameterDisplay();
  }
  midiCCOut(CClfo1_lfo2, lfo1_lfo2);
}

FLASHMEM void updatedco1_PW(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO1 PW", String(dco1_PW));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_PW, dco1_PW);
}

FLASHMEM void updatedco1_PWM_env(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO1 PWM Env", String(dco1_PWM_env));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_PWM_env, dco1_PWM_env);
}

FLASHMEM void updatedco1_PWM_lfo(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO1 PWM LFO", String(dco1_PWM_lfo));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_PWM_lfo, dco1_PWM_lfo);
}

FLASHMEM void updatedco1_pitch_env(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO1 Pitch Env", String(dco1_pitch_env));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_pitch_env, dco1_pitch_env);
}

FLASHMEM void updatedco1_pitch_lfo(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO1 Pitch LFO", String(dco1_pitch_lfo));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_pitch_lfo, dco1_pitch_lfo);
}

FLASHMEM void updatedco1_wave(bool announce) {
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
  midiCCOut(CCdco1_wave, dco1_wave);
}

FLASHMEM void updatedco1_range(bool announce) {
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
  midiCCOut(CCdco1_range, dco1_range);
}

FLASHMEM void updatedco1_tune(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO1 Tuning", String(dco1_tune));
    startParameterDisplay();
  }
  midiCCOut(CCdco1_tune, dco1_tune);
}

FLASHMEM void updatedco1_mode(bool announce) {
  if (announce) {
    switch (dco1_mode_str) {
      case 0:
        showCurrentParameterPage("DCO Sync", "Off");
        break;

      case 1:
        showCurrentParameterPage("DCO Sync", "Sync 1");
        break;

      case 2:
        showCurrentParameterPage("DCO Sync", "Sync 2");
        break;

      case 3:
        showCurrentParameterPage("DCO Sync", "Cross Mod");
        break;
    }
    startParameterDisplay();
  }
  midiCCOut(CCdco1_mode, dco1_mode);
}

FLASHMEM void updatelfo2_wave(bool announce) {
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
  midiCCOut(CClfo2_wave, lfo2_wave);
}

FLASHMEM void updatelfo2_rate(bool announce) {
  if (announce) {
    showCurrentParameterPage("LFO2 Rate", String(lfo2_rate));
    startParameterDisplay();
  }
  midiCCOut(CClfo2_rate, lfo2_rate);
}

FLASHMEM void updatelfo2_delay(bool announce) {
  if (announce) {
    showCurrentParameterPage("LFO2 Delay", String(lfo2_delay));
    startParameterDisplay();
  }
  midiCCOut(CClfo2_delay, lfo2_delay);
}

FLASHMEM void updatelfo2_lfo1(bool announce) {
  if (announce) {
    showCurrentParameterPage("LFO2 to LFO1", String(lfo2_lfo1));
    startParameterDisplay();
  }
  midiCCOut(CClfo2_lfo1, lfo2_lfo1);
}

FLASHMEM void updatedco2_PW(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 PW", String(dco2_PW));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_PW, dco2_PW);
}

FLASHMEM void updatedco2_PWM_env(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 PWM Env", String(dco2_PWM_env));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_PWM_env, dco2_PWM_env);
}

FLASHMEM void updatedco2_PWM_lfo(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 PWM LFO", String(dco2_PWM_lfo));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_PWM_lfo, dco2_PWM_lfo);
}

FLASHMEM void updatedco2_pitch_env(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 P. Env", String(dco2_pitch_env));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_pitch_env, dco2_pitch_env);
}

FLASHMEM void updatedco2_pitch_lfo(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 P. LFO", String(dco2_pitch_lfo));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_pitch_lfo, dco2_pitch_lfo);
}

FLASHMEM void updatedco2_wave(bool announce) {
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
  midiCCOut(CCdco2_wave, dco2_wave);
}

FLASHMEM void updatedco2_range(bool announce) {
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
  midiCCOut(CCdco2_range, dco2_range);
}

FLASHMEM void updatedco2_tune(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 Tuning", String(dco2_tune));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_tune, dco2_tune);
}

FLASHMEM void updatedco2_fine(bool announce) {
  if (announce) {
    showCurrentParameterPage("DCO2 Fine", String(dco2_fine));
    startParameterDisplay();
  }
  midiCCOut(CCdco2_fine, dco2_fine);
}

// FLASHMEM void updatevcoBPWM(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCO B PWM", vcoBPWM ? String(vcoBPWM) : "Off");
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoCPWM(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCO C PWM", vcoCPWM ? String(vcoCPWM) : "Off");
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updateXModDepth(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("XMOD Depth", XModDepth ? String(XModDepth) : "Off");
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoAInterval(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCO A Int", String(vcoAInterval));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoBInterval(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCO B Int", String(vcoBInterval));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoCInterval(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCO C Int", String(vcoCInterval));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoAFMDepth(bool announce) {
//   if (announce) {
//     if (vcoAFMDepth == 0) {
//       showCurrentParameterPage("A FM Depth", "Off");
//     } else {
//       showCurrentParameterPage("A FM Depth", String(vcoAFMDepth));
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoBFMDepth(bool announce) {
//   if (announce) {
//     if (vcoBFMDepth == 0) {
//       showCurrentParameterPage("B FM Depth", "Off");
//     } else {
//       showCurrentParameterPage("B FM Depth", String(vcoBFMDepth));
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoCFMDepth(bool announce) {
//   if (announce) {
//     if (vcoCFMDepth == 0) {
//       showCurrentParameterPage("C FM Depth", "Off");
//     } else {
//       showCurrentParameterPage("C FM Depth", String(vcoCFMDepth));
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoBDetune(bool announce) {
//   if (announce) {
//     int displayVal = vcoBDetune - 64;  // Center at 0
//     showCurrentParameterPage("VCO B Detune", String(displayVal));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoCDetune(bool announce) {
//   if (announce) {
//     int displayVal = vcoCDetune - 64;  // Center at 0
//     showCurrentParameterPage("VCO C Detune", String(displayVal));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatevcoAWave(bool announce) {
//   if (!vcoATable) {
//     if (announce) {
//       switch (vcoAWave) {
//         case 0:
//           showCurrentParameterPage("VCO A Wave", "Sine");
//           break;
//         case 1:
//           showCurrentParameterPage("VCO A Wave", "Saw");
//           break;
//         case 2:
//           showCurrentParameterPage("VCO A Wave", "Reverse Saw");
//           break;
//         case 3:
//           showCurrentParameterPage("VCO A Wave", "Square");
//           break;
//         case 4:
//           showCurrentParameterPage("VCO A Wave", "Triangle");
//           break;
//         case 5:
//           showCurrentParameterPage("VCO A Wave", "Pulse");
//           break;
//         case 6:
//           showCurrentParameterPage("VCO A Wave", "S & H");
//           break;
//       }
//       startParameterDisplay();
//     }
//   }
// }

// FLASHMEM void updatevcoBWave(bool announce) {
//   if (!vcoBTable) {
//     if (announce) {
//       switch (vcoBWave) {
//         case 0:
//           showCurrentParameterPage("VCO B Wave", "Sine");
//           break;
//         case 1:
//           showCurrentParameterPage("VCO B Wave", "Saw");
//           break;
//         case 2:
//           showCurrentParameterPage("VCO B Wave", "Reverse Saw");
//           break;
//         case 3:
//           showCurrentParameterPage("VCO B Wave", "Square");
//           break;
//         case 4:
//           showCurrentParameterPage("VCO B Wave", "Triangle");
//           break;
//         case 5:
//           showCurrentParameterPage("VCO B Wave", "Pulse");
//           break;
//         case 6:
//           showCurrentParameterPage("VCO B Wave", "S & H");
//           break;
//       }
//       startParameterDisplay();
//     }
//   }
// }

// FLASHMEM void updatevcoCWave(bool announce) {
//   if (!vcoCTable) {
//     if (announce) {
//       switch (vcoCWave) {
//         case 0:
//           showCurrentParameterPage("VCO C Wave", "Sine");
//           break;
//         case 1:
//           showCurrentParameterPage("VCO C Wave", "Saw");
//           break;
//         case 2:
//           showCurrentParameterPage("VCO C Wave", "Reverse Saw");
//           break;
//         case 3:
//           showCurrentParameterPage("VCO C Wave", "Square");
//           break;
//         case 4:
//           showCurrentParameterPage("VCO C Wave", "Triangle");
//           break;
//         case 5:
//           showCurrentParameterPage("VCO C Wave", "Pulse");
//           break;
//         case 6:
//           showCurrentParameterPage("VCO C Wave", "S & H");
//           break;
//       }
//       startParameterDisplay();
//     }
//   }
// }

// FLASHMEM void updatefilterCutoff(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCF Cutoff", String(filterCutoff));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatefilterResonance(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCF Res", String(filterResonance));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatefilterEGDepth(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("VCF EG Depth", String(filterEGDepth));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatefilterKeyTrack(bool announce) {
//   if (announce) {
//     if (filterKeyTrack == 0) {
//       showCurrentParameterPage("Filter Keytrack", "Off");
//     } else if (filterKeyTrack < 0) {
//       float positive_filterKeyTrack = abs(filterKeyTrack);
//       showCurrentParameterPage("Filter Keytrack", "- " + String(positive_filterKeyTrack));
//     } else {
//       showCurrentParameterPage("Filter Keytrack", "+ " + String(filterKeyTrack));
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatefilterLFODepth(bool announce) {
//   if (announce) {
//     if (filterLFODepth == 0) {
//       showCurrentParameterPage("LFO Depth", "Off");
//     } else if (filterLFODepth < 0) {
//       float positive_filterLFODepth = abs(filterLFODepth);
//       showCurrentParameterPage("LFO1 Depth", String(positive_filterLFODepth));
//     } else {
//       showCurrentParameterPage("LFO2 Depth", String(filterLFODepth));
//     }
//     startParameterDisplay();
//   }
// }



// FLASHMEM void updateeffectsMix(bool announce) {
//   if (announce) {
//     if (effectsMix == 0) {
//       showCurrentParameterPage("Effects Mix", "50/50");
//     } else if (effectsMix < 0) {
//       float positive_effectsMix = abs(effectsMix);
//       showCurrentParameterPage("Effects Dry", String(positive_effectsMix));
//     } else {
//       showCurrentParameterPage("Effects Wet", String(effectsMix));
//     }
//     startParameterDisplay();
//   }
// }

// void updateeffectsPot3SW(bool announce) {
//   // Ignore trigger if already mid-move
//   if (fast || slow) {
//     effectsPot3SW = false;  // clear trigger so it doesn't fire again
//     return;
//   }

//   if (effectsPot3SW) {  // Triggered by footswitch press
//     showCurrentParameterPage("Foot Switch", "Pressed");
//     startParameterDisplay();

//     if (!pot3ToggleState) {
//       if (effectPot3 < 127) {
//         slowpot3 = effectPot3;
//         fast = true;
//         slow = false;
//       } else {
//         fastpot3 = effectPot3;
//         slow = true;
//         fast = false;
//       }
//     } else {
//       if (effectPot3 < 127) {
//         fast = true;
//         slow = false;
//       } else {
//         slow = true;
//         fast = false;
//       }
//     }

//     pot3ToggleState = !pot3ToggleState;
//     effectsPot3SW = false;
//   }
// }

// void changeSpeed() {
//   if (slow) {
//     effectPot3--;
//     if (effectPot3 <= slowpot3) {
//       slow = false;
//     }
//   }

//   if (fast) {
//     effectPot3++;
//     if (effectPot3 >= fastpot3) {
//       fast = false;
//     }
//   }
// }

// FLASHMEM void updatevolumeLevel(bool announce) {
//   if (announce) {
//     showCurrentParameterPage("Volume", String(volumeLevel));
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatenoiseLevel(bool announce) {
//   if (announce) {
//     if (noiseLevel == 0) {
//       showCurrentParameterPage("Noise Level", "Off");
//     } else if (noiseLevel < 0) {
//       float positive_noiseLevel = abs(noiseLevel);
//       showCurrentParameterPage("Pink Level", String(positive_noiseLevel));
//     } else {
//       showCurrentParameterPage("White Level", String(noiseLevel));
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updateampLFODepth(bool announce) {
//   if (announce) {
//     if (ampLFODepth == 0) {
//       showCurrentParameterPage("LFO Depth", "Off");
//     } else if (ampLFODepth < 0) {
//       float positive_ampLFODepth = abs(ampLFODepth);
//       showCurrentParameterPage("LFO1 Depth", String(positive_ampLFODepth));
//     } else {
//       showCurrentParameterPage("LFO2 Depth", String(ampLFODepth));
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updatefilterPoleSwitch(bool announce) {
//   if (filterPoleSW == 1) {
//     if (announce) {
//       updatefilterType(1);
//     }
//     midiCCOut(CCfilterPoleSW, 127);

//   } else {
//     if (announce) {
//       updatefilterType(1);
//     }
//     midiCCOut(CCfilterPoleSW, 0);
//   }
// }

// FLASHMEM void updateegInvertSwitch(bool announce) {
//   if (egInvertSW == 1) {
//     if (announce) {
//       showCurrentParameterPage("EG Type", "Negative");
//       startParameterDisplay();
//     }
//     midiCCOut(CCegInvertSW, 127);

//   } else {
//     if (announce) {
//       showCurrentParameterPage("EG Type", "Positive");
//       startParameterDisplay();
//     }
//     midiCCOut(CCegInvertSW, 0);
//   }
// }

// FLASHMEM void updatefilterKeyTrackSwitch(bool announce) {
//   if (filterKeyTrackSW == 1) {
//     if (announce) {
//       showCurrentParameterPage("Key Track", "On");
//       startParameterDisplay();
//     }
//     midiCCOut(CCfilterKeyTrackSW, 127);

//   } else {
//     if (announce) {
//       showCurrentParameterPage("Key Track", "Off");
//       startParameterDisplay();
//     }
//     midiCCOut(CCfilterKeyTrackSW, 0);
//   }
// }

// FLASHMEM void updatefilterVelocitySwitch(bool announce) {
//   if (filterVelocitySW == 1) {
//     if (announce) {
//       showCurrentParameterPage("VCF Velocity", "On");
//       startParameterDisplay();
//     }
//     midiCCOut(CCfilterVelocitySW, 127);

//   } else {
//     if (announce) {
//       showCurrentParameterPage("VCF Velocity", "Off");
//       startParameterDisplay();
//     }
//     midiCCOut(CCfilterVelocitySW, 0);
//   }
// }

// FLASHMEM void updateampVelocitySwitch(bool announce) {
//   if (ampVelocitySW == 1) {
//     if (announce) {
//       showCurrentParameterPage("VCA Velocity", "On");
//       startParameterDisplay();
//     }
//     midiCCOut(CCampVelocitySW, 127);
//   } else {
//     if (announce) {
//       showCurrentParameterPage("VCA Velocity", "Off");
//       startParameterDisplay();
//     }
//     midiCCOut(CCampVelocitySW, 0);
//   }
// }

// FLASHMEM void updateFMSyncSwitch(bool announce) {
//   if (FMSyncSW == 1) {
//     showCurrentParameterPage("FM Group", "On");
//   } else {
//     showCurrentParameterPage("FM Group", "Off");
//   }
//   startParameterDisplay();
// }

// FLASHMEM void updatePWSyncSwitch(bool announce) {
//   if (PWSyncSW == 1) {
//     showCurrentParameterPage("PW Group", "On");
//   } else {
//     showCurrentParameterPage("PW Group", "Off");
//   }
//   startParameterDisplay();
// }

// FLASHMEM void updatePWMSyncSwitch(bool announce) {
//   if (PWMSyncSW == 1) {
//     showCurrentParameterPage("PWM Group", "On");
//   } else {
//     showCurrentParameterPage("PWM Group", "Off");
//   }
//   startParameterDisplay();
// }

// FLASHMEM void updatemultiSwitch(bool announce) {
//   if (multiSW == 1) {
//     if (announce) {
//       showCurrentParameterPage("Retrigger", "On");
//       startParameterDisplay();
//     }
//   } else {
//     if (announce) {
//       showCurrentParameterPage("Retrigger", "Off");
//       startParameterDisplay();
//     }
//   }
// }

// FLASHMEM void updatefilterType(bool announce) {
//   switch (filterType) {
//     case 0:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "3P LowPass");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "4P LowPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 0);
//       break;

//     case 1:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "1P LowPass");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "2P LowPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 1);
//       break;

//     case 2:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "3P HP + 1P LP");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "4P HighPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 2);
//       break;

//     case 3:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "1P HP + 1P LP");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "2P HighPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 3);
//       break;

//     case 4:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "2P HP + 1P LP");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "4P BandPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 4);
//       break;

//     case 5:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "2P BP + 1P LP");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "2P BandPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 5);
//       break;

//     case 6:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "3P AP + 1P LP");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "3P AllPass");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 6);
//       break;

//     case 7:
//       if (filterPoleSW == 1) {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "2P Notch + LP");
//         }
//       } else {
//         if (announce) {
//           showCurrentParameterPage("Filter Type", "Notch");
//         }
//       }
//       startParameterDisplay();
//       midiCCOut(CCfilterType, 7);
//       break;
//   }
// }

// FLASHMEM void updatevcoAOctave(bool announce) {
//   if (announce) {
//     switch (vcoAOctave) {
//       case 0:
//         showCurrentParameterPage("VCOA Octave", "16 Foot");
//         break;
//       case 1:
//         showCurrentParameterPage("VCOA Octave", "8 Foot");
//         break;
//       case 2:
//         showCurrentParameterPage("VCOA Octave", "4 Foot");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vcoAOctave) {
//     case 0:
//       octave = 0.5;

//       break;
//     case 1:
//       octave = 1.0;

//       break;
//     case 2:
//       octave = 2.0;

//       break;
//   }
//   pitchDirty = true;
// }

// FLASHMEM void updatevcoBOctave(bool announce) {
//   if (announce) {
//     switch (vcoBOctave) {
//       case 0:
//         showCurrentParameterPage("VCOB Octave", "16 Foot");
//         break;
//       case 1:
//         showCurrentParameterPage("VCOB Octave", "8 Foot");
//         break;
//       case 2:
//         showCurrentParameterPage("VCOB Octave", "4 Foot");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vcoBOctave) {
//     case 0:
//       octaveB = 0.5;

//       break;
//     case 1:
//       octaveB = 1.0;

//       break;
//     case 2:
//       octaveB = 2.0;

//       break;
//   }
//   pitchDirty = true;
// }

// FLASHMEM void updatevcoCOctave(bool announce) {
//   if (announce) {
//     switch (vcoCOctave) {
//       case 0:
//         showCurrentParameterPage("VCOC Octave", "16 Foot");
//         break;
//       case 1:
//         showCurrentParameterPage("VCOC Octave", "8 Foot");
//         break;
//       case 2:
//         showCurrentParameterPage("VCOC Octave", "4 Foot");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (vcoCOctave) {
//     case 0:
//       octaveC = 0.5;

//       break;
//     case 1:
//       octaveC = 1.0;

//       break;
//     case 2:
//       octaveC = 2.0;

//       break;
//   }
//   pitchDirty = true;
// }

// FLASHMEM void updateplayModeSW(bool announce) {
//   if (announce) {
//     switch (playModeSW) {
//       case 0:
//         showCurrentParameterPage("Play Mode", "Polyphonic");
//         break;
//       case 1:
//         showCurrentParameterPage("Play Mode", "Monophonic");
//         break;
//       case 2:
//         showCurrentParameterPage("Play Mode", "Unison");
//         break;
//     }
//     startParameterDisplay();
//   }
//   switch (playModeSW) {
//     case 0:

//       allNotesOff();
//       break;
//     case 1:

//       allNotesOff();
//       break;
//     case 2:

//       allNotesOff();
//       break;
//   }
//   updatenotePrioritySW(0);
// }

// FLASHMEM void updatenotePrioritySW(bool announce) {
//   if (playModeSW != 0) {
//     if (announce) {
//       switch (notePrioritySW) {
//         case 0:
//           showCurrentParameterPage("Note Priority", "Bottom");
//           break;
//         case 1:
//           showCurrentParameterPage("Note Priority", "Top");
//           break;
//         case 2:
//           showCurrentParameterPage("Note Priority", "Last");
//           break;
//       }
//       startParameterDisplay();
//     }
//     switch (notePrioritySW) {
//       case 0:

//         break;
//       case 1:

//         break;
//       case 2:

//         break;
//     }
//   }
// }

// FLASHMEM void updateLFO1Wave(bool announce) {
//   if (announce) {
//     switch (LFO1Wave) {
//       case 0:
//         showCurrentParameterPage("LFO1 Wave", "Sine");
//         break;
//       case 1:
//         showCurrentParameterPage("LFO1 Wave", "Saw");
//         break;
//       case 2:
//         showCurrentParameterPage("LFO1 Wave", "Reverse Saw");
//         break;
//       case 3:
//         showCurrentParameterPage("LFO1 Wave", "Square");
//         break;
//       case 4:
//         showCurrentParameterPage("LFO1 Wave", "Triangle");
//         break;
//       case 5:
//         showCurrentParameterPage("LFO1 Wave", "Pulse");
//         break;
//       case 6:
//         showCurrentParameterPage("LFO1 Wave", "S & H");
//         break;
//     }
//     startParameterDisplay();
//   }
// }

// FLASHMEM void updateLFO2Wave(bool announce) {
//   if (announce) {
//     switch (LFO2Wave) {
//       case 0:
//         showCurrentParameterPage("LFO2 Wave", "Sine");
//         break;
//       case 1:
//         showCurrentParameterPage("LFO2 Wave", "Saw");
//         break;
//       case 2:
//         showCurrentParameterPage("LFO2 Wave", "Reverse Saw");
//         break;
//       case 3:
//         showCurrentParameterPage("LFO2 Wave", "Square");
//         break;
//       case 4:
//         showCurrentParameterPage("LFO2 Wave", "Triangle");
//         break;
//       case 5:
//         showCurrentParameterPage("LFO2 Wave", "Pulse");
//         break;
//       case 6:
//         showCurrentParameterPage("LFO2 Wave", "S & H");
//         break;
//     }
//     startParameterDisplay();
//   }
// }

// void RotaryEncoderChanged(bool clockwise, int id) {

//   if (!accelerate) {
//     speed = 1;
//   } else {
//     speed = getEncoderSpeed(id);
//   }

//   if (!clockwise) {
//     speed = -speed;
//   }

//   switch (id) {

//     case 1:
//       ampLFODepth = (ampLFODepth + speed);
//       ampLFODepth = constrain(ampLFODepth, -127, 127);
//       updateampLFODepth(1);
//       break;

//     case 2:
//       volumeLevel = (volumeLevel + speed);
//       volumeLevel = constrain(volumeLevel, 0, 255);
//       updatevolumeLevel(1);
//       break;

//     case 3:
//       MWDepth = (MWDepth + speed);
//       MWDepth = constrain(MWDepth, 0, 127);
//       updateMWDepth(1);
//       break;


//     case 7:
//       if (!clockwise) {
//         PBDepth--;
//       } else {
//         PBDepth++;
//       }
//       PBDepth = constrain(PBDepth, 0, 12);
//       updatePBDepth(1);
//       break;

//     case 8:
//       LFO1Rate = (LFO1Rate + speed);
//       LFO1Rate = constrain(LFO1Rate, 0, 127);
//       updateLFO1Rate(1);
//       break;

//     case 9:
//       LFO2Rate = (LFO2Rate + speed);
//       LFO2Rate = constrain(LFO2Rate, 0, 127);
//       updateLFO2Rate(1);
//       break;

//     case 10:
//       LFO1Delay = (LFO1Delay + speed);
//       LFO1Delay = constrain(LFO1Delay, 0, 127);
//       updateLFO1Delay(1);
//       break;

//     case 11:
//       ampAttack = (ampAttack + speed);
//       ampAttack = constrain(ampAttack, 0, 127);
//       updateampAttack(1);
//       break;

//     case 12:
//       ampDecay = (ampDecay + speed);
//       ampDecay = constrain(ampDecay, 0, 127);
//       updateampDecay(1);
//       break;

//     case 13:
//       ampSustain = (ampSustain + speed);
//       ampSustain = constrain(ampSustain, 0, 100);
//       updateampSustain(1);
//       break;

//     case 14:
//       ATDepth = (ATDepth + speed);
//       ATDepth = constrain(ATDepth, 0, 127);
//       updateATDepth(1);
//       break;

//     case 15:
//       ampRelease = (ampRelease + speed);
//       ampRelease = constrain(ampRelease, 0, 127);
//       updateampRelease(1);
//       break;

//     case 16:
//       filterAttack = (filterAttack + speed);
//       filterAttack = constrain(filterAttack, 0, 127);
//       updatefilterAttack(1);
//       break;

//     case 17:
//       filterDecay = (filterDecay + speed);
//       filterDecay = constrain(filterDecay, 0, 127);
//       updatefilterDecay(1);
//       break;

//     case 18:
//       filterSustain = (filterSustain + speed);
//       filterSustain = constrain(filterSustain, 0, 100);
//       updatefilterSustain(1);
//       break;

//     case 19:
//       filterRelease = (filterRelease + speed);
//       filterRelease = constrain(filterRelease, 0, 127);
//       updatefilterRelease(1);
//       break;

//     case 20:
//       pitchAttack = (pitchAttack + speed);
//       pitchAttack = constrain(pitchAttack, 0, 127);
//       updatepitchAttack(1);
//       break;

//     case 21:
//       filterResonance = (filterResonance + speed);
//       filterResonance = constrain(filterResonance, 0, 255);
//       updatefilterResonance(1);
//       break;

//     case 22:
//       filterKeyTrack = (filterKeyTrack + speed);
//       filterKeyTrack = constrain(filterKeyTrack, -127, 127);
//       updatefilterKeyTrack(1);
//       break;

//     case 23:
//       noiseLevel = (noiseLevel + speed);
//       noiseLevel = constrain(noiseLevel, -127, 127);
//       updatenoiseLevel(1);
//       break;

//     case 24:
//       pitchDecay = (pitchDecay + speed);
//       pitchDecay = constrain(pitchDecay, 0, 127);
//       updatepitchDecay(1);
//       break;

//     case 25:
//       pitchSustain = (pitchSustain + speed);
//       pitchSustain = constrain(pitchSustain, 0, 100);
//       updatepitchSustain(1);
//       break;

//     case 26:
//       pitchRelease = (pitchRelease + speed);
//       pitchRelease = constrain(pitchRelease, 0, 127);
//       updatepitchRelease(1);
//       break;

//     case 27:
//       filterCutoff = (filterCutoff + speed);
//       filterCutoff = constrain(filterCutoff, 0, 255);
//       updatefilterCutoff(1);
//       break;

//     case 28:
//       filterEGDepth = (filterEGDepth + speed);
//       filterEGDepth = constrain(filterEGDepth, 0, 255);
//       updatefilterEGDepth(1);
//       break;

//     case 29:
//       vcoCFMDepth = (vcoCFMDepth + speed);
//       vcoCFMDepth = constrain(vcoCFMDepth, 0, 255);
//       updatevcoCFMDepth(1);
//       break;

//     case 30:
//       vcoBDetune = (vcoBDetune + speed);
//       vcoBDetune = constrain(vcoBDetune, 0, 127);
//       updatevcoBDetune(1);
//       break;

//     case 31:
//       vcoCDetune = (vcoCDetune + speed);
//       vcoCDetune = constrain(vcoCDetune, 0, 127);
//       updatevcoCDetune(1);
//       break;

//     case 32:
//       filterLFODepth = (filterLFODepth + speed);
//       filterLFODepth = constrain(filterLFODepth, -127, 127);
//       updatefilterLFODepth(1);
//       break;

//     case 33:
//       vcoAFMDepth = (vcoAFMDepth + speed);
//       vcoAFMDepth = constrain(vcoAFMDepth, 0, 255);
//       updatevcoAFMDepth(1);
//       break;

//     case 34:
//       vcoBFMDepth = (vcoBFMDepth + speed);
//       vcoBFMDepth = constrain(vcoBFMDepth, 0, 255);
//       updatevcoBFMDepth(1);
//       break;

//     case 35:
//       effectsMix = (effectsMix + speed);
//       effectsMix = constrain(effectsMix, -127, 127);
//       updateeffectsMix(1);
//       break;

//     case 36:
//       vcoALevel = (vcoALevel + speed);
//       vcoALevel = constrain(vcoALevel, 0, 255);
//       updatevcoALevel(1);
//       break;

//     case 37:
//       vcoBLevel = (vcoBLevel + speed);
//       vcoBLevel = constrain(vcoBLevel, 0, 255);
//       updatevcoBLevel(1);
//       break;

//     case 38:
//       vcoCLevel = (vcoCLevel + speed);
//       vcoCLevel = constrain(vcoCLevel, 0, 255);
//       updatevcoCLevel(1);
//       break;

//     case 39:
//       if (!vcoATable) {
//         vcoAPW = (vcoAPW + speed);
//         vcoAPW = constrain(vcoAPW, 0, 255);
//         updatevcoAPW(1);
//       } else {
//         if (!clockwise) {
//           vcoAWaveBank--;
//         } else {
//           vcoAWaveBank++;
//         }
//         vcoAWaveBank = constrain(vcoAWaveBank, 1, BANKS);
//         vcoAWaveNumber = 1;
//         showCurrentParameterPage("OscA Bank", String(Tablenames[vcoAWaveBank - 1]));
//         startParameterDisplay();
//         updatevcoAWave(0);
//       }
//       break;

//     case 40:
//       if (!vcoBTable) {
//         vcoBPW = (vcoBPW + speed);
//         vcoBPW = constrain(vcoBPW, 0, 255);
//         updatevcoBPW(1);
//       } else {
//         if (!clockwise) {
//           vcoBWaveBank--;
//         } else {
//           vcoBWaveBank++;
//         }
//         vcoBWaveBank = constrain(vcoBWaveBank, 1, BANKS);
//         vcoBWaveNumber = 1;
//         showCurrentParameterPage("OscB Bank", String(Tablenames[vcoBWaveBank - 1]));
//         startParameterDisplay();
//         updatevcoBWave(1);
//       }
//       break;

//     case 41:
//       if (!vcoCTable) {
//         vcoCPW = (vcoCPW + speed);
//         vcoCPW = constrain(vcoCPW, 0, 255);
//         updatevcoCPW(1);
//       } else {
//         if (!clockwise) {
//           vcoCWaveBank--;
//         } else {
//           vcoCWaveBank++;
//         }
//         vcoCWaveBank = constrain(vcoCWaveBank, 1, BANKS);
//         vcoCWaveNumber = 1;
//         showCurrentParameterPage("OscC Bank", String(Tablenames[vcoCWaveBank - 1]));
//         startParameterDisplay();
//         updatevcoCWave(1);
//       }
//       break;

//     case 42:
//       vcoAPWM = (vcoAPWM + speed);
//       vcoAPWM = constrain(vcoAPWM, 0, 255);
//       updatevcoAPWM(1);
//       break;

//     case 43:
//       vcoBPWM = (vcoBPWM + speed);
//       vcoBPWM = constrain(vcoBPWM, 0, 255);
//       updatevcoBPWM(1);
//       break;

//     case 44:
//       vcoCPWM = (vcoCPWM + speed);
//       vcoCPWM = constrain(vcoCPWM, 0, 255);
//       updatevcoCPWM(1);
//       break;

//     case 45:
//       if (!vcoATable) {
//         if (!clockwise) {
//           vcoAWave--;
//         } else {
//           vcoAWave++;
//         }
//         vcoAWave = constrain(vcoAWave, 0, 6);
//         updatevcoAWave(1);
//       } else {
//         vcoAWaveNumber = (vcoAWaveNumber + speed);
//         int bankIndex = vcoAWaveBank - 1;  // If your banks start at 1 instead
//         int maxWaves = tablesInBank[bankIndex];
//         vcoAWaveNumber = constrain(vcoAWaveNumber, 1, maxWaves);
//         updatevcoAWave(1);
//       }
//       break;

//     case 46:
//       if (!vcoBTable) {
//         if (!clockwise) {
//           vcoBWave--;
//         } else {
//           vcoBWave++;
//         }
//         vcoBWave = constrain(vcoBWave, 0, 6);
//         updatevcoBWave(1);
//       } else {
//         vcoBWaveNumber = (vcoBWaveNumber + speed);
//         int bankIndex = vcoBWaveBank - 1;  // If your banks start at 1 instead
//         int maxWaves = tablesInBank[bankIndex];
//         vcoBWaveNumber = constrain(vcoBWaveNumber, 1, maxWaves);
//         updatevcoBWave(1);
//       }
//       break;

//     case 47:
//       if (!vcoCTable) {
//         if (!clockwise) {
//           vcoCWave--;
//         } else {
//           vcoCWave++;
//         }
//         vcoCWave = constrain(vcoCWave, 0, 6);
//         updatevcoCWave(1);
//       } else {
//         vcoCWaveNumber = (vcoCWaveNumber + speed);
//         int bankIndex = vcoCWaveBank - 1;  // If your banks start at 1 instead
//         int maxWaves = tablesInBank[bankIndex];
//         vcoCWaveNumber = constrain(vcoCWaveNumber, 1, maxWaves);
//         updatevcoCWave(1);
//       }
//       break;

//     case 48:
//       if (!clockwise) {
//         vcoAInterval--;
//       } else {
//         vcoAInterval++;
//       }
//       vcoAInterval = constrain(vcoAInterval, -12, 12);
//       updatevcoAInterval(1);
//       break;

//     case 49:
//       if (!clockwise) {
//         vcoBInterval--;
//       } else {
//         vcoBInterval++;
//       }
//       vcoBInterval = constrain(vcoBInterval, -12, 12);
//       updatevcoBInterval(1);
//       break;

//     case 50:
//       if (!clockwise) {
//         vcoCInterval--;
//       } else {
//         vcoCInterval++;
//       }
//       vcoCInterval = constrain(vcoCInterval, -12, 12);
//       updatevcoCInterval(1);
//       break;

//     case 51:
//       XModDepth = (XModDepth + speed);
//       XModDepth = constrain(XModDepth, 0, 255);
//       updateXModDepth(1);
//       break;

//     default:
//       break;
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

  //Patchname
  updatePatchname();

  // updatevcoAWave(0);
  // updatevcoBWave(0);
  // updatevcoCWave(0);
  // updatevcoAPW(0);
  // updatevcoBPW(0);
  // updatevcoCPW(0);
  // updatevcoBDetune(0);
  // updatevcoCDetune(0);
  // updatevcoALevel(0);
  // updatevcoBLevel(0);
  // updatevcoCLevel(0);
  // updatevcoAInterval(0);
  // updatevcoBInterval(0);
  // updatevcoCInterval(0);
  // updatevcoAOctave(0);
  // updatevcoBOctave(0);
  // updatevcoCOctave(0);
  // updatefilterCutoff(0);
  // updatefilterResonance(0);
  // updatefilterEGDepth(0);
  // updatefilterKeyTrack(0);
  // updatefilterKeyTrackSwitch(0);
  // updatefilterVelocitySwitch(0);
  // updateampVelocitySwitch(0);
  // updatefilterLFODepth(0);
  // updatefilterPoleSwitch(0);
  // updatefilterType(0);
  // updateampLFODepth(0);
  // updatepitchAttack(0);
  // updatepitchDecay(0);
  // updatepitchSustain(0);
  // updatepitchRelease(0);
  // updateampAttack(0);
  // updateampDecay(0);
  // updateampSustain(0);
  // updateampRelease(0);
  // updatefilterAttack(0);
  // updatefilterDecay(0);
  // updatefilterSustain(0);
  // updatefilterRelease(0);
  // updateLFO1Wave(0);
  // updateLFO2Wave(0);
  // updateLFO1Rate(0);
  // updateLFO1Delay(0);
  // updateLFO2Rate(0);
  // updateXModDepth(0);
  // updatenoiseLevel(0);
  // updateeffectsMix(0);
  // updatevolumeLevel(0);
  // updateMWDepth(0);
  // updatePBDepth(0);
  // updateATDepth(0);

  // updatemultiSwitch(0);
  // updateegInvertSwitch(0);
  // updateplayModeSW(0);

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
}

void mainButtonChanged(Button *btn, bool released) {

  switch (btn->id) {
    case OSC1_OCT_BUTTON:

      break;

    case OSC1_WAVE_BUTTON:

      break;

    case OSC1_SUB_BUTTON:

      break;

    case OSC2_WAVE_BUTTON:

      break;

    case OSC2_XMOD_BUTTON:

      break;

    case OSC2_EG_BUTTON:

      break;

    case LFO1_WAVE_BUTTON:

      break;

    case LFO2_WAVE_BUTTON:

      break;

    case LFO3_WAVE_BUTTON:

      break;

    case ENV_SEL_BUTTON:

      break;

    case LFO_SEL_BUTTON:

      break;

    case OSC1_LEV_SW:

      break;

    case OSC2_DET_SW:

      break;

    case OSC2_LEV_SW:
      if (!released) {
      }
      break;

    case OSC2_EG_SW:
      if (!released) {
      }
      break;

    case VCF_EG_SW:
      if (!released) {
      }
      break;

    case VCF_KEYF_SW:
      if (!released) {
      }
      break;

    case VCF_VEL_SW:
      if (!released) {
      }
      break;

    case VCA_VEL_SW:
      if (!released) {
      }
      break;
  }
}

void checkMux() {

  mux1Read = adc->adc0->analogRead(MUX1_S);
  mux2Read = adc->adc0->analogRead(MUX2_S);
  mux3Read = adc->adc1->analogRead(MUX3_S);
  mux4Read = adc->adc1->analogRead(MUX4_S);

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

    // switch (muxInput) {
    //   case MUX3_REVERB_MIX:
    //     myControlChange(midiChannel, CCreverbLevel, mux3Read);
    //     break;
    //   case MUX3_REVERB_DAMP:
    //     myControlChange(midiChannel, CCreverbDamp, mux3Read);
    //     break;
    //   case MUX3_REVERB_DECAY:
    //     myControlChange(midiChannel, CCreverbDecay, mux3Read);
    //     break;
    //   case MUX3_DRIFT:
    //     myControlChange(midiChannel, CCdriftAmount, mux3Read);
    //     break;
    //   case MUX3_VCA_VELOCITY:
    //     myControlChange(midiChannel, CCvcaVelocity, mux3Read);
    //     break;
    //   case MUX3_VCA_RELEASE:
    //     myControlChange(midiChannel, CCvcaRelease, mux3Read);
    //     break;
    //   case MUX3_VCA_SUSTAIN:
    //     myControlChange(midiChannel, CCvcaSustain, mux3Read);
    //     break;
    //   case MUX3_VCA_DECAY:
    //     myControlChange(midiChannel, CCvcaDecay, mux3Read);
    //     break;
    //   case MUX3_VCF_SUSTAIN:
    //     myControlChange(midiChannel, CCvcfSustain, mux3Read);
    //     break;
    //   case MUX3_CONTOUR_AMOUNT:
    //     myControlChange(midiChannel, CCvcfContourAmount, mux3Read);
    //     break;
    //   case MUX3_VCF_RELEASE:
    //     myControlChange(midiChannel, CCvcfRelease, mux3Read);
    //     break;
    //   case MUX3_KB_TRACK:
    //     myControlChange(midiChannel, CCkbTrack, mux3Read);
    //     break;
    //   case MUX3_MASTER_VOLUME:
    //     myControlChange(midiChannel, CCmasterVolume, mux3Read);
    //     break;
    //   case MUX3_VCF_VELOCITY:
    //     myControlChange(midiChannel, CCvcfVelocity, mux3Read);
    //     break;
    //   case MUX3_MASTER_TUNE:
    //     myControlChange(midiChannel, CCmasterTune, mux3Read);
    //     break;
    // }
  }

  if (mux4Read > (mux4ValuesPrev[muxInput] + QUANTISE_FACTOR) || mux4Read < (mux4ValuesPrev[muxInput] - QUANTISE_FACTOR)) {
    mux4ValuesPrev[muxInput] = mux4Read;
    mux4Read = (mux4Read >> resolutionFrig);  // Change range to 0-127

    // switch (muxInput) {
    //   case MUX1_GLIDE:
    //     myControlChange(midiChannel, CCglide, mux1Read);
    //     break;
    //   case MUX1_UNISON_DETUNE:
    //     myControlChange(midiChannel, CCuniDetune, mux1Read);
    //     break;
    //   case MUX1_BEND_DEPTH:
    //     myControlChange(midiChannel, CCbendDepth, mux1Read);
    //     break;
    //   case MUX1_LFO_OSC3:
    //     myControlChange(midiChannel, CClfoOsc3, mux1Read);
    //     break;
    //   case MUX1_LFO_FILTER_CONTOUR:
    //     myControlChange(midiChannel, CClfoFilterContour, mux1Read);
    //     break;
    //   case MUX1_ARP_RATE:
    //     myControlChange(midiChannel, CCarpSpeed, mux1Read);
    //     break;
    //   case MUX1_PHASER_RATE:
    //     myControlChange(midiChannel, CCphaserSpeed, mux1Read);
    //     break;
    //   case MUX1_PHASER_DEPTH:
    //     myControlChange(midiChannel, CCphaserDepth, mux1Read);
    //     break;
    //   case MUX1_LFO_INITIAL_AMOUNT:
    //     myControlChange(midiChannel, CClfoInitialAmount, mux1Read);
    //     break;
    //   case MUX1_LFO_MOD_WHEEL_AMOUNT:
    //     myControlChange(midiChannel, CCmodWheel, mux1Read);
    //     break;
    //   case MUX1_LFO_RATE:
    //     myControlChange(midiChannel, CClfoSpeed, mux1Read);
    //     break;
    //   case MUX1_OSC2_FREQUENCY:
    //     myControlChange(midiChannel, CCosc2Frequency, mux1Read);
    //     break;
    //   case MUX1_OSC2_PW:
    //     myControlChange(midiChannel, CCosc2PW, mux1Read);
    //     break;
    //   case MUX1_OSC1_PW:
    //     myControlChange(midiChannel, CCosc1PW, mux1Read);
    //     break;
    //   case MUX1_OSC3_FREQUENCY:
    //     myControlChange(midiChannel, CCosc3Frequency, mux1Read);
    //     break;
    //   case MUX1_OSC3_PW:
    //     myControlChange(midiChannel, CCosc3PW, mux1Read);
    //     break;
    // }
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