/*
* ornament + crime // 4xCV DAC8565  // "ASR" 
*
* --------------------------------
* TR 1 = clock
* TR 2 = hold
* TR 3 = oct +
* TR 4 = oct -
*
* CV 1 = sample in
* CV 2 = index CV
* CV 3 = # notes (constrain)
* CV 4 = octaves/offset
*
* left  encoder = scale select
* right encoder = param. select
* 
* button 1 (top) =  oct +
* button 2       =  oct -
* --------------------------------
*
*/

#include <spi4teensy3.h>
#include <rotaryplus.h>
#include <EEPROM.h>

#include "O_C_gpio.h"
#include "ADC.h"
#include "DAC.h"
#include "EEPROMStorage.h"
#include "util_app.h"
#include "util_button.h"
#include "util_pagestorage.h"
#include "util_framebuffer.h"
#include "page_display_driver.h"
#include "weegfx.h"
#include "SH1106_128x64_driver.h"

//#define ENABLE_DEBUG_PINS
#include "util_debugpins.h"

// Work-around until there are fonts available
const char *u8g_font_10x20 = "";
const char *u8g_font_6x12 = "";

FrameBuffer<SH1106_128x64_Driver::kFrameSize, 2> frame_buffer;
PagedDisplayDriver<SH1106_128x64_Driver> display_driver;
weegfx::Graphics graphics;

unsigned long LAST_REDRAW_TIME = 0;
#define REDRAW_TIMEOUT_MS 1

Rotary encoder[2] =
{
  {encL1, encL2}, 
  {encR1, encR2}
}; 

//  UI mode select
extern uint8_t UI_MODE;
extern uint8_t MENU_REDRAW;

/*  ------------------------ ASR ------------------------------------  */

#define MAX_VALUE 65535 // DAC fullscale 
#define MAX_ITEMS 256   // ASR ring buffer size
#define OCTAVES 10      // # octaves
uint16_t octaves[OCTAVES+1] = {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535}; // in practice  
const uint16_t THEORY[OCTAVES+1] = {0, 6553, 13107, 19661, 26214, 32768, 39321, 45875, 52428, 58981, 65535}; // in theory  
extern const uint16_t _ZERO;

/*  ---------------------  CV   stuff  --------------------------------- */

#define _ADC_RATE 1000 // 100us = 1kHz
#define _ADC_RES  12
volatile uint_fast8_t _ADC = false;
extern ADC::CalibrationData adc_calibration_data;

/* --- read  ADC ------ */
#define ADC_SCAN() \
do { \
  if (_ADC) { \
    _ADC = false; \
    ADC::Scan(); \
  } \
} while (0)

/*  --------------------- clk / buttons / ISR -------------------------   */

uint32_t _CLK_TIMESTAMP = 0;
uint32_t _BUTTONS_TIMESTAMP = 0;
static const uint16_t TRIG_LENGTH = 150;
static const uint16_t DEBOUNCE = 250;

volatile int CLK_STATE[4] = {0,0,0,0};
#define CLK_STATE1 (CLK_STATE[TR1])

void FASTRUN tr1_ISR() {  
    CLK_STATE[TR1] = true; 
}  // main clock

void FASTRUN tr2_ISR() {
  CLK_STATE[TR2] = true;
}

void FASTRUN tr3_ISR() {
  CLK_STATE[TR3] = true;
}

void FASTRUN tr4_ISR() {
  CLK_STATE[TR4] = true;
}

uint32_t _UI_TIMESTAMP;

enum the_buttons 
{  
  BUTTON_TOP,
  BUTTON_BOTTOM,
  BUTTON_LEFT,
  BUTTON_RIGHT
};

enum encoders
{
  LEFT,
  RIGHT
};

volatile uint_fast8_t _ENC = false;
static const uint32_t _ENC_RATE = 15000;

/*  ------------------------ core timer ISR ---------------------------   */
// 60 = 16.666...kHz : Works, SPI transfer ends 2uS before next ISR
// 66 = 15.1515...kHz
// 72 = 13.888...kHz

static const uint32_t CORE_TIMER_RATE = 100; // 100uS = 10 Khz
IntervalTimer CORE_timer;

uint32_t ADC_timer_counter = 0;
uint32_t ENC_timer_counter = 0;

void FASTRUN CORE_timer_ISR() {
  DEBUG_PIN_SCOPE(DEBUG_PIN_2);

  if (display_driver.Flush())
    frame_buffer.read();

  DAC::WriteAll();

  if (display_driver.frame_valid()) {
    display_driver.Update();
  } else {
    if (frame_buffer.readable())
      display_driver.Begin(frame_buffer.readable_frame());
  }

  // Update ADC/ENC "ISR" here instead of having extra timer interrupts
  if (ADC_timer_counter < _ADC_RATE / CORE_TIMER_RATE - 1) {
    ++ADC_timer_counter;
  } else {
    ADC_timer_counter = 0;
    _ADC = true;
  }

  if (ENC_timer_counter < _ENC_RATE / CORE_TIMER_RATE - 1) {
    ++ENC_timer_counter;
  } else {
    ENC_timer_counter = 0;
    _ENC = true;
  }

  if (current_app->isr)
    current_app->isr();
}

/*       ---------------------------------------------------------         */

const uint8_t bitmap[8] = {
  0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f
};

void setup(){
  
  NVIC_SET_PRIORITY(IRQ_PORTB, 0); // TR1 = 0 = PTB16
  analogReadResolution(_ADC_RES);
  analogReadAveraging(0x10);
  spi4teensy3::init();
  delay(10);

  // pins 
  pinMode(butL, INPUT);
  pinMode(butR, INPUT);
  pinMode(but_top, INPUT);
  pinMode(but_bot, INPUT);
  buttons_init();
 
  pinMode(TR1, INPUT); // INPUT_PULLUP);
  pinMode(TR2, INPUT);
  pinMode(TR3, INPUT);
  pinMode(TR4, INPUT);

  DebugPins::Init();

  // clock ISR 
  attachInterrupt(TR1, tr1_ISR, FALLING);
  attachInterrupt(TR2, tr2_ISR, FALLING);
  attachInterrupt(TR3, tr3_ISR, FALLING);
  attachInterrupt(TR4, tr4_ISR, FALLING);
  // encoder ISR 
  attachInterrupt(encL1, left_encoder_ISR, CHANGE);
  attachInterrupt(encL2, left_encoder_ISR, CHANGE);
  attachInterrupt(encR1, right_encoder_ISR, CHANGE);
  attachInterrupt(encR2, right_encoder_ISR, CHANGE);

  Serial.begin(9600); 

  ADC::Init(&adc_calibration_data);
  DAC::Init();

  frame_buffer.Init();
  display_driver.Init();
  graphics.Init();
 
  // TODO This automatically invokes current_app->isr so it might be necessary
  // to have an "enabled" flag, or at least set current_app to nullptr during
  // load/save/app selection

  CORE_timer.begin(CORE_timer_ISR, CORE_TIMER_RATE);

  // splash screen, sort of ... 
  hello();
  delay(2000);
  // calibrate? else use EEPROM; else use things in theory :
  if (!digitalRead(butL))  calibrate_main();
  else if (EEPROM.read(0x2) > 0) read_settings(); 
  else in_theory(); // uncalibrated DAC code 
  delay(750);   
  // initialize 
  init_DACtable();
  init_apps();
}


/*  ---------    main loop  --------  */

void loop() {
  _UI_TIMESTAMP = millis();
  UI_MODE = 1;

  while (1) {
    // don't change current_app while it's running
    if (SELECT_APP) select_app();
    current_app->loop();
    if (UI_MODE) timeout();
    if (millis() - LAST_REDRAW_TIME > REDRAW_TIMEOUT_MS)
      MENU_REDRAW = 1;
  }
}
