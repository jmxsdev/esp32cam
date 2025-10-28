#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>  // Para peticiones HTTP
#include "fb_gfx.h"
#include "fd_forward.h"

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
const char* python_api_url = "http://192.168.1.100:5000/recognize"; // Cambia por IP de tu PC
const char* log_api_url = "http://192.168.1.100:5000/log"; // API para logging

// ===========================
// Face Recognition Configuration
// ===========================
static mtmn_config_t mtmn_config = {0};
bool faceRecognitionEnabled = true;
int facesDetectedCount = 0;
int knownFacesCount = 0;
int unknownFacesCount = 0;

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

// Función para enviar a API Python (logging)
void sendToAPI(const char* face_type) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(log_api_url);
    http.addHeader("Content-Type", "application/json");
    
    String jsonPayload = "{\"face_type\":\"" + String(face_type) + "\",\"device\":\"ESP32-CAM\"}";
    int httpCode = http.POST(jsonPayload);
    
    if (httpCode > 0) {
      Serial.printf("✅ Log API: %s - Código: %d\n", face_type, httpCode);
    } else {
      Serial.printf("❌ Error Log API: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
}

// ===========================
// Face Recognition Functions
// ===========================

// Función para inicializar detección facial
void setupFaceDetection() {
    mtmn_config = mtmn_init_config();
    Serial.println("✅ Detección facial inicializada");
}

// Función principal de reconocimiento facial
void processFaceRecognition() {
    static unsigned long lastProcess = 0;
    
    // Procesar cada 5 segundos para no saturar
    if (millis() - lastProcess < 5000 || !faceRecognitionEnabled) {
        return;
    }
    lastProcess = millis();
    
    // Capturar frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("❌ Error capturando frame");
        return;
    }
    
    // Solo procesar si es formato JPEG
    if (fb->format != PIXFORMAT_JPEG) {
        esp_camera_fb_return(fb);
        return;
    }
    
    Serial.println("📸 Enviando frame a API Python para reconocimiento...");
    
    // Enviar a API Python para reconocimiento
    sendFrameToAPI(fb);
    
    esp_camera_fb_return(fb);
}

// Función para enviar frame a API Python
void sendFrameToAPI(camera_fb_t *fb) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi no conectado");
        return;
    }
    
    HTTPClient http;
    http.begin(python_api_url);
    http.addHeader("Content-Type", "image/jpeg");
    http.setTimeout(10000); // 10 segundos timeout
    
    // Enviar imagen JPEG
    int httpCode = http.POST(fb->buf, fb->len);
    
    if (httpCode > 0) {
        String response = http.getString();
        Serial.printf("✅ API Response: %d - %s\n", httpCode, response.c_str());
        
        // Procesar respuesta
        processAPIResponse(response);
    } else {
        Serial.printf("❌ API Error: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}

// Procesar respuesta de la API
void processAPIResponse(String response) {
    // Respuesta esperada: {"status":"success","face_type":"known","confidence":0.85}
    if (response.indexOf("\"face_type\":\"known\"") > 0 || response.indexOf("'face_type': 'known'") > 0) {
        Serial.println("✅ ROSTRO CONOCIDO detectado");
        controlLEDs(true, false);
        knownFacesCount++;
        sendToAPI("known"); // Enviar a tu API de logging
        
        // Mantener LED encendido por 3 segundos
        delay(3000);
        controlLEDs(false, false);
    } 
    else if (response.indexOf("\"face_type\":\"unknown\"") > 0 || response.indexOf("'face_type': 'unknown'") > 0) {
        Serial.println("🚨 ROSTRO DESCONOCIDO - INTRUSO");
        controlLEDs(false, true);
        unknownFacesCount++;
        sendToAPI("unknown"); // Enviar a tu API de logging
        
        // Mantener LED encendido por 3 segundos
        delay(3000);
        controlLEDs(false, false);
    }
    else if (response.indexOf("\"face_type\":\"no_face\"") > 0) {
        Serial.println("👀 No se detectaron rostros");
        // No hacer nada, LEDs permanecen apagados
    }
    else {
        Serial.println("❌ Respuesta de API no reconocida");
    }
    
    facesDetectedCount++;
}

// ===========================
// SIMULACIÓN de Reconocimiento Facial (para pruebas sin API)
// ===========================
void simulateFaceRecognition() {
  static unsigned long lastDetection = 0;
  static bool knownFace = true;
  
  // Simular detección cada 15 segundos
  if (millis() - lastDetection > 15000) {
    lastDetection = millis();
    
    if (knownFace) {
      Serial.println("✅ [SIMULACIÓN] ROSTRO CONOCIDO DETECTADO");
      controlLEDs(true, false);  // Verde ON, Rojo OFF
      sendToAPI("known");
    } else {
      Serial.println("🚨 [SIMULACIÓN] ROSTRO DESCONOCIDO - INTRUSO");
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

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("🚀 Iniciando ESP32-CAM con Reconocimiento Facial REAL...");

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
  // Configuración de Cámara
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
  config.frame_size = FRAMESIZE_SVGA; // Reducido para mejor rendimiento
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
  
  // Configurar para mejor rendimiento en reconocimiento
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_VGA); // Balance entre calidad y rendimiento
  }

#if defined(LED_GPIO_NUM)
  setupLedFlash();
#endif

  // ===========================
  // Conectar WiFi
  // ===========================
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  
  Serial.print("📡 Conectando WiFi");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi conectado");
    Serial.print("🌐 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Error WiFi - Continuando sin conexión");
  }

  // ===========================
  // Inicializar Reconocimiento Facial
  // ===========================
  setupFaceDetection();

  // ===========================
  // Iniciar Servidor Web
  // ===========================
  startCameraServer();
  Serial.println("🚀 Servidor web iniciado");

  Serial.println("🎯 Sistema listo - Use la IP mostrada arriba para acceder al dashboard");
  Serial.println("🔍 Reconocimiento facial ACTIVADO - Enviando imágenes a API Python cada 5 segundos");
}

void loop() {
  // Procesar reconocimiento facial REAL si hay conexión WiFi
  if (WiFi.status() == WL_CONNECTED) {
    processFaceRecognition();
  } else {
    // Si no hay WiFi, usar simulación
    simulateFaceRecognition();
  }
  
  delay(1000);
}

// ===========================
// Función para obtener estadísticas (útil para el dashboard)
// ===========================
String getFaceRecognitionStats() {
  String stats = "{";
  stats += "\"faces_detected\":" + String(facesDetectedCount) + ",";
  stats += "\"known_faces\":" + String(knownFacesCount) + ",";
  stats += "\"unknown_faces\":" + String(unknownFacesCount) + ",";
  stats += "\"recognition_enabled\":" + String(faceRecognitionEnabled);
  stats += "}";
  return stats;
}