#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SimpleRotary.h>
#include <MD_AD9833.h>

#define PIN_AD_FSYNC 10
#define PIN_ROTARY_CLK A1
#define PIN_ROTARY_DT A2
#define PIN_ROTARY_SW A3

#define MAX_FREQ_KHZ 5000 //5 MHz

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SELECTOR_SIZE 4

#define OFFSET_X_FREQ 10
#define OFFSET_Y_FREQ 4

#define FONT_X_SIZE 10
#define FONT_Y_SIZE 14
#define FONT_X_SPACE 2

#define ICON_SPACER 4
#define ICON_Y_OFFSET 37
#define ICON_X_SIZE 27
#define ICON_Y_SIZE 25
#define ICON_IMG_SIZE 21

const unsigned char IMG_SIN [] PROGMEM = {
  0x0e, 0x00, 0x00, 0x31, 0x80, 0x00, 0x20, 0x80, 0x00, 0x40, 0x40, 0x00, 0x40, 0x40, 0x00, 0x40,
  0x40, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20,
  0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x10, 0x10,
  0x00, 0x10, 0x10, 0x00, 0x10, 0x10, 0x00, 0x08, 0x20, 0x00, 0x0c, 0x60, 0x00, 0x03, 0x80
};

const unsigned char IMG_SQR [] PROGMEM = {
  0xff, 0xe0, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80,
  0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20, 0x00, 0x80, 0x20,
  0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08,
  0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x20, 0x08, 0x00, 0x3f, 0xf8
};

const unsigned char IMG_TRI [] PROGMEM = {
  0x01, 0x00, 0x00, 0x02, 0x80, 0x00, 0x06, 0x80, 0x00, 0x04, 0x80, 0x00, 0x08, 0x40, 0x00, 0x18,
  0x40, 0x00, 0x10, 0x40, 0x00, 0x20, 0x20, 0x00, 0x40, 0x20, 0x00, 0x40, 0x20, 0x00, 0x80, 0x10,
  0x08, 0x00, 0x10, 0x10, 0x00, 0x10, 0x30, 0x00, 0x08, 0x20, 0x00, 0x08, 0x40, 0x00, 0x08, 0x40,
  0x00, 0x04, 0x80, 0x00, 0x05, 0x80, 0x00, 0x07, 0x00, 0x00, 0x02, 0x00, 0x00, 0x02, 0x00
};

const unsigned char IMG_OFF [] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0xfb, 0xe0, 0x42, 0x82, 0x00, 0x42, 0x82, 0x00, 0x42, 0x82,
  0x00, 0x42, 0xf3, 0xc0, 0x42, 0x82, 0x00, 0x42, 0x82, 0x00, 0x42, 0x82, 0x00, 0x3c, 0x82, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

enum class State { RUNNING, SET_UNIT, SET_DIG1, SET_DIG2, SET_DIG3, SET_DIG4, SET_DIG5};
enum class Event { NONE, ROTATE_LEFT, ROTATE_RIGHT, PUSH, LONG };
enum class Unit { HZ, KHZ, MHZ };

MD_AD9833	generator(PIN_AD_FSYNC);
SimpleRotary rotary(PIN_ROTARY_DT, PIN_ROTARY_CLK, PIN_ROTARY_SW);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

State state = State::RUNNING;
Event event = Event::NONE;
Unit unit = Unit::HZ;
MD_AD9833::mode_t mode = MD_AD9833::MODE_OFF;

float freq = 0;
bool redraw = true;
// --------------------------------------------------------------
void setup()   {
  Serial.begin(9600);
  generator.begin();
  initValues();

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Failed init OLED"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  redrawUI();
}

//----------------------------------------------------------------------------------
void loop() {
  redraw = true;
  event = rotaryLoop();

  switch (event) {
    case Event::PUSH:
      //RUNNING -> SET_UNIT -> SET_DIG1 -> SET_DIG2 -> SET_DIG3 -> SET_DIG4 -> RUNNING
      Serial.println(F("Event::PUSH"));
      switch (state) {
        case State::RUNNING:
          state = State::SET_UNIT;
          break;
        case State::SET_UNIT:
          state = State::SET_DIG1;
          break;
        case State::SET_DIG1:
          state = State::SET_DIG2;
          break;
        case State::SET_DIG2:
          state = State::SET_DIG3;
          break;
        case State::SET_DIG3:
          state = State::SET_DIG4;
          break;
        case State::SET_DIG4:
          state = State::SET_DIG5;
          break;
        case State::SET_DIG5:
        default:
          state = State::RUNNING;
          applyFreq();
          break;
      }
      break; //Event::PUSH

    case Event::LONG:
      Serial.println(F("Event::LONG"));
      state = State::RUNNING;
      applyFreq();
      break; //Event::LONG

    case Event::ROTATE_LEFT:
      Serial.println(F("Event::ROTATE_LEFT"));
      if (state == State::SET_UNIT) {
        changeUnit(true);
        applyFreq();
      } else if (state == State::RUNNING) {
        changeMode(true);
        applyMode();
      } else {
        changeFreq(true);
        applyFreq();
      }
      break; //Event::ROTATE_LEFT

    case Event::ROTATE_RIGHT:
      Serial.println(F("Event::ROTATE_RIGHT"));
      if (state == State::SET_UNIT) {
        changeUnit(false);
        applyFreq();
      } else if (state == State::RUNNING) {
        changeMode(false);
        applyMode();
      } else {
        changeFreq(false);
        applyFreq();
      }
      break; //Event::ROTATE_RIGHT

    default:
      redraw = false;
      break;
  }

  if (redraw) {
    redrawUI();
  }
}
//----------------------------------------------------------------------------------
void initValues() {
  mode = generator.getMode();
  freq = generator.getFrequency(MD_AD9833::CHAN_0);
  Serial.print(F("INIT FREQ: "));
  Serial.print(freq);
  Serial.print(F(" MODE: "));
  Serial.println(mode);

  if (freq > 1000000) {
    unit = Unit::MHZ;
    freq /= 1000000;
  } else if (freq > 1000) {
    unit = Unit::KHZ;
    freq /= 1000;
  }
}
//----------------------------------------------------------------------------------
Event rotaryLoop() {
  byte rotation = rotary.rotate();
  byte push = rotary.pushType(1000);

  if ( rotation == 1 ) {
    return Event::ROTATE_RIGHT;
  } else if ( rotation == 2 ) {
    return Event::ROTATE_LEFT;
  } else if ( push == 1 ) {
    return Event::PUSH;
  } else if ( push == 2 ) {
    return Event::LONG;
  } else {
    return Event::NONE;
  }
}
//----------------------------------------------------------------------------------
void redrawUI() {
  display.clearDisplay();
  drawFreq();
  drawSelector();
  drawModes();
  display.display();
}
//----------------------------------------------------------------------------------
void changeDigit() {
  switch (state) {
    case State::SET_DIG1:
      state = State::SET_DIG2;
      break;
    case State::SET_DIG2:
      state = State::SET_DIG3;
      break;
    case State::SET_DIG3:
      state = State::SET_DIG4;
      break;
    case State::SET_DIG4:
      state = State::SET_DIG5;
      break;
    case State::SET_DIG5:
      state = State::RUNNING;
      break;
    default:
      break;
  }
}
//----------------------------------------------------------------------------------
void changeMode(bool increase) {
  switch (mode) {
    case MD_AD9833::MODE_OFF:
      mode = increase ? MD_AD9833::MODE_SQUARE1 : mode;
      break;
    case MD_AD9833::MODE_SQUARE1:
      mode = increase ? MD_AD9833::MODE_SINE : MD_AD9833::MODE_OFF;
      break;
    case MD_AD9833::MODE_SINE:
      mode = increase ? MD_AD9833::MODE_TRIANGLE : MD_AD9833::MODE_SQUARE1;
      break;
    case MD_AD9833::MODE_TRIANGLE:
      mode = increase ? mode : MD_AD9833::MODE_SINE;
      break;
    default:
      break;
  }
}
//----------------------------------------------------------------------------------
void changeFreq(bool increase) {
  switch (state) {
    case State::SET_DIG1:
      countFreq(increase, 1);
      break;
    case State::SET_DIG2:
      countFreq(increase, 10);
      break;
    case State::SET_DIG3:
      countFreq(increase, 100);
      break;
    case State::SET_DIG4:
      countFreq(increase, 1000);
      break;
    case State::SET_DIG5:
      countFreq(increase, 10000);
      break;
  }
}
//----------------------------------------------------------------------------------
void changeUnit(bool increase) {
  if (unit == Unit::HZ) {
    unit = increase ? Unit::KHZ : Unit::MHZ;
  } else if (unit == Unit::KHZ) {
    unit = increase ? Unit::MHZ : Unit::HZ;
  } else {
    unit = increase ? Unit::HZ : Unit::KHZ;
  }

  //Checking if the frequency is within the limit of Ad9833
  if (unit == Unit::MHZ && freq >= MAX_FREQ_KHZ / 1000) {
    freq = MAX_FREQ_KHZ / 1000;
    return;
  }
  if (unit == Unit::KHZ && freq >= MAX_FREQ_KHZ) {
    freq = MAX_FREQ_KHZ;
    return;
  }
}
//----------------------------------------------------------------------------------
void countFreq(bool increase, long change) {
  //Checking if the frequency is within the limit of Ad9833
  if (increase && unit == Unit::MHZ && freq + change >= MAX_FREQ_KHZ / 1000) {
    freq = MAX_FREQ_KHZ / 1000;
    return;
  }
  if (increase && unit == Unit::KHZ && freq + change >= MAX_FREQ_KHZ) {
    freq = MAX_FREQ_KHZ;
    return;
  }

  //Checking if the digit is within the 0-9 range
  int digit = (((long)freq) % (change * 10)) / change;
  if (digit >= 9 && increase) {
    return;
  }
  if (digit <= 0 && !increase) {
    return;
  }

  if (increase) {
    freq += change;
  } else {
    freq -= change;
  }
}
//----------------------------------------------------------------------------------
int selectorPosition() {
  switch (state) {
    case State::SET_UNIT:
      return 7;
    case State::SET_DIG1:
      return 4;
    case State::SET_DIG2:
      return 3;
    case State::SET_DIG3:
      return 2;
    case State::SET_DIG4:
      return 1;
    case State::SET_DIG5:
      return 0;
    case State::RUNNING:
      switch (mode) {
        case MD_AD9833::MODE_OFF:
          return 10;
        case MD_AD9833::MODE_SQUARE1:
          return 11;
        case MD_AD9833::MODE_SINE:
          return 12;
        case MD_AD9833::MODE_TRIANGLE:
          return 13;
      }
    default:
      return -1;
  }
}
//----------------------------------------------------------------------------------
int modePosition() {
  switch (mode) {
    case MD_AD9833::MODE_SQUARE1:
      return 1;
    case MD_AD9833::MODE_SINE:
      return 2;
    case MD_AD9833::MODE_TRIANGLE:
      return 3;
    default:
      return 0;
  }
}
//----------------------------------------------------------------------------------
void drawFreq() {
  char buffer[10];
  sprintf(buffer, "%s%ld %s", generateFreqPrefix(), (long)freq, generateFreqPostfix());
  display.setCursor(OFFSET_X_FREQ, OFFSET_Y_FREQ);
  display.write(buffer);
}
//----------------------------------------------------------------------------------
void drawSelector() {
  int position = selectorPosition();
  if (position == -1) {
    return;
  }
  if (position < 10) {
    drawSelectorWithDirection(OFFSET_X_FREQ + FONT_X_SIZE / 2 + ((FONT_X_SPACE + FONT_X_SIZE)*position), OFFSET_Y_FREQ + 16, SELECTOR_SIZE, false);
  } else {
    int pos = position - 10;
    int x = ICON_SPACER + (ICON_SPACER * pos) + (ICON_X_SIZE / 2) + (ICON_X_SIZE * pos);
    int y = ICON_Y_OFFSET - SELECTOR_SIZE;
    drawSelectorWithDirection(x, y, SELECTOR_SIZE, true);
  }
}
//----------------------------------------------------------------------------------
void drawSelectorWithDirection(int x, int y, int size, bool flip) {
  if (flip) {
    for (int i = 0; i < size; i++) {
      display.drawTriangle(
        (x), (y - i), (x - size - i), (y - size - i), (x + size + i), (y - size - i),
        SSD1306_WHITE);
    }
  } else {
    for (int i = 0; i < size; i++) {
      display.drawTriangle(
        (x), (y + i), (x - size - i), (y + size + i), (x + size + i), (y + size + i),
        SSD1306_WHITE);
    }
  }
}
//----------------------------------------------------------------------------------
void drawModes() {
  int position = modePosition();
  drawIcon(ICON_SPACER, ICON_Y_OFFSET, IMG_OFF);
  drawIcon(ICON_SPACER * 2 + ICON_X_SIZE, ICON_Y_OFFSET, IMG_SQR);
  drawIcon(ICON_SPACER * 3 + ICON_X_SIZE * 2, ICON_Y_OFFSET, IMG_SIN);
  drawIcon(ICON_SPACER * 4 + ICON_X_SIZE * 3, ICON_Y_OFFSET, IMG_TRI);
  drawIconFrameBold(ICON_SPACER * (position + 1) + ICON_X_SIZE * position, ICON_Y_OFFSET);
}
//----------------------------------------------------------------------------------
void drawIcon(int xPos, int yPos, const uint8_t* icon) {
  drawIconFrameDotted(xPos, yPos);
  display.drawBitmap(xPos + 3, yPos + 2, icon, 21, 21, 1);
}
//----------------------------------------------------------------------------------
void drawIconFrameDotted(int xPos, int yPos) {
  for (int i = xPos; i < xPos + ICON_X_SIZE ; i = i + 2) {
    display.drawPixel(i, yPos, SSD1306_WHITE);
    display.drawPixel(i, yPos + ICON_Y_SIZE - 1, SSD1306_WHITE);
  }
  for (int i = yPos; i < yPos + ICON_Y_SIZE ; i = i + 2) {
    display.drawPixel(xPos, i, SSD1306_WHITE);
    display.drawPixel(xPos + ICON_X_SIZE - 1, i, SSD1306_WHITE);
  }
}
//----------------------------------------------------------------------------------
void drawIconFrameNormal(int xPos, int yPos) {
  display.drawRect(xPos, yPos, ICON_X_SIZE, ICON_Y_SIZE, SSD1306_WHITE);
}
//----------------------------------------------------------------------------------
void drawIconFrameBold(int xPos, int yPos) {
  display.drawRect(xPos, yPos, ICON_X_SIZE, ICON_Y_SIZE, SSD1306_WHITE);
  display.drawRect(xPos + 1, yPos + 1, ICON_X_SIZE - 2, ICON_Y_SIZE - 2, SSD1306_WHITE);
  display.drawRect(xPos - 1, yPos - 1, ICON_X_SIZE + 2, ICON_Y_SIZE + 2, SSD1306_WHITE);
}
//----------------------------------------------------------------------------------
char* generateFreqPrefix() {
  char* prefix;
  if (freq < 10) {
    prefix = "    ";
  } else if (freq < 100) {
    prefix = "   ";
  } else if (freq < 1000) {
    prefix = "  ";
  } else if (freq < 10000) {
    prefix = " ";
  } else {
    prefix = "";
  }
  return prefix;
}
//----------------------------------------------------------------------------------
char* generateFreqPostfix() {
  char* postfix;
  switch (unit) {
    case Unit::HZ:
      postfix = " Hz";
      break;
    case Unit::KHZ:
      postfix = "kHz";
      break;
    case Unit::MHZ:
      postfix = "MHz";
      break;
  }
  return postfix;
}
//----------------------------------------------------------------------------------
void applyFreq() {
  float frequence = freq;
  switch (unit) {
    case Unit::MHZ:
      frequence *= 1000;
    case Unit::KHZ:
      frequence *= 1000;
    default:
      break;
  }
  Serial.print(F("APPLY FREQ: "));
  Serial.println(frequence);
  generator.setFrequency(MD_AD9833::CHAN_0, frequence);
}
//----------------------------------------------------------------------------------
void applyMode() {
  Serial.print(F("APPLY MODE: "));
  Serial.println(mode);
  generator.setMode(mode);
}
