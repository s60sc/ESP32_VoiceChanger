
// Voice Changer filters
//
// Biquad parameters:
// - filter type
// - normalised central frequency (central freq in Hz / sample rate)
// - Q factor : default is 0.707 - sqrt(2), relevant to Pass type filters
// - peak gain in dB : only relevant to peak and shelf filters
// - use graphical calculators to see what the frequency response curve is:
//     https://www.earlevel.com/main/2021/09/02/biquad-calculator-v3/
//     https://arachnoid.com/BiQuadDesigner/index.html
//
// s60sc 2021

#include "appGlobals.h"
#include "Biquad.h"

// web filter parameters
bool RING_MOD;
bool BAND_PASS;
bool HIGH_PASS;
bool LOW_PASS;
bool HIGH_SHELF;
bool LOW_SHELF;
bool PEAK;
bool CLIPPING;
bool REVERB;
float BP_Q;    // sharpness of filter
float BP_FREQ; // center frequency for band pass
uint16_t BP_CAS; // number of cascaded filters
float HP_Q;    // sharpness of filter
float HP_FREQ; // high frequency for high pass
uint16_t HP_CAS; // number of cascaded filters
float LP_Q;    // sharpness of filter
float LP_FREQ; // low frequency for low pass
uint16_t LP_CAS; // number of cascaded filters
float HS_GAIN; // amplify gain above frequency
float HS_FREQ; // high frequency for high shelf
float LS_GAIN; // amplify gain below frequency
float LS_FREQ; // low frequency for low shelf
float PK_GAIN; // amplify gain around frequency
float PK_FREQ; // center frequency for peak
float PK_Q;    // sharpness of filter
int SW_FREQ;    // frequency of generated sine wave
uint8_t SW_AMP;    // amplitude of generated sine wave
int CLIP_FACTOR; // factor used to clip higher amplitudes
int DECAY_FACTOR; // factor used control reverb decay
float PITCH_SHIFT; // factor used shift pitch up or down

// local definitions
static const int MAX_FILTERS = 10;
static Biquad *filter[MAX_FILTERS];
static int filtIdx = 0;
static int8_t* sineWaveTable;
static uint32_t dataPoints;
static float Qvals[40];

static int factorial(int top) {
  int fact = 0;
  for (int i = 1; i <= top; i++) fact += i;
  return fact;
}

static void generateSineWave(uint16_t frequency, uint8_t amplitude) {
  // pre generate sine wave table at given frequency
  dataPoints = SAMPLE_RATE / frequency; // number of data points for given freq
  sineWaveTable = (int8_t*)malloc(dataPoints);
  for (int i = 0; i < dataPoints; i++) {
    sineWaveTable[i] = static_cast<int8_t>(sin(M_PI * 2 * frequency * i / SAMPLE_RATE) * amplitude);
  }
  LOG_INF("Generated %i sine wave data points", dataPoints);
}

static void calcQvals() {
  // calculate ideal Q vals for sequence of cascaded high or low pass filters
  // cascades is number of cascaded biquads
  int idx = 0;
  for (int c = 1; c < 9; c++) {
    for (int i = 0; i < c; i++) Qvals[idx++] = 1 / (2 * cos((1 + (i * 2)) * PI / (c * 4)));
  }
}

static float nyquist(float freq, uint16_t srate) {
  float retval = freq / (float)srate;
  if (retval > 0.5) {
    retval = 0.5;
    LOG_WRN("Cutoff frequency reduced as too high");
  }
  return retval;
}

static inline void initBiquad(int ftype, float freq, float Qval, float gain, int cascade) {
  float cutoff = nyquist(freq, SAMPLE_RATE);
  // if only one filter use user requested Qval, else use predefined Q values for Butterworth response
  int iOffset = factorial(cascade - 1);
  for (int i = 0; i < cascade; i++) {
    Qval = (cascade == 1) ? Qval : Qvals[iOffset+i];
    filter[filtIdx++] = new Biquad(ftype, cutoff, Qval, gain);
  }
}

void setupFilters() {
  calcQvals();
  filtIdx = 0;
  long framesize = sampleBytes / sizeof(int16_t);
  if (RING_MOD) generateSineWave(SW_FREQ, SW_AMP);
  if (BAND_PASS) initBiquad(bq_type_bandpass, BP_FREQ, BP_Q, 0, BP_CAS);
  if (HIGH_PASS) initBiquad(bq_type_highpass, HP_FREQ, HP_Q, 0, HP_CAS);
  if (LOW_PASS) initBiquad(bq_type_lowpass, LP_FREQ, LP_Q, 0, LP_CAS);
  if (HIGH_SHELF) initBiquad(bq_type_highshelf, HS_FREQ, 1, HS_GAIN, 1);
  if (LOW_SHELF) initBiquad(bq_type_lowshelf, LS_FREQ, 1, LS_GAIN, 1);
  if (PEAK) initBiquad(bq_type_peak, PK_FREQ, PK_Q, PK_GAIN, 1);
  if (PITCH_SHIFT != 1.0) smbPitchShiftInit(PITCH_SHIFT, framesize, OSAMP, SAMPLE_RATE);
}

void applyFilters() { 
  if (!DISABLE) {
    // modify input signal using required filters
    for (int k = 0; k < filtIdx; k++) {
      // apply each required biquad filter in turn
      for (int i = 0; i < DMA_BUFF_LEN; i++)  
        sampleBuffer[i] = (int16_t)filter[k]->process((float)sampleBuffer[i]); 
    }
  
    if (RING_MOD) {
      // output dalek style voice, by multiplying input value with sine wave value
      for (int i = 0; i < DMA_BUFF_LEN; i++) { 
        int32_t thisSample = (int32_t)sampleBuffer[i] * sineWaveTable[i % dataPoints];
        sampleBuffer[i] = (int16_t)(thisSample / SW_AMP); 
      }
    }
    
    // add reverb
    if (REVERB) {
      static int16_t reverbBuff[REVERB_SAMPLES] = {0};
      static size_t reverbPtr = 0;
      for (int i = 0; i < DMA_BUFF_LEN; i++) {
        int16_t reverbed = sampleBuffer[i] + reverbBuff[reverbPtr] / (DECAY_FACTOR + 1);
        sampleBuffer[i] = reverbBuff[reverbPtr] = reverbed;
        reverbPtr = (reverbPtr + 1) % REVERB_SAMPLES;
      }
    }
  }
  applyVolume();

  // change pitch if required, resource intensive
  if (PITCH_SHIFT != 1.0) smbPitchShift(DMA_BUFF_LEN / sizeof(int16_t), sampleBuffer, sampleBuffer);

  if (!DISABLE) {
    // clip higher amplitudes 
    if (CLIPPING) {
      // clip factor: 1 = soft clip, 10 = hard clip
      float clipFactor = 1 + CLIP_FACTOR / 6.0;
      for (int i = 0; i < DMA_BUFF_LEN; i++) {
        float inputF = (float)(sampleBuffer[i]) / SHRT_MAX;
        float c = inputF * clipFactor;
        sampleBuffer[i] = (int16_t)(SHRT_MAX * (1 / clipFactor * (c / (1.0 + 0.28 * (c * c)))));
      }
    }
  }
}
