#include "MIDIService.h"

// ============================================================================
// MIDI.h — MIDI menu
//
// Duplicates the three MIDI entries currently in Settings so they are
// accessible from the new MIDI menu.  The underlying handlers call the same
// EEPROM store functions; either menu will update the other's value the
// next time its refresh_value() fires.
// ============================================================================

// Reuse the same label arrays style as Settings.h
static const char* midiMidiChValues[]     = { "All", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* midiMidiOutChValues[]  = { "Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "\0" };
static const char* midiMidiParamValues[]  = { "Off", "Send Params", "\0" };

// Forward declarations
void midiMIDICh(int index, const char *value);
void midiMIDIOutCh(int index, const char *value);
void midiUpdateParams(int index, const char *value);

int idxMIDICh();
int idxMIDIOutCh();
int idxUpdateParams();

// Handlers
void midiMIDICh(int index, const char *value) {
  if (strcmp(value, "All") == 0) {
    midiChannel = MIDI_CHANNEL_OMNI;
  } else {
    midiChannel = atoi(value);
  }
  storeMidiChannel(midiChannel);
}

void midiMIDIOutCh(int index, const char *value) {
  if (strcmp(value, "Off") == 0) {
    midiOutCh = 0;
  } else {
    midiOutCh = atoi(value);
  }
  storeMidiOutCh(midiOutCh);
}

void midiUpdateParams(int index, const char *value) {
  if (strcmp(value, "Send Params") == 0) {
    updateParams = true;
  } else {
    updateParams = false;
  }
  storeUpdateParams(updateParams ? 1 : 0);
}

// currentIndex callbacks
int idxMIDICh()        { return getMIDIChannel(); }
int idxMIDIOutCh()     { return getMIDIOutCh(); }
int idxUpdateParams()  { return getUpdateParams() ? 1 : 0; }

// ============================================================================
// setUpMIDI()
// ============================================================================

void setUpMIDI() {
  midimenu::append({ "MIDI Ch.",     midiMidiChValues,    17, midiMIDICh,        idxMIDICh       });
  midimenu::append({ "MIDI Out Ch.", midiMidiOutChValues, 17, midiMIDIOutCh,     idxMIDIOutCh    });
  midimenu::append({ "MIDI Params",  midiMidiParamValues,  2, midiUpdateParams,  idxUpdateParams });
}
