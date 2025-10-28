#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "YasiruDEX";
const char* password = "freewifi";

// Sorter motor pins
#define SORTER_STEP_PIN 12
#define SORTER_DIR_PIN 14
#define SORTER_EN_PIN 13

// Conveyor motor pins
#define CONVEYOR_STEP_PIN 2
#define CONVEYOR_DIR_PIN 4
#define CONVEYOR_EN_PIN 5

// Motion parameters
const int stepsPerRev = 200;
volatile int conveyorDelayTime = 30;   // microseconds per half-cycle
volatile bool conveyorRunning = false;
bool sorterForward = true;

// Anomaly handling
volatile bool anomalyMode = false;
volatile long anomalyStepCount = 0;
const long anomalyTotalSteps = stepsPerRev * 10;

// Create web server
WebServer server(80);

// Hardware timer
hw_timer_t* stepTimer = NULL;

// Function prototypes
void moveSorter(long steps);
void IRAM_ATTR onStepTimer();

// === SETUP ===
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Setup pins
  pinMode(SORTER_STEP_PIN, OUTPUT);
  pinMode(SORTER_DIR_PIN, OUTPUT);
  pinMode(SORTER_EN_PIN, OUTPUT);
  pinMode(CONVEYOR_STEP_PIN, OUTPUT);
  pinMode(CONVEYOR_DIR_PIN, OUTPUT);
  pinMode(CONVEYOR_EN_PIN, OUTPUT);

  digitalWrite(SORTER_EN_PIN, LOW);
  digitalWrite(CONVEYOR_EN_PIN, LOW);
  digitalWrite(CONVEYOR_DIR_PIN, HIGH);
  digitalWrite(CONVEYOR_STEP_PIN, LOW);

  Serial.println("Stepper drivers ENABLED");

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // === Create hardware timer ===
  // New API in ESP32 Core 3.x: timerBegin(frequency)
  stepTimer = timerBegin(1000000); // 1 MHz timer (1 µs tick)
  timerAttachInterrupt(stepTimer, &onStepTimer);
  timerAlarm(stepTimer, conveyorDelayTime, true, 0); // auto-reload on

  // Define HTTP endpoints
  server.on("/flip", handleFlip);
  server.on("/continuous", handleContinuous);
  server.on("/stop", handleStop);
  server.on("/anomaly", handleAnomaly);
  server.on("/status", handleStatus);
  server.on("/", handleRoot);

  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Ready: http://" + WiFi.localIP().toString());
}

// === MAIN LOOP ===
void loop() {
  server.handleClient();
}

// === TIMER ISR ===
void IRAM_ATTR onStepTimer() {
  static bool stepState = false;
  if (conveyorRunning || anomalyMode) {
    digitalWrite(CONVEYOR_STEP_PIN, stepState);
    stepState = !stepState;

    if (anomalyMode && !stepState) {
      anomalyStepCount++;
      if (anomalyStepCount >= anomalyTotalSteps * 2) {
        anomalyMode = false;
        anomalyStepCount = 0;
        conveyorRunning = false;
      }
    }
  }
}

// === API HANDLERS ===
void handleFlip() {
  if (anomalyMode) {
    server.send(409, "application/json", "{\"status\":\"error\",\"message\":\"Anomaly active\"}");
    return;
  }

  bool wasRunning = conveyorRunning;
  conveyorRunning = false;
  delay(100);

  digitalWrite(SORTER_EN_PIN, LOW);
  delay(10);

  if (sorterForward) {
    Serial.println("Sorter BACKWARD");
    digitalWrite(SORTER_DIR_PIN, LOW);
    moveSorter(stepsPerRev * 8);
    sorterForward = false;
  } else {
    Serial.println("Sorter FORWARD");
    digitalWrite(SORTER_DIR_PIN, HIGH);
    moveSorter(stepsPerRev * 8);
    sorterForward = true;
  }

  conveyorRunning = wasRunning;
  String response = "{\"status\":\"success\",\"direction\":\"";
  response += sorterForward ? "forward" : "backward";
  response += "\"}";
  server.send(200, "application/json", response);
}

void handleContinuous() {
  if (anomalyMode) {
    server.send(409, "application/json", "{\"status\":\"error\",\"message\":\"Anomaly active\"}");
    return;
  }

  if (server.hasArg("speed")) {
    int speed = server.arg("speed").toInt();
    if (speed >= 50 && speed <= 5000) {
      conveyorDelayTime = speed;
      timerAlarm(stepTimer, conveyorDelayTime, true, 0);
      conveyorRunning = true;

      Serial.printf("Conveyor running at %d µs per half-cycle\n", speed);

      String response = "{\"status\":\"success\",\"speed\":";
      response += String(speed);
      response += ",\"running\":true}";
      server.send(200, "application/json", response);
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Speed 50-5000\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing speed\"}");
  }
}

void handleStop() {
  conveyorRunning = false;
  digitalWrite(CONVEYOR_STEP_PIN, LOW);
  Serial.println("Conveyor STOPPED");
  server.send(200, "application/json", "{\"status\":\"success\",\"running\":false}");
}

void handleAnomaly() {
  Serial.println("ANOMALY MODE");
  anomalyMode = true;
  anomalyStepCount = 0;
  conveyorRunning = false;
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Anomaly: 10 rev\"}");
}

void handleStatus() {
  String status = "{\"conveyor_running\":";
  status += conveyorRunning ? "true" : "false";
  status += ",\"conveyor_speed\":";
  status += String(conveyorDelayTime);
  status += ",\"anomaly_active\":";
  status += anomalyMode ? "true" : "false";
  status += ",\"sorter_direction\":\"";
  status += sorterForward ? "forward" : "backward";
  status += "\"}";
  server.send(200, "application/json", status);
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>body{font-family:Arial;margin:20px;background:#f5f5f5;}";
  html += ".btn{padding:15px 25px;margin:10px 5px;border:none;border-radius:8px;";
  html += "font-size:16px;cursor:pointer;color:white;text-decoration:none;}";
  html += ".green{background:#28a745;}.red{background:#dc3545;}.blue{background:#007bff;}.orange{background:#fd7e14;}";
  html += "h1{color:#333;}.container{background:white;padding:20px;border-radius:10px;}</style></head><body>";
  html += "<div class='container'><h1>Stepper Motor Control</h1>";
  html += "<p><strong>IP:</strong> " + WiFi.localIP().toString() + "</p>";
  html += "<h2>Conveyor Speed</h2>";
  html += "<a href='/continuous?speed=100' class='btn green'>Very Fast (100us)</a>";
  html += "<a href='/continuous?speed=200' class='btn green'>Fast (200us)</a>";
  html += "<a href='/continuous?speed=500' class='btn green'>Medium (500us)</a>";
  html += "<a href='/continuous?speed=1000' class='btn green'>Slow (1000us)</a>";
  html += "<a href='/continuous?speed=2000' class='btn green'>Very Slow (2000us)</a>";
  html += "<h2>Controls</h2>";
  html += "<a href='/flip' class='btn blue'>Flip Sorter</a>";
  html += "<a href='/stop' class='btn red'>Stop Conveyor</a>";
  html += "<a href='/anomaly' class='btn orange'>Anomaly (10 rev)</a>";
  html += "<a href='/status' class='btn blue'>Status</a>";
  html += "</div></body></html>";
  server.send(200, "text/html", html);
}

// === SORTER STEPPER ===
void moveSorter(long steps) {
  for (long i = 0; i < steps; i++) {
    digitalWrite(SORTER_STEP_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(SORTER_STEP_PIN, LOW);
    delayMicroseconds(500);
    if (i % 200 == 0 && i > 0) Serial.print(".");
  }
  Serial.println();
}
