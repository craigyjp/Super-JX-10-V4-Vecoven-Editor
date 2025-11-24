// This optional setting causes Encoder to use more optimized code,
// It must be defined before Encoder.h is included.
#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Encoder.h>
#include <Bounce.h>
#include "TButton.h"
#include <ADC.h>
#include <ADC_util.h>

ADC *adc = new ADC();

//Teensy 4.1 - Mux Pins
#define MUX_0 29
#define MUX_1 30
#define MUX_2 31
#define MUX_3 32

#define MUX1_S A10  // ADC0
#define MUX2_S A11  // ADC0
#define MUX3_S A12  // ADC1
#define MUX4_S A13  // ADC1

//Mux 1 Connections
#define MUX1_MOD_LFO 0
#define MUX1_LFO1_RATE 1
#define MUX1_LFO1_DELAY 2
#define MUX1_LFO1_LFO2_MOD 3
#define MUX1_DCO1_PW 4
#define MUX1_DCO1_PWM_ENV 5
#define MUX1_DCO1_PWM_LFO 6
#define MUX1_DCO1_PITCH_ENV 7
#define MUX1_DCO1_PITCH_LFO 8
#define MUX1_DCO1_WAVE 9
#define MUX1_DCO1_RANGE 10
#define MUX1_DCO1_TUNE 11
#define MUX1_PORTAMENTO 12
#define MUX1_LFO1_WAVE 13
#define MUX1_SPARE_14 14
#define MUX1_SPARE_15 15

//Mux 2 Connections
#define MUX2_BEND_RANGE 0
#define MUX2_LFO2_RATE 1
#define MUX2_LFO2_DELAY 2
#define MUX2_LFO2_LFO1_MOD 3
#define MUX2_DCO2_PW 4
#define MUX2_DCO2_PWM_ENV 5
#define MUX2_DCO2_PWM_LFO 6
#define MUX2_DCO2_PITCH_ENV 7
#define MUX2_DCO2_PITCH_LFO 8
#define MUX2_DCO2_WAVE 9
#define MUX2_DCO2_RANGE 10
#define MUX2_DCO2_TUNE 11
#define MUX2_DCO2_FINE 12
#define MUX2_DCO1_MODE 13
#define MUX2_LFO2_WAVE 14
#define MUX2_SPARE_15 15

//Mux 3 Connections
#define MUX3_DCO1_LEVEL 0
#define MUX3_DCO2_LEVEL 1
#define MUX3_DCO2_MOD 2
#define MUX3_VCF_HPF 3
#define MUX3_VCF_CUTOFF 4
#define MUX3_VCF_RES 5
#define MUX3_VCF_KB 6
#define MUX3_VCF_ENV 7
#define MUX3_VCF_LFO1 8
#define MUX3_VCF_LFO2 9
#define MUX3_VCA_MOD 10
#define MUX3_AT_VIB 11
#define MUX3_AT_LPF 12
#define MUX3_AT_VOL 13
#define MUX3_BALANCE 14
#define MUX3_VOLUME 15

//Mux 4 Connections
#define MUX4_T1 0
#define MUX4_L1 1
#define MUX4_T2 2
#define MUX4_L2 3
#define MUX4_T3 4
#define MUX4_L3 5
#define MUX4_T4 6
#define MUX4_5STAGE_MODE 7
#define MUX4_ATTACK 8
#define MUX4_DECAY 9
#define MUX4_SUSTAIN 10
#define MUX4_RELEASE 11
#define MUX4_ADSR_MODE 12
#define MUX4_DUAL_DETUNE 13
#define MUX4_UNISON_DETUNE 14
#define MUX4_SPARE_15 15


#include "Rotary.h"
#include "RotaryEncOverMCP.h"


// Pins for MCP23017
#define GPA0 0
#define GPA1 1
#define GPA2 2
#define GPA3 3
#define GPA4 4
#define GPA5 5
#define GPA6 6
#define GPA7 7
#define GPB0 8
#define GPB1 9
#define GPB2 10
#define GPB3 11
#define GPB4 12
#define GPB5 13
#define GPB6 14
#define GPB7 15

#define LFO1_SYNC_BUTTON 0
#define LFO2_SYNC_BUTTON 1
#define DCO1_PWM_ENV_SOURCE_BUTTON 2
#define DCO2_PWM_ENV_SOURCE_BUTTON 3
#define DCO1_PWM_ENV_POLARITY_BUTTON 4
#define DCO2_PWM_ENV_POLARITY_BUTTON 5
#define DCO1_PWM_DYN_BUTTON 6
#define DCO2_PWM_DYN_BUTTON 7
#define DCO1_PWM_LFO_SOURCE_BUTTON 8
#define DCO2_PWM_LFO_SOURCE_BUTTON 9
#define DCO1_PITCH_LFO_SOURCE_BUTTON 10
#define DCO2_PITCH_LFO_SOURCE_BUTTON 11
#define DCO1_PITCH_DYN_BUTTON 12
#define DCO2_PITCH_DYN_BUTTON 13
#define DCO1_PITCH_ENV_POLARITY_BUTTON 14
#define DCO2_PITCH_ENV_POLARITY_BUTTON 15
#define DCO1_PITCH_ENV_SOURCE_BUTTON 16
#define DCO2_PITCH_ENV_SOURCE_BUTTON 17

#define DCO_MIX_ENV_POLARITY_BUTTON 18
#define DCO_MIX_ENV_SOURCE_BUTTON 19
#define VCF_ENV_POLARITY_BUTTON 20
#define DCO_MIX_DYN_BUTTON 21
#define ENV5STAGE_SELECT_BUTTON 22
#define VCA_DYN_BUTTON 23
#define VCA_ENV_SOURCE_BUTTON 24
#define VCF_ENV_SOURCE_BUTTON 25
#define VCF_DYN_BUTTON 26
#define ADSR_SELECT_BUTTON 27
#define LOWER_UPPER_BUTTON 28
#define CHORUS_BUTTON 29
#define PORTAMENTO_BUTTON 30

#define OCTAVE_DOWN_BUTTON 31
#define OCTAVE_UP_BUTTON 32
#define BEND_ENABLE_BUTTON 33
#define KEY_SINGLE_BUTTON 34
#define ASSIGN_POLY_BUTTON 35
#define ASSIGN_MONO_BUTTON 36
#define ASSIGN_UNI_BUTTON 37
#define KEY_DUAL_BUTTON 38
#define KEY_SPLIT_BUTTON 39
#define KEY_SPECIAL_BUTTON 40

//void RotaryEncoderChanged (bool clockwise, int id);

void mainButtonChanged(Button *btn, bool released);

// I2C MCP23017 GPIO expanders

Adafruit_MCP23017 mcp1;
Adafruit_MCP23017 mcp2;
Adafruit_MCP23017 mcp3;
Adafruit_MCP23017 mcp4;
Adafruit_MCP23017 mcp5;
Adafruit_MCP23017 mcp6;
Adafruit_MCP23017 mcp7;
Adafruit_MCP23017 mcp8;

//Array of pointers of all MCPs
Adafruit_MCP23017 *allMCPs[] = { &mcp1, &mcp2, &mcp3, &mcp4, &mcp5, &mcp6, &mcp7, &mcp8 };

// // My encoders
// /* Array of all rotary encoders and their pins */
RotaryEncOverMCP rotaryEncoders[] = {

};

// after your rotaryEncoders[] definition
constexpr size_t NUM_MCP = sizeof(allMCPs) / sizeof(allMCPs[0]);
constexpr int numMCPs = (int)(sizeof(allMCPs) / sizeof(*allMCPs));
constexpr int numEncoders = (int)(sizeof(rotaryEncoders) / sizeof(*rotaryEncoders));

Button lfo1_sync_Button = Button(&mcp1, 0, LFO1_SYNC_BUTTON, &mainButtonChanged);
Button lfo2_sync_Button = Button(&mcp2, 0, LFO2_SYNC_BUTTON, &mainButtonChanged);
Button dco1_PWM_env_pol_Button = Button(&mcp1, 3, DCO1_PWM_ENV_POLARITY_BUTTON, &mainButtonChanged);
Button dco2_PWM_env_pol_Button = Button(&mcp2, 3, DCO2_PWM_ENV_POLARITY_BUTTON, &mainButtonChanged);
Button dco1_PWM_env_src_Button = Button(&mcp1, 4, DCO1_PWM_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button dco2_PWM_env_src_Button = Button(&mcp2, 4, DCO2_PWM_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button dco1_PWM_dyn_Button = Button(&mcp1, 5, DCO1_PWM_DYN_BUTTON, &mainButtonChanged);
Button dco2_PWM_dyn_Button = Button(&mcp2, 5, DCO2_PWM_DYN_BUTTON, &mainButtonChanged);
Button dco1_PWM_lfo_src_Button = Button(&mcp4, 0, DCO1_PWM_LFO_SOURCE_BUTTON, &mainButtonChanged);
Button dco2_PWM_lfo_src_Button = Button(&mcp3, 0, DCO2_PWM_LFO_SOURCE_BUTTON, &mainButtonChanged);
Button dco1_pitch_lfo_src_Button = Button(&mcp4, 4, DCO1_PITCH_LFO_SOURCE_BUTTON, &mainButtonChanged);
Button dco2_pitch_lfo_src_Button = Button(&mcp3, 4, DCO2_PITCH_LFO_SOURCE_BUTTON, &mainButtonChanged);
Button dco1_pitch_dyn_Button = Button(&mcp4, 5, DCO1_PITCH_DYN_BUTTON, &mainButtonChanged);
Button dco2_pitch_dyn_Button = Button(&mcp3, 5, DCO2_PITCH_DYN_BUTTON, &mainButtonChanged);
Button dco1_pitch_env_pol_Button = Button(&mcp4, 8, DCO1_PITCH_ENV_POLARITY_BUTTON, &mainButtonChanged);
Button dco2_pitch_env_pol_Button = Button(&mcp3, 8, DCO2_PITCH_ENV_POLARITY_BUTTON, &mainButtonChanged);
Button dco1_pitch_env_src_Button = Button(&mcp4, 12, DCO1_PITCH_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button dco2_pitch_env_src_Button = Button(&mcp3, 12, DCO2_PITCH_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button dco_mix_env_pol_Button = Button(&mcp4, 3, DCO_MIX_ENV_POLARITY_BUTTON, &mainButtonChanged);
Button dco_mix_env_src_Button = Button(&mcp5, 8, DCO_MIX_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button vcf_env_pol_Button = Button(&mcp5, 2, VCF_ENV_POLARITY_BUTTON, &mainButtonChanged);
Button dco_mix_dyn_Button = Button(&mcp5, 5, DCO_MIX_DYN_BUTTON, &mainButtonChanged);
Button env5stage_select_Button = Button(&mcp5, 12, ENV5STAGE_SELECT_BUTTON, &mainButtonChanged);
Button vca_dyn_Button = Button(&mcp6, 0, VCA_DYN_BUTTON, &mainButtonChanged);
Button vca_env_src_Button = Button(&mcp6, 1, VCA_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button vcf_env_src_Button = Button(&mcp6, 4, VCF_ENV_SOURCE_BUTTON, &mainButtonChanged);
Button vcf_dyn_Button = Button(&mcp6, 5, VCF_DYN_BUTTON, &mainButtonChanged);
Button adsr_select_Button = Button(&mcp6, 8, ADSR_SELECT_BUTTON, &mainButtonChanged);
Button lower_upper_Button = Button(&mcp1, 13, LOWER_UPPER_BUTTON, &mainButtonChanged);
Button chorus_Button = Button(&mcp6, 13, CHORUS_BUTTON, &mainButtonChanged);
Button portamento_Button = Button(&mcp3, 3, PORTAMENTO_BUTTON, &mainButtonChanged);

Button octave_down_Button = Button(&mcp7, 8, OCTAVE_DOWN_BUTTON, &mainButtonChanged);
Button octave_up_Button = Button(&mcp7, 9, OCTAVE_UP_BUTTON, &mainButtonChanged);
Button bend_enable_Button = Button(&mcp7, 10, BEND_ENABLE_BUTTON, &mainButtonChanged);
Button key_single_Button = Button(&mcp7, 11, KEY_SINGLE_BUTTON, &mainButtonChanged);
Button assign_poly_Button = Button(&mcp8, 1, ASSIGN_POLY_BUTTON, &mainButtonChanged);
Button assign_mono_Button = Button(&mcp8, 0, ASSIGN_MONO_BUTTON, &mainButtonChanged);
Button assign_uni_Button = Button(&mcp8, 8, ASSIGN_UNI_BUTTON, &mainButtonChanged);
Button key_dual_Button = Button(&mcp8, 9, KEY_DUAL_BUTTON, &mainButtonChanged);
Button key_split_Button = Button(&mcp8, 11, KEY_SPLIT_BUTTON, &mainButtonChanged);
Button key_special_Button = Button(&mcp8, 13, KEY_SPECIAL_BUTTON, &mainButtonChanged);

Button *mainButtons[] = {
  &lfo1_sync_Button,
  &lfo2_sync_Button,
  &dco1_PWM_env_src_Button,
  &dco2_PWM_env_src_Button,
  &dco1_PWM_env_pol_Button,
  &dco2_PWM_env_pol_Button,
  &dco1_PWM_dyn_Button,
  &dco2_PWM_dyn_Button,
  &dco1_PWM_lfo_src_Button,
  &dco2_PWM_lfo_src_Button,
  &dco1_pitch_lfo_src_Button,
  &dco2_pitch_lfo_src_Button,
  &dco1_pitch_dyn_Button,
  &dco2_pitch_dyn_Button,
  &dco1_pitch_env_pol_Button,
  &dco2_pitch_env_pol_Button,
  &dco1_pitch_env_src_Button,
  &dco1_pitch_env_src_Button,
  &dco2_pitch_env_src_Button,
  &dco_mix_env_pol_Button,
  &dco_mix_env_src_Button,
  &vcf_env_pol_Button,
  &dco_mix_dyn_Button,
  &env5stage_select_Button,
  &vca_dyn_Button,
  &vca_env_src_Button,
  &vcf_env_src_Button,
  &vcf_dyn_Button,
  &adsr_select_Button,
  &lower_upper_Button,
  &chorus_Button,
  &portamento_Button,
  &octave_down_Button,
  &octave_up_Button,
  &bend_enable_Button,
  &key_single_Button,
  &assign_poly_Button,
  &assign_mono_Button,
  &assign_uni_Button,
  &key_dual_Button,
  &key_split_Button,
  &key_special_Button,
};

Button *allButtons[] = {
  &lfo1_sync_Button, &lfo2_sync_Button, &dco1_PWM_env_src_Button, &dco2_PWM_env_src_Button, &dco1_PWM_env_pol_Button, &dco2_PWM_env_pol_Button, &dco1_PWM_dyn_Button, &dco2_PWM_dyn_Button,
  &dco1_PWM_lfo_src_Button, &dco2_PWM_lfo_src_Button, &dco1_pitch_lfo_src_Button, &dco2_pitch_lfo_src_Button, &dco1_pitch_dyn_Button, &dco2_pitch_dyn_Button,
  &dco1_pitch_env_pol_Button, &dco2_pitch_env_pol_Button, &dco1_pitch_env_src_Button, &dco1_pitch_env_src_Button, &dco2_pitch_env_src_Button,
  &dco_mix_env_pol_Button, &dco_mix_env_src_Button, &vcf_env_pol_Button, &dco_mix_dyn_Button, &env5stage_select_Button,
  &vca_dyn_Button, &vca_env_src_Button, &vcf_env_src_Button, &vcf_dyn_Button, &adsr_select_Button, &lower_upper_Button, &chorus_Button, &portamento_Button,
  &octave_down_Button, &octave_up_Button, &bend_enable_Button, &key_single_Button, &assign_poly_Button, &assign_mono_Button, &assign_uni_Button,
  &key_dual_Button, &key_split_Button, &key_special_Button
};

// an array of vectors to hold pointers to the encoders on each MCP
std::vector<RotaryEncOverMCP *> encByMCP[NUM_MCP];

// // GP1

#define LFO1_SYNC_RED 1
#define LFO1_SYNC_GREEN 2

#define DCO1_PWM_DYN_RED 6
#define DCO1_PWM_DYN_GREEN 7

#define DCO1_PWM_ENV_SOURCE_RED 8
#define DCO1_PWM_ENV_SOURCE_GREEN 9

#define UPPER_SELECT 11
#define LOWER_SELECT 12

#define DCO1_ENV_POL_RED 14
#define DCO1_ENV_POL_GREEN 15

// // GP2

#define LFO2_SYNC_RED 1
#define LFO2_SYNC_GREEN 2

#define DCO2_PWM_DYN_RED 6
#define DCO2_PWM_DYN_GREEN 7

#define DCO2_PWM_ENV_SOURCE_RED 8
#define DCO2_PWM_ENV_SOURCE_GREEN 9

#define PORTAMENTO_LOWER_RED 10
#define PORTAMENTO_UPPER_GREEN 11

#define DCO2_ENV_POL_RED 14
#define DCO2_ENV_POL_GREEN 15

// // GP3

#define DCO2_PWM_LFO_SEL_RED 1
#define DCO2_PWM_LFO_SEL_GREEN 2

#define DCO2_PITCH_DYN_RED 6
#define DCO2_PITCH_DYN_GREEN 7

#define DCO2_PITCH_ENV_POL_RED 9
#define DCO2_PITCH_ENV_POL_GREEN 10
#define DCO2_PITCH_ENV_SOURCE_RED 11

#define DCO2_PITCH_ENV_SOURCE_GREEN 13
#define DCO2_PITCH_LFO_SEL_RED 14
#define DCO2_PITCH_LFO_SEL_GREEN 15

// // GP4

#define DCO1_PWM_LFO_SEL_RED 1
#define DCO1_PWM_LFO_SEL_GREEN 2

#define DCO1_PITCH_DYN_RED 6
#define DCO1_PITCH_DYN_GREEN 7

#define DCO1_PITCH_ENV_POL_RED 9
#define DCO1_PITCH_ENV_POL_GREEN 10
#define DCO1_PITCH_ENV_SOURCE_RED 11

#define DCO1_PITCH_ENV_SOURCE_GREEN 13
#define DCO1_PITCH_LFO_SEL_RED 14
#define DCO1_PITCH_LFO_SEL_GREEN 15

// // GP5

#define VCF_ENV_SOURCE_RED 0
#define VCF_ENV_SOURCE_GREEN 1
#define VCF_ENV_POL_RED 3
#define VCF_ENV_POL_GREEN 4
#define DCO_MIX_DYN_RED 6
#define DCO_MIX_DYN_GREEN 7

#define DCO_MIX_ENV_SOURCE_RED 9
#define DCO_MIX_ENV_SOURCE_GREEN 10
#define ENV5STAGE_SELECT_RED 11
#define ENV5STAGE_SELECT_GREEN 13
#define DCO_MIX_ENV_POL_RED 14
#define DCO_MIX_ENV_POL_GREEN 15

// // GP6

#define VCA_ENV_SOURCE_RED 2
#define VCA_ENV_SOURCE_GREEN 3
#define VCF_DYN_RED 6
#define VCF_DYN_GREEN 7

#define ADSR_SELECT_RED 9
#define ADSR_SELECT_GREEN 10
#define VCA_DYN_RED 11
#define VCA_DYN_GREEN 12
#define CHORUS_SELECT_RED 14
#define CHORUS_SELECT_GREEN 15

// // GP7

#define OCTAVE_UP_RED 4
#define OCTAVE_UP_GREEN 5
#define OCTAVE_DOWN_RED 6
#define OCTAVE_DOWN_GREEN 7

#define KEY_SINGLE_RED 12
#define KEY_SINGLE_GREEN 13
#define BEND_ENABLE_RED 14
#define BEND_ENABLE_GREEN 15

// // GP8

#define ASSIGN_UNI_RED 2
#define ASSIGN_UNI_GREEN 3
#define ASSIGN_MONO_RED 4
#define ASSIGN_MONO_GREEN 5
#define ASSIGN_POLY_RED 6
#define ASSIGN_POLY_GREEN 7

#define KEY_DUAL_RED 10
#define KEY_SPLIT_RED 12
#define KEY_SPECIAL_RED 14
#define KEY_SPECIAL_GREEN 15

//Teensy 4.1 Pins

#define RECALL_SW 33
#define SAVE_SW 34
#define SETTINGS_SW 35
#define BACK_SW 36

#define ENCODER_PINA 4
#define ENCODER_PINB 5

#define MUXCHANNELS 16
#define QUANTISE_FACTOR 1

#define DEBOUNCE 30

static int mux1ValuesPrev[MUXCHANNELS] = {};
static int mux2ValuesPrev[MUXCHANNELS] = {};
static int mux3ValuesPrev[MUXCHANNELS] = {};
static int mux4ValuesPrev[MUXCHANNELS] = {};

static int mux1Read = 0;
static int mux2Read = 0;
static int mux3Read = 0;
static int mux4Read = 0;

static byte muxInput = 0;

static long encPrevious = 0;

//These are pushbuttons and require debouncing

TButton saveButton{ SAVE_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton settingsButton{ SETTINGS_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton backButton{ BACK_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton recallButton{ RECALL_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };  // on encoder

Encoder encoder(ENCODER_PINB, ENCODER_PINA);  //This often needs the pins swapping depending on the encoder

void setupHardware() {

  adc->adc0->setAveraging(32);                                          // set number of averages 0, 4, 8, 16 or 32.
  adc->adc0->setResolution(8);                                          // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED);  // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);           // change the sampling speed

  //MUXs on ADC1
  adc->adc1->setAveraging(32);                                          // set number of averages 0, 4, 8, 16 or 32.
  adc->adc1->setResolution(8);                                          // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc1->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED);  // change the conversion speed
  adc->adc1->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);           // change the sampling speed

  //Mux address pins

  pinMode(MUX_0, OUTPUT);
  pinMode(MUX_1, OUTPUT);
  pinMode(MUX_2, OUTPUT);
  pinMode(MUX_3, OUTPUT);

  digitalWrite(MUX_0, LOW);
  digitalWrite(MUX_1, LOW);
  digitalWrite(MUX_2, LOW);
  digitalWrite(MUX_3, LOW);

  //Mux ADC
  pinMode(MUX1_S, INPUT_DISABLE);
  pinMode(MUX2_S, INPUT_DISABLE);
  pinMode(MUX3_S, INPUT_DISABLE);
  pinMode(MUX4_S, INPUT_DISABLE);

  //Switches
  pinMode(RECALL_SW, INPUT_PULLUP);  //On encoder
  pinMode(SAVE_SW, INPUT_PULLUP);
  pinMode(SETTINGS_SW, INPUT_PULLUP);
  pinMode(BACK_SW, INPUT_PULLUP);
}

void setupMCPoutputs() {

  mcp1.pinMode(1, OUTPUT);   // pin 1 = GPA7 of MCP2301X
  mcp1.pinMode(2, OUTPUT);   // pin 2 = GPA7 of MCP2301X
  mcp1.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp1.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp1.pinMode(8, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp1.pinMode(9, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp1.pinMode(11, OUTPUT);  // pin 11 = GPA7 of MCP2301X
  mcp1.pinMode(12, OUTPUT);  // pin 12 = GPA7 of MCP2301X
  mcp1.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp1.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp2.pinMode(1, OUTPUT);   // pin 1 = GPA7 of MCP2301X
  mcp2.pinMode(2, OUTPUT);   // pin 2 = GPA7 of MCP2301X
  mcp2.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp2.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp2.pinMode(8, OUTPUT);   // pin 8 = GPA7 of MCP2301X
  mcp2.pinMode(9, OUTPUT);   // pin 9 = GPA7 of MCP2301X
  mcp2.pinMode(10, OUTPUT);  // pin 10 = GPA7 of MCP2301X
  mcp2.pinMode(11, OUTPUT);  // pin 11 = GPA7 of MCP2301X
  mcp2.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp2.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp3.pinMode(1, OUTPUT);   // pin 1 = GPA7 of MCP2301X
  mcp3.pinMode(2, OUTPUT);   // pin 2 = GPA7 of MCP2301X
  mcp3.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp3.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp3.pinMode(9, OUTPUT);   // pin 9 = GPA7 of MCP2301X
  mcp3.pinMode(10, OUTPUT);  // pin 10 = GPA7 of MCP2301X
  mcp3.pinMode(11, OUTPUT);  // pin 11 = GPA7 of MCP2301X
  mcp3.pinMode(13, OUTPUT);  // pin 13 = GPA7 of MCP2301X
  mcp3.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp3.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp4.pinMode(1, OUTPUT);   // pin 1 = GPA7 of MCP2301X
  mcp4.pinMode(2, OUTPUT);   // pin 2 = GPA7 of MCP2301X
  mcp4.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp4.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp4.pinMode(9, OUTPUT);   // pin 9 = GPA7 of MCP2301X
  mcp4.pinMode(10, OUTPUT);  // pin 10 = GPA7 of MCP2301X
  mcp4.pinMode(11, OUTPUT);  // pin 11 = GPA7 of MCP2301X
  mcp4.pinMode(13, OUTPUT);  // pin 13 = GPA7 of MCP2301X
  mcp4.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp4.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp5.pinMode(0, OUTPUT);   // pin 0 = GPA7 of MCP2301X
  mcp5.pinMode(1, OUTPUT);   // pin 1 = GPA7 of MCP2301X
  mcp5.pinMode(3, OUTPUT);   // pin 3 = GPA7 of MCP2301X
  mcp5.pinMode(4, OUTPUT);   // pin 4 = GPA7 of MCP2301X
  mcp5.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp5.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp5.pinMode(9, OUTPUT);   // pin 9 = GPA7 of MCP2301X
  mcp5.pinMode(10, OUTPUT);  // pin 10 = GPA7 of MCP2301X
  mcp5.pinMode(11, OUTPUT);  // pin 11 = GPA7 of MCP2301X
  mcp5.pinMode(13, OUTPUT);  // pin 13 = GPA7 of MCP2301X
  mcp5.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp5.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp6.pinMode(2, OUTPUT);   // pin 1 = GPA7 of MCP2301X
  mcp6.pinMode(3, OUTPUT);   // pin 2 = GPA7 of MCP2301X
  mcp6.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp6.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp6.pinMode(9, OUTPUT);   // pin 9 = GPA7 of MCP2301X
  mcp6.pinMode(10, OUTPUT);  // pin 10 = GPA7 of MCP2301X
  mcp6.pinMode(11, OUTPUT);  // pin 11 = GPA7 of MCP2301X
  mcp6.pinMode(12, OUTPUT);  // pin 12 = GPA7 of MCP2301X
  mcp6.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp6.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp7.pinMode(4, OUTPUT);   // pin 4 = GPA7 of MCP2301X
  mcp7.pinMode(5, OUTPUT);   // pin 5 = GPA7 of MCP2301X
  mcp7.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp7.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp7.pinMode(12, OUTPUT);  // pin 12 = GPA7 of MCP2301X
  mcp7.pinMode(13, OUTPUT);  // pin 13 = GPA7 of MCP2301X
  mcp7.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp7.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X

  mcp8.pinMode(2, OUTPUT);   // pin 2 = GPA7 of MCP2301X
  mcp8.pinMode(3, OUTPUT);   // pin 3 = GPA7 of MCP2301X
  mcp8.pinMode(4, OUTPUT);   // pin 4 = GPA7 of MCP2301X
  mcp8.pinMode(5, OUTPUT);   // pin 5 = GPA7 of MCP2301X
  mcp8.pinMode(6, OUTPUT);   // pin 6 = GPA7 of MCP2301X
  mcp8.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp8.pinMode(10, OUTPUT);  // pin 10 = GPA7 of MCP2301X
  mcp8.pinMode(12, OUTPUT);  // pin 12 = GPA7 of MCP2301X
  mcp8.pinMode(14, OUTPUT);  // pin 14 = GPA7 of MCP2301X
  mcp8.pinMode(15, OUTPUT);  // pin 15 = GPA7 of MCP2301X
}
