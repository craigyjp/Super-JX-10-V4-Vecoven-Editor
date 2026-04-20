#include "SettingsService.h"


// ─────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────
void settingsMIDICh(int index, const char *value);
void settingsMIDIOutCh(int index, const char *value);
void settingsEncoderDir(int index, const char *value);
void settingsUpdateParams(int index, const char *value);

int currentIndexMIDICh();
int currentIndexMIDIOutCh();
int currentIndexEncoderDir();
int currentIndexUpdateParams();


void settingsMIDICh(int index, const char *value) {
  if (strcmp(value, "ALL") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void settingsMIDIOutCh(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    midiOutCh = 0;
  } else {
    midiOutCh = atoi(value);
  }
  storeMidiOutCh(midiOutCh);
}

void settingsEncoderDir(int index, const char *value) {
  if (strcmp(value, "Type 1") == 0) {
    encCW = true;
  } else {
    encCW = false;
  }
  storeEncoderDir(encCW ? 1 : 0);
}

void settingsUpdateParams(int index, const char *value) {
  if (strcmp(value, "Send Params") == 0) {
    updateParams = true;
  } else {
    updateParams = false;
  }
  storeUpdateParams(updateParams ? 1 : 0);
}

// ─────────────────────────────────────────────
// Current index functions
// ─────────────────────────────────────────────

int currentIndexMIDICh() {
  return getMIDIChannel();
}

int currentIndexMIDIOutCh() {
  return getMIDIOutCh();
}

int currentIndexEncoderDir() {
  return getEncoderDir() ? 0 : 1;
}

int currentIndexUpdateParams() {
  return getUpdateParams() ? 1 : 0;
}

// ─────────────────────────────────────────────
// Setup — build labels first, then append
// ─────────────────────────────────────────────
static const char* midiChValues[]     = { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* midiOutChValues[]  = { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* encoderValues[]    = { "Type 1", "Type 2", "\0" };
static const char* midiParamValues[]  = { "Off", "Send Params", "\0" };

void setUpSettings() {

  settings::append({ "MIDI Ch.",     midiChValues,      17, settingsMIDICh,      currentIndexMIDICh });
  settings::append({ "MIDI Out Ch.", midiOutChValues,   17, settingsMIDIOutCh,   currentIndexMIDIOutCh });
  settings::append({ "Encoder",      encoderValues,      2, settingsEncoderDir,  currentIndexEncoderDir });
  settings::append({ "MIDI Params",  midiParamValues,    2, settingsUpdateParams,currentIndexUpdateParams });
}
