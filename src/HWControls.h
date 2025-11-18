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

#define MUX1_S A10   // ADC0
#define MUX2_S A11   // ADC0
#define MUX3_S A12   // ADC1
#define MUX4_S A13   // ADC1

//Mux 1 Connections
#define MUX1_LFO1_WAVE 0
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
#define MUX1_DCO1_MODE 12
#define MUX1_SPARE_13 13
#define MUX1_SPARE_14 14
#define MUX1_SPARE_15 15

//Mux 2 Connections
#define MUX2_LFO2_WAVE 0
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
#define MUX2_SPARE_13 13
#define MUX2_SPARE_14 14
#define MUX2_SPARE_15 15

//Mux 3 Connections
#define MUX3_REVERB_MIX 0
#define MUX3_REVERB_DAMP 1
#define MUX3_REVERB_DECAY 2
#define MUX3_DRIFT 3
#define MUX3_VCA_VELOCITY 4
#define MUX3_VCA_RELEASE 5
#define MUX3_VCA_SUSTAIN 6
#define MUX3_VCA_DECAY 7
#define MUX3_VCF_SUSTAIN 8
#define MUX3_CONTOUR_AMOUNT 9
#define MUX3_VCF_RELEASE 10
#define MUX3_KB_TRACK 11
#define MUX3_MASTER_VOLUME 12
#define MUX3_VCF_VELOCITY 13
#define MUX3_MASTER_TUNE 14
#define MUX3_SPARE_15 15

//Mux 4 Connections
#define MUX4_REVERB_MIX 0
#define MUX4_REVERB_DAMP 1
#define MUX4_REVERB_DECAY 2
#define MUX4_DRIFT 3
#define MUX4_VCA_VELOCITY 4
#define MUX4_VCA_RELEASE 5
#define MUX4_VCA_SUSTAIN 6
#define MUX4_VCA_DECAY 7
#define MUX4_VCF_SUSTAIN 8
#define MUX4_CONTOUR_AMOUNT 9
#define MUX4_VCF_RELEASE 10
#define MUX4_KB_TRACK 11
#define MUX4_MASTER_VOLUME 12
#define MUX4_VCF_VELOCITY 13
#define MUX4_MASTER_TUNE 14
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

#define OSC1_OCT_BUTTON 0
#define OSC1_WAVE_BUTTON 1
#define OSC1_SUB_BUTTON 2
#define OSC2_WAVE_BUTTON 3
#define OSC2_XMOD_BUTTON 4
#define OSC2_EG_BUTTON 5
#define LFO1_WAVE_BUTTON 6
#define LFO2_WAVE_BUTTON 7
#define LFO3_WAVE_BUTTON 8
#define ENV_SEL_BUTTON 9
#define LFO_SEL_BUTTON 10
#define OSC1_LEV_SW 11
#define OSC2_DET_SW 12
#define OSC2_LEV_SW 13
#define OSC2_EG_SW 14
#define VCF_EG_SW 15
#define VCF_KEYF_SW 16
#define VCF_VEL_SW 17
#define VCA_VEL_SW 18

//void RotaryEncoderChanged (bool clockwise, int id);

void mainButtonChanged(Button *btn, bool released);

// I2C MCP23017 GPIO expanders

Adafruit_MCP23017 mcp1;
Adafruit_MCP23017 mcp2;
Adafruit_MCP23017 mcp3;
Adafruit_MCP23017 mcp4;
Adafruit_MCP23017 mcp5;
Adafruit_MCP23017 mcp6;


//Array of pointers of all MCPs
Adafruit_MCP23017 *allMCPs[] = {&mcp1, &mcp2, &mcp3, &mcp4, &mcp5, &mcp6};

// // My encoders
// /* Array of all rotary encoders and their pins */
RotaryEncOverMCP rotaryEncoders[] = {

};

// after your rotaryEncoders[] definition
constexpr size_t NUM_MCP = sizeof(allMCPs) / sizeof(allMCPs[0]);
constexpr int numMCPs = (int)(sizeof(allMCPs) / sizeof(*allMCPs));
constexpr int numEncoders = (int)(sizeof(rotaryEncoders) / sizeof(*rotaryEncoders));

Button osc1_oct_Button = Button(&mcp1, 8, OSC1_OCT_BUTTON, &mainButtonChanged);
Button osc1_wave_Button = Button(&mcp1, 9, OSC1_WAVE_BUTTON, &mainButtonChanged);
Button osc1_sub_Button = Button(&mcp1, 10, OSC1_SUB_BUTTON, &mainButtonChanged);
Button osc2_wave_Button = Button(&mcp2, 8, OSC2_WAVE_BUTTON, &mainButtonChanged);
Button osc2_xmod_Button = Button(&mcp2, 9, OSC2_XMOD_BUTTON, &mainButtonChanged);
Button osc2_eg_Button = Button(&mcp2, 10, OSC2_EG_BUTTON, &mainButtonChanged);
Button lfo1_wave_Button = Button(&mcp4, 4, LFO1_WAVE_BUTTON, &mainButtonChanged);
Button lfo2_wave_Button = Button(&mcp4, 5, LFO2_WAVE_BUTTON, &mainButtonChanged);
Button lfo3_wave_Button = Button(&mcp4, 12, LFO3_WAVE_BUTTON, &mainButtonChanged);
Button env_sel_Button = Button(&mcp5, 12, ENV_SEL_BUTTON, &mainButtonChanged);
Button lfo_sel_Button = Button(&mcp5, 13, LFO_SEL_BUTTON, &mainButtonChanged);
Button osc1_lev_Button = Button(&mcp3, 6, OSC1_LEV_SW, &mainButtonChanged);
Button osc2_det_Button = Button(&mcp3, 14, OSC2_DET_SW, &mainButtonChanged);
Button osc2_lev_Button = Button(&mcp4, 13, OSC2_LEV_SW, &mainButtonChanged);
Button osc2_egd_Button = Button(&mcp5, 4, OSC2_EG_SW, &mainButtonChanged);
Button vcf_eg_Button = Button(&mcp5, 5, VCF_EG_SW, &mainButtonChanged);
Button vcf_keyf_Button = Button(&mcp5, 6, VCF_KEYF_SW, &mainButtonChanged);
Button vcf_vel_Button = Button(&mcp6, 6, VCF_VEL_SW, &mainButtonChanged);
Button vca_vel_Button = Button(&mcp6, 14, VCA_VEL_SW, &mainButtonChanged);

Button *mainButtons[] = {
        &osc1_oct_Button, &osc1_wave_Button, &osc1_sub_Button, &osc2_wave_Button, &osc2_xmod_Button, &osc2_eg_Button, &lfo1_wave_Button, &lfo2_wave_Button, &lfo3_wave_Button, &env_sel_Button, &lfo_sel_Button,
        &osc1_lev_Button, &osc2_det_Button, &osc2_lev_Button, &osc2_egd_Button, &vcf_eg_Button, &vcf_keyf_Button, &vcf_vel_Button, &vca_vel_Button,
};

Button *allButtons[] = {
        &osc1_oct_Button, &osc1_wave_Button, &osc1_sub_Button, &osc2_wave_Button, &osc2_xmod_Button, &osc2_eg_Button,
        &lfo1_wave_Button, &lfo2_wave_Button, &lfo3_wave_Button, &env_sel_Button, &lfo_sel_Button,
        &osc1_lev_Button, &osc2_det_Button, &osc2_lev_Button, &osc2_egd_Button, &vcf_eg_Button, &vcf_keyf_Button, &vcf_vel_Button, &vca_vel_Button
};

// an array of vectors to hold pointers to the encoders on each MCP
std::vector<RotaryEncOverMCP*> encByMCP[NUM_MCP];

// // GP1

#define NOTE_PRIORITY_GREEN 7

#define NOTE_PRIORITY_RED 15

// // GP2

#define PLAY_MODE_GREEN 7

#define PLAY_MODE_RED 15

// // GP3

#define FILTER_VELOCITY_RED 7

#define EG_INVERT_LED 15

// // GP4

#define C_OCTAVE_GREEN 6
#define C_OCTAVE_RED 7

#define FILTER_POLE_RED 14
#define KEYTRACK_RED 15

// // GP5

#define A_OCTAVE_GREEN 6
#define A_OCTAVE_RED 7

#define B_OCTAVE_GREEN 14
#define B_OCTAVE_RED 15

// // GP6

#define FM_C_GREEN 6
#define FM_C_RED 7

#define FM_B_GREEN 14
#define FM_B_RED 15

//Teensy 4.1 Pins

#define RECALL_SW 33
#define SAVE_SW 34
#define SETTINGS_SW 35
#define BACK_SW 36

#define ENCODER_PINA 4
#define ENCODER_PINB 5

#define MUXCHANNELS 16
#define QUANTISE_FACTOR 3

#define DEBOUNCE 30

static int mux1ValuesPrev[MUXCHANNELS] = {};
static int mux2ValuesPrev[MUXCHANNELS] = {};
static int mux3ValuesPrev[MUXCHANNELS] = {};
static int mux4ValuesPrev[MUXCHANNELS] = {};

static byte muxInput = 0;

static int mux1Read = 0;
static int mux2Read = 0;
static int mux3Read = 0;
static int mux4Read = 0;

static long encPrevious = 0;

//These are pushbuttons and require debouncing

TButton saveButton{ SAVE_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton settingsButton{ SETTINGS_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton backButton{ BACK_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION };
TButton recallButton{ RECALL_SW, LOW, HOLD_DURATION, DEBOUNCE, CLICK_DURATION }; // on encoder

Encoder encoder(ENCODER_PINB, ENCODER_PINA);  //This often needs the pins swapping depending on the encoder

void setupHardware() {

  //Volume Pot is on ADC0
  adc->adc0->setAveraging(32);                                          // set number of averages 0, 4, 8, 16 or 32.
  adc->adc0->setResolution(8);                                         // set bits of resolution  8, 10, 12 or 16 bits.
  adc->adc0->setConversionSpeed(ADC_CONVERSION_SPEED::VERY_LOW_SPEED);  // change the conversion speed
  adc->adc0->setSamplingSpeed(ADC_SAMPLING_SPEED::MED_SPEED);           // change the sampling speed

  //MUXs on ADC1
  adc->adc1->setAveraging(32);                                          // set number of averages 0, 4, 8, 16 or 32.
  adc->adc1->setResolution(8);                                         // set bits of resolution  8, 10, 12 or 16 bits.
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
  mcp1.pinMode(7, OUTPUT);   // pin 7 = GPA7 of MCP2301X
  mcp1.pinMode(15, OUTPUT);  // pin 15 = GPB7 of MCP2301X

}
