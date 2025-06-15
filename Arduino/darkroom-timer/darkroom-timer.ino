#include <TM1637Display.h>  // Library for controlling 4-digit 7-segment LED display

//#define ENABLE_STARTUP_SONG  // Uncomment to enable startup melody

// --- Pin configuration ---
const int CLK = 2;                      // Clock pin for TM1637
const int DIO = 3;                      // Data I/O pin for TM1637
const int START_BUTTON_PIN = 4;        // Button to start the timer
const int STOP_BUTTON_PIN  = 5;        // Button to stop the timer
const int RESET_BUTTON_PIN = 6;        // Button to reset or power on/off
const int BUZZER_PIN       = 7;        // Buzzer for audible ticks and feedback

const int LED_BRIGHTNESS = 0;          // Brightness level (0 = dim, 7 = bright)
const int RESET_LONG_PRESS_MILLIS = 500; // Long press duration to power off

TM1637Display display(CLK, DIO);       // Initialize the display

// --- Timer state variables ---
unsigned long startMillis = 0;          // Start time of current session
unsigned long lastDeltaSeconds = 0;     // Last second tracked for updates
unsigned long resetPressedStartMillis = 0; // When RESET was pressed

bool colonOn = false;                   // Whether colon is currently visible
bool isRunning = false;                 // Whether timer is running
bool isOn = true;                       // Power state of device
bool tickEnabled = true;                // Whether ticking sound is enabled
int displaySeconds = 0;                 // Time currently displayed (in seconds)

void setup() {
  display.setBrightness(LED_BRIGHTNESS);  // Set initial display brightness
  display.showNumberDecEx(0, 0b01000000, true); // Display 00:00 with colon

  // Configure button and buzzer pins
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

#ifdef ENABLE_STARTUP_SONG
  startup();  // Optional melody
#endif

  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off at startup
  startMillis = millis();
}

void loop() {
  if(!isOn){
    // Device is off; only RESET button is active
    bool resetPressed = digitalRead(RESET_BUTTON_PIN) == LOW;
    display.clear();
    if(resetPressed){
      if(resetPressedStartMillis == 0){
        powerOn();  // Turn device back on
      }
    }else{
      resetPressedStartMillis = 0; // Button released
    }
  }else{
    // Device is on

    // --- Handle RESET button press duration ---
    bool resetPressed = digitalRead(RESET_BUTTON_PIN) == LOW;
    bool resetLongPressed = false;
    bool resetShortPressed = false;
    unsigned long resetPressDuration = 0;

    if(resetPressed){
      if(resetPressedStartMillis == 0){
        resetPressedStartMillis = millis();
      }
    }

    if(resetPressedStartMillis > 0){
      resetPressDuration = millis() - resetPressedStartMillis;
      resetShortPressed = true;
      if(resetPressDuration > RESET_LONG_PRESS_MILLIS){
        resetLongPressed = true;
        resetShortPressed = false;
      }
      if(!resetPressed){
        resetPressedStartMillis = 0;
      }
    }

    if(resetLongPressed){
      powerOff(); // Power down device
      return;
    }

    // --- Time and display logic ---
    unsigned long currentMillis = millis();
    unsigned long deltaMillis = currentMillis - startMillis;
    unsigned long deltaSeconds = deltaMillis / 1000;

    colonOn = isRunning ? (deltaMillis % 1000 < 500) : true;  // Blink colon if running

    if (isRunning && deltaSeconds > lastDeltaSeconds) {
      lastDeltaSeconds++;
      displaySeconds++;
      if(tickEnabled){
        click();  // Tick sound
      }
    }

    // Display time as MM:SS
    int minutes = displaySeconds / 60;
    int seconds = displaySeconds % 60;
    int timeVal = minutes * 100 + seconds;
    display.showNumberDecEx(timeVal, colonOn ? 0b01000000 : 0, true);

    // --- Button combinations ---
    bool startResetPressed = digitalRead(START_BUTTON_PIN) == LOW && digitalRead(RESET_BUTTON_PIN) == LOW;
    bool stopResetPressed  = digitalRead(STOP_BUTTON_PIN)  == LOW && digitalRead(RESET_BUTTON_PIN) == LOW;
    bool startPressed      = !isRunning && digitalRead(START_BUTTON_PIN) == LOW;
    bool stopPressed       = isRunning && digitalRead(STOP_BUTTON_PIN) == LOW;

    // --- Button actions ---
    if(startResetPressed){
      enableTick();
    }else if (stopResetPressed) {
      disableTick();
    }else if (startPressed) {
      isRunning = true;
      startMillis = millis();
      lastDeltaSeconds = 0;
    }else if(stopPressed){
      isRunning = false;
    }else if(resetShortPressed){
      startMillis = millis();
      displaySeconds = 0;
      lastDeltaSeconds = 0;
      display.showNumberDecEx(0, 0b01000000, true); // Reset to 00:00
    }
  }
  delay(10);  // Basic debounce / loop delay
}

// --- Sound a short tick (if enabled) ---
void click() {
  tone(BUZZER_PIN, 7000, 5);  // Short high-pitched tone (7kHz, 5ms)
  delay(10);
  noTone(BUZZER_PIN);
}

// --- Enable ticking with triad tone ---
void enableTick() {
  if (!tickEnabled) {
    tickEnabled = true;
    tone(BUZZER_PIN, 440, 50);  // A
    delay(30);
    tone(BUZZER_PIN, 554, 50);  // C#
    delay(30);
    tone(BUZZER_PIN, 659, 50);  // E
    delay(200);
    noTone(BUZZER_PIN);
  }
}

// --- Disable ticking with descending tone ---
void disableTick() {
  if (tickEnabled) {
    tickEnabled = false;
    tone(BUZZER_PIN, 659, 50);  // E
    delay(30);
    tone(BUZZER_PIN, 554, 50);  // C#
    delay(30);
    tone(BUZZER_PIN, 440, 50);  // A
    delay(200);
    noTone(BUZZER_PIN);
  }
}

// --- Power the unit on ---
void powerOn(){
  if(!isOn){
    powerOnOffSound();
    isOn = true;
    isRunning = false;
    displaySeconds = 0;
  }
}

// --- Power the unit off ---
void powerOff(){
  if(isOn){
    powerOnOffSound();
    isOn = false;
  }
}

// --- Short beep pair for power state change ---
void powerOnOffSound() {
  tone(BUZZER_PIN, 7000, 1);
  delay(100);
  tone(BUZZER_PIN, 7000, 1); 
  delay(20);
  noTone(BUZZER_PIN);
}

#ifdef ENABLE_STARTUP_SONG
// --- Optional melody on startup ---
void startup() {
  int notes[] = {
    330, 440, 523, 494, 330, 494, 587,   // 16th notes
    523, 659, 415, 659                   // 8th notes
  };

  int durations[] = {
    120, 120, 120, 120, 120, 120, 120,   // 16ths
    240, 240, 240, 240                   // 8ths
  };

  delay(120); // Initial rest

  for (int i = 0; i < sizeof(notes)/sizeof(notes[0]); i++) {
    tone(BUZZER_PIN, notes[i], durations[i] - 20);  // Staccato effect
    delay(durations[i]);
  }

  noTone(BUZZER_PIN);
}
#endif
