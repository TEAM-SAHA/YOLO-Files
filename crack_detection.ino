/*
 * ============================================================
 *  PIPES Crack Detection — ESP32-CAM
 *  Model: YOLOv8n INT8 TFLite | Dataset: PIPES v1 (6777 images)
 *  Author: Md Zeeshan Raza
 * ============================================================
 *
 *  WIRING (ESP32-CAM MB — no adapter needed, direct USB):
 *  Just plug ESP32-CAM MB into PC via USB cable. Done.
 *
 *  HOW IT WORKS:
 *  1. ESP32-CAM captures a frame
 *  2. Resizes to 320x320
 *  3. Runs crack detection model
 *  4. If crack found → LED ON + Serial alert + WiFi alert
 *  5. Repeats every 2 seconds
 * ============================================================
 */

#include <Arduino.h>
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "fb_gfx.h"
#include <WiFi.h>
#include <WebServer.h>

// ============================================================
//  YOUR SETTINGS — change these
// ============================================================
const char* WIFI_SSID     = "YOUR_WIFI_NAME";   // your WiFi name
const char* WIFI_PASSWORD = "YOUR_WIFI_PASS";   // your WiFi password
const float CONFIDENCE_THRESHOLD = 0.45;         // 45% confidence to count as crack
// ============================================================

// ESP32-CAM MB pin definitions
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define LED_GPIO_NUM       4  // onboard flash LED

// Web server on port 80
WebServer server(80);

// Detection state
bool crackDetected   = false;
float lastConfidence = 0.0;
int   totalDetections = 0;
unsigned long lastDetectionTime = 0;

// ============================================================
//  Camera setup
// ============================================================
bool setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 12;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  Serial.println("Camera initialized OK");
  return true;
}

// ============================================================
//  WiFi Dashboard — view results in browser
// ============================================================
void handleRoot() {
  String color  = crackDetected ? "#E74C3C" : "#2ECC71";
  String status = crackDetected ? "⚠️ CRACK DETECTED" : "✅ NO CRACK";

  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta http-equiv='refresh' content='2'>"; // auto refresh every 2s
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>PIPES Crack Detection</title>";
  html += "<style>";
  html += "body{font-family:Arial,sans-serif;background:#1a1a2e;color:white;text-align:center;padding:20px;}";
  html += ".status{font-size:2em;font-weight:bold;padding:20px;border-radius:10px;margin:20px auto;max-width:400px;background:" + color + ";}";
  html += ".info{background:#16213e;border-radius:10px;padding:15px;margin:10px auto;max-width:400px;}";
  html += ".label{color:#aaa;font-size:0.85em;} .val{font-size:1.3em;font-weight:bold;}";
  html += "h1{color:#e94560;}";
  html += "</style></head><body>";
  html += "<h1>🔍 PIPES Crack Detector</h1>";
  html += "<div class='status'>" + status + "</div>";
  html += "<div class='info'>";
  html += "<p><span class='label'>Confidence</span><br><span class='val'>" + String(lastConfidence * 100, 1) + "%</span></p>";
  html += "<p><span class='label'>Total detections</span><br><span class='val'>" + String(totalDetections) + "</span></p>";
  html += "<p><span class='label'>Threshold</span><br><span class='val'>" + String(CONFIDENCE_THRESHOLD * 100, 0) + "%</span></p>";
  html += "<p><span class='label'>Model</span><br><span class='val'>YOLOv8n INT8</span></p>";
  html += "</div>";
  html += "<p style='color:#aaa;font-size:0.8em;'>Auto-refreshes every 2 seconds</p>";
  html += "<p style='color:#aaa;font-size:0.8em;'>PIPES v1 | Md Zeeshan Raza</p>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"crack\":" + String(crackDetected ? "true" : "false") + ",";
  json += "\"confidence\":" + String(lastConfidence, 3) + ",";
  json += "\"total_detections\":" + String(totalDetections);
  json += "}";
  server.send(200, "application/json", json);
}

// ============================================================
//  Simulated detection (replace with real TFLite inference)
// ============================================================
/*
  NOTE FOR DEPLOYMENT:
  Full TFLite inference on ESP32-CAM requires the
  TensorFlow Lite for Microcontrollers library.
  
  The detection logic below is a working simulation
  that demonstrates the full pipeline. To add real
  inference, include model.h and follow the
  TFLite Micro inference steps shown in comments below.
*/

// Simulated inference result — replace with real TFLite call
struct DetectionResult {
  bool  crackFound;
  float confidence;
};

DetectionResult runInference(camera_fb_t* fb) {
  DetectionResult result;

  /*
  // ---- REAL TFLITE INFERENCE (uncomment when model.h is added) ----
  //
  // #include "model.h"
  // #include "tensorflow/lite/micro/all_ops_resolver.h"
  // #include "tensorflow/lite/micro/micro_interpreter.h"
  // #include "tensorflow/lite/schema/schema_generated.h"
  //
  // static tflite::MicroInterpreter* interpreter = nullptr;
  // static TfLiteTensor* input = nullptr;
  //
  // const tflite::Model* model = tflite::GetModel(model_tflite);
  // static tflite::AllOpsResolver resolver;
  // const int kTensorArenaSize = 512 * 1024; // 512KB
  // static uint8_t tensor_arena[kTensorArenaSize];
  // static tflite::MicroInterpreter static_interpreter(
  //     model, resolver, tensor_arena, kTensorArenaSize);
  // interpreter = &static_interpreter;
  // interpreter->AllocateTensors();
  // input = interpreter->input(0);
  //
  // // Copy image data to input tensor
  // memcpy(input->data.uint8, fb->buf, input->bytes);
  //
  // interpreter->Invoke();
  //
  // TfLiteTensor* output = interpreter->output(0);
  // result.confidence = output->data.f[0];
  // result.crackFound = result.confidence > CONFIDENCE_THRESHOLD;
  // ----------------------------------------------------------------
  */

  // Simulated result — brightness-based heuristic as placeholder
  // (Replace this block with real TFLite inference above)
  long totalBrightness = 0;
  int  sampleSize = min((int)fb->len, 1000);
  for (int i = 0; i < sampleSize; i++) {
    totalBrightness += fb->buf[i];
  }
  float avgBrightness = totalBrightness / (float)sampleSize;

  // Dark regions in image may indicate cracks
  result.confidence = 1.0 - (avgBrightness / 255.0);
  result.crackFound = result.confidence > CONFIDENCE_THRESHOLD;

  return result;
}

// ============================================================
//  Setup
// ============================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=============================");
  Serial.println("  PIPES Crack Detection");
  Serial.println("  YOLOv8n | Md Zeeshan Raza");
  Serial.println("=============================\n");

  // LED pin
  pinMode(LED_GPIO_NUM, OUTPUT);
  digitalWrite(LED_GPIO_NUM, LOW);

  // Startup blink
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_GPIO_NUM, HIGH); delay(200);
    digitalWrite(LED_GPIO_NUM, LOW);  delay(200);
  }

  // Camera
  if (!setupCamera()) {
    Serial.println("Camera failed — check connections");
    while (true) {
      digitalWrite(LED_GPIO_NUM, HIGH); delay(100);
      digitalWrite(LED_GPIO_NUM, LOW);  delay(100);
    }
  }

  // WiFi
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 20) {
    delay(500);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("Dashboard URL: http://");
    Serial.println(WiFi.localIP());

    server.on("/",       handleRoot);
    server.on("/status", handleStatus);
    server.begin();
    Serial.println("Web dashboard running");
  } else {
    Serial.println("\nWiFi not connected — running offline mode");
  }

  Serial.println("\n✅ System ready — scanning for cracks...\n");
}

// ============================================================
//  Main loop
// ============================================================
void loop() {
  server.handleClient();

  // Capture frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    return;
  }

  // Run inference
  DetectionResult result = runInference(fb);
  esp_camera_fb_return(fb);

  // Update state
  crackDetected   = result.crackFound;
  lastConfidence  = result.confidence;

  // Alert
  if (crackDetected) {
    totalDetections++;
    lastDetectionTime = millis();

    // LED ON
    digitalWrite(LED_GPIO_NUM, HIGH);

    Serial.println("==================================");
    Serial.println("  ⚠️  CRACK DETECTED!");
    Serial.printf("  Confidence : %.1f%%\n", result.confidence * 100);
    Serial.printf("  Detection  : #%d\n", totalDetections);
    Serial.println("==================================");

    delay(1000);
    digitalWrite(LED_GPIO_NUM, LOW);

  } else {
    digitalWrite(LED_GPIO_NUM, LOW);
    Serial.printf("No crack | confidence: %.1f%% | scans: %d\n",
                  result.confidence * 100, totalDetections);
  }

  delay(2000); // scan every 2 seconds
}
