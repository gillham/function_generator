/*
 *
 * Extremely primitive function generator for Arduino boards.
 *
 * Copyright (c) 2011,2012,2013 Andrew Gillham
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GILLHAM ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ANDREW GILLHAM BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 */

/*
 * LCD Keypad shield version.  Supports "DF Robot" v1.0 boards.
 * These shields are available for around $10 shipped on eBay and
 * other places.  The newer versions use different resistance values
 * for the keys, check their sample code.
 * I will try to add a #define for the new boards so you dont' have
 * to edit the values.  I don't have a newer board for testing yet.
 *
 * The select button will start / stop the generator at any time.
 * Use the up/down buttons to move between function/rate/start.
 * Use the left/right buttons to change the values.  On the start menu
 * press the right button to start.
 *
 * Depending on the function you select, the valid rates will change.
 * The square wave uses Arduino clock (divided in half) directly so it 
 * supports up to 8MHz on a 16MHz Arduino.
 * The patterns are done via an ISR and max out at 100KHz.  The bit rate
 * is 100KHz and it sends 8 bits so the cycle rate between two bits toggling
 * is only 50KHz.  I might update the rates later to be more accurate, but
 * for now this is how it is setup.
 *
 * The square wave is quite precise as it is just hardware driven directly off
 * your clock.  If you have an accurate crystal, you have accurate output.
 * The pattern generation is accurate down to 500Hz.  Below 500Hz the clock
 * division error skews it slightly.  You can see it using a logic analyzer.
 *
 * Square wave -- pin is toggled on/off the same amount of time.
 * Narrow pulse -- pin is on 1 bit, off 7.
 * Wide pulse -- pin is on 7 bits, off 1.
 * Directional -- Not sure what to call this, but basically it is a pattern
 * that will tell you if your scope or logic analyzer is presenting the data
 * backwards or something.  Mostly useful for my logic_analyzer development.
 * 
 * 
 */

#include <LiquidCrystal.h>

// buttonClick states
enum {
  NOCLICK=20, DOWNCLICK, SHORT, LONG };
int buttonClick = NOCLICK;

// button states
// this is a list of possible buttons.
// order is based on LCD Keypad values.
enum {
  NONE=0, SELECT, LEFT, DOWN, UP, RIGHT };

// stores currently active button
byte buttonState = NONE;

// used with millis() to track SHORT/LONG clicks.
unsigned long buttonPressed;

// function generators get called with stop or start.
enum { 
  F_STOP, F_START };

unsigned char lcdmessage[14];

// what rates do we support.
// minimum & maximum are hardware limits generally
// functionrates should be set so that patterns don't use a rate higher than 100KHz
// we can't use an ISR any faster than 100KHz to toggle a pin.
int numrates = 14;
unsigned long ratelist[] = { 
  50, 100, 200, 500, 1000, 10000, 20000, 50000, 100000, 200000, 500000, 1000000, 2000000, 4000000, 8000000 };
int functionrates[] = { 
  14, 8, 8, 8 };
unsigned long cyclerate;
int rate = 0;

// position & patterns are used inside the ISR to generate
// the non-square wave signals.
byte position = 0;
byte patterns[][8] = {
  { 
    0, 1, 0, 1, 0, 1, 0, 1                       }
  , { 
    0, 0, 0, 0, 0, 0, 0, 1                       }
  , {
    1, 1, 1, 1, 1, 1, 1, 0                       }
  , {
    0, 0, 0, 1, 1, 1, 0, 1                       } 
};


const int backlightPin = 10;
const int signalPin = 11;
const int buttonPin = 0; // analog pin

int timerRunning = 0;

enum { 
  STOPPED, RUNNING };

int deviceState = STOPPED;

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);     // initialize the library with the numbers of the interface pins


char *mainMenulist[] = { 
  "Function", "Rate", "Start", 0 };

// start is a reserved word.
enum {
  FUNCTION=0, RATE, M_START };

char *mainMenuhelp[] = { 
  "Selects signal", "Manual or preset", "Start generator", 0 };
int menuchoice = 0;

char *functionList[] = {
  "Square", "Narrow Pulse", "Wide Pulse", "Directional", 0 };

enum { 
  SQUARE=0, NARROW, WIDE, DIRECTIONAL };
byte function = 0;

//
// setup LCD and appropriate pins.
//
void setup() {

  lcd.begin(16, 2);
  lcd.setCursor(0, 0);
  lcd.noAutoscroll();
  lcd.display();

  pinMode(signalPin, OUTPUT);
  pinMode(buttonPin, INPUT);

  Serial.begin(57600);

  startup();
}

void loop()
{

  // select can be used at anytime to start/stop the generator.
  if (buttonClick == DOWNCLICK && buttonState == SELECT) {
    if (deviceState == RUNNING) {
      if (function == SQUARE) {
        squareWave(F_STOP);
      } 
      else {
        patternFunction(F_STOP);
      }
    } 
    else {
      if (function == SQUARE) {
        squareWave(F_START);
      } 
      else {
        patternFunction(F_START);
      }
    }
  }

  // while the generator is running, UP/DOWN can be used to change the rate.
  if ((buttonClick == DOWN || buttonClick == UP) && deviceState == RUNNING) {
    setrate(buttonClick);
  } 

  // if we're stopped buttons operate the menu.
  if (buttonClick == DOWNCLICK && deviceState == STOPPED) {
    mainMenu(buttonState);
  } 

  // square wave uses the timer hardware to toggle so we can 
  // waste cpu with animation
  if (deviceState == RUNNING && function == SQUARE) {
    twiddle();
  }
  // check for button presses.
  getbuttons();

}


void mainMenu(int button) {

  switch(button) {
  case DOWN:
    menuchoice++;
    if (menuchoice > 2)
      menuchoice = 0;
    break;  
  case UP:
    menuchoice--;
    if (menuchoice < 0)
      menuchoice = 2;
    break;
  case RIGHT:
    switch(menuchoice) {
    case 0:
      function++;
      if (function > 3)
        function = 0;
      numrates = functionrates[function];
      rate = 0;
      break;
    case 1:
      rate++;
      if (rate > numrates)
        rate = 0;
      break;
    case 2:
      switch(function) {
      case SQUARE:
        squareWave(F_START);
        break;
      case NARROW:
      case WIDE:
      case DIRECTIONAL:
        patternFunction(F_START);
        break;
      default:
        break;
      }
      return;
      break;
    }
    break;
  case LEFT:
    switch(menuchoice) {
    case 0:
      Serial.print("function: ");
      Serial.println(function, DEC);
      function--;
      // 'byte' goes to 255 not negative if we decrement 0.
      if (function > 3)
        function = 3;
      numrates = functionrates[function];
      rate = 0;
      break;
    case 1:
      rate--;
      if (rate < 0)
        rate = numrates;
      break;
    case 2:
      //squareWave(STOP);
      break;
    }
    break;
  } 

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("==>");
  lcd.setCursor(4,0);
  lcd.print(mainMenulist[menuchoice]);
  lcd.setCursor(0,1);
  if (menuchoice == RATE) {
    lcd.print("rate: ");
    lcd.print(ratelist[rate], DEC);
    lcd.print(" Hz");
  }
  else if (menuchoice == FUNCTION) {
    lcd.print(functionList[function]);
  }
  else {
    lcd.print(mainMenuhelp[menuchoice]);
  }
}

void getbuttons() {

  int buttonNow = NONE;
  int debounce1 = NONE;
  int debounce2 = NONE;

  buttonClick = NOCLICK;

  // look for button presses and note them.
  // debounce by reading twice with a 2ms delay.
  debounce1 = get_button();
  delay(2);
  debounce2 = get_button();

  if (debounce1 == debounce2)
    buttonNow = debounce1;


  // we now know the debounced state of the button, but we don't know if it changed.
  // so we compare the current reading with the current state.
  // if we did change state and are now up, we know it is the up part of a short click.
  if (buttonNow != buttonState) {
    if (buttonNow != NONE) {
      buttonPressed = millis();
      buttonClick = DOWNCLICK;
    } 
    else {
      // return which button was fully clicked (down & up)
      buttonClick = buttonState;
    }
    buttonState = buttonNow;
  }
  // detect a long click. (> 1 second button press)
  if (buttonClick == DOWNCLICK && ((millis() - buttonPressed) > 1000)) {
    buttonClick = LONG;
  }
}


// read and decode buttons via ADC
// this works on the original v1.0.
// newer boards might need different values.
char get_button(void)  {
  int val;  

  val = analogRead(buttonPin);          // map analog input to button pushed

  // check ranges for buttons and set up the return value
  if (val < 74) {
    val = 5;  
  }                  // RIGHT button was pressed
  else if (val < 242) {
    val = 4;  
  }                  // UP button was pressed
  else if (val < 426) {
    val = 3;  
  }                  // DOWN button was pressed
  else if (val < 632) {
    val = 2;  
  }                  // LEFT button was pressed
  else if (val < 886) {
    val = 1; 
  }                   // SELECT button was pressed
  else {
    val = 0;  
  }                  // NO buton was pressed

  return val;
}

void setrate(int button) {
  int starttimer = 0;

  switch(button) {
  case UP:
    rate++;
    starttimer = 1;
    break;
  case DOWN:
    rate--;
    starttimer = 1;
    break;
  }
  if (rate > numrates)
    rate = 0;
  if (rate < 0)
    rate = numrates;
  cyclerate = ratelist[rate];

  // if the timer is running we restart it with the new rate
  if (starttimer)
    starttimer2();
}

void squareWave(int orders) {
  unsigned long tmp;

  if (orders == F_START) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Starting...");
    setrate(NONE);
    starttimer2();
  } 
  else {
    // stop is the only other option right now.
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Stopping...");
    stoptimer2();
  }
}

void patternFunction(int orders) {

  if (orders == F_START) {
    setrate(NONE);
    starttimer2();
  } 
  else {
    stoptimer2();
  }
}

void starttimer2(void) {
  unsigned long tmp;

  // reset timer to zero
  TCNT2 = 0;
  TCCR2A = 0;
  TCCR2B = 0;
  OCR2A = 0;

  if (function == SQUARE) {
    // Set CTC mode and toggle on compare.
    TCCR2A = _BV (COM2A0) | _BV (WGM21);
  } 
  else {
    // Set CTC mode and interrupt on compare.
    TCCR2A = _BV (WGM21);
    TIMSK2 = _BV (OCIE2A);
  }

  // No prescaler on 16MHz Arduino
  // works from 8MHz down to 31.250KHz
  if (cyclerate > 31250) {
    tmp = F_CPU / 2 / cyclerate;
    OCR2A = tmp - 1;
    TCCR2B = _BV (CS20);
  }

  if (cyclerate > 3922 && cyclerate <= 31250) {
    tmp = F_CPU / 2 / 8 / cyclerate;
    OCR2A = tmp - 1;
    TCCR2B = _BV (CS21);
  }
  if (cyclerate > 981 && cyclerate <= 3922) {
    tmp = F_CPU / 2 / 32 / cyclerate;
    OCR2A = tmp - 1;
    TCCR2B = _BV (CS21) | _BV (CS20);
  }

  if (cyclerate > 491 && cyclerate <= 981) {
    tmp = F_CPU / 2 / 64 / cyclerate;
    OCR2A = tmp - 1;
    TCCR2B = _BV (CS22);
  }

  if (cyclerate > 123 && cyclerate <= 491) {
    tmp = F_CPU / 2 / 256 / cyclerate;
    OCR2A = tmp - 1;
    TCCR2B = _BV (CS22) | _BV (CS21);
  }
  if (cyclerate > 31 && cyclerate <= 123) {
    tmp = F_CPU / 2 / 1024 / cyclerate;
    OCR2A = tmp - 1;
    TCCR2B = _BV (CS22) | _BV (CS21) | _BV (CS20);
  }

  // timer is running.
  timerRunning = 1;
  deviceState = RUNNING;
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(functionList[function]);
  lcd.setCursor(14,0);
  lcd.print("**");
  lcd.setCursor(0,1);
  lcd.print("rate: ");
  lcd.print(ratelist[rate], DEC);
  lcd.print(" Hz");
}

void stoptimer2(void) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Stopping...");

  // stop timer2
  TCCR2B = 0;
  OCR2A = 0;
  TCNT2 = 0;
  TIMSK0 = 0;
  timerRunning = 0;
  startup();
}

void startup(void) {

  //brag();
  //usage();

  // initialize variables to reset state.
  buttonState = NONE;
  deviceState = STOPPED;
  buttonPressed = 0;

  mainMenu(NONE);
}

void brag(void) {
  lcd.setCursor(0,0);
  //lcd.print((unsigned char*)PSTR("Signal Function"));
  lcd.print("Signal Function");
  lcd.setCursor(0,1);
  lcd.print("Generator");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("(c) 2013");
  lcd.setCursor(0,1);
  lcd.print("Andrew Gillham");
  delay(3000);
  lcd.clear();
}

void usage(void) {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Click to cycle");
  lcd.setCursor(0,1);
  lcd.print("signal rates");
  delay(3000);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Hold to start");
  lcd.setCursor(0,1);
  lcd.print("or stop signal");
  delay(3000);
  lcd.clear();
}

void twiddle(void) {
  static int i;

  switch(i) {
  case 1:
    lcd.setCursor(14,0);
    lcd.print(" ");
    lcd.setCursor(14,0);
    lcd.print("*");
    break;
  case 2:
    lcd.setCursor(14,0);
    lcd.print(" ");
    lcd.setCursor(15,0);
    lcd.print("*");
    break;
  case 3:
    lcd.setCursor(15,0);
    lcd.print(" ");
    lcd.setCursor(15,0);
    lcd.print("*");
    break;
  case 4:
    lcd.setCursor(15,0);
    lcd.print(" ");
    lcd.setCursor(14,0);
    lcd.print("*");
    // fall through.
  default:
    i = 0;
    break;
  }
  i++;
  delay(150);
}

/*
 * Interrupt handler used by non square wave functions.
 * We just read the next position in the pattern and set
 * the bit or clear it.
 * We use the modulus to get positions 0 through 7
 */
ISR(TIMER2_COMPA_vect)
{
  // increment byte variable and let it wrap.
  position++;

  // if the pattern has a 1 in this position we set the bit, else clear it.
  if(patterns[function][position%8]) {
    PORTB |= _BV(PINB3);
  } 
  else {
    PORTB &= ~(_BV(PINB3));
  }
}

