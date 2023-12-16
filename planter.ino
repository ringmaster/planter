#include <Adafruit_NeoPixel.h>
#include <Adafruit_LIS3DH.h>
#include "Adafruit_seesaw.h"


#define PIN PIN_EXTERNAL_NEOPIXELS

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(32, PIN, NEO_GRB + NEO_KHZ800);

Adafruit_LIS3DH lis = Adafruit_LIS3DH();

Adafruit_seesaw ss;


#define CLICKTHRESHHOLD 80

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

int ringColor = 16;
int brightness = 50;
int mode = 0;
unsigned long timeout = 0;
int ring[32][3];

void setup() {
  // core1 setup
  Serial.begin(115200);

  if (! lis.begin(0x18)) {   // change this to 0x19 for alternative i2c address
    Serial.println("Couldnt start LIS3DH");
    while (1) yield();
  }

  if (!ss.begin(0x36)) {
    Serial.println("ERROR! seesaw not found");
    while(1) delay(1);
  } else {
    Serial.print("seesaw started! version: ");
    Serial.println(ss.getVersion(), HEX);
  }

  lis.setRange(LIS3DH_RANGE_2_G);

  lis.setClick(2, CLICKTHRESHHOLD);

  pinMode(PIN_EXTERNAL_POWER, OUTPUT);
  digitalWrite(PIN_EXTERNAL_POWER, HIGH);

  strip.begin();
  strip.setBrightness(brightness);
  strip.show();

  for(int i;i < 32; i++){
    for(int z;z < 3;z++) {
      ring[i][z] = 0;
    }
  }

  pinMode(PIN_EXTERNAL_BUTTON, INPUT_PULLUP);
}

uint8_t x = 0;
bool clict = false;
uint16_t capslow = 0;
const uint8_t scalespeed = 4;
uint8_t fadeColor = 0;
int effect = 0;

void loop() {
  x++;
  
  // Use tap sensor to cycle modes
  uint8_t click = lis.getClick();
  if (click & 0x30) {
    // Click function always triggers twice for some reason.  Ignore second tap
    if(clict) {
      clict = false;
    }
    else {
      // Only register taps farther apart than one second
      if(millis() > timeout + 1000) {
        Serial.print("Click detected (0x"); Serial.print(click, HEX); Serial.print("): ");
        if (click & 0x10 && mode != 3) {
          mode++;
          if (mode > 3) mode = 0;
          Serial.print(" single click, new mode: ");
          Serial.print(mode);
          clict = true;
          timeout = millis();
        }
        // Not sure what double-tap controls yet
        if (click & 0x20 && mode == 3) {
          mode = 0;
          Serial.print(" double click ");
          Serial.print(brightness);
          clict = true;
          timeout = millis();
        }
        Serial.println();
      }
    }
  }

  // Switch back to color cycle after 30 seconds
  if(millis() > timeout + 30000 && mode != 3 && mode !=0) {
    Serial.println("Mode 30 second timeout");
    mode = 0;
  }

  switch(mode) {
    case 0:
      digitalWrite(PIN_EXTERNAL_POWER, HIGH);
      switch(effect) {
        case 1:
          if(x % 30 == 0) fadeColor++;
          colorWipe(fadeColor);
        break;
        case 0:
          if(x % 100 == 0) {
            for(uint16_t i=0; i<strip.numPixels(); i++) {
              uint32_t col = strip.getPixelColor(i);
              if(col != 0) {
                col = (col & 255) >> 1;
                strip.setPixelColor(i, col, col, col);
              }
            }
            uint16_t id = random(strip.numPixels());
            if(strip.getPixelColor(id) == 0) {
              strip.setPixelColor(id, 255,255,255);
            }
          }
          strip.show();
        break;
      }
    break;
    case 1:
      showCapValue();
    break;
    case 2:
      showTempValue();
    break;
    case 3:
      digitalWrite(PIN_EXTERNAL_POWER, LOW);
    break;
  }

}

void showCapValue() {
  uint16_t capread = ss.touchRead(0);
  // capacitive value bounces a lot, so average the last scalespeed values
  capslow = (capslow * scalespeed + capread) / (scalespeed + 1);
  Serial.print("Capacitive: "); Serial.println(capread);
  colorScale(170, map(capslow, 300, 1016, 0, 100));
}

void showTempValue() {
  float tempF = (ss.getTemp() * 9/5) + 32;
  Serial.print("Temperature: "); Serial.print(tempF); Serial.println("*F");
  colorRange(170, 255, map(tempF, 50, 100, 0, 100));
}

// Fill all dots with a color
void colorWipe(byte WheelPos) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, Wheel(WheelPos));
  }
  strip.show();
}

// Scale brightness of color across the range
void colorScale(byte WheelPos, byte pct) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.ColorHSV(256*WheelPos, 255, 100*i/strip.numPixels()));
  }
  strip.setPixelColor(pct * strip.numPixels() / 100, strip.Color(255, 255, 255));
  strip.setPixelColor((pct * strip.numPixels() / 100) - 1, strip.Color(0, 0, 0));
  strip.setPixelColor((pct * strip.numPixels() / 100) + 1, strip.Color(0, 0, 0));
  strip.show();
}

void colorRange(byte bottom, byte top, byte pct) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    uint16_t colspot = map(i, 0, strip.numPixels(), 256 * bottom, 256 * top);
    strip.setPixelColor(i, strip.ColorHSV(colspot, 255, 128));
  }
  strip.setPixelColor(pct * strip.numPixels() / 100, strip.Color(255, 255, 255));
  strip.setPixelColor((pct * strip.numPixels() / 100) - 1, strip.Color(0, 0, 0));
  strip.setPixelColor((pct * strip.numPixels() / 100) + 1, strip.Color(0, 0, 0));
  strip.show();
}


// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}


// audio runs on core 2!

#include <I2S.h>

#include "boot.h"
#include "certainly.h"

struct {
  const uint8_t *data;
  uint32_t       len;
  uint32_t       rate;
} sound[] = {
  certainlyAudioData, sizeof(certainlyAudioData), certainlySampleRate,
  bootAudioData   , sizeof(bootAudioData)   , bootSampleRate,
};
#define N_SOUNDS (sizeof(sound) / sizeof(sound[0]))

I2S i2s(OUTPUT);

uint8_t sndIdx = 0;
int oldmode = 0;

void setup1(void) {
  i2s.setBCLK(PIN_I2S_BIT_CLOCK);
  i2s.setDATA(PIN_I2S_DATA);
  i2s.setBitsPerSample(16);
}

void loop1() {
  if (mode != oldmode) {
    oldmode = mode;
    Serial.printf("Core #2 Playing audio clip #%d\n", sndIdx);
    play_i2s(sound[sndIdx].data, sound[sndIdx].len, sound[sndIdx].rate);
  }
  /*
  delay(5000);
  if(++sndIdx >= N_SOUNDS) sndIdx = 0;
  */
}

void play_i2s(const uint8_t *data, uint32_t len, uint32_t rate) {

  // start I2S at the sample rate with 16-bits per sample
  if (!i2s.begin(rate)) {
    Serial.println("Failed to initialize I2S!");
    delay(500);
    i2s.end();
    return;
  }
  
  for(uint32_t i=0; i<len; i++) {
    uint16_t sample = (uint16_t)data[i] * 32;
    // write the same sample twice, once for left and once for the right channel
    i2s.write(sample);
    i2s.write(sample);
  }
  i2s.end();
}