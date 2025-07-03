// ==================================================
// DarkroomDualTimer.ino
// Combined Stopwatch + Countdown Timer for Darkroom
// Features: mode switching, arpeggio tones, TM1637 display
// ==================================================

#include <ezButton.h>
#include <TM1637Display.h>
#include <EEPROM.h>
#include <avr/sleep.h>

// === CONFIGURATION ===
const int EEPROM_ADDR_MODE = 0;       // stores currentMode
const int EEPROM_ADDR_TICK = 1;       // stores tickEnabled
const int EEPROM_ADDR_INIT     = 100;  // a safe location for our marker
const byte EEPROM_MAGIC_VALUE = 0x42; // arbitrary non-zero value

const int NUM_BUTTONS = 3;
const unsigned long LONG_PRESS_THRESHOLD_MILLIS = 500UL;
const unsigned long CHORD_TIME_WINDOW_MILLIS = 100UL;

const unsigned long TICK_INTERVAL_MILLIS = 1000UL; // Timer tick every second

const unsigned long STOPWATCH_STOPPED_RESET_MILLIS = 30UL * 1000UL; // Time after stop when timer resets automatically
const unsigned long IDLE_SLEEP_TIMEOUT_MILLIS = 10UL * 60UL * 1000UL; // 10 minutes

const unsigned long TIMER_ALARM_PULSE_MILLIS = 50UL;  // Duration of each beep in the alarm (ms)
const unsigned long TIMER_ALARM_GAP_MILLIS = 50UL;    // Pause between beeps in alarm
const uint8_t TIMER_ALARM_PULSE_COUNT = 3;    // Number of beeps in each alarm burst
const unsigned int TIMER_ALARM_TONE_HZ = 1760;        // Frequency of the alarm tone in Hz
const uint16_t TIMER_ALARM_MAX_SECONDS = 5;   // Alarm duration (after this, screen flashes silently)
const bool TIMER_ALARM_DEBUG = false;     // If true, lets you test alarm manually at 00:00
const unsigned long VLP_THRESHOLD_MILLIS = 1200UL;  // 1.5 seconds
const unsigned long FAST_INCREMENT_INTERVAL_MILLIS = 200UL;  // Increment 4 times per second
const uint8_t FAST_INCREMENT_MINUTES = 5; // 5 minutes per interval

// === PINS ===
// Using Green and Black on 2 and 3 to allow them to interrupt sleep
const int buttonPins[NUM_BUTTONS] = {2, 4, 3};  // Green, Red, Black
const int CLK = 6;
const int DIO = 7;
const int LED_BRIGHTNESS = 0;
const int BUZZER_PIN = 5;
enum ButtonID { GREEN = 0, RED = 1, BLACK = 2 };
const char* buttonNames[NUM_BUTTONS] = { "Green", "Red", "Black" };

TM1637Display display(CLK, DIO);

enum TimerMode {
  MODE_STOPWATCH,
  MODE_TIMER
};

TimerMode currentMode = MODE_STOPWATCH;


// === Device Variables ===
bool tickEnabled = true;                // Clock ticks
volatile bool wakeFromSleepTriggered0 = false;
volatile bool wakeFromSleepTriggered1 = false;
unsigned long lastInteractionMillis = 0;

// === Stopwatch Variables ===
unsigned long stopwatchStartMillis = 0;          // Start time of current session
unsigned long stopwatchLastDeltaSeconds = 0;     // Last second tracked for updates
unsigned long stopwatchStopMillis = 0;           // Stop time of current session
uint16_t stopwatchDisplaySeconds = 0;                 // Time currently displayed (in seconds)
bool stopwatchRunning = false;

// === Timer Variables ==
unsigned long timerMillis = 0;  // Current countdown time in milliseconds
unsigned long timerLastTick = 0;         // When the last second tick occurred
uint8_t timerSetMinutes = 0;        // Time to count down from (set by user)
uint8_t timerSetSeconds = 0;
bool timerColonOn = false;
bool timerLastColonOn = false;
bool blackVeryLongPressActive = false;
unsigned long lastFastAddTime = 0;

// Alarm State
bool timerAlarmActive = false;   // Is the alarm currently sounding?
bool timerAlarmToneOn = false;   // Is the beep tone currently playing?
unsigned long timerAlarmTimer = 0; // Time alarm started

// Timer State
enum CountdownState { SETUP, RUNNING, PAUSED, ALARM, OFF }; // Different modes the timer can be in
CountdownState countdownState = SETUP;         // Start in SETUP mode

// === STRUCTS ===
struct ButtonState {
  ezButton button;
  unsigned long pressedAt = 0;
  unsigned long releasedAt = 0;
  bool isPressed = false;
  bool wasHandled = false;
  ButtonState(int pin) : button(pin) {}
};

struct PressEvents {
  bool redShortPress = false;
  bool redLongPress = false;
  bool greenShortPress = false;
  bool greenLongPress = false;
  bool blackShortPress = false;
  bool blackLongPress = false;

  bool redBlackShortPress = false;
  bool redBlackLongPress = false;
  bool greenBlackShortPress = false;
  bool greenBlackLongPress = false;
  bool redGreenShortPress = false;
  bool redGreenLongPress = false;

  bool greenRedBlackShortPress = false;
  bool greenRedBlackLongPress = false;

  void reset() { *this = PressEvents(); }
};

// === GLOBAL STATE ===
ButtonState* buttons[NUM_BUTTONS] = {
  new ButtonState(buttonPins[GREEN]),
  new ButtonState(buttonPins[RED]),
  new ButtonState(buttonPins[BLACK])
};
PressEvents pressEvents;

// === PROTOTYPES ===
void doResetStopwatch();
void doResetTimer();
void doReset();

// === INITIALIZATION ===
void loadSettings() {
  byte marker = EEPROM.read(EEPROM_ADDR_INIT);

  if (marker != EEPROM_MAGIC_VALUE) {
    // First time setup
    currentMode = MODE_STOPWATCH;
    tickEnabled = true;

    EEPROM.update(EEPROM_ADDR_MODE, currentMode);
    EEPROM.update(EEPROM_ADDR_TICK, tickEnabled ? 1 : 0);
    EEPROM.update(EEPROM_ADDR_INIT, EEPROM_MAGIC_VALUE);
  } else {
    // Load previously saved values
    byte modeByte = EEPROM.read(EEPROM_ADDR_MODE);
    currentMode = (modeByte == MODE_TIMER) ? MODE_TIMER : MODE_STOPWATCH;

    byte tickByte = EEPROM.read(EEPROM_ADDR_TICK);
    tickEnabled = (tickByte != 0);
  }
}

void wakeFromSleepInterrupt0() {
  wakeFromSleepTriggered0 = true;              // Just set the flag
}
void wakeFromSleepInterrupt1() {
  wakeFromSleepTriggered1 = true;              // Just set the flag
}
void enterSleepMode() {
  // ✅ Show "OFF" before sleeping
  uint8_t offMessage[4] = {
    0x3F,  // O
    0x71,  // F
    0x71,  // F
    0x00   // (blank)
  };
  display.setSegments(offMessage);
  doubleTick();

  // Prepare for sleep
  display.clear();       // Blank display to reduce draw
  noTone(BUZZER_PIN);    // Ensure buzzer is silent

  // Set sleep mode
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();

  // Ensure no interrupts before attaching
  noInterrupts();
  attachInterrupt(digitalPinToInterrupt(2), wakeFromSleepInterrupt0, LOW);
  attachInterrupt(digitalPinToInterrupt(3), wakeFromSleepInterrupt1, LOW);
  interrupts();

  sleep_cpu();  // 💡 The Nano actually sleeps here

  // 💤 ... Wakes up here ...
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(2));
  detachInterrupt(digitalPinToInterrupt(3));

  doReset();
}
void setup() {
  loadSettings();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i]->button.setDebounceTime(50);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }
  display.setBrightness(LED_BRIGHTNESS);
  doReset();
}

void loop() {
  pressEvents.reset();

  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i]->button.loop();

    if (buttons[i]->button.isPressed()) {
      buttons[i]->pressedAt = millis();
      buttons[i]->isPressed = true;
      buttons[i]->wasHandled = false;
    }

    if (buttons[i]->button.isReleased()) {
      buttons[i]->releasedAt = millis();
      buttons[i]->isPressed = false;
    }
  }

  resolveEventsIfReady();

  // This has to happen after the "wake from sleep" logic so that
  // the lastInteractionMillis has been updated by the reset post-wake
  // to avoid some crazy can't wake up scenario.
  bool shouldSleepForInactivity = (millis() - lastInteractionMillis > IDLE_SLEEP_TIMEOUT_MILLIS
);

  // Handle global buttons

  if(pressEvents.redGreenShortPress) {
    toggleMode();
    return;
  }

  if(pressEvents.redLongPress || shouldSleepForInactivity){
    enterSleepMode();
    return;
  }

  if(tickEnabled){
    if(pressEvents.redBlackShortPress){
      toggleTick();
      return;
    }
  }else{
    if(pressEvents.greenBlackShortPress){
      toggleTick();
      return;
    }
  }

  switch(currentMode){
    case MODE_STOPWATCH:
      stopwatchLoop();
      break;
    case MODE_TIMER:
      timerLoop();
      break;
    default:
      break;
  }
  delay(1);
}

// Global means of determine if something is running
boolean isRunning(){
  switch(currentMode){
    case MODE_STOPWATCH:
      return stopwatchRunning;
      break;
    case MODE_TIMER:
      return countdownState == RUNNING;
      break;
    default:
      return false;
      break;
  }
}

void doReset(){
  // No matter what, if we are resetting, we don't want the thing
  // to think it needs to shut off because it has timed out.
  lastInteractionMillis = millis();
  display.setBrightness(LED_BRIGHTNESS);
  // display.clear();
  noTone(BUZZER_PIN);
  
  switch(currentMode){
    case MODE_STOPWATCH:
      doResetStopwatch();
      break;
    case MODE_TIMER:
      doResetTimer();
      break;
    default:
      break;
  }
}

void toggleMode(){
  if (currentMode == MODE_TIMER) {
    currentMode = MODE_STOPWATCH;
    playStopwatchModeTone();
    doResetStopwatch();
  }else{
    currentMode = MODE_TIMER;
    playCountdownModeTone();
    doResetTimer();
  }
  EEPROM.update(EEPROM_ADDR_MODE, currentMode);
}

void toggleTick(){
  if(tickEnabled){
    playDescendingArpeggio();
    tickEnabled = false;
  }else{
    playAscendingArpeggio();
    tickEnabled = true;
  }
  EEPROM.update(EEPROM_ADDR_TICK, tickEnabled ? 1 : 0);
}

// ============================================
// Stopwatch Mode
// ============================================
void stopwatchLoop(){
  if(pressEvents.blackShortPress){
    bool wasRunning = stopwatchRunning;
    doResetStopwatch();
    if(wasRunning){
      stopwatchStartMillis = millis();
      stopwatchRunning = true;
    }
  }
  if(stopwatchRunning){
    if(pressEvents.redShortPress){
      stopwatchRunning = false;
      stopwatchStopMillis = millis();
    }
  }else{
    if(pressEvents.greenShortPress){
      stopwatchStartMillis = millis();
      stopwatchLastDeltaSeconds = 0;
      stopwatchRunning = true;
    }
  }
  unsigned long currentMillis = millis();
  unsigned long deltaMillis = currentMillis - stopwatchStartMillis;
  unsigned long deltaSeconds = deltaMillis / 1000;

  boolean colonOn = stopwatchRunning ? (deltaMillis % 1000 < 500) : true;  // Blink colon if running

  if(stopwatchRunning){
    // We're doing something. Don't sleep!  
    lastInteractionMillis = millis();
  }

  if (stopwatchRunning && deltaSeconds > stopwatchLastDeltaSeconds) {
    stopwatchLastDeltaSeconds++;
    stopwatchDisplaySeconds++;
    tick();  // Tick sound
  }

  // If stopped, wait 10 seconds, then automatically clear to zero
  if(!stopwatchRunning && stopwatchStopMillis > 0 && millis() - stopwatchStopMillis > STOPWATCH_STOPPED_RESET_MILLIS){
    doubleTick();
    doReset();
  }

  // Display time as MM:SS
  uint8_t minutes = stopwatchDisplaySeconds / 60;
  uint8_t seconds = stopwatchDisplaySeconds % 60;
  uint16_t timeVal = minutes * 100 + seconds;
  display.showNumberDecEx(timeVal, colonOn ? 0b01000000 : 0, true);
}

void doResetStopwatch(){
  stopwatchRunning = false;
  stopwatchStartMillis = 0;
  stopwatchDisplaySeconds = 0;
  stopwatchLastDeltaSeconds = 0;
  stopwatchStopMillis = 0;
  display.showNumberDecEx(0, 0b01000000, true); // Reset to 00:00
}

// ============================================
// Timer Mode
// ============================================
void timerLoop(){
  handleTimerButtons();

  // Special case for very long press of black
  // There really has to be a better place for this!
  if (countdownState == SETUP) {
    bool blackHeld = !digitalRead(buttonPins[BLACK]);  // active low

    if (blackHeld) {
      unsigned long heldTime = millis() - buttons[BLACK]->pressedAt;

      // Sanity cap: if the device just woke from sleep, skip obviously stale times
      if (buttons[BLACK]->pressedAt > 0 && heldTime > VLP_THRESHOLD_MILLIS){
        blackVeryLongPressActive = true;

        if (millis() - lastFastAddTime > FAST_INCREMENT_INTERVAL_MILLIS) {
          // Go backwards to trim off any unsightly minutes not on a multiple of FAST_INCREMENT_MINUTES
          timerSetMinutes = timerSetMinutes - timerSetMinutes%FAST_INCREMENT_MINUTES;
          // Now add FAST_INCREMENT_MINUTES minutes.
          timerSetMinutes += FAST_INCREMENT_MINUTES;
          timerSetMinutes = min(timerSetMinutes, 99);
          quietTick();
          updateTimerDisplay();
          lastFastAddTime = millis();
        }
      }
      return;
    } else {
      blackVeryLongPressActive = false;
    }
  }

  unsigned long now = millis();

  
// === Blink the colon on and off once per second in RUNNING mode ===
  if (countdownState == RUNNING) {
    // We're doing something. Don't sleep!  
    lastInteractionMillis = millis();

    unsigned long phase = now - timerLastTick;
    timerColonOn = (phase < 500);
    if (timerColonOn != timerLastColonOn) {
      timerLastColonOn = timerColonOn;
      updateTimerDisplay();
    }
  } else {
    // Colon stays ON in other modes
    if (!timerColonOn) {
      timerColonOn = true;
      timerLastColonOn = true;
      updateTimerDisplay();
    }
  }

  // === Countdown logic: subtract 1 second every tick ===
  if (countdownState == RUNNING && now - timerLastTick >= TICK_INTERVAL_MILLIS) {
    timerLastTick = now;
    tick(); // short tick sound

    if (timerMillis >= TICK_INTERVAL_MILLIS) {
      timerMillis -= TICK_INTERVAL_MILLIS;
      updateTimerDisplay();
    } else {
      countdownState = ALARM;
      startTimerAlarm();
    }
  }

  // === ALARM: sound 3 short beeps per second ===
  if (countdownState == ALARM && timerAlarmActive) {
    unsigned long alarmMillis = (millis() - timerAlarmTimer);
    unsigned long windowMillis = alarmMillis % 1000;

    unsigned long soundDuration = TIMER_ALARM_PULSE_COUNT * TIMER_ALARM_PULSE_MILLIS +
                                  (TIMER_ALARM_PULSE_COUNT - 1) * TIMER_ALARM_GAP_MILLIS;

    bool found = false;
    if (alarmMillis / 1000 < TIMER_ALARM_MAX_SECONDS) {
      if (windowMillis < soundDuration) {
        for (int i = 0; i < TIMER_ALARM_PULSE_COUNT; i++) {
          int leadingEdge = i * (TIMER_ALARM_PULSE_MILLIS + TIMER_ALARM_GAP_MILLIS);
          int trailingEdge = TIMER_ALARM_PULSE_MILLIS + i * (TIMER_ALARM_PULSE_MILLIS + TIMER_ALARM_GAP_MILLIS);
          if (leadingEdge < windowMillis && windowMillis < trailingEdge) {
            found = true;
            break;
          }
        }
        if (found && !timerAlarmToneOn) {
          tone(BUZZER_PIN, TIMER_ALARM_TONE_HZ, TIMER_ALARM_PULSE_MILLIS);
          timerAlarmToneOn = true;
        }
      }
    }

    if (!found) {
      noTone(BUZZER_PIN);
      timerAlarmToneOn = false;
    }

    updateTimerDisplay();  // blink effect during alarm
  }

  
// === Turn off display in OFF mode ===
  if (countdownState == OFF) {
    display.clear();
  }
}

void doResetTimer(){
  // Clear VLP state
  blackVeryLongPressActive = false;
  lastFastAddTime = 0;
  resetAllButtons();
  // Reset other stuff
  timerSetMinutes = 0;
  timerSetSeconds = 0;
  timerMillis = 0;
  timerAlarmActive = false;
  countdownState = SETUP;
  updateTimerDisplay();
}


// === CHECK BUTTONS ===
// Handles logic for button presses, including short and long presses
void handleTimerButtons() {

  // === GREEN button: Start, Resume, or Trigger Alarm in DEBUG mode ===
  if (pressEvents.greenShortPress) {
    if (countdownState == SETUP && (timerSetMinutes > 0 || timerSetSeconds > 0)) {
      timerMillis = (timerSetMinutes * 60UL + timerSetSeconds) * 1000UL;
      timerLastTick = millis();
      countdownState = RUNNING;
      timerSetMinutes = 0;
      timerSetSeconds = 0;
    } else if (countdownState == SETUP && timerSetMinutes == 0 && timerSetSeconds == 0 && TIMER_ALARM_DEBUG) {
      countdownState = ALARM;
      startTimerAlarm();
    } else if (countdownState == PAUSED) {
      countdownState = RUNNING;
      timerLastTick = millis();
    }
  }

  if (pressEvents.blackShortPress) {
    if (countdownState == OFF) {
      countdownState = SETUP;
      timerSetMinutes = 1;
      timerSetSeconds = 0;
      updateTimerDisplay();
    } else if (countdownState == SETUP) {
      timerSetMinutes = (timerSetMinutes + 1) % 100;
      timerSetSeconds = 0;
      updateTimerDisplay();
    } else if (countdownState == PAUSED) {
      timerMillis = 0;
      countdownState = SETUP;
      updateTimerDisplay();
    }
  }

  if (pressEvents.blackLongPress) {
    if (countdownState == SETUP) {
      timerSetMinutes = 0;
      timerSetSeconds = 0;
      updateTimerDisplay();
    } else if (countdownState == PAUSED) {
      timerMillis = 0;
      countdownState = SETUP;
      updateTimerDisplay();
    }    
  }

  // === RED button: Short = stop/pause, Long = power off ===
  if (pressEvents.redShortPress) {
    switch (countdownState) {
      case RUNNING:
        countdownState = PAUSED;
        break;
      case ALARM:
        stopTimerAlarm();
        countdownState = SETUP;
        timerMillis = 0;
        updateTimerDisplay();
        break;
      default:
        break;
    }
  }
  if(pressEvents.redLongPress){
    stopTimerAlarm();
    countdownState = OFF;
    playQuietDoubleClick();
  }
}


// === Start alarm sequence ===
void startTimerAlarm() {
  timerAlarmActive = true;
  timerAlarmTimer = millis();
  timerAlarmToneOn = false;
}


// === Stop alarm sequence ===
void stopTimerAlarm() {
  timerAlarmActive = false;
  timerAlarmTimer = 0;
  noTone(BUZZER_PIN);
  timerAlarmToneOn = false;
}

// === Update display based on current timer or alarm ===
void updateTimerDisplay() {
  if (countdownState == OFF) return;

  // Flash the display during alarm
  if (timerAlarmActive) {
    if ((timerAlarmTimer - millis()) % 500 > 250) {
      display.clear();
      return;
    }
  }

  uint8_t minutes = 0;
  uint8_t seconds = 0;

  if (countdownState == SETUP) {
    minutes = timerSetMinutes;
    seconds = timerSetSeconds;
  } else {
    unsigned long timeLeft = timerMillis / 1000;
    minutes = timeLeft / 60;
    seconds = timeLeft % 60;
  }

  uint8_t colon = timerColonOn ? 0b01000000 : 0;

  if (minutes < 10) {
    // Show M:SS (3 digits, no leading zero)
    uint8_t digits[4] = {0, 0, 0, 0};

    // Map digits manually
    digits[1] = display.encodeDigit(minutes);
    digits[2] = display.encodeDigit(seconds / 10);
    digits[3] = display.encodeDigit(seconds % 10);

    if (colon) {
      digits[1] |= 0b10000000;  // add colon between digit 1 and 2
    }

    display.setSegments(digits);
  } else {
    // Show MM:SS normally (4 digits, leading digit visible)
    display.showNumberDecEx(minutes * 100 + seconds, colon, true);
  }
}
//
// === BUTTON EVENT HANDLER
//
void resetAllButtons() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    buttons[i]->pressedAt = 0;
    buttons[i]->releasedAt = 0;
    buttons[i]->wasHandled = false;
  }
}
bool longPressTickPlayed = false;
void resolveEventsIfReady() {
  bool ready = true;
  bool anyPressed = false;

  // Give an audible tick at long-press threshold
  if(!isRunning() && !longPressTickPlayed){
    for (int i = 0; i < NUM_BUTTONS; i++) {
      if (!buttons[i]->wasHandled && buttons[i]->pressedAt > 0) {
        if (buttons[i]->isPressed) {
          if(millis() - buttons[i]->pressedAt > LONG_PRESS_THRESHOLD_MILLIS){
            // Currently we only want the "long press click" to happen for black
            if(i == BLACK){
              quietTick();
            }
            longPressTickPlayed = true;
          }
        }
      }
    }
  }

  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!buttons[i]->wasHandled && buttons[i]->pressedAt > 0) {
      if (buttons[i]->isPressed) {
        ready = false;  // someone still down
      } else {
        anyPressed = true;
      }
    }
  }

  if (!ready || !anyPressed) return;

  longPressTickPlayed = false;

  int involved[NUM_BUTTONS];
  uint8_t count = 0;
  for (int i = 0; i < NUM_BUTTONS; i++) {
    if (!buttons[i]->wasHandled && buttons[i]->pressedAt > 0 && !buttons[i]->isPressed) {
      involved[count++] = i;
    }
  }

  if (count == 0) return;

  // Bubble sort (no STL)
  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (involved[j] < involved[i]) {
        int temp = involved[i];
        involved[i] = involved[j];
        involved[j] = temp;
      }
    }
  }

  unsigned long first = buttons[involved[0]]->pressedAt;
  unsigned long last = first;
  unsigned long longest = 0;

  for (int i = 0; i < count; i++) {
    unsigned long t = buttons[involved[i]]->pressedAt;
    if (t < first) first = t;
    if (t > last) last = t;

    unsigned long duration = buttons[involved[i]]->releasedAt - buttons[involved[i]]->pressedAt;
    if (duration > longest) longest = duration;
  }

  bool isLong = longest >= LONG_PRESS_THRESHOLD_MILLIS;
  bool isVeryLong = longest >= VLP_THRESHOLD_MILLIS;
  bool isChord = (last - first) <= CHORD_TIME_WINDOW_MILLIS && count > 1;

  if (isChord) {
    recordChordEvent(involved, count, isLong);
  } else {
    for (int i = 0; i < count; i++) {
      // We only support VLP for black, while in SETUP mode.
      if(involved[i] == BLACK && countdownState == SETUP && isVeryLong){
        continue; // Don't record "long" press...it's a "very long" press
      }
      recordSoloEvent(involved[i], isLong);
    }
  }

  lastInteractionMillis = millis();  // ✅ Log time of valid button event

  // Mark handled
  for (int i = 0; i < count; i++) {
    buttons[involved[i]]->wasHandled = true;
    buttons[involved[i]]->pressedAt = 0;
    buttons[involved[i]]->releasedAt = 0;
  }
}

void recordSoloEvent(int id, bool isLong) {
  switch (id) {
    case GREEN: pressEvents.greenShortPress = !isLong; pressEvents.greenLongPress = isLong; break;
    case RED:   pressEvents.redShortPress   = !isLong; pressEvents.redLongPress   = isLong; break;
    case BLACK: pressEvents.blackShortPress = !isLong; pressEvents.blackLongPress = isLong; break;
  }
}

void recordChordEvent(int* ids, int count, bool isLong) {
  if (count == 2) {
    if (ids[0] == RED && ids[1] == BLACK)
      isLong ? pressEvents.redBlackLongPress = true : pressEvents.redBlackShortPress = true;
    else if (ids[0] == GREEN && ids[1] == BLACK)
      isLong ? pressEvents.greenBlackLongPress = true : pressEvents.greenBlackShortPress = true;
    else if (ids[0] == GREEN && ids[1] == RED)
      isLong ? pressEvents.redGreenLongPress = true : pressEvents.redGreenShortPress = true;
  } else if (count == 3 &&
             ids[0] == GREEN && ids[1] == RED && ids[2] == BLACK) {
    isLong ? pressEvents.greenRedBlackLongPress = true : pressEvents.greenRedBlackShortPress = true;
  }
}

//
// === SOUNDS ==
// // --- Sound a short tick (if enabled) ---
void tick() {
  if(tickEnabled){
    tone(BUZZER_PIN, 7000, 5);  // Short high-pitched tone (7kHz, 5ms)
    delay(10);
    noTone(BUZZER_PIN);
  }
}

void quietTick() {
  tone(BUZZER_PIN, 7000, 1);  // Short high-pitched tone (7kHz, 1ms)
  delay(1);
  noTone(BUZZER_PIN);
}

// --- Short beep pair for power state change ---
void doubleTick() {
  tone(BUZZER_PIN, 7000, 1);
  delay(100);
  tone(BUZZER_PIN, 7000, 1); 
  delay(20);
  noTone(BUZZER_PIN);
}

// === Play two short tones (shutdown confirmation) ===
void playQuietDoubleClick() {
  tone(BUZZER_PIN, 1200, 40);
  delay(80);
  tone(BUZZER_PIN, 900, 40);
  delay(100);
}

// --- Enable ticking with triad tone ---
void playAscendingArpeggio() {
  tone(BUZZER_PIN, 440, 50);  // A
  delay(30);
  tone(BUZZER_PIN, 554, 50);  // C#
  delay(30);
  tone(BUZZER_PIN, 659, 50);  // E
  delay(200);
  noTone(BUZZER_PIN);
}

// --- Disable ticking with descending tone ---
void playDescendingArpeggio() {
  tone(BUZZER_PIN, 659, 50);  // E
  delay(30);
  tone(BUZZER_PIN, 554, 50);  // C#
  delay(30);
  tone(BUZZER_PIN, 440, 50);  // A
  delay(200);
  noTone(BUZZER_PIN);
}

void playStopwatchModeTone() {
  // Tone
  tone(BUZZER_PIN, 660, 50); delay(60);
  tone(BUZZER_PIN, 660, 50); delay(60);
  tone(BUZZER_PIN, 220, 150); delay(200);
  noTone(BUZZER_PIN);

  // Flash "5" as a proxy for "S"
  uint8_t flash5[4] = { 0, 0, 0, 0x6D }; // Digit 5 = segments A, F, G, C, D
  display.setSegments(flash5);
  delay(250);
  display.clear();
  delay(100);
  display.setSegments(flash5);
  delay(250);
  display.clear();
}

void playCountdownModeTone() {
  // Tone
  tone(BUZZER_PIN, 220, 50); delay(60);
  tone(BUZZER_PIN, 220, 50); delay(60);
  tone(BUZZER_PIN, 660, 150); delay(200);
  noTone(BUZZER_PIN);

  // Flash "A" for Alarm / Countdown
  uint8_t flashA[4] = { 0x77, 0, 0, 0 }; // "A" leftmost
  display.setSegments(flashA);
  delay(250);
  display.clear();
  delay(100);
  display.setSegments(flashA);
  delay(250);
  display.clear();
}