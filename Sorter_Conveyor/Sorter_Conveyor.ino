#include <WiFi.h>
#include <WebServer.h>
#include <esp_task_wdt.h>

// WiFi credentials
const char* ssid = "YasiruDEX";
const char* password = "freewifi";

// Sorter motor pins
#define SORTER_STEP_PIN 12
#define SORTER_DIR_PIN 14
#define SORTER_EN_PIN 13      // ENABLE pin for sorter driver

// Conveyor motor pins
#define CONVEYOR_STEP_PIN 16
#define CONVEYOR_DIR_PIN 17
#define CONVEYOR_EN_PIN 5     // ENABLE pin for conveyor driver

// Motion parameters
const int stepsPerRev = 200;
volatile int conveyorDelayTime = 500;
volatile bool conveyorRunning = false;
bool sorterForward = true;

// Anomaly handling
volatile bool anomalyMode = false;
volatile long anomalyStepCount = 0;
const long anomalyTotalSteps = stepsPerRev * 10;

// Create web server on port 80
WebServer server(80);

// Task handle for conveyor motor
TaskHandle_t conveyorTaskHandle = NULL;

// Function prototypes
void moveSorter(long steps);

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Deinitialize the default watchdog first
  esp_task_wdt_deinit();
  Serial.println("Watchdog deinitialized");
  
  // Set pin modes
  pinMode(SORTER_STEP_PIN, OUTPUT);
  pinMode(SORTER_DIR_PIN, OUTPUT);
  pinMode(SORTER_EN_PIN, OUTPUT);
  pinMode(CONVEYOR_STEP_PIN, OUTPUT);
  pinMode(CONVEYOR_DIR_PIN, OUTPUT);
  pinMode(CONVEYOR_EN_PIN, OUTPUT);
  
  // Initialize all pins to safe states
  digitalWrite(SORTER_STEP_PIN, LOW);
  digitalWrite(SORTER_DIR_PIN, LOW);
  digitalWrite(CONVEYOR_STEP_PIN, LOW);
  digitalWrite(CONVEYOR_DIR_PIN, HIGH);
  
  // CRITICAL: Enable drivers (LOW = enabled)
  digitalWrite(SORTER_EN_PIN, LOW);
  digitalWrite(CONVEYOR_EN_PIN, LOW);
  
  Serial.println("Stepper drivers ENABLED");
  delay(100);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
  
  // Create conveyor motor task on Core 0
  xTaskCreatePinnedToCore(
    conveyorTask,
    "ConveyorMotor",
    10000,
    NULL,
    2,
    &conveyorTaskHandle,
    0
  );
  
  Serial.println("Conveyor motor task created on Core 0");
  
  // Define API endpoints
  server.on("/flip", handleFlip);
  server.on("/continuous", handleContinuous);
  server.on("/stop", handleStop);
  server.on("/anomaly", handleAnomaly);
  server.on("/status", handleStatus);
  server.on("/", handleRoot);
  
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("\n=== READY ===");
  Serial.println("Test: http://" + WiFi.localIP().toString() + "/continuous?speed=500");
}

void loop() {
  server.handleClient();
  delay(1); // Small yield to keep things responsive
}

// Conveyor motor task - runs on Core 0
void conveyorTask(void *pvParameters) {
  Serial.print("Conveyor task on Core: ");
  Serial.println(xPortGetCoreID());
  
  unsigned long stepCounter = 0;
  
  while (true) {
    if (anomalyMode) {
      if (anomalyStepCount < anomalyTotalSteps) {
        digitalWrite(CONVEYOR_STEP_PIN, HIGH);
        delayMicroseconds(5);
        digitalWrite(CONVEYOR_STEP_PIN, LOW);
        delayMicroseconds(conveyorDelayTime);
        anomalyStepCount++;
        
        // Yield every 200 steps to allow other tasks to run
        if (anomalyStepCount % 200 == 0) {
          vTaskDelay(1);
        }
        
        if (anomalyStepCount % stepsPerRev == 0) {
          Serial.print("Anomaly: ");
          Serial.print(anomalyStepCount / stepsPerRev);
          Serial.println(" rev");
        }
      } else {
        anomalyMode = false;
        anomalyStepCount = 0;
        conveyorRunning = false;
        Serial.println("Anomaly complete");
      }
    }
    else if (conveyorRunning) {
      digitalWrite(CONVEYOR_STEP_PIN, HIGH);
      delayMicroseconds(5);
      digitalWrite(CONVEYOR_STEP_PIN, LOW);
      delayMicroseconds(conveyorDelayTime);
      
      // Yield every 200 steps to allow system tasks to run
      stepCounter++;
      if (stepCounter % 200 == 0) {
        vTaskDelay(1);
      }
    }
    else {
      stepCounter = 0;
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
  }
}

// API Handlers

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
  
  Serial.println("Flip complete");
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
      digitalWrite(CONVEYOR_EN_PIN, LOW);
      delay(10);
      conveyorRunning = true;
      
      Serial.print("Conveyor: ");
      Serial.print(speed);
      Serial.println(" us");
      
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
  digitalWrite(CONVEYOR_EN_PIN, LOW);
  delay(10);
  
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
  html += "<style>";
  html += "body{font-family:Arial;margin:20px;background:#f5f5f5;}";
  html += ".btn{display:inline-block;padding:15px 25px;margin:10px 5px;";
  html += "border:none;border-radius:8px;font-size:16px;cursor:pointer;";
  html += "color:white;text-decoration:none;}";
  html += ".green{background:#28a745;}";
  html += ".red{background:#dc3545;}";
  html += ".blue{background:#007bff;}";
  html += ".orange{background:#fd7e14;}";
  html += "h1{color:#333;}";
  html += ".container{background:white;padding:20px;border-radius:10px;}";
  html += "</style></head><body><div class='container'>";
  html += "<h1>Stepper Motor Control</h1>";
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
  
  html += "<h2>Troubleshooting</h2>";
  html += "<ul>";
  html += "<li>Motor not moving? Check ENABLE pin wiring</li>";
  html += "<li>Jerky motion? Try slower speeds (500-1000us)</li>";
  html += "<li>Check Serial Monitor at 115200 baud</li>";
  html += "</ul>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
}

void moveSorter(long steps) {
  for (long i = 0; i < steps; i++) {
    digitalWrite(SORTER_STEP_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(SORTER_STEP_PIN, LOW);
    delayMicroseconds(500);
    
    if (i % 200 == 0 && i > 0) {
      Serial.print(".");
    }
  }
  Serial.println();
}
