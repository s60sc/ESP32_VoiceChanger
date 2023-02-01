# ESP32-VoiceChanger

ESP32 application to change a voice to be eg stormtrooper or dalek sounding, either in real time for cosplay or as a recording. Recordings can be downloaded to the browser as a WAV file
for playback on a media player.
Can be hosted on a ESP32 or ESP32-S3.

## Installation

Download github files into the Arduino IDE sketch folder, removing `-master` from the application folder name.
Compile with PSRAM enabled if available, and the following Partition scheme:
* ESP32 - `Minimal SPIFFS (...)`
* ESP32S3 - `8M with spiffs (...)`

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

Analog devices are not supported by I2S on ESP32-S3.

The application can be controlled by hardware buttons connected to pins defined via the app web page.

For realtime voice changing, the microphone must be acoustically shielded from the speaker to prevent feedback squeal.


## Usage

Voice changing is achieved by applying software filters:
* Bandpass: emphasise a particular range of frequencies
* Highcut (lowpass): attentuate higher frequencies 
* Lowcut (highpass): attentuate lower frequencies
* Peak: amplify particular frequencies
* Lowshelf: amplify lower frequencies
* Highshelf: amplify higher frequencies
* Ring modulator: use sinewave to create a dalek style voice 
* Clipping: reduce higher amplitudes depending on clippping hardness factor
* Reverb: add reverberation, depending on decay factor

Biquad filters can also be cascaded to accentuate a particular effect. For more detail on biquad filters see eg. https://arachnoid.com/BiQuadDesigner/index.html

## Web page controller

Control buttons:
* Save: save current configuration to storage
* Record: save microphone input to PSRAM (up to 60 secs (ESP32) / 180 secs (ESP32S3) at 16kHz) without filtering, but with Preamp Gain applied
* Play: play recording currently in PSRAM using current filter settings
* Stop: stop current activity
* Output: download current recording using current filtering to browser as file named `VoiceChanger.wav` 
* PassThru: microphone input filtered and output to speaker directly

As the recorded data is not filtered it can be replayed with different filter configurations to find the best filter combination and settings.

Other settings:
* Preamp Gain: microphone gain
* Volume: amplifier volume level
* Brightness: LED brightness level
* Analog Control: if on, volume and brightness are controlled by potentiometer instead of web page
* Disable: if on, disables current filter settings without changing them to hear original

Example configuration for radio style voice:  
* Low Cut: Frequency 1500, Cascade 2
* High Cut: Frequency 2000, Q Factor 0.7
* Low Shelf: Frequency 2500, Gain dB 6 
* Peak: Frequency 400, Q Factor 0.7, Gain dB 3  

Example configuration for dalek style voice:  
* Low Cut: Frequency 100, Q Factor 0.7
* High Cut: Frequency 2000, Q Factor 0.7
* Ring Mod: Frequency 50

![image1](extras/VC.png)


## Configuration Tabs

* **Show Log**: Opens web socket to view log messages dynamically.

* **OTA Update**: Update application bin file or files in **/data** folder using OTA.

* **Edit Config**:

  * **Reboot & Save**: Save configuration changes and restart the ESP to apply.

  * **Clear NVS**: Clear current passwords.

  * **Reload /data**: Reload data files from github.

  * **Wifi**: WiFi and webserver settings.

  * **Pins**: Define pins used by microphone, amplifier, buttons.


Browser functions only tested on Chrome.


