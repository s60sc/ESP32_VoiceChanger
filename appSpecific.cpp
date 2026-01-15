
// Voice Changer passes microphone input through a set of filters and
// outputs the resulting sound to an amplifier in real time.
// Optional potentiometer via analog pin to provide manual volume & brightness control
//
// The operation of the Voice Changer can be configured via a browser connected
// to a web server hosted on the ESP32
//
// s60sc 2021, 2023, 2024

#include "appGlobals.h"

const size_t prvtkey_len = 0;
const size_t cacert_len = 0;
const char* prvtkey_pem = "";
const char* cacert_pem = "";

// record & play button pins
int buttonPlayPin;
int buttonRecPin;
int buttonPassPin;
int buttonStopPin; 
int sanalogPin; // for volume & brightness potentiometer control, must be an analog pin
int saudioLedPin; // pin for audio controlled output lights
int saudioClockPin; // if defined, pin for audio controlled led bar clock
int saudioDataPin; // if defined, pin for audio controlled led bar data
int switchModePin; // switch use of pot between vol (high) & brightness (low)

// other settings
uint8_t BRIGHTNESS = 3; // audio led level 
bool DISABLE = false; // temporarily disable filter settings on browser
bool USE_POT = false; // whether external volume / brightness control potentiometer being used
volatile audioAction THIS_ACTION = NO_ACTION;
SemaphoreHandle_t audioSemaphore = NULL; // disables interrupts whilst state change occuring

// n/a
bool streamVid = false;
bool streamAud = false;
bool streamSrt = false;

static inline void IRAM_ATTR wakeTask(TaskHandle_t thisTask) {
  // utility function to resume task from pin interrupt
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(thisTask, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// ISRs called by pin interrupt
void IRAM_ATTR recISR() {
  if (xSemaphoreTakeFromISR(audioSemaphore, NULL) == pdTRUE) {
    THIS_ACTION = RECORD_ACTION;
    wakeTask(audioHandle);
  }
}

void IRAM_ATTR playISR() {
  if (xSemaphoreTakeFromISR(audioSemaphore, NULL) == pdTRUE) {
    THIS_ACTION = PLAY_ACTION;
    wakeTask(audioHandle);
  }
}

void IRAM_ATTR passISR() {
  if (xSemaphoreTakeFromISR(audioSemaphore, NULL) == pdTRUE) {
    THIS_ACTION = PASS_ACTION;
    wakeTask(audioHandle);
  }
}

void IRAM_ATTR stopISR() {
  stopAudio = true;
}

uint8_t getBrightness() {
  // get required led output brightness (0 .. 7) from analog control if selected
  // else use web page setting
  static uint8_t saveBright = BRIGHTNESS;
  if (USE_POT && switchModePin) {
    // use current pot setting if active, else use previous value
    if (!digitalRead(switchModePin)) saveBright = (smoothAnalog(sanalogPin) >> 9); // (0 .. 8)
    return saveBright * 2;
  } else return BRIGHTNESS * 2; // use web value
}

int8_t checkPotVol(int8_t adjVol) {
  if (USE_POT && switchModePin > 0) {
    // update saveVol if pot switched to volume control
    if (digitalRead(switchModePin)) adjVol = (smoothAnalog(sanalogPin) >> 8); // 0 .. 15, as analog is 12 bits
  }
  return adjVol;
}

void displayAudioLed(int16_t audioSample) {
  if (lampPin || ledBarUse) {
    audioSample = audioSample == SHRT_MIN ? abs(++audioSample) : abs(audioSample);
    float ledLevel = (float)(audioSample) / SHRT_MAX;
    // use sample of sound level for single LED brightness by setting PWM duty cycle
    setLamp((uint8_t)(ledLevel * getBrightness()));
    // use sample of sound level to display on MY9921 controlled LED bar as gauge (PPM meter)
    ledBarGauge(ledLevel);
  }
}

void setupAudioLed() {
  // use MY9921 controlled LED bar as gauge
  ledBarClock = saudioClockPin;
  ledBarData = saudioDataPin;
  ledBarUse = (ledBarClock && ledBarData) ? true : false;
  // setup PWM controlled single LED
  lampPin = saudioLedPin;
}

void setupVC() {
  // setup control buttons, pull to ground to activate
  if (buttonRecPin > 0) {
    pinMode(buttonRecPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonRecPin), recISR, FALLING);
  }
  if (buttonPlayPin > 0) {
    pinMode(buttonPlayPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonPlayPin), playISR, FALLING);
  }
  if (buttonPassPin > 0) {
    pinMode(buttonPassPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonPassPin), passISR, FALLING);
  }
  if (buttonStopPin > 0) {
    pinMode(buttonStopPin, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(buttonStopPin), stopISR, FALLING);
  }
  if (switchModePin > 0) pinMode(switchModePin, INPUT_PULLUP);

  audioSemaphore = xSemaphoreCreateBinary();
  prepAudio(); // report on device status
  xSemaphoreGive(audioSemaphore);
}

static void doDownload(httpd_req_t* req) {
  // download recording to browser, applying current filters
  if (psramFound()) {
    size_t downloadBytes = updateWavHeader();
    setupFilters();
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/octet");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=VoiceChanger.wav");
    char contentLength[10];
    sprintf(contentLength, "%i", downloadBytes);
    httpd_resp_set_hdr(req, "Content-Length", contentLength);
    if (downloadBytes) {
      // use chunked encoding 
      httpd_resp_send_chunk(req, (char*)audioBuffer, WAV_HDR_LEN);
      size_t chunksize = 0, ramPtr = WAV_HDR_LEN;
      do {
        chunksize = std::min((size_t)DMA_BUFF_LEN, downloadBytes - ramPtr); 
        memcpy(sampleBuffer, audioBuffer + ramPtr, chunksize);
        applyFilters();
        if (httpd_resp_send_chunk(req, (char*)sampleBuffer, chunksize) != ESP_OK) break;  
        ramPtr += chunksize;
      } while (chunksize != 0);
      LOG_INF("Downloaded recording, size: %0.1fkB", (float)(downloadBytes/1024.0));
    } else LOG_WRN("Recorded content is empty");
    httpd_resp_sendstr_chunk(req, NULL); // signal end of data
  } else LOG_WRN("PSRAM and recording needed for download"); 
}

/************************ webServer callbacks *************************/

bool updateAppStatus(const char* variable, const char* value, bool fromUser) {
  // update vars from configs and browser input
  bool res = true;
  int intVal = atoi(value);
  float fltVal = atof(value);
  if (!strcmp(variable, "custom")) return res;
  else if (!strcmp(variable, "RM")) RING_MOD = (bool)intVal;
  else if (!strcmp(variable, "BP")) BAND_PASS = (bool)intVal;
  else if (!strcmp(variable, "HP")) HIGH_PASS = (bool)intVal;
  else if (!strcmp(variable, "LP")) LOW_PASS = (bool)intVal;
  else if (!strcmp(variable, "HS")) HIGH_SHELF = (bool)intVal;
  else if (!strcmp(variable, "LS")) LOW_SHELF = (bool)intVal;
  else if (!strcmp(variable, "PK")) PEAK = (bool)intVal;
  else if (!strcmp(variable, "CP")) CLIPPING = (bool)intVal;
  else if (!strcmp(variable, "RV")) REVERB = (bool)intVal;
  // integer
  else if (!strcmp(variable, "BPcas")) BP_CAS = intVal;
  else if (!strcmp(variable, "HPcas")) HP_CAS = intVal;
  else if (!strcmp(variable, "LPcas")) LP_CAS = intVal;
  else if (!strcmp(variable, "SineFreq")) SW_FREQ = intVal;
  else if (!strcmp(variable, "SineAmp")) SW_AMP = intVal << 5;
  else if (!strcmp(variable, "ClipFac")) CLIP_FACTOR = intVal;
  else if (!strcmp(variable, "DecayFac")) DECAY_FACTOR = intVal;
  else if (!strcmp(variable, "micGain")) micGain = intVal;
  else if (!strcmp(variable, "ampVol")) ampVol = intVal; 
  else if (!strcmp(variable, "Bright")) BRIGHTNESS = intVal;
  else if (!strcmp(variable, "Srate")) SAMPLE_RATE = intVal; 

  // binary integer
  else if (!strcmp(variable, "MicChan")) setI2Schan(intVal);
  
  // float
  else if (!strcmp(variable, "BPqval")) BP_Q = fltVal;
  else if (!strcmp(variable, "HPqval")) HP_Q = fltVal;
  else if (!strcmp(variable, "LPqval")) LP_Q = fltVal;
  else if (!strcmp(variable, "PKqval")) PK_Q = fltVal;
  else if (!strcmp(variable, "Pitch")) PITCH_SHIFT = fltVal;  

  // bool
  else if (!strcmp(variable, "Disable")) DISABLE = (bool)intVal;
  else if (!strcmp(variable, "VolPot")) USE_POT = (bool)intVal;
  else if (!strcmp(variable, "mType")) I2Smic = bool(intVal);
  else if (!strcmp(variable, "micRem")) {
    micRem = bool(intVal);
    LOG_INF("Remote mic is %s", micRem ? "On" : "Off"); 
  }
  else if (!strcmp(variable, "spkrRem")) {
    spkrRem = (bool)intVal;
    LOG_INF("Remote speaker is %s", spkrRem ? "On" : "Off");
  }
  // float with no decimal places
  else if (!strcmp(variable, "BPfreq")) BP_FREQ = fltVal; 
  else if (!strcmp(variable, "HPfreq")) HP_FREQ = fltVal;
  else if (!strcmp(variable, "LPfreq")) LP_FREQ = fltVal;  
  else if (!strcmp(variable, "HSfreq")) HS_FREQ = fltVal; 
  else if (!strcmp(variable, "HSgain")) HS_GAIN = fltVal;
  else if (!strcmp(variable, "LSfreq")) LS_FREQ = fltVal;
  else if (!strcmp(variable, "LSgain")) LS_GAIN = fltVal;
  else if (!strcmp(variable, "PKfreq")) PK_FREQ = fltVal;
  else if (!strcmp(variable, "PKgain")) PK_GAIN = fltVal;

  // pins
  else if (!strcmp(variable, "mampBckIo")) mampBckIo = intVal;
  else if (!strcmp(variable, "mampSwsIo")) mampSwsIo = intVal;
  else if (!strcmp(variable, "mampSdIo")) mampSdIo = intVal;
  else if (!strcmp(variable, "micSckPin")) micSckPin = intVal;
  else if (!strcmp(variable, "micSWsPin")) micSWsPin = intVal;
  else if (!strcmp(variable, "micSdPin")) micSdPin = intVal;
  else if (!strcmp(variable, "buttonPlayPin")) buttonPlayPin = intVal;
  else if (!strcmp(variable, "buttonRecPin")) buttonRecPin = intVal;
  else if (!strcmp(variable, "buttonPassPin")) buttonPassPin = intVal;
  else if (!strcmp(variable, "buttonStopPin")) buttonStopPin = intVal;
  else if (!strcmp(variable, "sanalogPin")) sanalogPin = intVal;
  else if (!strcmp(variable, "saudioLedPin")) saudioLedPin = intVal;
  else if (!strcmp(variable, "saudioClockPin")) saudioClockPin = intVal;
  else if (!strcmp(variable, "saudioDataPin")) saudioDataPin = intVal;
  else if (!strcmp(variable, "switchModePin")) switchModePin = intVal;
  return res; 
}

void appSpecificWsHandler(const char* wsMsg) {
  // message from web socket
  int wsLen = strlen(wsMsg) - 1;
  switch ((char)wsMsg[0]) {
    case 'X':
      // no action
    break;
    case 'H':
      // keepalive heartbeat, return status
    break;
    case 'S':
      // status request
      buildJsonString(wsLen); // required config number
      logPrint("%s\n", jsonBuff);
    break;
    case 'U':
      // update or control request
      memcpy(jsonBuff, wsMsg + 1, wsLen); // remove 'U'
      parseJson(wsLen);
    break;
    case 'K':
      // no action
    break;
    default:
      LOG_WRN("unknown command %c", (char)wsMsg[0]);
    break;
  }
}

void wsJsonSend(const char* keyStr, const char* valStr) {
  // send key val as json over websocket
  char jsondata[100];
  sprintf(jsondata, "{\"cfgGroup\":\"-1\", \"%s\":\"%s\"}", keyStr, valStr);
  wsAsyncSendText(jsondata);
}

void buildAppJsonString(bool filter) {
  // build app specific part of json string
  char* p = jsonBuff + 1;
  *p = 0;
}

esp_err_t appSpecificWebHandler(httpd_req_t* req, const char* variable, const char* value) {
  // request handling specific to VoiceChanger
  int intVal = atoi(value);
  if (!strcmp(variable, "action")) {
    if (intVal == UPDATE_CONFIG) updateStatus("save", "1"); // save current status in config
    else if (intVal == WAV_ACTION) doDownload(req);
    else {
      // action request, allow return to web handler immediately,
      // otherwise if action takes too long, watchdog would be triggered
      THIS_ACTION = (audioAction)intVal;
      stopAudio = true; // stop any unstopped action
      wsAsyncSendText("#M0"); // stop browser mic sending
      if ((THIS_ACTION != STOP_ACTION) && (xSemaphoreTake(audioSemaphore, pdMS_TO_TICKS(200)) == pdTRUE)) xTaskNotifyGive(audioHandle);
      else LOG_WRN("Waiting for previous action to terminate");
    }
  } else return ESP_FAIL;
  return ESP_OK;
}

void appSpecificWsBinHandler(uint8_t* wsMsg, size_t wsMsgLen) {
  browserMicInput(wsMsg, wsMsgLen);
}

esp_err_t appSpecificSustainHandler(httpd_req_t* req) {
  return ESP_OK;
}

void externalAlert(const char* subject, const char* message) {
  // alert any configured external servers
}

bool appDataFiles() {
  // callback from setupAssist.cpp, for any app specific files
  return true;
}

void doAppPing() {}

void OTAprereq() {
  stopPing();
}

void stepperDone() {
}

/************** default app configuration **************/
const char* appConfig = R"~(
BPfreq~4000~98~T~n/a
BPqval~0.7~98~T~n/a
HPfreq~4000~98~T~n/a
HPqval~0.7~98~T~n/a
LPfreq~4000~98~T~n/a
LPqval~0.7~98~T~n/a
HSfreq~4000~98~T~n/a
HSgain~3~98~T~n/a
LSfreq~4000~98~T~n/a
LSgain~3~98~T~n/a
PKfreq~4000~98~T~n/a
PKgain~3~98~T~n/a
BP~0~98~T~n/a
HP~0~98~T~n/a
LP~0~98~T~n/a
HS~0~98~T~n/a
LS~0~98~T~n/a
PK~0~98~T~n/a
BPcas~1~98~T~n/a
HPcas~1~98~T~n/a
LPcas~1~98~T~n/a
PKqval~0.7~98~T~n/a
CP~0~98~T~n/a
ClipFac~1~98~T~n/a
RV~0~98~T~n/a
DecayFac~1~98~T~n/a
RM~0~98~T~n/a
SineFreq~80~98~T~n/a
SineAmp~5~98~T~n/a
Pitch~1~98~T~n/a
ampVol~3~98~T~n/a
micGain~3~98~T~n/a
Bright~3~98~T~n/a
Srate~16000~98~T~n/a
MicChan~1~98~T~n/a
mType~1~98~T~n/a
Disable~0~98~T~n/a
VolPot~0~98~T~n/a
restart~~99~T~na
ST_SSID~~0~T~Wifi SSID name
ST_Pass~~0~T~Wifi SSID password
ST_ip~~0~T~Static IP address
ST_gw~~0~T~Router IP address
ST_sn~255.255.255.0~0~T~Router subnet
ST_ns1~~0~T~DNS server
ST_ns2~~0~T~Alt DNS server
AP_ip~~0~T~AP IP Address if not 192.168.4.1
AP_sn~~0~T~AP subnet
AP_gw~~0~T~AP gateway
allowAP~1~0~C~Allow simultaneous AP
AP_Pass~~0~T~AP Password
Auth_Name~~0~T~Optional user name for web page login
Auth_Pass~~0~T~Optional web page password
wifiTimeoutSecs~30~0~N~WiFi connect timeout (secs)
timezone~GMT0~0~T~Timezone string: tinyurl.com/TZstring
logType~0~99~N~Output log selection
micSckPin~-1~1~N~Microphone I2S SCK pin
micSWsPin~-1~1~N~Microphone I2S WS, PDM CLK pin
micSdPin~-1~1~N~Microphone I2S SD, PDM DAT pin
mampBckIo~-1~1~N~Amplifier I2S BCLK (SCK) pin
mampSwsIo~-1~1~N~Amplifier I2S LRCLK (WS) pin
mampSdIo~-1~1~N~Amplifier I2S DIN pin
buttonPlayPin~~1~N~Pin to start play
buttonRecPin~~1~N~Pin to start record
buttonPassPin~~1~N~Pin to start passthru
buttonStopPin~~1~N~Pin to stop: play, record, passthru
sanalogPin~~1~N~Analog pin for volume & brightness pot control
saudioLedPin~~1~N~Pin for audio controlled output lights
saudioClockPin~~1~N~Pin for audio controlled LED bar clock
saudioDataPin~~1~N~Pin for audio controlled LED bar data
switchModePin~~1~N~Pin to use pot for brightness (hi) or vol (lo)
usePing~1~0~C~Use ping
)~";
