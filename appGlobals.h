// Global VoiceChanger declarations
//
// s60sc 2022

#pragma once
#include "globals.h"

/******************** User modifiable defines *******************/

#define ALLOW_SPACES true // set true to allow whitespace in configs.txt key values

// web server ports
#define HTTP_PORT 80 // app control
#define HTTPS_PORT 443 // secure app control

//#define USE_WS2812

/*********************** Fixed defines leave as is ***********************/ 
/** Do not change anything below here unless you know what you are doing **/    

//#define DEV_ONLY // leave commented out
#define STATIC_IP_OCTAL "152" // dev only
#define DEBUG_MEM false // leave as false
#define FLUSH_DELAY 0 // for debugging crashes
#define DBG_ON false // esp debug output
#define DOT_MAX 50
#define HOSTNAME_GRP 0

#define APP_NAME "VoiceChanger" // max 15 chars
#define APP_VER "1.3"

#define HTTP_CLIENTS 2 // http, ws
#define MAX_STREAMS 0
#define INDEX_PAGE_PATH DATA_DIR "/VC" HTML_EXT
#define FILE_NAME_LEN 64
#define IN_FILE_NAME_LEN 128
#define JSON_BUFF_LEN (2 * 1024) // set big enough to hold json string
#define MAX_CONFIGS 80 // > number of entries in configs.txt
#define GITHUB_PATH "/s60sc/ESP32-VoiceChanger/main"

#define STORAGE LittleFS // One of LittleFS or SD_MMC 
#define RAMSIZE (1024 * 8) 
#define CHUNKSIZE (1024 * 4)
#define MIN_RAM 8 // min object size stored in ram instead of PSRAM default is 4096
#define MAX_RAM 4096 // max object size stored in ram instead of PSRAM default is 4096
#define TLS_HEAP (64 * 1024) // min free heap for TLS session
#define WARN_HEAP (32 * 1024) // low free heap warning
#define WARN_ALLOC (16 * 1024) // low free max allocatable free heap block
#define MAX_ALERT 1024

#define INCLUDE_FTP_HFS false // ftp.cpp (file upload)
#define INCLUDE_SMTP false    // smtp.cpp (email)
#define INCLUDE_MQTT false    // mqtt.cpp
#define INCLUDE_TGRAM false   // telegram.cpp
#define INCLUDE_CERTS false   // certificates.cpp (https and server certificate checking)
#define INCLUDE_WEBDAV true   // webDav.cpp (WebDAV protocol)
#define INCLUDE_AUDIO true    // audio.cpp (microphone and speaker)

#define ISVC // VC specific code in generics
#define IS_IO_EXTENDER false // must be false except for IO_Extender
#define EXTPIN 100

// to determine if newer data files need to be loaded
#define CFG_VER 2

#ifdef CONFIG_IDF_TARGET_ESP32S3 
#define SERVER_STACK_SIZE (1024 * 8)
#define DS18B20_STACK_SIZE (1024 * 2)
#define STICK_STACK_SIZE (1024 * 4)
#else
#define SERVER_STACK_SIZE (1024 * 4)
#define DS18B20_STACK_SIZE (1024)
#define STICK_STACK_SIZE (1024 * 2)
#endif
#define BATT_STACK_SIZE (1024 * 2)
#define EMAIL_STACK_SIZE (1024 * 6)
#define FS_STACK_SIZE (1024 * 4)
#define LOG_STACK_SIZE (1024 * 3)
#define AUDIO_STACK_SIZE (1024 * 4)
#define MICREM_STACK_SIZE (1024 * 2)
#define MQTT_STACK_SIZE (1024 * 4)
#define PING_STACK_SIZE (1024 * 5)
#define SERVO_STACK_SIZE (1024)
#define SUSTAIN_STACK_SIZE (1024 * 4)
#define TGRAM_STACK_SIZE (1024 * 6)
#define TELEM_STACK_SIZE (1024 * 4)
#define UART_STACK_SIZE (1024 * 2)

// task priorities
#define HTTP_PRI 5
#define AUDIO_PRI 5
#define MICREM_PRI 5
#define STICK_PRI 5
#define LED_PRI 1
#define SERVO_PRI 1
#define LOG_PRI 1
#define BATT_PRI 1
#define IDLEMON_PRI 5

#define FILE_EXT "wav"
#define DMA_BUFF_LEN 1024 // used for I2S buffer size
#define DMA_BUFF_CNT 4
#define REVERB_SAMPLES 1600
#define OSAMP 4 // 4 for moderate quality, 32 for best quality
#define MIC_GAIN_CENTER 3 // mid point


/******************** Function declarations *******************/

enum audioAction {NO_ACTION, UPDATE_CONFIG, RECORD_ACTION, PLAY_ACTION, PASS_ACTION, WAV_ACTION, STOP_ACTION};

// global app specific functions
void applyFilters();
void applyVolume();
void audioRequest(audioAction doAction);
int8_t checkPotVol(int8_t adjVol);
void displayAudioLed(int16_t audioSample);
uint8_t getBrightness();
void ledBarGauge(float level);
void makeRecordingRem(bool isRecording);
void outputRec();
void passThru();
void playRecording();
bool prepAudio();
void prepPeripherals();
void remoteMicHandler(uint8_t* wsMsg, size_t wsMsgLen);
void restartI2S();
void setLamp(uint8_t lampVal);
void setupAudioLed();
void setupFilters();
void setupVC();
void setupWeb();
void smbPitchShiftInit(float _pitchShift, long _fftFrameSize, long _osamp, float sampleRate);
void smbPitchShift(size_t numSampsToProcess, int16_t *indata, int16_t *outdata);
size_t updateWavHeader();
void updateVars(const char* jsonKey, const char* jsonVal); 
void wsJsonSend(const char* keyStr, const char* valStr);

/******************** Global app declarations *******************/

extern const char* appConfig;

extern int mampBckIo;
extern int mampSwsIo;
extern int mampSdIo;
extern int micSckPin;
extern int micSWsPin;
extern int micSdPin;
extern bool I2Smic;
extern bool I2Samp;
extern bool micUse;
extern bool micRem;

// record & play button pins
extern int buttonPlayPin;
extern int buttonRecPin;
extern int buttonPassPin;
extern int buttonStopPin; 
extern int sanalogPin; 
extern int saudioLedPin; 
extern int saudioClockPin; 
extern int saudioDataPin;
extern int ledBarClock;
extern int ledBarData;
extern int switchModePin;
extern bool volatile stopAudio;
extern TaskHandle_t audioHandle;

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
extern int micGain; // microphone preamplification factor
extern int8_t ampVol; // amplifier volume factor
extern uint8_t BRIGHTNESS; // audio led level brightness
extern bool DISABLE; // temporarily disable filter settings on browser

// other settings
extern bool USE_POT; // whether external volume control potentiometer being used
extern uint32_t SAMPLE_RATE; // audio rate in Hz
extern int16_t* sampleBuffer; // audio samples output buffer
extern const size_t sampleBytes;
extern uint8_t* audioBuffer; // store recording
extern volatile audioAction THIS_ACTION;
extern bool lampUse; // true to audio led
extern bool ledBarUse; // true to MY9921 led bar
extern int lampPin; // if useLamp is true

// I2S port settings (I2S_NUM_1 does not support PDM or ADC)
extern i2s_port_t I2S_MIC_PORT;
extern i2s_port_t I2S_AMP_PORT;
