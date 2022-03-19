
// Voice Changer passes microphone input through a set of filters and
// outputs the resulting sound to an amplifier in real time.
// The microphone input, and the output to amplifier, each make use of a
// separate I2S peripheral in the ESP32
// I2S, PDM and analog microphones are supported.
// I2S and analog amplifiers are supported.
// Optional 100k pot via analog to provide manual volume & brightness control
//
// The operation of the Voice Changer can be configured via a browser connected
// to a web server hosted on the ESP32
//
// s60sc 2021

#include "myConfig.h"

// I2S devices
deviceType MIC_TYPE;
deviceType AMP_TYPE;
i2s_port_t I2S_MIC_PORT;
i2s_port_t I2S_AMP_PORT;

// other settings
uint8_t PREAMP_GAIN; // microphone preamplification factor
int8_t AMP_VOL; // amplifier volume factor
uint8_t BRIGHTNESS = 3; // audio led level 
bool DISABLE; // temporarily disable filter settings on browser
bool USE_POT; // whether external volume / brightness control potentiometer being used
uint16_t SAMPLE_RATE = 16000;  // audio rate in Hz
int16_t SAMPLE_BUFFER[DMA_BUFF_LEN]; // audio samples output buffer
volatile actionType THIS_ACTION;

static bool volatile stopped = false;
static TaskHandle_t actionHandle = NULL;
static SemaphoreHandle_t actionSemaphore;

static const int psramMax = ONEMEG * 2;
uint8_t* recordBuffer;
static size_t recordBytesUsed = 0;
static int recordSize = 0;
static const uint8_t sampleWidth = sizeof(int16_t); 
static const size_t sampleBytes = DMA_BUFF_LEN * sampleWidth;
static const char* deviceLabels[3] = {"I2S", "PDM", "ADC"};
static fs::FS fp = STORAGE;

// i2s config for microphone
static i2s_config_t i2s_mic_config = {
  .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
  .sample_rate = SAMPLE_RATE,
  .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
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
  .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // RIGHT required for DAC output on cam module
  .communication_format = I2S_COMM_FORMAT_STAND_I2S,
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
  .dma_buf_count = DMA_BUFF_CNT,
  .dma_buf_len = DMA_BUFF_LEN,
  .use_apll = false,
  .tx_desc_auto_clear = true,
  .fixed_mclk = 0
};

// i2s microphone pins
static i2s_pin_config_t i2s_mic_pins = {
  .bck_io_num = MIC_SCK_IO,
  .ws_io_num = MIC_WS_IO,
  .data_out_num = I2S_PIN_NO_CHANGE,
  .data_in_num = MIC_SD_IO
};

// I2S amplifier pins
static i2s_pin_config_t i2s_amp_pins = {
  .bck_io_num = AMP_BCK_IO,
  .ws_io_num = AMP_WS_IO,
  .data_out_num = AMP_SD_IO,
  .data_in_num = I2S_PIN_NO_CHANGE
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

static inline uint16_t getAnalog() {
  // get averaged analog pin value (12 bit)
  uint32_t level = 0; 
  for (int j = 0; j < ADC_SAMPLES; j++) level += analogRead(ANALOG_PIN); 
  level /= ADC_SAMPLES;
  return level;
}

static inline uint8_t getBrightness() {
  // get required led output brightness (0 .. 7) from analog control if selected
  // else use web page setting
  static uint8_t saveBright = BRIGHTNESS;
  if (USE_POT) {
    // use current pot setting if active, else use previous value
    if (!digitalRead(SWITCH_MODE_PIN)) saveBright = (getAnalog() >> 9); // (0 .. 8)
    return saveBright;
  } else return BRIGHTNESS; // use web value
}

static inline int8_t getVolumeControl() {
  // determine required volume setting (relative to current)
  static int8_t saveVol = 1;
  if (USE_POT) {
    // update saveVol if pot switched to volume control
    if (digitalRead(SWITCH_MODE_PIN)) saveVol = (getAnalog() >> 8); // 0 .. 15, as analog is 12 bits
  } else saveVol = AMP_VOL * 2; // use web page setting
  if (saveVol == 0) return 0; // turn off volume
  return saveVol > 5 ? saveVol - 5 : saveVol - 7; // increase or reduce volume, 6 is unity
}

static void setupMic() {
  // setup microphone based on its type
  int i2sMode = I2S_MODE_MASTER | I2S_MODE_RX;
  switch (MIC_TYPE) {
    case I2S_TYPE:
      // Setup I2S microphone config & pins
      i2s_mic_pins.bck_io_num = MIC_SCK_IO;
      break;
    case PDM_TYPE:
      // Setup PDM microphone config & pins
      i2sMode |= I2S_MODE_PDM;
      i2s_mic_pins.bck_io_num = I2S_PIN_NO_CHANGE;
    break;
    case ADC_TYPE:
      // Setup ADC microphone config & pins
      i2sMode |= I2S_MODE_ADC_BUILT_IN;
    break;
  }
  i2s_mic_config.mode = (i2s_mode_t)i2sMode;
  LOG_INF("Setup %s microphone", deviceLabels[MIC_TYPE]);
}

static void setupAmp() {
  // setup amplifier based on its type
  int i2sMode = I2S_MODE_MASTER | I2S_MODE_TX;
  switch (AMP_TYPE) {
    case PDM_TYPE:
      // Setup PDM amp config
      i2sMode |= I2S_MODE_PDM;
    break;
    case ADC_TYPE:
      // Setup ADC amp config
      i2sMode |= I2S_MODE_DAC_BUILT_IN;
    break;
  }
  i2s_amp_config.mode = (i2s_mode_t)i2sMode;  
  LOG_INF("Setup %s amplifier", deviceLabels[AMP_TYPE]);                 
}

static esp_err_t startupMic() {
  // install & start up the I2S peripheral as microphone when activated
  esp_err_t res = i2s_driver_install(I2S_MIC_PORT, &i2s_mic_config, 0, NULL);
  if (res == ESP_OK) {
    if (MIC_TYPE == ADC_TYPE) {
      // ADC device
      i2s_set_adc_mode(I2S_ADC_UNIT, I2S_ADC_CHANNEL); // init ADC pad
      i2s_adc_enable(I2S_MIC_PORT); // enable the ADC for I2S
    } else {
      // I2S or PDM device
      i2s_set_pin(I2S_MIC_PORT, &i2s_mic_pins);
    }
    // clear the DMA buffers
    i2s_zero_dma_buffer(I2S_MIC_PORT);  
    // set required sample rate
    i2s_set_clk(I2S_MIC_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    LOG_INF("Start microphone on port %i", I2S_MIC_PORT);
  } else LOG_WRN("Unable to startup mic");
  return res;
}

static esp_err_t startupAmp() {
  // install & start up the I2S peripheral as amplifier
  esp_err_t res = i2s_driver_install(I2S_AMP_PORT, &i2s_amp_config, 0, NULL);
  if (res == ESP_OK) {
    if (AMP_TYPE == ADC_TYPE) i2s_set_dac_mode(I2S_DAC_CHANNEL); // DAC device
    else i2s_set_pin(I2S_AMP_PORT, &i2s_amp_pins);     // I2S or PDM device
    // clear the DMA buffers
    i2s_zero_dma_buffer(I2S_AMP_PORT);
    // set required sample rate
    i2s_set_clk(I2S_AMP_PORT, SAMPLE_RATE, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
    LOG_INF("Start amplifier on port %i", I2S_AMP_PORT);
  } else LOG_WRN("Unable to startup amp");
  return res;
}

static size_t micInput() {
  // read sample from microphone if DMA ready
  size_t bytesRead = 0;
  i2s_read(I2S_MIC_PORT, SAMPLE_BUFFER, sampleBytes, &bytesRead, MIC_TIMEOUT);
  if (bytesRead > 0) {
    uint8_t gainFactor = pow(2, PREAMP_GAIN);
    for (int i = 0; i < bytesRead / sampleWidth; i++) {
      int32_t sample = SAMPLE_BUFFER[i];
      // if ADC mic, convert 12 bit unsigned to signed
      if (MIC_TYPE == ADC_TYPE) sample = (sample & 0xfff) - 2048;
      // apply preamp gain
      SAMPLE_BUFFER[i] = constrain(sample * gainFactor, SHRT_MIN, SHRT_MAX);    }
  }
  return bytesRead;
}

static void ampOutput(size_t bytesRead = sampleBytes, bool speaker = true) {
  // output to amplifier
  // apply required filtering and volume
  applyFilters(getVolumeControl());

  size_t bytesWritten = 0;
  if (speaker) i2s_write(I2S_AMP_PORT, SAMPLE_BUFFER, bytesRead, &bytesWritten, AMP_TIMEOUT);

  // use sound level for led brightness by changing duty cycle
  uint8_t ledLevel = constrain((abs(SAMPLE_BUFFER[0]) >> (8-getBrightness())), 0, UCHAR_MAX);
  ledcWrite(0, ledLevel);
}

/*********************************************/

static void makeRecording() {
  if (psramFound()) {
    LOG_INF("Recording ...");
    recordBytesUsed = WAV_HDR_LEN; // leave space for wave header
    while (recordBytesUsed < psramMax) {
      size_t bytesRead = micInput();
      memcpy(recordBuffer + recordBytesUsed, SAMPLE_BUFFER, bytesRead);
      recordBytesUsed += bytesRead;
      if (stopped) break;
    }
    recordSize = recordBytesUsed / sampleWidth - WAV_HDR_LEN;
    LOG_INF("%s recording of %d samples", stopped ? "Stopped" : "Finished",  recordSize);  
  } else LOG_WRN("PSRAM needed to record and play");
}

static void playRecording() {
  if (psramFound()) {
    LOG_INF("Playing, initial volume: %d", getVolumeControl()); 
    for (int i = WAV_HDR_LEN; i < recordBytesUsed; i += sampleBytes) { 
      memcpy(SAMPLE_BUFFER, recordBuffer+i, sampleBytes);
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

esp_err_t restartDevices() {
  // setup peripherals, depending on what is being connected
  // start / restart peripherals after configuration changed from web
  i2s_adc_disable(I2S_NUM_0);
  i2s_adc_disable(I2S_NUM_1);
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_uninstall(I2S_NUM_1);
  setupMic(); 
  setupAmp();
  esp_err_t restartOK = startupMic();
  if (restartOK == ESP_OK) restartOK = startupAmp();  
  return restartOK;
}

static void actionTask(void* parameter) {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (restartDevices() == ESP_OK) {
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
      }
    }
    stopped = false;
    ledcWrite(0, 0); // switch off led
    xSemaphoreGive(actionSemaphore);
  }
  vTaskDelete(NULL);
}

static void setupAudioLed() {
  // setup pwm controlled led
  ledcSetup(0, 4000, 8); // 12 kHz PWM, 8-bit resolution
  ledcAttachPin(AUDIO_LED_PIN, 0); // assign pin to channel
}

void OTAprereq() {} // dummy

void setup() {
  Serial.begin(115200);
  if ((fs::SPIFFSFS*)&STORAGE == &SPIFFS) startSpiffs();
  else prepSD_MMC();
  setupAudioLed();
  loadConfig();

  // connect wifi or start config AP if router details not available
  startWifi();
  startWebServer();
  LOG_INF("I2S preparation");
  if (psramFound()) recordBuffer = (uint8_t*)ps_malloc(psramMax);
  else LOG_WRN("No PSRAM available");
  // setup control buttons, pull to ground to activate
  pinMode(BUTTON_REC_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PLAY_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PASS_PIN, INPUT_PULLUP);
  pinMode(BUTTON_STOP_PIN, INPUT_PULLUP);
  pinMode(SWITCH_MODE_PIN, INPUT_PULLUP);
  actionSemaphore = xSemaphoreCreateBinary();
  attachInterrupt(digitalPinToInterrupt(BUTTON_REC_PIN), recISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PLAY_PIN), playISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PASS_PIN), passISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_STOP_PIN), stopISR, FALLING);
  xTaskCreate(&actionTask, "actionTask", 4096, NULL, 6, &actionHandle);
  xSemaphoreGive(actionSemaphore);
  checkMemory();
  LOG_INF("Initial volume: %d", getVolumeControl()); // prime analog read
  LOG_INF(APP_NAME " v" APP_VER " ready ...");
}

void loop() {}
