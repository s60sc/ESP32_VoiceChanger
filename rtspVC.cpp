// 
// Provide RTSP Server to output audio stream that can be received on device
// with RTSP client - eg VLC
//
// s60sc 2025

#include "appGlobals.h"

bool RTSPAudio = false;
int16_t* RTSPAudioBuffer = NULL;
size_t RTSPAudioBytes = 0;

#if INCLUDE_RTSP

#include <ESP32-RTSPServer.h> // https://github.com/rjsachse/ESP32-RTSPServer.git

RTSPServer rtspServer;

static const int rtspPort = 554;
static const uint16_t rtpAudioPort = 5432;
static uint8_t rtpTTL = 1;
IPAddress rtpIp = IPAddress(239, 255, 0, 1); 

static void sendRTSPAudio(void* p) {
  // send audio chunks via RTSP
// chunk > 1024 causes panic abort in rtsp lib due to buffer overrun
  RTSPAudioBytes = 0;
  while (true) {
    if (RTSPAudioBytes && rtspServer.readyToSendAudio()) { 
      rtspServer.sendRTSPAudio(RTSPAudioBuffer, RTSPAudioBytes);
      RTSPAudioBytes = 0;
    }
    delay(20);
  }
  vTaskDelete(NULL);
}

void prepRTSP() {
  // Initialize the RTSP server for audio
  rtspServer.transport = RTSPServer::AUDIO_ONLY;
  rtspServer.sampleRate = SAMPLE_RATE; 
  rtspServer.rtspPort = rtspPort; 
  rtspServer.rtpAudioPort = rtpAudioPort; 
  rtspServer.rtpIp = rtpIp; 
  rtspServer.rtpTTL = rtpTTL; 
    
  if (rtspServer.init()) { 
    LOG_INF("RTSP server started successfully with transport: AUDIO only @ %uHz", SAMPLE_RATE);
    if (RTSPAudioBuffer == NULL) RTSPAudioBuffer = (int16_t*)malloc(sampleBytes);
    LOG_INF("Connect to: rtsp://%s:%d", WiFi.localIP().toString().c_str(), rtspServer.rtspPort);
    xTaskCreate(sendRTSPAudio, "sendRTSPAudio", 1024 * 4, NULL, 5, NULL);
    RTSPAudio = true;
  } else LOG_ERR("Failed to start RTSP server"); 
}

#endif
