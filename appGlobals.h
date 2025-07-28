// Global VoiceChanger declarations
//
// s60sc 2022

#pragma once
#include "globals.h"

/******************** User modifiable defines *******************/

#define INCLUDE_RTSP false // allow RTSP (see rtsp.cpp)

#define ALLOW_SPACES false // set true to allow whitespace in configs.txt key values

// web server ports
#define HTTP_PORT 80 // app control
#define HTTPS_PORT 443 // secure app control

/*********************** Fixed defines leave as is ***********************/ 
/** Do not change anything below here unless you know what you are doing **/

#define STATIC_IP_OCTAL "152" // dev only
#define DEBUG_MEM false // leave as false
#define FLUSH_DELAY 0 // for debugging crashes
#define DBG_ON false // esp debug output
#define DOT_MAX 50
#define HOSTNAME_GRP 0
#define USE_IP6 false

#define APP_NAME "VoiceChanger" // max 15 chars
#define APP_VER "1.8"

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

#define INCLUDE_WEBDAV true   // webDav.cpp (WebDAV protocol)
#define INCLUDE_AUDIO true    // audio.cpp (microphone and speaker)
#define INCLUDE_PERIPH true   // peripherals.cpp

#define ISVC // VC specific code in generics

// to determine if newer data files need to be loaded
#define CFG_VER 5

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
#define STICK_PRI 5
#define LED_PRI 1
#define SERVO_PRI 1
#define LOG_PRI 1
#define BATT_PRI 1

#define FILE_EXT "wav"
#define DMA_BUFF_LEN 1024 // used for I2S buffer size
#define DMA_BUFF_CNT 4
#define REVERB_SAMPLES 1600
#define OSAMP 4 // 4 for moderate quality, 32 for best quality
#define MIC_GAIN_CENTER 3 // mid point



/******************** Function declarations *******************/

enum audioAction {NO_ACTION, UPDATE_CONFIG, RECORD_ACTION, PLAY_ACTION, PASS_ACTION, WAV_ACTION, STOP_ACTION};
enum stepperModel {BYJ_48, BIPOLAR_8mm};

// global app specific functions
void applyFilters();
void applyVolume();
void browserMicInput(uint8_t* wsMsg, size_t wsMsgLen);
int8_t checkPotVol(int8_t adjVol);
void closeI2S();
void displayAudioLed(int16_t audioSample);
uint8_t getBrightness();
void ledBarGauge(float level);
void prepAudio();
void prepPeripherals();
void prepRTSP();
void setI2Schan(int whichChan);
void setLamp(uint8_t lampVal);
void setupAudioLed();
void setupFilters();
void setupVC();
void setupWeb();
void smbPitchShiftInit(float _pitchShift, long _fftFrameSize, long _osamp, float sampleRate);
void smbPitchShift(size_t numSampsToProcess, int16_t *indata, int16_t *outdata);
void stepperDone();
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
extern bool micRem;
extern bool spkrRem;
extern TaskHandle_t audioHandle;

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
extern SemaphoreHandle_t audioSemaphore;

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
extern uint8_t* audioBuffer; // streaming
extern size_t audioBytes;
extern volatile audioAction THIS_ACTION;
extern bool ledBarUse; // true for MY9921 led bar
extern int lampPin; // if useLamp is true
extern uint8_t* recAudioBuffer; // store recording
extern size_t recAudioBytes;

// RTSP 
extern int quality; // Variable to hold quality for RTSP frame
extern bool rtspVideo;
extern bool rtspAudio;
extern bool rtspSubtitles;
extern int rtspPort;
extern uint16_t rtpVideoPort;
extern uint16_t rtpAudioPort;
extern uint16_t rtpSubtitlesPort;
extern char RTP_ip[];
extern uint8_t rtspMaxClients;
extern uint8_t rtpTTL;
extern char RTSP_Name[];
extern char RTSP_Pass[];

// n/a
extern bool streamVid;
extern bool streamAud;
extern bool streamSrt;
