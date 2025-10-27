#include <WiFi.h>
#include <WebServer.h>

// Replace with your network credentials
const char* ssid = "YasiruDEX";
const char* password = "freewifi";

// Sorter motor pins
#define SORTER_STEP_PIN 12
#define SORTER_DIR_PIN 14

// Conveyor motor pins
#define CONVEYOR_STEP_PIN 2
#define CONVEYOR_DIR_PIN 4

// Motion parameters
const int stepsPerRev = 200;
int conveyorDelayTime = 200;  // Default speed (microseconds)
bool conveyorRunning = false;
bool sorterForward = true;    // Track sorter direction

// Anomaly handling
bool anomalyMode = false;
long anomalyStepCount = 0;
const long anomalyTotalSteps = stepsPerRev * 10;  // 10 revolutions

// Create web server on port 80
WebServer server(80);

unsigned long lastConveyorStep = 0;

void setup() {
  Serial.begin(115200);
  
  // Set pin modes
  pinMode(SORTER_STEP_PIN, OUTPUT);
  pinMode(SORTER_DIR_PIN, OUTPUT);
  pinMode(CONVEYOR_STEP_PIN, OUTPUT);
  pinMode(CONVEYOR_DIR_PIN, OUTPUT);
  
  digitalWrite(CONVEYOR_DIR_PIN, HIGH);
  
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
  
  // Define API endpoints
  server.on("/flip", handleFlip);
  server.on("/continuous", handleContinuous);
  server.on("/stop", handleStop);
  server.on("/anomaly", handleAnomaly);
  server.on("/", handleRoot);
  
  server.begin();
  Serial.println("HTTP server started");
  Serial.println("Available endpoints:");
  Serial.println("  GET /flip - Toggle sorter direction");
  Serial.println("  GET /continuous?speed=<value> - Start conveyor at speed");
  Serial.println("  GET /stop - Stop conveyor");
  Serial.println("  GET /anomaly - Move conveyor 10 revolutions");
}

void loop() {
  server.handleClient();
  
  // Handle anomaly mode (highest priority)
  if (anomalyMode) {
    if (anomalyStepCount < anomalyTotalSteps) {
      digitalWrite(CONVEYOR_STEP_PIN, HIGH);
      delayMicroseconds(conveyorDelayTime);
      digitalWrite(CONVEYOR_STEP_PIN, LOW);
      delayMicroseconds(conveyorDelayTime);
      anomalyStepCount++;
    } else {
      anomalyMode = false;
      anomalyStepCount = 0;
      conveyorRunning = false;
      Serial.println("Anomaly mode completed");
    }
    return;  // Skip other operations during anomaly
  }
  
  // Handle continuous conveyor movement
  if (conveyorRunning) {
    unsigned long currentMicros = micros();
    if (currentMicros - lastConveyorStep >= conveyorDelayTime) {
      digitalWrite(CONVEYOR_STEP_PIN, HIGH);
      delayMicroseconds(10);
      digitalWrite(CONVEYOR_STEP_PIN, LOW);
      lastConveyorStep = currentMicros;
    }
  }
}

// API Endpoint Handlers

// GET /flip - Toggle sorter motor direction
void handleFlip() {
  if (anomalyMode) {
    server.send(409, "application/json", "{\"status\":\"error\",\"message\":\"Anomaly mode active\"}");
    return;
  }
  
  if (sorterForward) {
    Serial.println("Sorter: Moving backward");
    digitalWrite(SORTER_DIR_PIN, LOW);
    moveSorter(stepsPerRev * 8);  // 8 revolutions backward
    sorterForward = false;
  } else {
    Serial.println("Sorter: Moving forward");
    digitalWrite(SORTER_DIR_PIN, HIGH);
    moveSorter(stepsPerRev * 8);  // 8 revolutions forward
    sorterForward = true;
  }
  
  String response = "{\"status\":\"success\",\"direction\":\"" + 
                    String(sorterForward ? "forward" : "backward") + "\"}";
  server.send(200, "application/json", response);
}

// GET /continuous?speed=<value> - Start conveyor at specified speed
void handleContinuous() {
  if (anomalyMode) {
    server.send(409, "application/json", "{\"status\":\"error\",\"message\":\"Anomaly mode active\"}");
    return;
  }
  
  if (server.hasArg("speed")) {
    int speed = server.arg("speed").toInt();
    if (speed > 0 && speed <= 10000) {
      conveyorDelayTime = speed;
      conveyorRunning = true;
      lastConveyorStep = micros();
      
      Serial.print("Conveyor started at speed: ");
      Serial.print(speed);
      Serial.println(" μs");
      
      String response = "{\"status\":\"success\",\"speed\":" + String(speed) + ",\"running\":true}";
      server.send(200, "application/json", response);
    } else {
      server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Speed must be between 1-10000\"}");
    }
  } else {
    server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"Missing speed parameter\"}");
  }
}

// GET /stop - Stop conveyor motor
void handleStop() {
  conveyorRunning = false;
  Serial.println("Conveyor stopped");
  server.send(200, "application/json", "{\"status\":\"success\",\"running\":false}");
}

// GET /anomaly - Move conveyor 10 revolutions (stops all other actions)
void handleAnomaly() {
  anomalyMode = true;
  anomalyStepCount = 0;
  conveyorRunning = false;
  
  Serial.println("Anomaly mode activated - Moving conveyor 10 revolutions");
  server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Anomaly mode activated\",\"revolutions\":10}");
}

// GET / - Root endpoint with API documentation
void handleRoot() {
  String html = "<html><body><h1>Stepper Motor Control API</h1>";
  html += "<h2>Available Endpoints:</h2>";
  html += "<ul>";
  html += "<li><b>GET /flip</b> - Toggle sorter motor direction (forward/backward 8 revolutions)</li>";
  html += "<li><b>GET /continuous?speed=VALUE</b> - Start conveyor at specified speed (1-10000 microseconds)</li>";
  html += "<li><b>GET /stop</b> - Stop conveyor motor</li>";
  html += "<li><b>GET /anomaly</b> - Emergency: Move conveyor 10 revolutions (stops all other operations)</li>";
  html += "</ul>";
  html += "<h2>Quick Test Links:</h2>";
  html += "<p><a href='/flip'>Flip Sorter</a></p>";
  html += "<p><a href='/continuous?speed=200'>Start Conveyor (speed=200)</a></p>";
  html += "<p><a href='/stop'>Stop Conveyor</a></p>";
  html += "<p><a href='/anomaly'>Trigger Anomaly</a></p>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

// Helper function to move sorter motor
void moveSorter(long steps) {
  for (long i = 0; i < steps; i++) {
    digitalWrite(SORTER_STEP_PIN, HIGH);
    delayMicroseconds(200);
    digitalWrite(SORTER_STEP_PIN, LOW);
    delayMicroseconds(200);
  }
}
