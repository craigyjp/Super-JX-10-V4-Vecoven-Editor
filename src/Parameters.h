//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = 1;  //(EEPROM)
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
int displayMode = 0;

bool manualSyncInProgress = false;
bool suppressParamAnnounce = true;
bool bootInitInProgress = true;

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
int lfo1_waveU;
int lfo1_waveL;
int lfo1_wave_str;

int lfo1_rate;
int lfo1_rateU;
int lfo1_rateL;
int lfo1_rate_str;

int lfo1_delay;
int lfo1_delayU;
int lfo1_delayL;
int lfo1_delay_str;

int lfo1_lfo2;
int lfo1_lfo2U;
int lfo1_lfo2L;
int lfo1_lfo2_str;

int lfo1_sync;
int lfo1_syncU;
int lfo1_syncL;

int lfo2_wave;
int lfo2_waveU;
int lfo2_waveL;
int lfo2_wave_str;

int lfo2_rate;
int lfo2_rateU;
int lfo2_rateL;
int lfo2_rate_str;

int lfo2_delay;
int lfo2_delayU;
int lfo2_delayL;
int lfo2_delay_str;

int lfo2_lfo1;
int lfo2_lfo1U;
int lfo2_lfo1L;
int lfo2_lfo1_str;

int lfo2_sync;
int lfo2_syncU;
int lfo2_syncL;

int dco1_PW;
int dco1_PWU;
int dco1_PWL;
int dco1_PW_str;

int dco1_PWM_env;
int dco1_PWM_envU;
int dco1_PWM_envL;
int dco1_PWM_env_str;

int dco1_PWM_lfo;
int dco1_PWM_lfoU;
int dco1_PWM_lfoL;
int dco1_PWM_lfo_str;

int dco1_PWM_dyn;
int dco1_PWM_dynU;
int dco1_PWM_dynL;

int dco1_PWM_env_source = 0;
int dco1_PWM_env_sourceU = 0;
int dco1_PWM_env_sourceL = 0;

int dco1_PWM_env_pol = 0;
int dco1_PWM_env_polU = 0;
int dco1_PWM_env_polL = 0;

int dco1_PWM_lfo_source;
int dco1_PWM_lfo_sourceU;
int dco1_PWM_lfo_sourceL;

int dco1_pitch_env;
int dco1_pitch_envU;
int dco1_pitch_envL;
int dco1_pitch_env_str;

int dco1_pitch_env_source = 0;
int dco1_pitch_env_sourceU = 0;
int dco1_pitch_env_sourceL = 0;

int dco1_pitch_env_pol = 0;
int dco1_pitch_env_polU = 0;
int dco1_pitch_env_polL = 0;

int dco1_pitch_lfo;
int dco1_pitch_lfoU;
int dco1_pitch_lfoL;
int dco1_pitch_lfo_str;

int dco1_pitch_lfo_source;
int dco1_pitch_lfo_sourceU;
int dco1_pitch_lfo_sourceL;

int dco1_pitch_dyn;
int dco1_pitch_dynU;
int dco1_pitch_dynL;

int dco1_wave;
int dco1_waveU;
int dco1_waveL;
int dco1_wave_str;

int dco1_range;
int dco1_rangeU;
int dco1_rangeL;
int dco1_range_str;

int dco1_tune;
int dco1_tuneU;
int dco1_tuneL;
int dco1_tune_str;

int dco1_mode;
int dco1_modeU;
int dco1_modeL;
int dco1_mode_str;

int dco2_PW;
int dco2_PWU;
int dco2_PWL;
int dco2_PW_str;

int dco2_PWM_env;
int dco2_PWM_envU;
int dco2_PWM_envL;
int dco2_PWM_env_str;

int dco2_PWM_lfo;
int dco2_PWM_lfoU;
int dco2_PWM_lfoL;
int dco2_PWM_lfo_str;

int dco2_PWM_dyn;
int dco2_PWM_dynU;
int dco2_PWM_dynL;

int dco2_PWM_env_source = 0;
int dco2_PWM_env_sourceU = 0;
int dco2_PWM_env_sourceL = 0;

int dco2_PWM_env_pol = 0;
int dco2_PWM_env_polU = 0;
int dco2_PWM_env_polL = 0;

int dco2_PWM_lfo_source;
int dco2_PWM_lfo_sourceU;
int dco2_PWM_lfo_sourceL;

int dco2_pitch_env;
int dco2_pitch_envU;
int dco2_pitch_envL;
int dco2_pitch_env_str;

int dco2_pitch_env_source = 0;
int dco2_pitch_env_sourceU = 0;
int dco2_pitch_env_sourceL = 0;

int dco2_pitch_env_pol = 0;
int dco2_pitch_env_polU = 0;
int dco2_pitch_env_polL = 0;

int dco2_pitch_lfo;
int dco2_pitch_lfoU;
int dco2_pitch_lfoL;
int dco2_pitch_lfo_str;

int dco2_pitch_lfo_source;
int dco2_pitch_lfo_sourceU;
int dco2_pitch_lfo_sourceL;

int dco2_pitch_dyn;
int dco2_pitch_dynU;
int dco2_pitch_dynL;

int dco2_wave;
int dco2_waveU;
int dco2_waveL;
int dco2_wave_str;

int dco2_range;
int dco2_rangeU;
int dco2_rangeL;
int dco2_range_str;

int dco2_tune;
int dco2_tuneU;
int dco2_tuneL;
int dco2_tune_str;

int dco2_fine;
int dco2_fineU;
int dco2_fineL;
int dco2_fine_str;

int dco1_level;
int dco1_levelU;
int dco1_levelL;
int dco1_level_str;

int dco2_level;
int dco2_levelU;
int dco2_levelL;
int dco2_level_str;

int dco2_mod;
int dco2_modU;
int dco2_modL;
int dco2_mod_str;

int dco_mix_env_pol = 0;
int dco_mix_env_polU = 0;
int dco_mix_env_polL = 0;

int dco_mix_env_source = 0;
int dco_mix_env_sourceU = 0;
int dco_mix_env_sourceL = 0;

int dco_mix_dyn;
int dco_mix_dynU;
int dco_mix_dynL;

int vcf_hpf;
int vcf_hpfU;
int vcf_hpfL;
int vcf_hpf_str;

int vcf_cutoff;
int vcf_cutoffU;
int vcf_cutoffL;
int vcf_cutoff_str;

int vcf_res;
int vcf_resU;
int vcf_resL;
int vcf_res_str;

int vcf_kb;
int vcf_kbU;
int vcf_kbL;
int vcf_kb_str;

int vcf_env;
int vcf_envU;
int vcf_envL;
int vcf_env_str;

int vcf_lfo1;
int vcf_lfo1U;
int vcf_lfo1L;
int vcf_lfo1_str;

int vcf_lfo2;
int vcf_lfo2U;
int vcf_lfo2L;
int vcf_lfo2_str;

int vcf_env_source = 0;
int vcf_env_sourceU = 0;
int vcf_env_sourceL = 0;

int vcf_env_pol = 0;
int vcf_env_polU = 0;
int vcf_env_polL = 0;

int vcf_dyn;
int vcf_dynU;
int vcf_dynL;

int vca_mod;
int vca_modU;
int vca_modL;
int vca_mod_str;

int vca_env_source = 0;
int vca_env_sourceU = 0;
int vca_env_sourceL = 0;

int vca_dyn;
int vca_dynU;
int vca_dynL;

int portamento_sw;
int portamento_swU;
int portamento_swL;

int time1;
int time1U;
int time1L;
int time1_str;

int level1;
int level1U;
int level1L;
int level1_str;

int time2;
int time2U;
int time2L;
int time2_str;

int level2;
int level2U;
int level2L;
int level2_str;

int time3;
int time3U;
int time3L;
int time3_str;

int level3;
int level3U;
int level3L;
int level3_str;

int time4;
int time4U;
int time4L;
int time4_str;

int env5stage_mode;
int env5stage_modeU;
int env5stage_modeL;
int env5stage_mode_str;

int env2_time1;
int env2_time1U;
int env2_time1L;
int env2_time1_str;

int env2_level1;
int env2_level1U;
int env2_level1L;
int env2_level1_str;

int env2_time2;
int env2_time2U;
int env2_time2L;
int env2_time2_str;

int env2_level2;
int env2_level2U;
int env2_level2L;
int env2_level2_str;

int env2_time3;
int env2_time3U;
int env2_time3L;
int env2_time3_str;

int env2_level3;
int env2_level3U;
int env2_level3L;
int env2_level3_str;

int env2_time4;
int env2_time4U;
int env2_time4L;
int env2_time4_str;

int env2_5stage_mode;
int env2_5stage_modeU;
int env2_5stage_modeL;
int env2_5stage_mode_str;

int attack;
int attackU;
int attackL;
int attack_str;

int decay;
int decayU;
int decayL;
int decay_str;

int sustain;
int sustainU;
int sustainL;
int sustain_str;

int release;
int releaseU;
int releaseL;
int release_str;

int adsr_mode;
int adsr_modeU;
int adsr_modeL;
int adsr_mode_str;

int env4_attack;
int env4_attackU;
int env4_attackL;
int env4_attack_str;

int env4_decay;
int env4_decayU;
int env4_decayL;
int env4_decay_str;

int env4_sustain;
int env4_sustainU;
int env4_sustainL;
int env4_sustain_str;

int env4_release;
int env4_releaseU;
int env4_releaseL;
int env4_release_str;

int env4_adsr_mode;
int env4_adsr_modeU;
int env4_adsr_modeL;
int env4_adsr_mode_str;

int chorus;
int chorusU;
int chorusL;

int unisondetune;
int unisondetuneU;
int unisondetuneL;
int unisondetune_str;

int mod_lfo;
int mod_lfoU;
int mod_lfoL;
int mod_lfo_str;

int octave_down = 0;
int octave_downU = 0;
int octave_downL = 0;

bool octave_down_upwards = true;  // true = going up, false = going down
int octave_up = 0;
int octave_upU = 0;
int octave_upL = 0;
bool octave_up_upwards = true;

int bend_enableU;
int bend_enableL;
int assignU;
int assignL; 

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
int volume;
int volume_str;
int dualdetune;
int dualdetune_str;
int bend_range;
int bend_range_str;
bool set10ctave = false;

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

// 5x8 custom chars (each row uses bits 0..4)

byte midBar2[] = {
  0b00110,
  0b00110,
  0b00110,
  0b00110,  // center bar row 1
  0b00110,  // center bar row 2
  0b00110,
  0b00110,
  0b00110
};

byte triUpSolid[] = {
  0b00100,
  0b01110,
  0b11111,
  0b11111,
  0b11111,
  0b00000,
  0b00000,
  0b00000
};

byte triDownSolid[] = {
  0b00000,
  0b00000,
  0b00000,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000
};






