// Sorter motor pins
#define SORTER_STEP_PIN 12   // Pulse pin (PUL) for Sorter motor
#define SORTER_DIR_PIN 14    // Direction pin (DIR) for Sorter motor

// Conveyor motor pins
#define CONVEYOR_STEP_PIN 2   // Pulse pin (PUL) for Conveyor motor
#define CONVEYOR_DIR_PIN 4    // Direction pin (DIR) for Conveyor motor

// Motion parameters
const int delayTime = 200;  // Microseconds between pulses (controls speed)

void setup() {
  // Set pin modes
  pinMode(SORTER_STEP_PIN, OUTPUT);
  pinMode(SORTER_DIR_PIN, OUTPUT);
  pinMode(CONVEYOR_STEP_PIN, OUTPUT);
  pinMode(CONVEYOR_DIR_PIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("Sorter and Conveyor motors rotating at constant speed...");

  // Set directions (change HIGH/LOW to reverse)
  digitalWrite(SORTER_DIR_PIN, HIGH);
  digitalWrite(CONVEYOR_DIR_PIN, HIGH);
}

void loop() {
  // Generate pulses for both motors simultaneously
  digitalWrite(SORTER_STEP_PIN, HIGH);
  digitalWrite(CONVEYOR_STEP_PIN, HIGH);
  delayMicroseconds(delayTime);

  digitalWrite(SORTER_STEP_PIN, LOW);
  digitalWrite(CONVEYOR_STEP_PIN, LOW);
  delayMicroseconds(delayTime);
}
