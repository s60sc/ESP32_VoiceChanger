// Global declarations
//
// s60sc 2021

#pragma once

/******************** User modifiable defines *******************/

#define MIC_TIMEOUT 10 // ms for mic read abandoned if no input
#define AMP_TIMEOUT 1000 // ms for amp write abandoned if no output

// I2S Microphone pins
// INMP441 I2S microphone pinout, connect L/R to GND for left channel
// MP34DT01 PDM microphone pinout, connect SEL to GND for left channel
#define MIC_SCK_IO 4 // I2S SCK
#define MIC_WS_IO 12 // I2S WS, PDM CLK
#define MIC_SD_IO 33 // I2S SD, PDM DAT, ADC  

// MAX9814 ADC microphone pinout, Gain & AR can be left disconnected
#define I2S_ADC_UNIT    ADC_UNIT_1
#define I2S_ADC_CHANNEL ADC1_CHANNEL_5 // Out pin GPIO33

// I2S Amplifier pins
// MAX98357A I2S, GAIN see notes, leave as mono
// Gain: 100k to GND works, not direct to GND
#define AMP_WS_IO  21 // I2S LRCLK  [breakout 5]
#define AMP_BCK_IO 22 // I2S BCLK   [breakout 8]
#define AMP_SD_IO  25 // I2S DIN    [breakout 18]

// ICSK025A amplifier (LM386) DAC)
// use right channel 
#define I2S_DAC_CHANNEL I2S_DAC_CHANNEL_RIGHT_EN // GPIO25 (J5+ pin)

// record & play button pins
#define BUTTON_PLAY_PIN 15
#define BUTTON_REC_PIN  14
#define BUTTON_PASS_PIN 13
#define BUTTON_STOP_PIN  3 
#define ANALOG_PIN 36 // for volume & brightness potentiometer control, must be an analog pin
#define AUDIO_LED_PIN 23 // pin for audio controlled output lights
#define SWITCH_MODE_PIN 26 // switch use of pot between vol (high) & brightness (low)

// Wifi - utils.cpp
#define MAX_CLIENTS 2 // allowing too many concurrent web clients can cause errors
#define ALLOW_AP true  // set to true to allow AP to startup if cannot reconnect to STA (router)
#define AP_PASSWD "123456789" // wifi AP password
#define WIFI_TIMEOUT_MS (30 * 1000) // how often to check wifi status
#define RESPONSE_TIMEOUT 10000  // time to wait for FTP or SMTP response

#define STORAGE SPIFFS // use of SPIFFS or SD_MMC 

// batt monitoring - utils.cpp
#define VOLTAGE_DIVIDER 0 // for use with pin 33 if non zero, see battery monitoring in utils.cpp                                                                   
#define LOW_VOLTAGE 3.0 // voltage level at which to send out email alert
#define BATT_INTERVAL 5 // interval in minutes to check battery voltage

#define ALLOW_SPACES false // set true to allow whitespace in configs.txt
//#define USE_LOG_COLORS  // uncomment to colorise log messages (eg if using idf.py, but not arduino)

/** Do not change anything below here unless you know what you are doing **/

/********************* fixed defines leave as is *******************/ 
 
#define APP_NAME "VoiceChanger" // max 15 chars
#define APP_VER "1.1"

#define DATA_DIR "/data"
#define HTML_EXT ".htm"
#define TEXT_EXT ".txt"
#define JS_EXT ".js"
#define CSS_EXT ".css"
#define INDEX_PAGE_PATH DATA_DIR "/VC" HTML_EXT
#define CONFIG_FILE_PATH DATA_DIR "/VCconfigs" TEXT_EXT
#define LOG_FILE_PATH DATA_DIR "/log" TEXT_EXT
#define FILE_NAME_LEN 64
#define ONEMEG (1024 * 1024)
#define MAX_PWD_LEN 64
#define JSON_BUFF_LEN (2 * 1024) // set big enough to hold json string
#define GITHUB_URL "https://raw.githubusercontent.com/s60sc/ESP32-VoiceChanger/master"

#define FILE_EXT "wav"
#define DMA_BUFF_LEN 1024 // used for I2S buffer size
#define WAV_HDR_LEN 44
#define ADC_SAMPLES 4
#define DMA_BUFF_CNT 4
#define RAMSIZE (8 * 1024) // set this to multiple of SD card sector size (512 or 1024 bytes)
#define REVERB_SAMPLES 1600

#define FILLSTAR "****************************************************************"
#define FLUSH_DELAY 0 // for debugging crashes
//#define INCLUDE_FTP 
//#define INCLUDE_SMTP
//#define DEV_ONLY // leave commented out

enum deviceType {I2S_TYPE = 0, PDM_TYPE = 1, ADC_TYPE = 2};
enum actionType {NO_ACTION, UPDATE_CONFIG, RECORD_ACTION, PLAY_ACTION, PASS_ACTION, WAV_ACTION, STOP_ACTION};

/******************** Libraries *******************/

#include "Arduino.h"
#include <driver/i2s.h>
#include "esp_adc_cal.h"
#include "esp_http_server.h"
#include <ESPmDNS.h> 
#include "lwip/sockets.h"
#include <map>
#include "ping/ping_sock.h"
#include <Preferences.h>
#include <regex>
#include <SD_MMC.h>
#include <SPIFFS.h> 
#include <sstream>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>


/******************** Function declarations *******************/

// global app specific functions
void actionRequest(actionType doAction);
void applyFilters(int8_t volume);
size_t buildWavHeader();
void outputRec();
void setupFilters();
void setupWeb();
void updateVars(const char* jsonKey, const char* jsonVal); 


// global general utility functions in utils.cpp / utilsSD.cpp
void buildJsonString(bool quick);
bool checkDataFiles();
bool checkFreeSpace();
void checkMemory();
void dateFormat(char* inBuff, size_t inBuffLen, bool isFolder);
void deleteFolderOrFile(const char* deleteThis);
void devSetup();
void doRestart();
void emailAlert(const char* _subject, const char* _message);
void flush_log(bool andClose = false);
bool getLocalNTP();
void getOldestDir(char* oldestDir);
void listBuff(const uint8_t* b, size_t len); 
bool listDir(const char* fname, char* jsonBuff, size_t jsonBuffLen, const char* extension);
bool loadConfig();
void logPrint(const char *fmtStr, ...);
void OTAprereq();
bool prepSD_MMC();
void remote_log_init();
void removeChar(char *s, char c);
void reset_log();
void setupADC();
void showProgress();
void startOTAtask();
void startSecTimer(bool startTimer);
bool startSpiffs(bool deleteAll = false);
void startWebServer();
bool startWifi();
void syncToBrowser(const char *val);
bool updateStatus(const char* variable, const char* value);
void urlDecode(char* inVal);


/******************** Global utility declarations *******************/

extern String AP_SSID;
extern char   AP_Pass[];
extern char   AP_ip[];
extern char   AP_sn[];
extern char   AP_gw[];

extern char hostName[]; //Host name for ddns
extern char ST_SSID[]; //Router ssid
extern char ST_Pass[]; //Router passd

extern char ST_ip[]; //Leave blank for dhcp
extern char ST_sn[];
extern char ST_gw[];
extern char ST_ns1[];
extern char ST_ns2[];

#ifdef INCLUDE_FTP 
// ftp server
extern char ftp_server[];
extern char ftp_user[];
extern uint16_t ftp_port;
extern char ftp_pass[];
extern char ftp_wd[];
#endif 

#ifdef INCLUDE_SMTP
// SMTP server
extern char smtp_login[];
extern char smtp_pass[];
extern char smtp_email[];
extern char smtp_server[];
extern uint16_t smtp_port;
#endif
extern size_t smtpBufferSize;
extern byte* SMTPbuffer;

extern char timezone[];
extern char* jsonBuff; 
extern bool dbgVerbose;
extern byte logMode;
extern bool timeSynchronized;
extern const char* defaultPage_html;

/******************** Global app declarations *******************/

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

// other web settings
extern uint8_t PREAMP_GAIN; // microphone preamplification factor
extern int8_t AMP_VOL; // amplifier volume factor
extern uint8_t BRIGHTNESS; // audio led level brightness
extern bool DISABLE; // temporarily disable filter settings on browser

// other settings
extern bool USE_POT; // whether external volume control potentiometer being used
extern uint16_t SAMPLE_RATE; // audio rate in Hz
extern int16_t SAMPLE_BUFFER[DMA_BUFF_LEN]; // audio samples output buffer
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


/*********************** Log formatting ************************/

#ifdef USE_LOG_COLORS
#define LOG_COLOR_ERR  "\033[0;31m" // red
#define LOG_COLOR_WRN  "\033[0;33m" // yellow
#define LOG_COLOR_DBG  "\033[0;36m" // cyan
#define LOG_NO_COLOR   "\033[0m"
#else
#define LOG_COLOR_ERR
#define LOG_COLOR_WRN
#define LOG_COLOR_DBG
#define LOG_NO_COLOR
#endif 

#define INF_FORMAT(format) "[%s %s] " format "\n", esp_log_system_timestamp(), __FUNCTION__
#define LOG_INF(format, ...) logPrint(INF_FORMAT(format), ##__VA_ARGS__)
#define WRN_FORMAT(format) LOG_COLOR_WRN "[%s WARN %s] " format LOG_NO_COLOR "\n", esp_log_system_timestamp(), __FUNCTION__
#define LOG_WRN(format, ...) logPrint(WRN_FORMAT(format), ##__VA_ARGS__)
#define ERR_FORMAT(format) LOG_COLOR_ERR "[%s ERROR @ %s:%u] " format LOG_NO_COLOR "\n", esp_log_system_timestamp(), pathToFileName(__FILE__), __LINE__
#define LOG_ERR(format, ...) logPrint(ERR_FORMAT(format), ##__VA_ARGS__)
#define DBG_FORMAT(format) LOG_COLOR_DBG "[%s DEBUG @ %s:%u] " format LOG_NO_COLOR "\n", esp_log_system_timestamp(), pathToFileName(__FILE__), __LINE__
#define LOG_DBG(format, ...) if (dbgVerbose) logPrint(DBG_FORMAT(format), ##__VA_ARGS__)
#define LOG_PRT(buff, bufflen) log_print_buf((const uint8_t*)buff, bufflen)
  
