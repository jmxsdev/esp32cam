#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>  // Para peticiones HTTP

// ===========================
// Select camera model in board_config.h
// ===========================
#include "board_config.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Jmxs";
const char *password = "Sjlp.2022";

// ===========================
// LED Configuration for YOUR ESP32-CAM
// ===========================
#define LED_KNOWN 12      // GPIO12 - LED Verde (Rostro Conocido)
#define LED_UNKNOWN 13    // GPIO13 - LED Rojo (Rostro Desconocido)

// ===========================
// API Configuration
// ===========================
const char* api_url = "http://192.168.1.100:5000/recognize"; // Cambia por IP de tu PC

// Variables globales
bool faceRecognitionActive = true;

void startCameraServer();
void setupLedFlash();

// Función para controlar LEDs
void controlLEDs(bool known, bool unknown) {
  digitalWrite(LED_KNOWN, known);
  digitalWrite(LED_UNKNOWN, unknown);
  Serial.printf("LEDs - Conocido: %d, Desconocido: %d\n", known, unknown);
}

// Función para enviar a API Python
void sendToAPI(const char* face_type) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(api_url);
    http.addHeader("Content-Type", "application/json");
    
    String jsonPayload = "{\"face_type\":\"" + String(face_type) + "\"}";
    int httpCode = http.POST(jsonPayload);
    
    if (httpCode > 0) {
      Serial.printf("✅ API: %s - Código: %d\n", face_type, httpCode);
    } else {
      Serial.printf("❌ Error API: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("🚀 Iniciando ESP32-CAM con Control de LEDs...");

  // ===========================
  // INICIALIZAR LEDs
  // ===========================
  pinMode(LED_KNOWN, OUTPUT);
  pinMode(LED_UNKNOWN, OUTPUT);
  controlLEDs(false, false); // Apagar ambos al inicio
  
  // Test rápido de LEDs
  Serial.println("💡 Probando LEDs...");
  controlLEDs(true, false);
  delay(300);
  controlLEDs(false, true);
  delay(300);
  controlLEDs(false, false);

  // ===========================
  // Configuración de Cámara (TU CÓDIGO ORIGINAL)
  // ===========================
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }
  
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  // ===========================
  // Conectar WiFi
  // ===========================
  WiFi.begin(ssid, password);
  Serial.print("📡 Conectando WiFi");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado");
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Error WiFi");
  }

  // ===========================
  // Iniciar Servidor Web
  // ===========================
  startCameraServer();
  Serial.println("🚀 Servidor web iniciado");

  Serial.println("🎯 Sistema listo - Use la IP mostrada arriba");
}

// ===========================
// SIMULACIÓN de Reconocimiento Facial
// ===========================
void simulateFaceRecognition() {
  static unsigned long lastDetection = 0;
  static bool knownFace = true;
  
  // Simular detección cada 10 segundos
  if (millis() - lastDetection > 10000) {
    lastDetection = millis();
    
    if (knownFace) {
      Serial.println("✅ ROSTRO CONOCIDO DETECTADO");
      controlLEDs(true, false);  // Verde ON, Rojo OFF
      sendToAPI("known");
    } else {
      Serial.println("🚨 ROSTRO DESCONOCIDO - INTRUSO");
      controlLEDs(false, true);  // Verde OFF, Rojo ON
      sendToAPI("unknown");
    }
    
    // Alternar para próxima simulación
    knownFace = !knownFace;
    
    // Apagar LEDs después de 3 segundos
    delay(3000);
    controlLEDs(false, false);
  }
}

void loop() {
  // Simular reconocimiento facial (para pruebas)
  simulateFaceRecognition();
  
  delay(1000);
}