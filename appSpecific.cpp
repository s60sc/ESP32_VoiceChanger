// voiceChanger specific config functions
//
// s60sc 2022

#include "appGlobals.h"

bool updateAppStatus(const char* variable, const char* value) {
  // update vars from configs and browser input
  bool res = true;
  int intVal = atoi(value);
  float fltVal = atof(value);
  if (!strcmp(variable, "RM")) RING_MOD = (bool)intVal;
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
  else if (!strcmp(variable, "PreAmp")) PREAMP_GAIN = intVal;  
  else if (!strcmp(variable, "AmpVol")) AMP_VOL = intVal; 
  else if (!strcmp(variable, "Bright")) BRIGHTNESS = intVal;      
  else if (!strcmp(variable, "Srate")) SAMPLE_RATE = intVal; 
  else if (!strcmp(variable, "mType")) MIC_TYPE = (deviceType)intVal;
  else if (!strcmp(variable, "aType")) AMP_TYPE = (deviceType)intVal;
  
  // binary integer
  else if (!strcmp(variable, "MicChan")) {
    // set I2S port for microphone, amp is opposite
    if (intVal) {
      I2S_MIC_PORT = I2S_NUM_0;
      I2S_AMP_PORT = I2S_NUM_1;
    } else {
      I2S_MIC_PORT = I2S_NUM_1;
      I2S_AMP_PORT = I2S_NUM_0;
    }
  }
  
  // float
  else if (!strcmp(variable, "BPqval")) BP_Q = fltVal;
  else if (!strcmp(variable, "HPqval")) HP_Q = fltVal;
  else if (!strcmp(variable, "LPqval")) LP_Q = fltVal;
  else if (!strcmp(variable, "PKqval")) PK_Q = fltVal;

  // bool
  else if (!strcmp(variable, "Disable")) DISABLE = (bool)intVal;
  else if (!strcmp(variable, "VolPot")) USE_POT = (bool)intVal;

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
  else if (!strcmp(variable, "ampBckIo")) ampBckIo = intVal;
  else if (!strcmp(variable, "ampWsIo")) ampWsIo = intVal;
  else if (!strcmp(variable, "ampSdIo")) ampSdIo = intVal;
  else if (!strcmp(variable, "amicSckIo")) amicSckIo = intVal;
  else if (!strcmp(variable, "amicWsIo")) amicWsIo = intVal;
  else if (!strcmp(variable, "amicSdIo")) amicSdIo = intVal;
  else if (!strcmp(variable, "buttonPlayPin")) buttonPlayPin = intVal;
  else if (!strcmp(variable, "buttonRecPin")) buttonRecPin = intVal;
  else if (!strcmp(variable, "buttonPassPin")) buttonPassPin = intVal;
  else if (!strcmp(variable, "buttonStopPin")) buttonStopPin = intVal;
  else if (!strcmp(variable, "sanalogPin")) sanalogPin = intVal;
  else if (!strcmp(variable, "saudioLedPin")) saudioLedPin = intVal;
  else if (!strcmp(variable, "switchModePin")) switchModePin = intVal;
  return res; 
}

void wsAppSpecificHandler(const char* wsMsg) {
  // message from web socket
  int wsLen = strlen(wsMsg) - 1;
  switch ((char)wsMsg[0]) {
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
      // kill websocket connection
      killWebSocket();
    break;
    default:
      LOG_WRN("unknown command %c", (char)wsMsg[0]);
    break;
  }
}

void buildAppJsonString(bool filter) {
  // build app specific part of json string 
  char* p = jsonBuff + 1;
  *p = 0;
}

static void doDownload(httpd_req_t* req) {
  // download recording to browser
  if (psramFound()) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_type(req, "application/octet");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=VoiceChanger.wav");
    size_t downloadBytes = buildWavHeader();
    char contentLength[10];
    sprintf(contentLength, "%i", downloadBytes);
    httpd_resp_set_hdr(req, "Content-Length", contentLength);
    // use chunked encoding 
    size_t chunksize, ramPtr = 0;
    do {
      chunksize =  std::min(CHUNKSIZE, (int)(downloadBytes - ramPtr));  
      if (httpd_resp_send_chunk(req, (char*)recordBuffer+ramPtr, chunksize) != ESP_OK) break;     
      ramPtr += chunksize;
    } while (chunksize != 0);
    
    httpd_resp_send_chunk(req, NULL, 0);
    LOG_INF("Downloaded recording, size: %0.1fkB", (float)(downloadBytes/1024.0));
  } else LOG_WRN("PSRAM needed to record and play");
  httpd_resp_send(req, NULL, 0); 
}

esp_err_t webAppSpecificHandler(httpd_req_t* req, const char* variable, const char* value) {
  // request handling specific to VoiceChanger
  int intVal = atoi(value);
  if (!strcmp(variable, "action")) {
    if (intVal == UPDATE_CONFIG) updateStatus("save", "1"); // save current status in config
    else if (intVal == WAV_ACTION) doDownload(req);
    else actionRequest((actionType)intVal); // carry out required action
  }
  return ESP_OK;
}

bool appDataFiles() {
  // callback from setupAssist.cpp, for any app specific files 
  return true;
}

void OTAprereq() {} // dummy

void doAppPing() {} // dummy
