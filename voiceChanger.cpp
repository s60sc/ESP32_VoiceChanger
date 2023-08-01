
// Voice Changer passes microphone input through a set of filters and
// outputs the resulting sound to an amplifier in real time.
// The microphone input, and the output to amplifier, each make use of a
// separate I2S peripheral in the ESP32 or ESP32S3
// I2S, PDM and (ESP32 only) analog microphones are supported.
// I2S and (ESP32 only) analog amplifiers are supported.
// Optional potentiometer via analog pin to provide manual volume & brightness control
//
// The operation of the Voice Changer can be configured via a browser connected
// to a web server hosted on the ESP32
//
// s60sc 2021, 2023

#include "appGlobals.h"

// I2S devices
deviceType MIC_TYPE;
deviceType AMP_TYPE;
i2s_port_t I2S_MIC_PORT;
i2s_port_t I2S_AMP_PORT;

/********* pin selection **********/

// I2S Microphone pins
// INMP441 I2S microphone pinout, connect L/R to GND for left channel
// MP34DT01 PDM microphone pinout, connect SEL to GND for left channel
int amicSckIo; // I2S SCK
int amicWsIo; // I2S WS, PDM CLK
int amicSdIo; // I2S SD, PDM DAT, ADC  

// I2S Amplifier pins
// MAX98357A 
// SD leave as mono (unconnected)
// Gain: 100k to GND works, not direct to GND. Unconnected is 9 dB 
int ampBckIo; // I2S BCLK 
int ampWsIo; // I2S LRCLK 
int ampSdIo; // I2S DIN

int ampTimeout = 1000; // ms for amp write abandoned if no output

// record & play button pins
int buttonPlayPin;
int buttonRecPin;
int buttonPassPin;
int buttonStopPin; 
int sanalogPin; // for volume & brightness potentiometer control, must be an analog pin
int saudioLedPin; // pin for audio controlled output lights
int switchModePin; // switch use of pot between vol (high) & brightness (low)
static i2s_pin_config_t i2s_mic_pins;
static i2s_pin_config_t i2s_amp_pins;

// other settings
uint8_t PREAMP_GAIN = 3; // microphone preamplification factor
int8_t AMP_VOL = 3; // amplifier volume factor
uint8_t BRIGHTNESS = 3; // audio led level 
bool DISABLE = false; // temporarily disable filter settings on browser
bool USE_POT = false; // whether external volume / brightness control potentiometer being used
uint16_t SAMPLE_RATE = 16000;  // audio rate in Hz
int16_t SAMPLE_BUFFER[DMA_BUFF_LEN]; // audio samples output buffer
volatile actionType THIS_ACTION;

static bool volatile stopped = false;
static TaskHandle_t actionHandle = NULL;
static SemaphoreHandle_t actionSemaphore;
static QueueHandle_t i2s_queue = NULL;
static i2s_event_t event;

#ifdef CONFIG_IDF_TARGET_ESP32S3
static const int psramMax = ONEMEG * 6;
#else
static const int psramMax = ONEMEG * 2;
#endif
uint8_t* recordBuffer;
static size_t recordBytesUsed = 0;
static int recordSize = 0;
static const uint8_t sampleWidth = sizeof(int32_t); 
static const uint8_t audWidth = sizeof(int16_t);
static const size_t sampleBytes = DMA_BUFF_LEN * sampleWidth;
const size_t audBytes = DMA_BUFF_LEN * audWidth;
static int32_t sampleBuffer[sampleBytes]; // audio samples output buffer
int16_t audBuffer[audBytes];
static const char* deviceLabels[3] = {"I2S", "PDM", "ADC"};
static fs::FS fp = STORAGE;

// i2s config for microphone
static i2s_config_t i2s_mic_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // I2S_BITS_PER_SAMPLE_16BIT doesnt work
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = DMA_BUFF_CNT,
  .dma_buf_len = DMA_BUFF_LEN,
  .use_apll = false,
  .tx_desc_auto_clear = false,
  .fixed_mclk = 0
};

// I2S config for amplifier
static i2s_config_t i2s_amp_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
  .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // was right
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = DMA_BUFF_CNT,
  .dma_buf_len = DMA_BUFF_LEN,
  .use_apll = false,
  .tx_desc_auto_clear = true,
  .fixed_mclk = 0
};

static inline void IRAM_ATTR wakeTask(TaskHandle_t thisTask) {
  // utility function to resume task from pin interrupt or web request
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  vTaskNotifyGiveFromISR(thisTask, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

// ISRs called by pin interrupt
void IRAM_ATTR recISR() {
  if (xSemaphoreTakeFromISR(actionSemaphore, NULL) == pdTRUE) {
    THIS_ACTION = RECORD_ACTION;
    wakeTask(actionHandle);
  }
}

void playISR() {
  if (xSemaphoreTakeFromISR(actionSemaphore, NULL) == pdTRUE) {
    THIS_ACTION = PLAY_ACTION;
    wakeTask(actionHandle);
  }
}

void IRAM_ATTR passISR() {
  if (xSemaphoreTakeFromISR(actionSemaphore, NULL) == pdTRUE) {
    THIS_ACTION = PASS_ACTION;
    wakeTask(actionHandle);
  }
}

void IRAM_ATTR stopISR() {
  stopped = true;
}

void actionRequest(actionType doAction) {
  // web request, allow return to web handler immediately,
  // otherwise if action takes too long, watchdog would be triggered
  if (doAction == STOP_ACTION) stopped = true;
  else {
    if (xSemaphoreTake(actionSemaphore, 0) == pdTRUE) {
      THIS_ACTION = doAction;  
      wakeTask(actionHandle);
    }
  }
}

/*******************************************************/

static inline uint8_t getBrightness() {
  // get required led output brightness (0 .. 7) from analog control if selected
  // else use web page setting
  static uint8_t saveBright = BRIGHTNESS;
  if (USE_POT && switchModePin > 0) {
    // use current pot setting if active, else use previous value
    if (!digitalRead(switchModePin)) saveBright = (smoothAnalog(sanalogPin) >> 9); // (0 .. 8)
    return saveBright;
  } else return BRIGHTNESS; // use web value
}

int8_t getVolumeControl() {
  // determine required volume setting (relative to current)
  static int8_t saveVol = 1;
  if (USE_POT && switchModePin > 0) {
    // update saveVol if pot switched to volume control
    if (digitalRead(switchModePin)) saveVol = (smoothAnalog(sanalogPin) >> 8); // 0 .. 15, as analog is 12 bits
  } else saveVol = AMP_VOL * 2; // use web page setting
  if (saveVol == 0) return 0; // turn off volume
  // // increase or reduce volume, 6 is unity eg midpoint of pot / web slider
  return saveVol > 5 ? saveVol - 5 : saveVol - 7; 
}

static esp_err_t setupMic() {
  // setup microphone based on its type, default I2S
  int i2sMode = I2S_MODE_MASTER | I2S_MODE_RX;
  i2s_mic_pins.bck_io_num = amicSckIo; 
  i2s_mic_pins.ws_io_num = amicWsIo;
  i2s_mic_pins.data_in_num = amicSdIo;
  i2s_mic_pins.data_out_num = I2S_PIN_NO_CHANGE;
  
  if (amicWsIo == amicSdIo) LOG_WRN("Microphone pins not defined");
  else {
    switch (MIC_TYPE) {
      case I2S_TYPE:
        // Setup I2S microphone config & pins
      break;
      case PDM_TYPE:
        // Setup PDM microphone config & pins
        i2sMode |= I2S_MODE_PDM;
        i2s_mic_pins.bck_io_num = I2S_PIN_NO_CHANGE;
      break;
      case ADC_TYPE:
        // Setup ADC microphone config & pins
  #ifdef CONFIG_IDF_TARGET_ESP32
        i2sMode |= I2S_MODE_ADC_BUILT_IN;
  #else
        LOG_WRN("Analog not supported");
        return ESP_FAIL;
  #endif
      break;
    }
    i2s_mic_config.mode = (i2s_mode_t)i2sMode;
    // set required sample rate
    i2s_mic_config.sample_rate = SAMPLE_RATE;
    LOG_INF("Setup %s microphone", deviceLabels[MIC_TYPE]);
    return ESP_OK;
  }
  return ESP_FAIL;
}

static esp_err_t setupAmp() {
  // setup amplifier based on its type, default I2S
  int i2sMode = I2S_MODE_MASTER | I2S_MODE_TX; 
  i2s_amp_pins.bck_io_num = ampBckIo;
  i2s_amp_pins.ws_io_num = ampWsIo;
  i2s_amp_pins.data_out_num = ampSdIo;
  i2s_amp_pins.data_in_num = I2S_PIN_NO_CHANGE;

  if (ampSdIo == ampWsIo) LOG_WRN("Amplifier pins not defined");
  else {
    switch (AMP_TYPE) {
      case PDM_TYPE:
        // Setup PDM amp config
        i2sMode |= I2S_MODE_PDM;
      break;
      case ADC_TYPE:
        // Setup ADC amp config
  #ifdef CONFIG_IDF_TARGET_ESP32
        i2sMode |= I2S_MODE_DAC_BUILT_IN;
  #else
        LOG_WRN("Analog not supported");
        return ESP_FAIL;
  #endif
      break;
      default: // ignore
      break;    
    }
    i2s_amp_config.sample_rate = SAMPLE_RATE;
    i2s_amp_config.mode = (i2s_mode_t)i2sMode;  
    LOG_INF("Setup %s amplifier", deviceLabels[AMP_TYPE]);       
    return ESP_OK; 
  }
  return ESP_FAIL;         
}

static esp_err_t startupMic() {
  // install & start up the I2S peripheral as microphone when activated
  int queueSize = 4;
//LOG_CHK("amicSckIo %d, amicWsIo %d, amicSdIo %d", i2s_mic_pins.bck_io_num, i2s_mic_pins.ws_io_num, i2s_mic_pins.data_in_num);
  i2s_queue = xQueueCreate(queueSize, sizeof(i2s_event_t));
  esp_err_t res = i2s_driver_install(I2S_MIC_PORT, &i2s_mic_config, queueSize, &i2s_queue);
  if (res == ESP_OK) {
    if (MIC_TYPE == ADC_TYPE) {
      // ADC device
#ifdef CONFIG_IDF_TARGET_ESP32
      i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL); // init ADC pad
      i2s_adc_enable(I2S_MIC_PORT); // enable the ADC for I2S
#endif
    } else {
      // I2S or PDM device
      i2s_set_pin(I2S_MIC_PORT, &i2s_mic_pins);
    }
    // clear the DMA buffers
    i2s_zero_dma_buffer(I2S_MIC_PORT);  
    LOG_INF("Start microphone on port %i", I2S_MIC_PORT);
  } else LOG_WRN("Unable to startup mic");
  return res;
}

static esp_err_t startupAmp() {
  // install & start up the I2S peripheral as amplifier
  esp_err_t res = i2s_driver_install(I2S_AMP_PORT, &i2s_amp_config, 0, NULL);
  if (res == ESP_OK) {
    if (AMP_TYPE == ADC_TYPE) {
#ifdef CONFIG_IDF_TARGET_ESP32
      i2s_set_dac_mode(I2S_DAC_CHANNEL); // DAC device
#endif
    }
    else i2s_set_pin(I2S_AMP_PORT, &i2s_amp_pins);     // I2S or PDM device
    // clear the DMA buffers
    i2s_zero_dma_buffer(I2S_AMP_PORT);
    LOG_INF("Start amplifier on port %i", I2S_AMP_PORT);
  } else LOG_WRN("Unable to startup amp");
  return res;
}

static size_t micInput() {
  // read sample from microphone if DMA ready
  size_t bytesRead = 0;
  const uint8_t gainFactor = 16 + 1 - PREAMP_GAIN; 
  // wait a period for i2s buffer to be ready
  if (xQueueReceive(i2s_queue, &event, (2 * SAMPLE_RATE) /  portTICK_PERIOD_MS) == pdPASS) {
    if (event.type == I2S_EVENT_RX_DONE) {
      i2s_read(I2S_MIC_PORT, sampleBuffer, sampleBytes, &bytesRead, portMAX_DELAY);
      int samplesRead = bytesRead / sampleWidth;
      // process each sample, convert to amplified 16 bit 
      for (int i = 0; i < samplesRead; i++) {
        int32_t sample = sampleBuffer[i];
        // if ADC mic, convert 12 bit unsigned to signed
        if (MIC_TYPE == ADC_TYPE) sample = (sample & 0xfff) - 2048;
        // apply preamp gain
        audBuffer[i] = constrain(sample >> gainFactor, SHRT_MIN, SHRT_MAX);   
      }
      bytesRead = samplesRead * audWidth;
    }
  }
  return bytesRead;
}

static void ampOutput(size_t bytesRead = audBytes, bool speaker = true) {
  // output to amplifier
  // apply required filtering and volume
  applyFilters(getVolumeControl());

  size_t bytesWritten = 0;
  if (speaker) i2s_write(I2S_AMP_PORT, audBuffer, bytesRead, &bytesWritten, ampTimeout);

  // use sample of sound level for led brightness by changing duty cycle
if (saudioLedPin > 0) {
    uint8_t ledLevel = constrain((abs(audBuffer[0]) >> (8-getBrightness())), 0, UCHAR_MAX);
    ledcWrite(0, ledLevel);
  }
}

/*********************************************/

static void makeRecording() {
  if (psramFound()) {
    LOG_INF("Recording ...");
    recordBytesUsed = WAV_HDR_LEN; // leave space for wave header
    while (recordBytesUsed < psramMax) {
      size_t bytesRead = micInput();
      memcpy(recordBuffer + recordBytesUsed, audBuffer, bytesRead);
      recordBytesUsed += bytesRead;
      if (stopped) break;
    }
    recordSize = recordBytesUsed / audWidth - WAV_HDR_LEN;
    LOG_INF("%s recording of %d samples", stopped ? "Stopped" : "Finished",  recordSize);  
  } else LOG_WRN("PSRAM needed to record and play");
}

static void playRecording() {
  if (psramFound()) {
    LOG_INF("Playing, initial volume: %d", getVolumeControl()); 
    for (int i = WAV_HDR_LEN; i < recordBytesUsed; i += audBytes) { 
      memcpy(audBuffer, recordBuffer+i, audBytes);
      ampOutput();
      if (stopped) break;
    }
    LOG_INF("%s playing of %d samples", stopped ? "Stopped" : "Finished", recordSize);
  } else LOG_WRN("PSRAM needed to record and play");
}

size_t buildWavHeader() {
  // build WAV file header for wav file download
  memcpy(recordBuffer, "RIFF", 4);
  uint32_t val32 = WAV_HDR_LEN + recordBytesUsed - 8;
  memcpy(recordBuffer+4, &val32, 4);
  memcpy(recordBuffer+8, "WAVE", 4);
  memcpy(recordBuffer+12, "fmt ", 4);
  val32 = 16;
  memcpy(recordBuffer+16, &val32, 4); // ChunkLen
  uint16_t val16 = 1;
  memcpy(recordBuffer+20, &val16, 2); //  FormatType 
  memcpy(recordBuffer+22, &val16, 2); // NumChannels 
  val32 = SAMPLE_RATE;
  memcpy(recordBuffer+24, &val32, 4); // sample rate
  val32 = SAMPLE_RATE * 2;
  memcpy(recordBuffer+28, &val32, 4); // AvgBytesPerSec = SAMPLE_RATE * BlockAlign
  val16 = 2;
  memcpy(recordBuffer+32, &val16, 2); // BlockAlign
  val16 = 16;
  memcpy(recordBuffer+34, &val16, 2); // BitsPerSample
  memcpy(recordBuffer+36, "data", 4);
  size_t dataSize = recordBytesUsed - WAV_HDR_LEN;
  memcpy(recordBuffer+40, &dataSize, 4); // DataSize  
  return recordBytesUsed;
}

esp_err_t restartDevices(bool initDevices) {
  // setup peripherals, depending on what is being connected
  // start / restart peripherals after configuration changed from web
  if (initDevices) {
#ifdef CONFIG_IDF_TARGET_ESP32
    i2s_adc_disable(I2S_NUM_0);
    i2s_adc_disable(I2S_NUM_1);
#endif
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_1);
  }
  if ((I2S_AMP_PORT == I2S_NUM_1 && AMP_TYPE != I2S_TYPE) 
    || (I2S_MIC_PORT == I2S_NUM_1 && MIC_TYPE != I2S_TYPE))
    LOG_ERR("Only I2S device supported on I2S_NUM_1");
  else {
    esp_err_t restartOK = setupMic(); 
    if (restartOK == ESP_OK) restartOK = startupMic();
    if (restartOK == ESP_OK) {
      restartOK = setupAmp();
      if (restartOK == ESP_OK) startupAmp();  
      restartOK = ESP_OK; // allow mic without amp
    }
    return restartOK;
  }
  return ESP_FAIL;
}

static void actionTask(void* parameter) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (restartDevices(true) == ESP_OK) {
      setupFilters();
      switch (THIS_ACTION) {
        case RECORD_ACTION: 
          makeRecording(); // make recording
        break;
        case PLAY_ACTION: 
          playRecording(); // play previous recording
        break;
        case PASS_ACTION:
          LOG_INF("Passthru started");
          while (!stopped) {
            // play buffer from mic
            size_t bytesRead = micInput();
            if (bytesRead) ampOutput(bytesRead);
          }
          LOG_INF("Passthru stopped");
        break;
        default: // ingnore
        break;
      }
    }
    stopped = false;
    if (saudioLedPin > 0) ledcWrite(0, 0); // switch off led
    xSemaphoreGive(actionSemaphore);
  }
  vTaskDelete(NULL);
}

static void setupAudioLed() {
  // setup pwm controlled led
  if (saudioLedPin > 0) {
    ledcSetup(0, 4000, 8); // 12 kHz PWM, 8-bit resolution
    ledcAttachPin(saudioLedPin, 0); // assign pin to channel
  }
}

void setupVC() {
  setupAudioLed();
  setupADC();
  LOG_INF("I2S preparation");
  if (psramFound()) recordBuffer = (uint8_t*)ps_malloc(psramMax);
  else LOG_WRN("No PSRAM available");
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

  restartDevices(false); // report on device status
  actionSemaphore = xSemaphoreCreateBinary();
  xTaskCreate(&actionTask, "actionTask", 4096, NULL, 6, &actionHandle);
  xSemaphoreGive(actionSemaphore);
  LOG_INF("Initial volume: %d", getVolumeControl()); // prime analog read
}
