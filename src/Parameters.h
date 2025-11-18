//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = MIDI_CHANNEL_OMNI;  //(EEPROM)
byte midiOutCh = 1; 

struct VoiceAndNote {
  int note;
  int velocity;
  long timeOn;
};

struct VoiceAndNote voices[NO_OF_VOICES] = {
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 },
  { -1, -1, 0 }
};

bool voiceOn[NO_OF_VOICES] = { false, false, false, false, false, false, false, false };

int prevNote = 0;  //Initialised to middle value
bool notes[88] = { 0 }, initial_loop = 1;
int8_t noteOrder[80] = { 0 }, orderIndx = { 0 };
int noteMsg;

int note1freq;
int note2freq;
int note3freq;
int note4freq;
int note5freq;
int note6freq;
int note7freq;
int note8freq;


int patchNo = 1;               //Current patch no
int voiceToReturn = -1;        //Initialise
long earliestTime = millis();  //For voice allocation - initialise to now

//Delayed LFO
int numberOfNotes = 0;
int oldnumberOfNotes = 0;
unsigned long previousMillis = 0;
unsigned long interval = 1; //10 seconds
long delaytime  = 0;
int LFODelayGo = 0;
bool LFODelayGoA = false;
bool LFODelayGoB = false;
bool LFODelayGoC = false;

int resolutionFrig = 1;

uint32_t noteAgeCounter = 1;
int nextVoiceRR = 0;
uint8_t voiceNote[9];  // per-voice note index (0..127 or into noteFreqs)

//Unison Detune
byte unidetune = 0;
byte oldunidetune = 0;
byte uniNotes = 0;

float voiceDetune[8] = { 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000, 1.000 };

// adding encoders
bool rotaryEncoderChanged(int id, bool clockwise, int speed);
#define NUM_ENCODERS 51
unsigned long lastTransition[NUM_ENCODERS + 1];
unsigned long lastDisplayTriggerTime = 0;
bool waitingToUpdate = false;
const unsigned long displayTimeout = 5000;  // e.g. 5 seconds

int MIDIThru = midi::Thru::Off;  //(EEPROM)
String patchName = INITPATCHNAME;
bool encCW = true;  //This is to set the encoder to increment when turned CW - Settings Option
bool announce = true;
byte accelerate = 1;
int speed = 1;
bool updateParams = false;  //(EEPROM)
int old_value = 0;
int old_param_offset = 0;

// ---- UI controls (0..255 unless noted) ----
uint8_t uiCutoff      = 128;  // base cutoff
uint8_t uiKeytrackAmt = 128;  // keytrack depth
uint8_t uiEnvAmt      = 128;  // filter env depth
uint8_t uiLfoAmt      = 0;    // LFO depth to filter

// Dirty flag + lightweight throttle
volatile bool pitchDirty = true;
elapsedMillis msSincePitchUpdate;

// New JX parameters
int lfo1_wave;
int lfo1_wave_str;

int lfo1_rate;
int lfo1_delay;
int lfo1_lfo2;

int lfo2_wave;
int lfo2_wave_str;

int lfo2_rate;
int lfo2_delay;
int lfo2_lfo1;

int dco1_PW;
int dco1_PWM_env;
int dco1_PWM_lfo;
int dco1_pitch_env;
int dco1_pitch_lfo;
int dco1_wave;
int dco1_wave_str;
int dco1_range;
int dco1_range_str;
int dco1_tune;
int dco1_mode;
int dco1_mode_str;

int dco2_PW;
int dco2_PWM_env;
int dco2_PWM_lfo;
int dco2_pitch_env;
int dco2_pitch_lfo;
int dco2_wave;
int dco2_wave_str;
int dco2_range;
int dco2_range_str;
int dco2_tune;
int dco2_fine;


// old hybrid params

float atFMDepth = 0;
float mwFMDepth = 0;
float wheel = 0;
float depth = 0;
float detune = 1.00f;
float olddetune = 1.00f;
float bend = 1.00;
float octave = 1;
float octaveB = 1;
float octaveC = 1;
float tuneB = 1;
float tuneC = 1;

int vcoAWave = 0;
int vcoBWave = 0;
int vcoCWave = 0;

bool vcoATable = false;
bool vcoBTable = false;
bool vcoCTable = false;

int vcoAWaveNumber = 1;
int vcoBWaveNumber = 1;
int vcoCWaveNumber = 1;

int vcoAWaveBank = 1;
int vcoBWaveBank = 1;
int vcoCWaveBank = 1;

int vcoAInterval = 0;
int vcoBInterval = 0;
int vcoCInterval = 0;
int aInterval = 0;
int bInterval = 0;
int cInterval = 0;

float vcoAPW = 0;
float vcoBPW = 0;
float vcoCPW = 0;
float aPW = 0;
float bPW = 0;
float cPW = 0;

float vcoAPWM = 0;
float vcoBPWM = 0;
float vcoCPWM = 0;
float aPWM = 0;
float bPWM = 0;
float cPWM = 0;

float vcoBDetune = 0;
float vcoCDetune = 0;
float bDetune = 1.00;
float cDetune = 1.00;

int vcoAOctave = 1;
int vcoBOctave = 1;
int vcoCOctave = 1;

float vcoAFMDepth = 0;
float vcoBFMDepth = 0;
float vcoCFMDepth = 0;
float aFMDepth = 0;
float bFMDepth = 0;
float cFMDepth = 0;

float vcoALevel = 0;
float vcoBLevel = 0;
float vcoCLevel = 0;
float aLevel = 0;
float bLevel = 0;
float cLevel = 0;

float filterCutoff = 0;
float filterResonance = 0;
float filterEGDepth = 0;
float filterKeyTrack = 0;
float filterLFODepth;
bool filterKeyTrackSW = 0;
int egInvertSW = 0;

float ampLFODepth;
float XModDepth = 0;
float bXModDepth = 0;
float noiseLevel = 0;

float pitchAttack = 0;
float pitchDecay = 0;
float pitchSustain = 0;
float pitchRelease = 0;

float filterAttack = 0;
float filterDecay = 0;
float filterSustain = 0;
float filterRelease = 0;

float ampAttack = 0;
float ampDecay = 0;
float ampSustain = 0;
float ampRelease = 0;

float LFO1Rate = 0;
float LFO1Delay = 0;
float LFO2Rate = 0;
int LFO1Wave = 0;
int LFO2Wave = 0;

float effectPot1 = 0;
float effectPot2 = 0;
float effectPot3 = 0;
float effectsMix = 0;
float volumeLevel = 0;

int MWDepth = 0;
int PBDepth = 0;
int ATDepth = 0;

int filterType = 0;
bool filterPoleSW = 0;

bool filterVelocitySW = 0;
bool ampVelocitySW = 0;

int vcoAPWMsource = 0;
int vcoBPWMsource = 0;
int vcoCPWMsource = 0;

int vcoAFMsource = 0;
int vcoBFMsource = 0;
int vcoCFMsource = 0;
bool multiSW = 0;

int effectNumberSW = 0;
int effectBankSW = 0;

int playModeSW = 0;
int notePrioritySW = 0;

// Not stored

int FMSyncSW = 0;
int PWSyncSW = 0;
int PWMSyncSW = 0;
bool effectsPot3SW = false;
bool pot3ToggleState = false;  // false = go to fast, true = return to stored
int slowpot3 = 5;
int fastpot3 = 250;
bool fast = false;
bool slow = false;
int oldeffectPot3 = -99;




