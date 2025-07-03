const int LED_PIN = 13;
const int DOT = 200;
const int DASH = DOT * 3;
const int GAP = DOT;

void setup() {
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  // Morse "V" = ...-
  blink(DOT);
  blink(DOT);
  blink(DOT);
  blink(DASH);

  delay(2000); // Pause before repeating
}

void blink(int duration) {
  digitalWrite(LED_PIN, HIGH);
  delay(duration);
  digitalWrite(LED_PIN, LOW);
  delay(GAP);
}
