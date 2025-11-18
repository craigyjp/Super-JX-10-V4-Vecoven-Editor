//MIDI CC control numbers
//These broadly follow standard CC assignments
#define CCmodWheelinput 1 //pitch LFO amount - less from mod wheel

// Encoders
#define CCmasterVolume 7

#define CCdco1_range      0x00
#define CCdco1_wave       0x01
#define CCdco1_tune       0x02
#define CCdco1_pitch_lfo  0x03

#define CCdco1_pitch_env 0x05

#define CCdco2_range    0x08
#define CCdco2_wave     0x09
#define CCdco2_tune     0x0A
#define CCdco2_pitch_lfo 0x0B

#define CCdco2_pitch_env 0x0D

#define CCdco1_mode     0x10
#define CCdco2_fine     0x11

#define CCdco1_PW       0x14
#define CCdco1_PWM_env  0x15
#define CCdco1_PWM_lfo  0x16

#define CCdco2_PW       0x1A
#define CCdco2_PWM_env  0x1B
#define CCdco2_PWM_lfo  0x1C



#define CClfo1_wave   0x30
#define CClfo1_delay  0x31
#define CClfo1_rate   0x32
#define CClfo1_lfo2   0x33

#define CClfo2_wave   0x35
#define CClfo2_delay  0x36
#define CClfo2_rate   0x37
#define CClfo2_lfo1   0x38

// #define CCampLFODepth 10
// #define CCMWDepth 11
// #define CCPBDepth 12
// #define CCATDepth 13
// #define CCeffectPot1 14
// #define CCeffectPot2 15
// #define CCeffectPot3 16
// #define CCLFO1Rate 17
// #define CCLFO2Rate 18
// #define CCLFO1Delay 19

// #define CCampAttack 20
// #define CCampDecay 21
// #define CCampSustain 22
// #define CCampRelease 23
// #define CCfilterAttack 24
// #define CCfilterDecay 25
// #define CCfilterSustain 26
// #define CCfilterRelease 27
// #define CCpitchAttack 28
// #define CCpitchDecay 29
// #define CCpitchSustain 30
// #define CCpitchRelease 31

// #define CCfilterResonance 71
// #define CCfilterKeyTrack 33
// #define CCnoiseLevel 34
// #define CCfilterCutoff 74
// #define CCfilterEGDepth 36
// #define CCvcoCFMDepth 37
// #define CCvcoBDetune 38
// #define CCvcoCDetune 39
// #define CCfilterLFODepth 40
// #define CCvcoAFMDepth 41
// #define CCvcoBFMDepth 42
// #define CCeffectsMix 43
// #define CCvcoALevel 44
// #define CCvcoBLevel 45
// #define CCvcoCLevel 46
// #define CCvcoAPW 47
// #define CCvcoBPW 48
// #define CCvcoCPW 49
// #define CCvcoAPWM 50
// #define CCvcoBPWM 51
// #define CCvcoCPWM 52
// #define CCvcoAWave 53
// #define CCvcoBWave 54
// #define CCvcoCWave 55
// #define CCvcoAInterval 56
// #define CCvcoBInterval 57
// #define CCvcoCInterval 58
// #define CCXModDepth 59

// // Buttons
// #define CCnoiseLevelSW 69
// #define CCvcoAPWMsource 70
// #define CCvcoBPWMsource 32
// #define CCvcoCPWMsource 72

// #define CCvcoAFMsource 73
// #define CCvcoBFMsource 35
// #define CCvcoCFMsource 75

// #define CCLFO1Wave 76
// #define CCLFO2Wave 77

// #define CCfilterLFODepthSW 78
// #define CCampLFODepthSW 79

// #define CCfilterType 80
// #define CCfilterPoleSW 81

// #define CCvcoAOctave 82
// #define CCvcoBOctave 83
// #define CCvcoCOctave 84
// #define CCfilterEGDepthSW 85
// #define CCfilterKeyTrackSW 86
// #define CCfilterVelocitySW 87
// #define CCampVelocitySW 88

// #define CCFMSyncSW 89
// #define CCPWSyncSW 90
// #define CCPWMSyncSW 91
// #define CCmultiSW 92

// #define CCeffects3SW 93

// #define CCeffectNumSW 94
// #define CCeffectBankSW 95
// #define CCegInvertSW 96
// #define CCfilterKeyTrackZeroSW 97

// #define CCvcoATable 98
// #define CCvcoBTable 99
// #define CCvcoCTable 100
// #define CCplayModeSW 101
// #define CCnotePrioritySW 102
// #define CCeffectsMixSW 103


#define CCallnotesoff 123//Panic button
