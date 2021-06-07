#include <Arduino.h>
#include "TLC59108.h"
#include "Keyboard.h"
#include "InputDebounce.h"

#define HW_RESET_PIN 9
#define LED1_I2C_ADDR 0x41
#define LED2_I2C_ADDR 0x43

TLC59108 led1(LED1_I2C_ADDR);
TLC59108 led2(LED2_I2C_ADDR);

static byte BTN[4] = {10, 16, 14, 15};

#define BUTTON_DEBOUNCE_DELAY   50   // [ms]

static InputDebounce btn1;
static InputDebounce btn2;
static InputDebounce btn3;
static InputDebounce btn4;

static int KEY_MODI[4] = {KEY_LEFT_GUI,
                          KEY_LEFT_GUI,
                          KEY_LEFT_GUI,
                          KEY_LEFT_GUI};

static int KEY_SPEC[4] = {KEY_F9,
                          KEY_F10,
                          KEY_F11,
                          KEY_F12};

static byte swColorBuffer[4][3] = {};

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

void dimming(byte pwm) {
  led1.setLedOutputMode(TLC59108::LED_MODE::PWM_INDGRP);
  led2.setLedOutputMode(TLC59108::LED_MODE::PWM_INDGRP);
  led1.setRegister(TLC59108::REGISTER::GRPPWM::ADDR, pwm);
  led2.setRegister(TLC59108::REGISTER::GRPPWM::ADDR, pwm);
}

void disable_dimming() {
  led1.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);
  led2.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);
}

typedef enum {
  DISABLED=0,
  ON,
  OFF,
  SINGLE
} blink_mode_t;

class BlinkSwitch {
    private:
        blink_mode_t m_mode;
        unsigned long m_time; // last time for delay
        unsigned long m_on_time;  // how long the led should be turned on
        unsigned long m_off_time; // how long the led should be turned off
        byte m_switch;  // switch no. 1-4
        byte m_r;
        byte m_g;
        byte m_b;
    public:
        BlinkSwitch()
          : m_mode(DISABLED)
          , m_time(0)
          , m_on_time(0)
          , m_off_time(0)
          , m_switch(1)
          , m_r(0)
          , m_g(0)
          , m_b(0)
        { }
        BlinkSwitch(byte sw,
                    blink_mode_t mode = DISABLED,
                    unsigned long on_time  = 0,
                    unsigned long off_time = 0,
                    byte r = 0,
                    byte g = 0,
                    byte b = 0)
          : m_mode(mode)
          , m_on_time(on_time)
          , m_off_time(off_time)
          , m_switch(sw)
          , m_r(r)
          , m_g(g)
          , m_b(b)
          {
            m_time = millis();
            if (mode == DISABLED){
              reset();
            }
            if (mode == SINGLE) {
              swColor(m_switch, r, g, b);
            }
        }
        void reset() {
          swColor(m_switch,
                  swColorBuffer[m_switch-1][0],
                  swColorBuffer[m_switch-1][1],
                  swColorBuffer[m_switch-1][2]);
        }
        void process() {
          unsigned long current = millis();
          unsigned long delta = abs(current-m_time);
          
          if (m_on_time>0 && delta > m_on_time){
            if (m_mode == SINGLE) {
              reset();
              m_mode = DISABLED;
              m_time = current;
            }
          }
          current %= (m_on_time + m_off_time);
          if (m_mode == ON && current > m_on_time) {
            reset();
            m_mode = OFF;
          } else if (m_mode == OFF && current <= m_on_time) {
            swColor(m_switch, m_r, m_g, m_b);
            m_mode = ON;
          }
        }
};

BlinkSwitch blinking[4];

/* ** Serial Buffer Format **

  Serial buffer commands consist of one command per line, 
  
  where each command has the form
  
  	<command>[ <arg 1> [<arg 2> [...]]]
  
  i.e. Commands and arguments are separated by exactly one space char (0x20)
  
    ** Available Commands **
    
  1) COL: Set Color
  
  	COL <switch> <red> <green> <blue>
  	
  	with 
  	  <switch> as a number between 1 and 4 denoting the switch
  	  <red> <green> <blue> as three digit decimal color values 0 - 255
  
  2) DIM: Set Dimming
  
    DIM [<brightness>]
    
    with
      <brightness> as three digit decimal color values 0 - 255
    
    If brightness is not provided, dimming is disabled.

  3) BLNK: Set Blinking

    BLNK <switch> <mode> <red> <green> <blue>
    
    with 
  	  <switch> as a number between 1 and 4 denoting the switch
      <mode> as one of SINGLE, SHORT, LONG, OFF
  	  <red> <green> <blue> as three digit decimal color values 0 - 255
*/


#define SERIAL_BUF_SIZE 64
static char serialBuffer[SERIAL_BUF_SIZE];
static int serialBufferPos = 0;

#define SERIAL_MAX_ARGS 10
static int serialArgIdx[SERIAL_MAX_ARGS];
static int numArgs = 0;

void parseSerialBuffer();
void processSerialCommand();
void processCOL();
void processDIM();
void processBLNK();
int n(char* c);
int nnn(char* c);
void trigger_keypress(int idx);

void checkSerial() {
  if (Serial.available()) {
    char ch = Serial.read();
    if ((ch != '\n') && (serialBufferPos < SERIAL_BUF_SIZE)) {
      serialBuffer[serialBufferPos++] = ch;
    } else {
      serialBuffer[serialBufferPos] = '\0';
      
      parseSerialBuffer();
      processSerialCommand();
      
      serialBufferPos = 0;
   }
  }

  return;
}

void parseSerialBuffer() {
  // Replace spaces with string delimiters, and store argument indexes.
  // Also counts the arguments.
  
  numArgs = 0;

  for (int pos = 0;
       (serialBuffer[pos] != '\n') && (pos < SERIAL_BUF_SIZE);
       pos++) {
    if (serialBuffer[pos] == ' ') {
      // Mark end of argument string
      serialBuffer[pos] = '\0';

      if (numArgs < SERIAL_MAX_ARGS) {
        serialArgIdx[numArgs] = pos+1;
        ++numArgs;
      }
    }
  } 
}

char* getSerialArg(int idx) {
  if (idx >= SERIAL_MAX_ARGS) 
    return NULL;

  return serialBuffer+serialArgIdx[idx];
}

void processSerialCommand() {
  if (strcmp("COL\0", serialBuffer) == 0)
    processCOL(); 
  else if (strcmp("DIM\0", serialBuffer) == 0)
    processDIM(); 
  else if (strcmp("BLNK\0", serialBuffer) == 0)
    processBLNK(); 
  else
    Serial.write("Unknown command!\n")  ;
}

void processCOL() {
  if (numArgs != 4) {
    Serial.write("COL requires 4 arguments: <switch> <red> <green> <blue>!\n");
    return;
  }

  byte sw = n(getSerialArg(0));

  if ((sw < 1) || (sw > 4)) {
    Serial.write("Switch number must be between 1 and 4.\n");
    return;
  }

  byte r = nnn(getSerialArg(1));
  byte g = nnn(getSerialArg(2));
  byte b = nnn(getSerialArg(3));

  swColor(sw, r, g, b);
  swColorBuffer[sw-1][0] = r;
  swColorBuffer[sw-1][1] = g;
  swColorBuffer[sw-1][2] = b;

  Serial.write("OK\n");
}

void processDIM() {
  if (numArgs == 0) {
    disable_dimming();
  }
  if (numArgs != 1) {
    Serial.write("DIM can only handle up to one argument: <brightness>!\n");
    return;
  }

  byte pwm = (byte) String(getSerialArg(0)).toInt();

  dimming(pwm);

  Serial.write("OK\n");
}

void processBLNK() {
  if (numArgs != 5) {
    Serial.write("BLNK requires 4 arguments: <switch> <mode> <red> <green> <blue>!\n");
    return;
  }
  byte sw = n(getSerialArg(0));

  if ((sw < 1) || (sw > 4)) {
    Serial.write("Switch number must be between 1 and 4.\n");
    return;
  }
  char* mode_str = getSerialArg(1);

  byte r = (byte) String(getSerialArg(2)).toInt();
  byte g = (byte) String(getSerialArg(3)).toInt();
  byte b = (byte) String(getSerialArg(4)).toInt();

  // use 1 Hz for slow and 4 Hz for short blinks as NASA suggests
  // https://colorusage.arc.nasa.gov/flashing.php

  const unsigned long SHORT_MS = 250;
  const unsigned long LONG_MS = 1000;

  if (strcmp("SINGLE\0", mode_str) == 0) {
    blinking[sw-1] = BlinkSwitch(sw, SINGLE, LONG_MS, LONG_MS, r, g, b);
  } else if (strcmp("SHORT\0", mode_str) == 0) {
    blinking[sw-1] = BlinkSwitch(sw, ON, SHORT_MS, SHORT_MS, r, g, b);
  } else if (strcmp("LONG\0", mode_str) == 0) {
    blinking[sw-1] = BlinkSwitch(sw, ON, LONG_MS, LONG_MS, r, g, b);
  } else if (strcmp("OFF\0", mode_str) == 0) {
    blinking[sw-1] = BlinkSwitch(sw);
  } else {
    Serial.write("Mode must be one of SINGLE, SHORT, LONG, OFF.\n");
    return;
  }
  Serial.write("OK\n");
  return;
}

void button_pressed_callback(uint8_t pinIn) { 
  for (int i = 0; i < 4; i++)
    if (BTN[i] == pinIn) 
      trigger_keypress(i);
}

void trigger_keypress(int idx) {
  if (KEY_MODI[idx])
    Keyboard.press(KEY_MODI[idx]);

  if (KEY_SPEC[idx])
    Keyboard.press(KEY_SPEC[idx]);

  delay(5);
 
  Keyboard.releaseAll();
}

void setup() {
  // LED setup
  Wire.begin();
  led1.init(HW_RESET_PIN);
  led1.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);
  led2.init();
  led2.setLedOutputMode(TLC59108::LED_MODE::PWM_IND);

  // KEYBOARD setup
  Keyboard.begin();

  // DEBOUNCE setup
  btn1.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);
  btn2.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);
  btn3.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);
  btn4.registerCallbacks(button_pressed_callback, NULL, NULL, NULL);

  btn1.setup(BTN[0], BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
  btn2.setup(BTN[1], BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
  btn3.setup(BTN[2], BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
  btn4.setup(BTN[3], BUTTON_DEBOUNCE_DELAY, InputDebounce::PIM_EXT_PULL_DOWN_RES);
}

int n(char* c) {
  return c[0] - '0';
}

int nnn(char* c) {
  return n(c+0) * 100 + n(c+1) * 10 + n(c+2);
}

void loop(){
  // process buttons
  unsigned long now = millis();

  btn1.process(now);
  btn2.process(now);
  btn3.process(now);
  btn4.process(now);

  checkSerial();

  for(int i = 0; i < 4; ++i){
    blinking[i].process();
  }

  delay(1);
}
