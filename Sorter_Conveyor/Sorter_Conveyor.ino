// Sorter motor pins
#define SORTER_STEP_PIN 12   // Pulse pin (PUL) for Sorter motor
#define SORTER_DIR_PIN 14    // Direction pin (DIR) for Sorter motor

// Conveyor motor pins
#define CONVEYOR_STEP_PIN 2   // Pulse pin (PUL) for Conveyor motor
#define CONVEYOR_DIR_PIN 4    // Direction pin (DIR) for Conveyor motor

// Motion parameters
const int stepsPerRev = 200;   // Steps per revolution for sorter
const int delayTime = 200;     // Microseconds between pulses (controls speed)
int Rev = 8;                   // Number of revolutions for sorter

// State tracking for sorter
enum SorterState {
  MOVING_FORWARD,
  PAUSE_AFTER_FORWARD,
  MOVING_BACKWARD,
  PAUSE_AFTER_BACKWARD
};

SorterState sorterState = MOVING_FORWARD;
long sorterStepCount = 0;
long sorterTargetSteps = stepsPerRev * Rev;
unsigned long pauseStartTime = 0;
const unsigned long pauseDuration = 1000; // 1 second pause

void setup() {
  // Set pin modes
  pinMode(SORTER_STEP_PIN, OUTPUT);
  pinMode(SORTER_DIR_PIN, OUTPUT);
  pinMode(CONVEYOR_STEP_PIN, OUTPUT);
  pinMode(CONVEYOR_DIR_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("Sorter: alternating cycles | Conveyor: constant speed");

  // Set initial directions
  digitalWrite(SORTER_DIR_PIN, HIGH);    // Start forward
  digitalWrite(CONVEYOR_DIR_PIN, HIGH);  // Constant direction
}

void loop() {
  // Handle sorter motor state machine
  switch (sorterState) {
    case MOVING_FORWARD:
      if (sorterStepCount < sorterTargetSteps) {
        digitalWrite(SORTER_STEP_PIN, HIGH);
        delayMicroseconds(delayTime);
        digitalWrite(SORTER_STEP_PIN, LOW);
        delayMicroseconds(delayTime);
        sorterStepCount++;
      } else {
        Serial.println("Sorter: Forward complete, pausing...");
        sorterState = PAUSE_AFTER_FORWARD;
        pauseStartTime = millis();
        sorterStepCount = 0;
      }
      break;

    case PAUSE_AFTER_FORWARD:
      if (millis() - pauseStartTime >= pauseDuration) {
        Serial.println("Sorter: Moving backward...");
        digitalWrite(SORTER_DIR_PIN, LOW);  // Reverse direction
        sorterState = MOVING_BACKWARD;
      }
      break;

    case MOVING_BACKWARD:
      if (sorterStepCount < sorterTargetSteps) {
        digitalWrite(SORTER_STEP_PIN, HIGH);
        delayMicroseconds(delayTime);
        digitalWrite(SORTER_STEP_PIN, LOW);
        delayMicroseconds(delayTime);
        sorterStepCount++;
      } else {
        Serial.println("Sorter: Backward complete, pausing...");
        sorterState = PAUSE_AFTER_BACKWARD;
        pauseStartTime = millis();
        sorterStepCount = 0;
      }
      break;

    case PAUSE_AFTER_BACKWARD:
      if (millis() - pauseStartTime >= pauseDuration) {
        Serial.println("Sorter: Moving forward...");
        digitalWrite(SORTER_DIR_PIN, HIGH);  // Forward direction
        sorterState = MOVING_FORWARD;
      }
      break;
  }

  // Conveyor motor runs continuously
  digitalWrite(CONVEYOR_STEP_PIN, HIGH);
  delayMicroseconds(delayTime);
  digitalWrite(CONVEYOR_STEP_PIN, LOW);
  delayMicroseconds(delayTime);
}
