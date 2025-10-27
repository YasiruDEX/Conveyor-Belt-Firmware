// Stepper control pins
#define STEP_PIN 2   // Pulse pin
#define DIR_PIN 4    // Direction pin

// Motion parameters
const int stepsPerRev = 200;   // Adjust based on your motor/driver (e.g. 200 steps per rev)
const int delayTime = 200;     // Microseconds between pulses (controls speed)
int Rev = 8;

void setup() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("Stepper half-revolution forward and back started...");
}

void loop() {
  // Rotate half revolution forward
  Serial.println("Moving forward half revolution...");
  digitalWrite(DIR_PIN, HIGH);
  moveSteps(stepsPerRev * Rev);

  delay(1000); // short pause

  // Rotate half revolution backward
  Serial.println("Moving backward half revolution...");
  digitalWrite(DIR_PIN, LOW);
  moveSteps(stepsPerRev * Rev);

  delay(1000); // short pause before repeating
}

void moveSteps(long steps) {
  for (long i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(delayTime);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(delayTime);
  }
}
