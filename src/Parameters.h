//Values below are just for initialising and will be changed when synth is initialised to current panel controls & EEPROM settings
byte midiChannel = 1;  //(EEPROM)
byte midiOutCh = 1; 

struct VoiceAndNote {
  int note;
  int velocity;
  unsigned long timeOn;
  bool sustained;  // Sustain flag
  bool keyDown;
  double noteFreq;  // Note frequency
  int position;
  bool noteOn;
};

struct VoiceAndNote voices[NO_OF_VOICES] = {
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false },
  { -1, -1, 0, false, false, 0, -1, false }
};

// Tracks exactly which note each voice currently plays
int voiceToNoteLower[6] = { -1, -1, -1, -1, -1, -1 };
int voiceToNoteUpper[6] = { -1, -1, -1, -1, -1, -1 };

bool voiceOn[NO_OF_VOICES] = { false, false, false, false, false, false, false, false, false, false, false, false };

int noteMsg;
int prevNote = 0;  //Initialised to middle value
bool notes[128] = { 0 }, initial_loop = 1;
int8_t noteOrder[40] = { 0 }, orderIndx = { 0 };

bool notesWhole[128], notesLower[128], notesUpper[128];
byte noteOrderWhole[40], noteOrderLower[40], noteOrderUpper[40];
int orderIndxWhole = 0, orderIndxLower = 0, orderIndxUpper = 0;

int voiceAssignmentLower[128];
int voiceAssignmentUpper[128];

int noteVel;
int lastPlayedNote = -1;  // Track the last note played
int lastPlayedVoice = 0;  // Track the voice of the last note played
int lastUsedVoice = 0;    // Global variable to store the last used voice

int patchNo = 1;               //Current patch no
int voiceToReturn = -1;        //Initialise
unsigned long earliestTime = millis();  //For voice allocation - initialise to now

int resolutionFrig = 1;

uint32_t noteAgeCounter = 1;
int nextVoiceRR = 0;
uint8_t voiceNote[9];  // per-voice note index (0..127 or into noteFreqs)

// JP8 Hold
// -------------------- HOLD CONFIG --------------------

bool keyDownLower[128] = {0};
bool keyDownUpper[128] = {0};
bool keyDownWhole[128] = {0}; // optional (whole mode convenience)

bool holdManualLower = false;
bool holdManualUpper = false;
bool holdPedal = false;     // DP-2 pressed

bool holdLatchedLower[128] = {0}; // notes sustaining because Hold caught their note-off
bool holdLatchedUpper[128] = {0};

bool sustainPedalDown = false;     // CCsustain >= 64

// JP8 Arpeggiator
// -------------------- ARP CONFIG --------------------

// External clock pulse handling
volatile uint16_t arpExtTickCount = 0;
volatile uint32_t lastExtPulseUs = 0;
const uint16_t ARP_EXT_CLOCK_LOSS_MS = 250;

// Debounce / minimum pulse spacing (microseconds)
const uint32_t EXT_PULSE_MIN_US = 1500;

// LED flash request from ISR
volatile bool extClkLedPulseReq = false;
volatile uint32_t extClkLedPulseAtMs = 0;

// LED pulse width (ms)
const uint16_t EXT_LED_PULSE_MS = 30;

enum ArpMode : uint8_t { ARP_OFF=0, ARP_UP, ARP_DOWN, ARP_UPDOWN, ARP_RANDOM };
enum ArpClockSrc : uint8_t { ARPCLK_INTERNAL=0, ARPCLK_EXTERNAL, ARPCLK_MIDI };

volatile ArpClockSrc arpClockSrc = ARPCLK_INTERNAL;

enum ArpMidiDiv : uint8_t { ARP_DIV_8TH=0, ARP_DIV_8TH_TRIP, ARP_DIV_16TH };
volatile ArpMidiDiv arpMidiDiv = ARP_DIV_16TH;

// Your existing arpRate (0..255 assumed)
extern uint8_t arpRate;

// External / MIDI clock step accumulator
volatile uint16_t arpClkTickCount = 0;     // counts pulses/ticks until a step
volatile uint8_t arpTicksPerStep = 6;     // default: 16th @ MIDI clock (24ppqn -> 6 ticks)
bool midiClockRunning = false;          // true after Start/Continue, false after Stop
// 0 = 8th, 1 = 8th triplet, 2 = 16th
uint8_t arpMidiDivSW = 2; // pick your default

volatile ArpMode arpMode = ARP_OFF;
volatile uint8_t arpRange = 4;         // 1..4 octaves

volatile uint16_t arpStepMs = 125;      // step interval in ms (internal clock)
volatile float arpGate = 0.50f;         // not strictly needed; we do step-boundary note off

// In Split mode, JP-8 assigns arp to LOWER only. We'll honor that.
bool arpLowerOnlyWhenSplit = true;

// Prevent arp notes from affecting hold/keyDown tracking
bool arpInjecting = false;

uint8_t arpPattern[8] = {0};
uint8_t arpLen = 0;

// Transport over unfolded pattern
int16_t arpPos = -1;    // index into unfolded sequence
int8_t  arpDir = +1;    // for UPDOWN ping-pong

// Current sounding arp note (so we can note-off at next step)
bool arpNoteActive = false;
uint8_t arpCurrentNote = 0;
uint8_t arpCurrentVel = 100;

// Internal clock
uint32_t arpLastStepMs = 0;
bool arpRunning = false;

float arpHzTarget  = 4.0f;   // desired Hz from pot
float arpHzSmooth  = 4.0f;   // smoothed Hz used by engine
uint32_t arpNextStepUs = 0;  // absolute time of next step (micros)
uint32_t arpLastSmoothUs = 0;

// Remember last arp range (JP-8: defaults to 4 after power-up, then remembers last used)
uint8_t lastArpRange = 4;
bool arpEverEnabledSinceBoot = false;

// Save/restore keyboard assign modes when arp forces Poly2
uint8_t savedLowerKBMode = 0;
uint8_t savedUpperKBMode = 0;
bool arpForcedPoly2 = false;

// Encoders - is it needed?

// adding encoders
bool rotaryEncoderChanged(int id, bool clockwise, int speed);
#define NUM_ENCODERS 51
unsigned long lastTransition[NUM_ENCODERS + 1];
unsigned long lastDisplayTriggerTime = 0;
bool waitingToUpdate = false;
const unsigned long displayTimeout = 5000;  // e.g. 5 seconds

int MIDIThru = midi::Thru::Off;  //(EEPROM)
String patchName = INITPATCHNAME;
String toneNameU = INITTONEU;
String toneNameL = INITTONEL;
bool encCW = true;  //This is to set the encoder to increment when turned CW - Settings Option
bool announce = true;
byte accelerate = 1;
int speed = 1;
bool updateParams = false;  //(EEPROM)
int old_value = 0;
int old_param_offset = 0;
int displayMode = 0;
int editMode = 0;
int assignMode = 0;

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

int upperData[102];
int lowerData[102];
int swapData[102];
int panelData[102];
int patchData[30];

#define P_sysex 0
#define P_lfo1_wave 1
#define P_lfo1_rate 2
#define P_lfo1_delay 3
#define P_lfo1_lfo2 4
#define P_lfo1_sync 5
#define P_lfo2_wave 6
#define P_lfo2_rate 7
#define P_lfo2_delay 8
#define P_lfo2_lfo1 9
#define P_lfo2_sync 10
#define P_dco1_PW 11
#define P_dco1_PWM_env 12
#define P_dco1_PWM_lfo 13
#define P_dco1_PWM_dyn 14
#define P_dco1_PWM_env_source 15
#define P_dco1_PWM_env_pol 16
#define P_dco1_PWM_lfo_source 17
#define P_dco1_pitch_env 18
#define P_dco1_pitch_env_source 19
#define P_dco1_pitch_env_pol 20
#define P_dco1_pitch_lfo 21
#define P_dco1_pitch_lfo_source 22
#define P_dco1_pitch_dyn 23
#define P_dco1_wave 24
#define P_dco1_range 25
#define P_dco1_tune 26
#define P_dco1_mode 27
#define P_dco2_PW 28
#define P_dco2_PWM_env 29
#define P_dco2_PWM_lfo 30
#define P_dco2_PWM_dyn 31
#define P_dco2_PWM_env_source 32
#define P_dco2_PWM_env_pol 33
#define P_dco2_PWM_lfo_source 34
#define P_dco2_pitch_env 35
#define P_dco2_pitch_env_source 36
#define P_dco2_pitch_env_pol 37
#define P_dco2_pitch_lfo 38
#define P_dco2_pitch_lfo_source 39
#define P_dco2_pitch_dyn 40
#define P_dco2_wave 41
#define P_dco2_range 42
#define P_dco2_tune 43
#define P_dco2_fine 44
#define P_dco1_level 45
#define P_dco2_level 46
#define P_dco2_mod 47
#define P_dco_mix_env_pol 48
#define P_dco_mix_env_source 49
#define P_dco_mix_dyn 50
#define P_dco_mix_dynU 51
#define P_dco_mix_dynL 52
#define P_vcf_hpf 53
#define P_vcf_cutoff 54
#define P_vcf_res 55
#define P_vcf_kb 56
#define P_vcf_env 57
#define P_vcf_lfo1 58
#define P_vcf_lfo2 59
#define P_vcf_env_source 60
#define P_vcf_env_pol 61
#define P_vcf_dyn 62
#define P_vca_mod 63
#define P_vca_env_source 64
#define P_vca_dyn 65
#define P_portamento_sw 66
#define P_time1 67
#define P_level1 68
#define P_time2 69
#define P_level2 70
#define P_time3 71
#define P_level3 72
#define P_time4 73
#define P_env5stage_mode 74
#define P_env2_time1 75
#define P_env2_level1 76
#define P_env2_time2 77
#define P_env2_level2 78
#define P_env2_time3 79
#define P_env2_level3 80
#define P_env2_time4 82
#define P_env2_5stage_mode 83
#define P_attack 84
#define P_decay 85
#define P_sustain 86
#define P_release 87
#define P_adsr_mode 88
#define P_env4_attack 89
#define P_env4_decay 90
#define P_env4_sustain 91
#define P_env4_release 92
#define P_env4_adsr_mode 93
#define P_chorus 94
#define P_unisondetune 95
#define P_mod_lfo 96
#define P_octave_down 97
#define P_octave_up 98
#define P_bend_enable 99
#define P_assign 100

int arpRangeU;
int arpRangeL;
int arpModeU;
int arpModeL;

int at_vib;
int at_lpf;
int at_vol;
int balance;
int portamento;
int volume;
int dualdetune;
int bend_range;
int bend_enable;
int after_enable;
int keyMode = 0;
int chaseLevel;
int chaseMode;
int chaseTime;
int chasePlay;

int lfo1_wave_str;
int lfo1_rate_str;
int lfo1_delay_str;
int lfo1_lfo2_str;
int lfo2_wave_str;
int lfo2_rate_str;
int lfo2_delay_str;
int lfo2_lfo1_str;
int dco1_PW_str;
int dco1_PWM_env_str;
int dco1_PWM_lfo_str;
int dco1_pitch_env_str;
int dco1_pitch_lfo_str;
int dco1_wave_str;
int dco1_range_str;
int dco1_tune_str;
int dco1_mode_str;
int dco2_PW_str;
int dco2_PWM_env_str;
int dco2_PWM_lfo_str;
int dco2_pitch_env_str;
int dco2_pitch_lfo_str;
int dco2_wave_str;
int dco2_range_str;
int dco2_tune_str;
int dco2_fine_str;
int dco1_level_str;
int dco2_level_str;
int dco2_mod_str;
int vcf_hpf_str;
int vcf_cutoff_str;
int vcf_res_str;
int vcf_kb_str;
int vcf_env_str;
int vcf_lfo1_str;
int vcf_lfo2_str;
int vca_mod_str;
int level1_str;
int time1_str;
int time2_str;
int level2_str;
int time3_str;
int level3_str;
int time4_str;
int env5stage_mode_str;
int env2_time1_str;
int env2_level1_str;
int env2_time2_str;
int env2_level2_str;
int env2_time3_str;
int env2_level3_str;
int env2_time4_str;
int env2_5stage_mode_str;
int attack_str;
int decay_str;
int sustain_str;
int release_str;
int adsr_mode_str;
int env4_attack_str;
int env4_decay_str;
int env4_sustain_str;
int env4_release_str;
int env4_adsr_mode_str;
int unisondetune_str;
int mod_lfo_str;
int at_vib_str;
int at_lpf_str;
int at_vol_str;
int balance_str;
int portamento_str;
int volume_str;
int dualdetune_str;
int bend_range_str;
bool set10ctave = false;
bool octave_down_upwards = true;  // true = going up, false = going down
int octave_up = 0;
int octave_upU = 0;
int octave_upL = 0;
bool octave_up_upwards = true;


// Balance variables

static constexpr uint8_t kBalanceParam = 0x9E;
static constexpr uint8_t kBoardLowerPrefix = 0xF1;
static constexpr uint8_t kBoardUpperPrefix = 0xF9;
static constexpr uint8_t kBoardBothPrefix  = 0xF4; // keyMode 1/2 broadcast
static constexpr uint8_t kMaxLevel = 0x60;         // 96


// Dual detune
static constexpr uint8_t kDualDetuneParam = 0xB4; // TODO: set your actual param
static constexpr uint8_t kDualDetuneParam2 = 0xBE; // TODO: set your actual param
static constexpr uint8_t kDetuneZeroPos   = 0x2C; // "00"
static constexpr uint8_t kDetuneNegZero   = 0x2B; // "-00"
static constexpr uint8_t kDetuneMax       = 0x6B; // "+50"

// Pitch bend

static constexpr uint8_t kPrefixBroadcast = 0xF4;
static constexpr uint8_t kPitchSignParam  = 0xBF; // 0x00=negative, 0x7F=positive
static constexpr uint8_t kPitchValueParam = 0xB2; // 0..0x7F magnitude
static constexpr bool kInvertPb14 = true;

// None saved variables

int oldkeyMode = -1;
int adsr = 0;
int env5stage = 0;
bool upperSW = false;
bool oldupperSW = false;

uint8_t LAST_PARAM = 0x00;
uint8_t EXTRA_OFFSET = 0x00;
uint8_t board = 0xF4;

boolean dual_button;
boolean split_button;
boolean single_button;
boolean special_button;
boolean poly_button;
boolean mono_button;
boolean unison_button;

byte splitPoint = 60;
byte oldsplitPoint = 0;
byte newsplitPoint = 0;
byte splitTrans = 0;
byte oldsplitTrans = 0;
int lowerTranspose = 0;
int lowerSplitVoicePointer = 0;
int upperSplitVoicePointer = 0;

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






