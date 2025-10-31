#include <Arduino.h>
#include <OneButtonTiny.h>  //This is part of the one button library

#include <avr/io.h>
#include <avr/interrupt.h>
//#include <avr/eeprom.h>
#include <EEPROM.h>

// Set one parameter below for the version of the SAOuija you have
//#define SAOUIJA_FRONTLIT
#define SAOUIJA_BACKLIT

#define INITIALIZE_EEPROM  // Uncomment this only when first flashing the board. It overwrites the eeprom


// Row LOW, col HI to turn on LED
#define NROWS 7
#define NCOLS 6

#ifdef SAOUIJA_BACKLIT
  #define COL_OFF         HIGH
  #define COL_ON          LOW
#endif

#ifdef SAOUIJA_FRONTLIT
  #define COL_OFF         LOW
  #define COL_ON          HIGH
#endif

// Macro to convert hex row or column byte value to array index
#define HEX_ROW(hval) (((hval >> 4) & 0xF) - 1)
#define HEX_COL(hval) ((hval & 0xf) - 1)

enum MODES {MODE_DISPLAY, MODE_ENTER, MODE_STORE_ENTRY, NUM_MODES};
enum MODES FUNCTION_MODE = MODE_DISPLAY;

// Longest string that can be displayed and stored in EEPROM (shouldn't be longer than EEPROM_SIZE)
#define MAX_STRING_SIZE 64
// Number of LEDs illuminating numbers, leters and the word "SAOuija"
#define NNUMS   10
#define NLETS   26
#define NOUIJA  4

//High 4 bits are row, low 4 bits are column for the front mounted SAO
const byte YES                      = 0x16;
const byte NO                       = 0X11;
#ifdef SAOUIJA_BACKLIT
  const byte charlist[NLETS + NNUMS]  = { 0x71, 0x61, 0x51, 0x41, 0x31, 0x21, 0x24, 0x34, 0x44, 0x54, 0x74, 0x75, 0x76, 0x72, 0x62, 0x52, 0x42, 0x32, 0x22, 0x25, 0x35, 0x45, 0x55, 0x64, 0x65, 0x66, // Letters A-Z
                                        0x73, 0x63, 0x53, 0x43, 0x33, 0x23, 0x26, 0x36, 0x46, 0x56};  // Numbers 0-10 
  const byte ouija[NOUIJA]            = {0x12, 0x13, 0x14, 0x15};
  const uint8_t colPins[NCOLS]        = {13, 12, 11, 15, 16, 14};
#endif

#ifdef SAOUIJA_FRONTLIT
  const byte charlist[NLETS + NNUMS]  = {0x76, 0x66, 0x56, 0x46, 0x36, 0x26, 0x23, 0x33, 0x43, 0x53, 0x73, 0x72, 0x71, 0x75, 0x65, 0x55, 0x45, 0x35, 0x25, 0x22, 0x32, 0x42, 0x52, 0x63, 0x62, 0x61, // Letters A-Z
                                        0x74, 0x64, 0x54, 0x44, 0x34, 0x24, 0x21, 0x31, 0x41, 0x51}; // Numbers 0-10
  const byte ouija[NOUIJA]            = {0x15, 0x14, 0x13, 0x12};
  const uint8_t rowPins[NROWS] = {A5, A6, A7, 4, 5, A4, 10};
  const uint8_t colPins[NCOLS] = {11, 12, 13, A1, A2, A3};
#endif

const byte* numbers = charlist + NLETS;
const byte* letters = charlist;

volatile uint8_t       leds[NROWS][NCOLS];
volatile uint8_t       curRow = 0;
uint8_t                cycleCounter;
byte                   rowStates=0;


// Set up buttons
#define BTN1_PIN    7
#define BTN2_PIN    6

OneButtonTiny  buttonMode(BTN1_PIN, INPUT_PULLUP, true);
OneButtonTiny  buttonSel(BTN2_PIN, INPUT_PULLUP, true);

#ifdef SAOUIJA_BACKLIT
  // Set up shift register
  #define SHIFT_DATA      0
  #define SHIFT_LATCH     2
  #define SHIFT_CLOCK     3
  #define SHIFT_ENABLE    1

  // Shift Pins
  const uint8_t shiftRowsOut[NROWS] = {0,1,2,3,4,5,6};
  const uint8_t shiftModeLED = 7;
#endif

char        displayStr[MAX_STRING_SIZE];
uint8_t     displayStrPtr = 0;

char        entryStr[MAX_STRING_SIZE];
uint8_t     entryIndex = 0;       // Pointer to the location of the next character to add to the entry string

uint8_t     charListIndex = 0;    // Index of currently displayed character in entry mode
bool        acceptEntry = true;

void        (*displayAnim)();
void        (*entryAnim)();


void storeEEPROM(char *data) {

  int   addr     = 0;
  byte  len      = (byte) strlen(data);

  EEPROM.write(addr, len);
  addr ++;
  
  if (len) {
    for (int i = 0; i < min(len, MAX_STRING_SIZE - 2); i++) {
      EEPROM.write(addr, data[i]);
      addr++;
    }
    EEPROM.write(addr, '\0');
  }
}

// Copies EEPROM data (if any exists) to the string passed in
// Returns the length of the written string
int readEEPROM(char* dataStr) {
  byte  len = EEPROM.read(0);
  int   addr = 1;
  for (int i = 0; i < min(len, MAX_STRING_SIZE - 2); i++) {
    dataStr[i] = EEPROM.read(addr);
    addr++;
  }
  dataStr[len] = '\0';
  return len;
}

void clearLetters() {
  for(int i = 0; i < NLETS; i++) {
    ledOffHex(letters[i]);
  }
}

void clearNumbers() {
  for (int i = 0; i < NNUMS; i++) {
    ledOffHex(numbers[i]);
  }
}

void clearLEDs() {
  for (int i = 0; i < NROWS; i++) {
    for (int j = 0; j < NCOLS; j++) {
      leds[i][j] = 0;
    }
  }  
}

#ifdef SAOUIJA_BACKLIT
  void updateShiftRegister(byte state) {
    digitalWrite(SHIFT_LATCH, LOW);
    shiftOut(SHIFT_DATA, SHIFT_CLOCK, LSBFIRST, state);
    digitalWrite(SHIFT_LATCH, HIGH);
  }
#endif


// Draw to LEDs one column at a time.
void writeLEDs() {
  // Turn off previous row
#ifdef SAOUIJA_FRONTLIT
  digitalWrite(rowPins[curRow], HIGH);
#endif
#ifdef SAOUIJA_BACKLIT
  bitClear(rowStates, shiftRowsOut[curRow]);
  updateShiftRegister(rowStates);
#endif
  for (int i = 0; i < NCOLS; i++) {
    digitalWrite(colPins[i], COL_OFF);
  }

  // Turn on next Row of LEDs according to duty cycle
  curRow = (curRow + 1) % NROWS;
#ifdef SAOUIJA_FRONTLIT
  digitalWrite(rowPins[curRow], LOW);
#endif
#ifdef SAOUIJA_BACKLIT
  bitSet(rowStates, shiftRowsOut[curRow]);
  updateShiftRegister(rowStates);
#endif
  for (int i = 0; i < NCOLS; i++) {
    if (leds[curRow][i] && (cycleCounter <= leds[curRow][i])) {  // Implement duty cycle - only works on frontlit
      digitalWrite(colPins[i], COL_ON);
    }
  }
  cycleCounter = (cycleCounter + 1) % 255;
}

void setLED(uint8_t row, uint8_t col, uint8_t duty=255) {
  leds[row][col] = duty;
}
uint8_t getLED(uint8_t row, uint8_t col) {
  return leds[row][col];
}
void toggleLED(uint8_t row, uint8_t col) {
  if (leds[row][col]) {
    leds[row][col] = 0;
  } else {
    leds[row][col] = 255;
  }
}
void toggleHex(byte item) {
  if (item) {
  toggleLED(HEX_ROW(item), HEX_COL(item));
  }
}
void ledOff(uint8_t row, uint8_t col) {
  setLED(row, col, 0);
}
void setHex(byte item, uint8_t duty=255) {
  if (item) {
    setLED(HEX_ROW(item), HEX_COL(item), duty);
  }
}

void ledOffHex(byte item) {
  if (item) {
    setLED(HEX_ROW(item), HEX_COL(item), 0);
  }
}

char charFromListIndex(uint8_t index) {
  if (index < NLETS) {
    return charListIndex + 'A';
  } else if (index < NLETS + NNUMS) {
    return (charListIndex - NLETS) + '0';
  } else {
    return 0;
  }
}


byte charToLED(char c) {
  if (isAlpha(c)) {
    if (isLowerCase(c)) {
      c = toupper(c);
    }
    return letters[c - 'A'];
  } else if (isDigit(c)) {
    return numbers[c - '0'];
  } else {
    return 0;
  }
}


// select the correct option between yes/no
void showAcceptEntry() {
  if (acceptEntry) {
    ledOffHex(NO);
    setHex(YES, 255);
  } else {
    ledOffHex(YES);
    setHex(NO, 255);
  }
}

void toggleAcceptEntry() {
  if (FUNCTION_MODE == MODE_STORE_ENTRY) {
    acceptEntry = !acceptEntry;
    showAcceptEntry();
  }
}

void showCurChar() {
    setHex(charlist[charListIndex], 255);
}

void advanceCurChar() {
  if (FUNCTION_MODE == MODE_ENTER) {
    ledOffHex(charlist[charListIndex]);
    charListIndex = (charListIndex + 1) % (NNUMS + NLETS);
    showCurChar();
  }
}


// If current character is space (not on keyboard, but selected with mode button while in entry mode)
bool spaceChar   =   false;   

// Handler for short click on select button
void clickSel() {
  switch(FUNCTION_MODE) {
    case MODE_ENTER:
      if (spaceChar) {
        spaceChar = false;
        showCurChar();
      } else {
        advanceCurChar();
      }
      break;
    case MODE_STORE_ENTRY:
      toggleAcceptEntry();
      break;
  }
}


// Handler for short click on Mode button
void clickMode() {
  // If mode is clicked once while in entry mode, the character is cleared and the current character becomes a space character
  // but only for half a second.
  if (FUNCTION_MODE == MODE_ENTER) {
    spaceChar = true;
    clearNumbers();
    clearLetters();    
  }
}

void clearEntryString() {
  entryStr[0]   = '\0';
  entryIndex    =  0;
}

// Handler for long click on mode button
void longPressMode() {
  switch(FUNCTION_MODE){
    case MODE_DISPLAY:
      FUNCTION_MODE = MODE_ENTER;
      clearLEDs();
      charListIndex = 0;
      showCurChar();  
      break;
    case MODE_ENTER:
      FUNCTION_MODE = MODE_STORE_ENTRY;
      clearLEDs();
      entryStr[entryIndex++] = ' ';
      entryStr[entryIndex] = '\0';
      acceptEntry = true;
      showAcceptEntry();
      break;
    case MODE_STORE_ENTRY:
      FUNCTION_MODE = MODE_DISPLAY;
      clearEntryString();
      clearLEDs();
      break;
  }
}

// Handler for long click on select button
void longPressSel() {
  switch(FUNCTION_MODE) {
    case MODE_ENTER:
      if (entryIndex < MAX_STRING_SIZE) {   // Add the latest char to the entry string
        if (spaceChar) {
          entryStr[entryIndex] = ' ';
          spaceChar = false;
          showCurChar();
        } else {
          entryStr[entryIndex] = charFromListIndex(charListIndex);
        }
        entryAnim = staticOuija;
        entryIndex++;
      }
      break;
    case MODE_STORE_ENTRY:    // If YES is displayed , then store the current entry string to EEPROM (unless string is empty)
      if (acceptEntry && entryIndex > 0) {
        strcpy(displayStr, entryStr);
        storeEEPROM(entryStr);
      }
      clearEntryString();
      clearLEDs();
      FUNCTION_MODE = MODE_DISPLAY;
      break;
  }
}

// Initialize timer for writing to LEDs
void TCA0_init(void)
{
    TCA0.SPLIT.CTRLA = TCA_SPLIT_ENABLE_bm        // Enable TCA to run in split mode
    |    TCA_SPLIT_CLKSEL_DIV4_gc;                    // set clock source
    
    TCA0.SPLIT.CTRLD = TCA_SPLIT_SPLITM_bm;            // enable split mode
    
    TCA0.SPLIT.LPER = 0xA0;                            // 16MHz / 640 / 4 = 25 kHz
    TCA0.SPLIT.HPER = 0x28;                            // 16MHz / 160 / 4 = 100 kHz
    
    TCA0.SPLIT.INTCTRL = TCA_SPLIT_LUNF_bm;            // Low Underflow Interrupt Enabled
}

// Interrupt timer writes to LEDs
ISR(TCA0_LUNF_vect)
{
    writeLEDs();
    // do something
    TCA0_SPLIT_INTFLAGS = TCA_SPLIT_LUNF_bm;        // Clear interrupt flag by writing '1' (required)
}


// animatest SAOuija display  - shows we're in enter mode
void animOuija() {
  unsigned long display_ms          = 100;
  static int curLED                 = 0;
  static unsigned long lastCallTime = 0;
  static int dir                     = 1;

  if (millis() - lastCallTime > display_ms) {
    setHex(ouija[curLED], 0);
    curLED = (curLED + dir);
    if ((curLED <= 0) || curLED >= NOUIJA - 1) dir *= -1; 
    setHex(ouija[curLED], 255);
    lastCallTime = millis();
  }
}

// lights up the full OUIJA word for display_ms, then returns control to animOuija
void staticOuija() {
  unsigned long display_ms          = 300;
  static bool ouijaOn               = false;
  static unsigned long lastCallTime = 0;

  // Turn on LEDs when firstcalled
  if (!ouijaOn) {
    for (int i = 0; i < NOUIJA; i++) {
      setHex(ouija[i], 255);
    }
    lastCallTime = millis();
    ouijaOn = true;
  } else if (millis() - lastCallTime > display_ms) {  // Time up, turn off all ouija LEDs
    for (int i = 0; i < NOUIJA; i++) {
      setHex(ouija[i], 0);
    }
    ouijaOn = false;
    lastCallTime = 0;
    entryAnim = animOuija;  // Return control to other display function
  }
}

// Interval is time for each letter to be shown
void showDisplayStr() {
  unsigned long         display_ms = 600;
  unsigned long         pause_ms = 100;
  static unsigned long  lastCallTime = 0;
  static bool           isPaused = true;

  unsigned long tdiff = millis() - lastCallTime;

  if (isPaused) {
    if (tdiff < pause_ms) {
      return;
    } else {
      // Pause over, show next character
      isPaused = false;
      byte led = charToLED(displayStr[displayStrPtr]);
      if (led) {
      setHex(led, 255);
      } 
    }
  } else {
    if (tdiff < display_ms) {
      return;
    } else {
      // Turn off current character, start pause interval
      byte led = charToLED(displayStr[displayStrPtr]);
      if (led) {
        ledOffHex(led);
      }
      displayStrPtr = (displayStrPtr + 1) % strlen(displayStr);
      isPaused = true;
    }

  }
  lastCallTime = millis();
}

void setup() {

  // Setup timer
  TCA0_init();

  for (int i = 0; i < NCOLS; i++) {
    pinMode(colPins[i], OUTPUT);
    digitalWrite(colPins[i], COL_OFF);
  }

#ifdef SAOUIJA_FRONTLIT
    for (int i = 0; i < NROWS; i++) {
    pinMode(rowPins[i], OUTPUT);
  }
#endif


  // Initialize LEDs off
  #ifdef SAOUIJA_BACKLIT
    bitClear(rowStates, shiftModeLED);
  #endif
  curRow = 0;
  cycleCounter = 0;

#ifdef SAOUIJA_BACKLIT
  // Initialize shift register pins
  pinMode(SHIFT_LATCH, OUTPUT);
  pinMode(SHIFT_CLOCK, OUTPUT);
  pinMode(SHIFT_DATA, OUTPUT);
  pinMode(SHIFT_ENABLE, OUTPUT);
  
  // Enable shift register output
  digitalWrite(SHIFT_ENABLE, LOW);
#endif

  // Button handlers. Since we're not using double clicks,
  // speed up timings from default to make buttons more responsive
  buttonSel.setDebounceMs(25);
  buttonSel.setClickMs(100);
  buttonSel.setPressMs(800);
  buttonMode.setDebounceMs(25);
  buttonMode.setClickMs(100);
  buttonMode.setPressMs(800);

  buttonSel.attachClick(clickSel);
  buttonMode.attachClick(clickMode);
  buttonSel.attachLongPressStart(longPressSel);
  buttonMode.attachLongPressStart(longPressMode);

  // Initialize with some data
  #ifdef INITIALIZE_EEPROM
    storeEEPROM("HELLO WORLD ");
  #endif

  // Retrieve any stored string from EEPROM
  int len = readEEPROM(displayStr);
  if (!len) {
    strcpy(displayStr, "Hello World ");
  }
  delay(100);

  // Initialize animation functions
  displayAnim = showDisplayStr;
  entryAnim   = animOuija;

}

/*------------ end setup-----------------*/


void loop() {

  buttonSel.tick();
  buttonMode.tick();

  switch (FUNCTION_MODE) {
    case MODE_DISPLAY:
      (*displayAnim)();
      break;
    case MODE_ENTER:
      (*entryAnim)();
      break;
  }  

  delayMicroseconds(2);
  
}


