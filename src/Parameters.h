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
int lfo1_rate_str;
int lfo1_delay;
int lfo1_delay_str;
int lfo1_lfo2;
int lfo1_lfo2_str;
int lfo1_sync;

int lfo2_wave;
int lfo2_wave_str;

int lfo2_rate;
int lfo2_rate_str;
int lfo2_delay;
int lfo2_delay_str;
int lfo2_lfo1;
int lfo2_lfo1_str;
int lfo2_sync;

int dco1_PW;
int dco1_PW_str;
int dco1_PWM_env;
int dco1_PWM_env_str;
int dco1_PWM_lfo;
int dco1_PWM_lfo_str;
int dco1_PWM_dyn;
int dco1_PWM_env_source = 0;
int dco1_PWM_env_pol = 0;
int dco1_PWM_lfo_source;
int dco1_pitch_env;
int dco1_pitch_env_str;
int dco1_pitch_env_source = 0;
int dco1_pitch_env_pol = 0;
int dco1_pitch_lfo;
int dco1_pitch_lfo_str;
int dco1_pitch_lfo_source;
int dco1_pitch_dyn;
int dco1_wave;
int dco1_wave_str;
int dco1_range;
int dco1_range_str;
int dco1_tune;
int dco1_tune_str;
int dco1_mode;
int dco1_mode_str;

int dco2_PW;
int dco2_PW_str;
int dco2_PWM_env;
int dco2_PWM_env_str;
int dco2_PWM_lfo;
int dco2_PWM_lfo_str;
int dco2_PWM_dyn;
int dco2_PWM_env_source = 0;
int dco2_PWM_env_pol = 0;
int dco2_PWM_lfo_source;
int dco2_pitch_env;
int dco2_pitch_env_str;
int dco2_pitch_env_source = 0;
int dco2_pitch_env_pol = 0;
int dco2_pitch_lfo;
int dco2_pitch_lfo_str;
int dco2_pitch_lfo_source;
int dco2_pitch_dyn;
int dco2_wave;
int dco2_wave_str;
int dco2_range;
int dco2_range_str;
int dco2_tune;
int dco2_tune_str;
int dco2_fine;
int dco2_fine_str;

int dco1_level;
int dco1_level_str;
int dco2_level;
int dco2_level_str;
int dco2_mod;
int dco2_mod_str;
int dco_mix_env_pol = 0;
int dco_mix_env_source = 0;
int dco_mix_dyn;

int vcf_hpf;
int vcf_cutoff;
int vcf_res;
int vcf_kb;
int vcf_env;
int vcf_lfo1;
int vcf_lfo2;
int vcf_hpf_str;
int vcf_cutoff_str;
int vcf_res_str;
int vcf_kb_str;
int vcf_env_str;
int vcf_lfo1_str;
int vcf_lfo2_str;
int vcf_env_source = 0;
int vcf_env_pol = 0;
int vcf_dyn;

int vca_mod;
int vca_mod_str;
int vca_env_source = 0;
int vca_dyn;

int at_vib;
int at_lpf;
int at_vol;
int balance;
int at_vib_str;
int at_lpf_str;
int at_vol_str;
int balance_str;
int portamento;
int portamento_str;
int portamento_sw;
int portamento_sw_str;
int volume;
int volume_str;

int time1;
int level1;
int time2;
int level2;
int time3;
int level3;
int time4;
int time1_str;
int level1_str;
int time2_str;
int level2_str;
int time3_str;
int level3_str;
int time4_str;
int env5stage_mode;
int env5stage_mode_str;

int env2_time1;
int env2_level1;
int env2_time2;
int env2_level2;
int env2_time3;
int env2_level3;
int env2_time4;
int env2_5stage_mode;
int env2_time1_str;
int env2_level1_str;
int env2_time2_str;
int env2_level2_str;
int env2_time3_str;
int env2_level3_str;
int env2_time4_str;
int env2_5stage_mode_str;

int attack;
int decay;
int sustain;
int release;
int attack_str;
int decay_str;
int sustain_str;
int release_str;
int adsr_mode;
int adsr_mode_str;

int env4_attack;
int env4_attack_str;
int env4_decay;
int env4_decay_str;
int env4_sustain;
int env4_sustain_str;
int env4_release;
int env4_release_str;
int env4_adsr_mode;
int env4_adsr_mode_str;

int chorus;

int dualdetune;
int dualdetune_str;
int unisondetune;
int unisondetune_str;
int mod_lfo;
int mod_lfo_str;
int bend_range;
int bend_range_str;
bool set10ctave = false;
int octave_down = 0;
bool octave_down_upwards = true;  // true = going up, false = going down
int octave_up = 0;
bool octave_up_upwards = true;

int playmode = 0;
int adsr = 0;
int env5stage = 0;

boolean dual_button;
boolean split_button;
boolean single_button;
boolean special_button;
int keymode = 0;
boolean poly_button;
boolean mono_button;
boolean unison_button;

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




