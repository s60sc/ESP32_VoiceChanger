# ESP32-VoiceChanger

ESP32 application to change a voice to be eg stormtrooper or dalek sounding, either in real time for cosplay or as a recording. Recordings can be downloaded to the browser as a WAV file
for playback on a media player.

## Installation

Download github files into the Arduino IDE sketch folder, removing `-master` from the application folder name.
Configure the application using the `#define` statements in `myConfig.h`.
Compile with Partition Scheme: `Minimal SPIFFS (...)`,  and with PSRAM enabled if present.

On first use, a basic web page allows a wifi connection to be defined, which then downloads the contents of the **/data** folder from GitHub into the `SPIFFS` partition on the ESP32.


## Hardware

A microphone and amplifier with speaker needs to be connected to the ESP32. 
Optionally LEDs can be connected that will flash according to the sound level.
A potentiometer can also be connected to control amplifier volume and LED brightness.
To enable recording the ESP32 needs to host PSRAM.

The types of microphone and amplifier that can be connected are combinations of Analog, PDM, and I2S. 
At least one device must be I2S as the ESP32 only supports PDM and Analog on one I2S peripheral. Analog microphones are low quality.
Cheap I2S devices that have been successfully tested with this app are:
* INMP441 I2S microphone
* MAX98357A I2S 3W amplifier

Other devices tested are:
* MP34DT01 PDM microphone
* MAX9814 ADC microphone
* ICSK025A DAC 3W amplifier

The application can be controlled by hardware buttons connected to pins defined in `myConfig.h`, and by software buttons on the web page.

For realtime voice changing, the microphone must be acoustically shielded from the speaker to prevent feedback squeal.


## Usage

Voice changing is achieved by applying software filters:
* Bandpass: emphasise a prticular range of frequencies
* Highcut (lowpass): attentuate higher frequencies 
* Lowcut (highpass): attentuate lower frequencies
* Peak: amplify particular frequencies
* Lowshelf: amplify lower frequencies
* Highshelf: amplify higher frequencies
* Ring modulator: use sinewave to create a dalek style voice 
* Clipping: reduce higher amplitudes depending on clippping hardness factor
* Reverb: add reverberation, depending on decay factor

Biquad filters can also be cascaded to accentuate a particular effect. For more detail on biquad filters see eg. https://arachnoid.com/BiQuadDesigner/index.html

Control buttons:
* Save: save current configuration to storage
* Record: save microphone input to PSRAM (up to 60 secs at 16kHz) without filtering, but with Preamp Gain applied
* Play: play recording currently in PSRAM using current filter settings
* Stop: stop current activity
* Output: download current recording using current filtering to browser as file named `Audio.wav` 
* PassThru: microphone input filtered and output to speaker directly

As the recorded data is not filtered it can be replayed with different filter configurations to find the best filter combination and settings.

Other settings:
* Preamp Gain: microphone gain
* Volume: amplifier volume level
* Brightness: LED brightness level
* Analog Control: if on, volume and brightness are controlled by potentiometer insted of web page
* Disable: if on, disables current filter settings without changing them

Example configuration for radio style voice:  
* Low Cut: Frequency 1500, Cascade 2
* High Cut: Frequency 2000, Q Factor 0.7
* Low Shelf: Frequency 2500, Gain dB 6 
* Peak: Frequency 400, Q Factor 0.7, Gain dB 3  

Example configuration for dalek style voice:  
* Low Cut: Frequency 100, Q Factor 0.7
* High Cut: Frequency 2000, Q Factor 0.7
* Ring Mod: Frequency 50

Browser functions only tested on Chrome.

![image1](extras/VC.png)


