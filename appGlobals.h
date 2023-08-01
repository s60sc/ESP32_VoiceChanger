// Global VoiceChanger declarations
//
// s60sc 2022

#pragma once
#include "globals.h"

/******************** User modifiable defines *******************/

// MAX9814 ADC microphone pinout, Gain & AR can be left disconnected
#ifdef CONFIG_IDF_TARGET_ESP32
#define I2S_ADC_UNIT    ADC_UNIT_1
#define I2S_ADC_CHANNEL ADC1_CHANNEL_5 // Out pin GPIO37
#endif

// ICSK025A DAC amplifier (LM386)
// use right channel 
#ifdef CONFIG_IDF_TARGET_ESP32
#define I2S_DAC_CHANNEL I2S_DAC_CHANNEL_RIGHT_EN // GPIO25 (J5+ pin)
#endif

#define ALLOW_SPACES false // set true to allow whitespace in configs.txt key values

// web server ports
#define WEB_PORT 80 // app control
#define OTA_PORT (WEB_PORT + 1) // OTA update

/*********************** Fixed defines leave as is ***********************/ 
/** Do not change anything below here unless you know what you are doing **/    

//#define DEV_ONLY // leave commented out
#define STATIC_IP_OCTAL "152" // dev only
#define CHECK_MEM false // leave as false
#define FLUSH_DELAY 200 // for debugging crashes
 
#define APP_NAME "VoiceChanger" // max 15 chars
#define APP_VER "1.2.2"

#define MAX_CLIENTS 3 // allowing too many concurrent web clients can cause errors
#define INDEX_PAGE_PATH DATA_DIR "/VC" HTML_EXT
#define FILE_NAME_LEN 64
#define JSON_BUFF_LEN (2 * 1024) // set big enough to hold json string
#define MAX_CONFIGS 75 // > number of entries in configs.txt
#define GITHUB_URL "https://raw.githubusercontent.com/s60sc/ESP32-VoiceChanger/master"

#define STORAGE LittleFS // One of LittleFS or SD_MMC 
#define RAMSIZE (1024 * 8) 
#define CHUNKSIZE (1024 * 4)
#define RAM_LOG_LEN 5000 // size of ram stored system message log in bytes
//#define INCLUDE_FTP 
//#define INCLUDE_SMTP
//#define INCLUDE_SD
//#define INCLUDE_MQTT

#define IS_IO_EXTENDER false // must be false except for IO_Extender
#define EXTPIN 100

// to determine if newer data files need to be loaded
#define HTM_VER "1"
#define JS_VER "0"
#define CFG_VER "1"

#define FILE_EXT "wav"
#define DMA_BUFF_LEN 1024 // used for I2S buffer size
#define WAV_HDR_LEN 44
#define DMA_BUFF_CNT 4
#define REVERB_SAMPLES 1600
#define OSAMP 4 // 4 for moderate quality, 32 for best quality


/******************** Function declarations *******************/

enum deviceType {I2S_TYPE = 0, PDM_TYPE = 1, ADC_TYPE = 2};
enum actionType {NO_ACTION, UPDATE_CONFIG, RECORD_ACTION, PLAY_ACTION, PASS_ACTION, WAV_ACTION, STOP_ACTION};

// global app specific functions
void actionRequest(actionType doAction);
void applyFilters(int8_t volume);
size_t buildWavHeader();
int8_t getVolumeControl();
void outputRec();
void setupFilters();
void setupVC();
void setupWeb();
void smbPitchShiftInit(float _pitchShift, long _fftFrameSize, long _osamp, float sampleRate);
void smbPitchShift(size_t numSampsToProcess, int16_t *indata, int16_t *outdata);
void updateVars(const char* jsonKey, const char* jsonVal); 

/******************** Global app declarations *******************/

extern int ampBckIo;
extern int ampWsIo;
extern int ampSdIo;
extern int amicSckIo;
extern int amicWsIo;
extern int amicSdIo;

// record & play button pins
extern int buttonPlayPin;
extern int buttonRecPin;
extern int buttonPassPin;
extern int buttonStopPin; 
extern int sanalogPin; 
extern int saudioLedPin; 
extern int switchModePin;

// web filter parameters
extern bool RING_MOD;
extern bool BAND_PASS;
extern bool HIGH_PASS;
extern bool LOW_PASS;
extern bool HIGH_SHELF;
extern bool LOW_SHELF;
extern bool PEAK;
extern bool CLIPPING;
extern bool REVERB;
extern float BP_Q;    // sharpness of filter
extern float BP_FREQ; // center frequency for band pass
extern uint16_t BP_CAS; // number of cascaded filters
extern float HP_Q;    // sharpness of filter
extern float HP_FREQ; // high frequency for high pass
extern uint16_t HP_CAS; // number of cascaded filters
extern float LP_Q;    // sharpness of filter
extern float LP_FREQ; // low frequency for low pass
extern uint16_t LP_CAS; // number of cascaded filters
extern float HS_GAIN; // amplify gain above frequency
extern float HS_FREQ; // high frequency for high shelf
extern float LS_GAIN; // amplify gain below frequency
extern float LS_FREQ; // low frequency for low shelf
extern float PK_GAIN; // amplify gain around frequency
extern float PK_FREQ; // center frequency for peak
extern float PK_Q;    // sharpness of filter
extern int SW_FREQ;    // frequency of generated sine wave
extern uint8_t SW_AMP;    // amplitude of generated sine wave
extern int CLIP_FACTOR; // factor used to compress high volume
extern int DECAY_FACTOR; // factor used control reverb decay
extern float PITCH_SHIFT; // factor used shift pitch up or down

// other web settings
extern uint8_t PREAMP_GAIN; // microphone preamplification factor
extern int8_t AMP_VOL; // amplifier volume factor
extern uint8_t BRIGHTNESS; // audio led level brightness
extern bool DISABLE; // temporarily disable filter settings on browser

// other settings
extern bool USE_POT; // whether external volume control potentiometer being used
extern uint16_t SAMPLE_RATE; // audio rate in Hz
extern int16_t audBuffer[]; // audio samples output buffer
extern const size_t audBytes;
extern uint8_t* recordBuffer; // store recording
extern volatile actionType THIS_ACTION;

// define device types being used
extern deviceType MIC_TYPE;
extern deviceType AMP_TYPE;

// I2S port settings (I2S_NUM_1 does not support PDM or ADC)
extern i2s_port_t I2S_MIC_PORT;
extern i2s_port_t I2S_AMP_PORT;

/************************** structures ********************************/

struct WavHeader_Struct {
  char Riff[4]; // 'RIFF'
  uint32_t FileSize; // WAV_HDR_LEN + DataSize - 8
  char FileFormat[4]; // 'WAVE'   
  char FormatChunk[4]; //'fmt ' 
  uint32_t ChunkLen; // 16
  uint16_t FormatType;  // 1        
  uint16_t NumChannels; // 1 (mono)
  uint32_t SampleRate;     
  uint32_t AvgBytesPerSec ; // SampleRate * BlockAlign
  uint16_t BlockAlign ; // 2 : BitsPerSample * NumChannels / 8  
  uint16_t BitsPerSample; // 16    
  char DataChunk[4]; // 'data'
  uint32_t DataSize;         
};
