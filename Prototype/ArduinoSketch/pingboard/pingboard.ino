#include "TLC59108.h"
#include "Keyboard.h"

#define HW_RESET_PIN 9
#define LED1_I2C_ADDR 0x41
#define LED2_I2C_ADDR 0x43

TLC59108 led1(LED1_I2C_ADDR);
TLC59108 led2(LED2_I2C_ADDR);

static byte BTN[4] = {10, 16, 14, 15};

static int KEY_MODI[4] = {KEY_LEFT_GUI,
                          KEY_LEFT_GUI,
                          KEY_LEFT_GUI,
                          KEY_LEFT_GUI};

static int KEY_SPEC[4] = {KEY_F9,
                          KEY_F10,
                          KEY_F11,
                          KEY_F12};


// 0 - K1 RED
// 1 - K1 GREEN
// 2 - K1 BLUE
// 3 - K2 RED
// 4 - K2 GREEN
// 5 - K2 BLUE
// 6 - N/A
// 7 - N/A

void swColor(byte sw, byte r, byte g, byte b) {
  if ((sw == 1) || (sw == 2)) {
    byte base_channel = sw == 1 ? 3 : 0;

    led1.setBrightness(base_channel + 0, r);
    led1.setBrightness(base_channel + 1, g);
    led1.setBrightness(base_channel + 2, b);
  } else {
    byte base_channel = sw == 3 ? 3 : 0;

    led2.setBrightness(base_channel + 0, r);
    led2.setBrightness(base_channel + 1, g);
    led2.setBrightness(base_channel + 2, b);
  }
}

void bsColor(byte sw, bool state, byte r, byte g, byte b) {
  if (state)
    swColor(sw, r, g, b);
  else
    swColor(sw, 0, 0, 0);
}

static bool button_state[4] = {0, 0, 0, 0};

// Serial Buffer Format is
// SRRRGGGBBBn
// all digits as ASCII digit
// S switch number 1 to 4
// RRR three digit decimal red   0 - 255
// GGG three digit decimal green 0 - 255
// BBB three digit decimal blue  0 - 255
// n newline

#define SERIAL_BUFFER_SIZE 10
static char serialBuffer[SERIAL_BUFFER_SIZE];
static int serialBufferPos = 0;

void setup() {
  // LED setup
  Wire.begin();
  led1.init(HW_RESET_PIN);
  led1.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);
  led2.init();
  led2.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);

  // BUTTON setup
  for (int i = 0; i < 4; i++)
    pinMode(BTN[i], INPUT);

  // KEYBOARD setup
  Keyboard.begin();
}

int n(char* c) {
  return c[0] - '0';
}

int nnn(char* c) {
  return n(c+0) * 100 + n(c+1) * 10 + n(c+2);
}

void checkSerial() {
  if (Serial.available()) {
    char ch = Serial.read();
    if ((ch != '\n') && (serialBufferPos < SERIAL_BUFFER_SIZE)) {
      serialBuffer[serialBufferPos++] = ch;
    } else {
      if (serialBufferPos < 10) {
          Serial.write("Serial Buffer expects 10 digits (SRRRGGGBBB).\n");
          goto failed;
      }

      byte sw = n(serialBuffer+0);

      if ((sw < 1) || (sw > 4)) {
        Serial.write("Switch number must be between 1 and 4.\n");
        goto failed;
      }

      byte r = nnn(serialBuffer+1);
      byte g = nnn(serialBuffer+4);
      byte b = nnn(serialBuffer+7);

      swColor(sw, r, g, b);

      serialBufferPos = 0;
      Serial.write("OK\n");
    }
  }

  return;

failed:
  Serial.write("Error processing serial buffer content!\n");
  serialBufferPos = 0;
}

void loop(){
  // check for key press
  // and initiate button press if key has been pressed down
  for (int i = 0; i < 4; i++) {
    bool bs = digitalRead(BTN[i]);

    if (bs && !button_state[i]) {
      if (KEY_MODI[i])
        Keyboard.press(KEY_MODI[i]);

      if (KEY_SPEC[i])
        Keyboard.press(KEY_SPEC[i]);

      delay(50);
      Keyboard.releaseAll();
    }

    button_state[i] = bs;
  }

  checkSerial();

  delay(10);

//  bsColor(1, button_state[0], 95, 0, 0);
//  bsColor(2, button_state[1], 24, 197, 110);
//  bsColor(3, button_state[2], 25, 75, 255);
//  bsColor(4, button_state[3], 156, 131, 7);
}
