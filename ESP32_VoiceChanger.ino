  
// Voice Changer passes microphone input through a set of filters and
// outputs the resulting sound to an amplifier in real time.
// The microphone input, and the output to amplifier, each make use of a
// separate I2S peripheral in the ESP32 or ESP32S3
// I2S and PDM microphones are supported, and a mic on remote device using browser
// I2S and (ESP32 only) analog amplifiers are supported.
// Optional potentiometer via analog pin to provide manual volume & brightness control
// Audio controlled brightness can be applied to single LEDs or a MY9921 LED bar
//
// The operation of the Voice Changer can be configured via a browser connected
// to a web server hosted on the ESP32
//
// s60sc 2021, 2023

#include "appGlobals.h"

void setup() {
  logSetup();
  // prep SD card storage & load saved user configuration
  if (startStorage()) loadConfig();
#ifdef DEV_ONLY
  devSetup();
#endif

  // connect wifi or start config AP if router details not available
  startWifi(); 
  
  startWebServer();
  if (strlen(startupFailure)) LOG_ERR("%s", startupFailure);
  else {
    // start rest of services
    setupAudioLed();
    prepPeripherals();
    setupVC();
    checkMemory();
  }
}

void loop() {
  // confirm not blocked in setup
  LOG_INF("=============== Total tasks: %u ===============\n", uxTaskGetNumberOfTasks() - 1);
  delay(1000);
  vTaskDelete(NULL); // free 8k ram
}
