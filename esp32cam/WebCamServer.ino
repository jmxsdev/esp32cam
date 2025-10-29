#include "esp_http_server.h"
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
#define LED_FLASH 4       // GPIO4 - Flash de la cámara

// ===========================
// API Configuration
// ===========================
const char* python_api_url = "http://172.16.0.199:5000/recognize"; // Cambia por IP de tu PC

// ===========================
// Face Recognition Variables
// ===========================
bool faceRecognitionEnabled = true;
int facesDetectedCount = 0;
int knownFacesCount = 0;
int unknownFacesCount = 0;

// Variables para control de timing
unsigned long lastCaptureTime = 0;
const unsigned long CAPTURE_INTERVAL = 1000; // 1 segundo entre capturas

// ===========================
// Declaraciones de funciones
// ===========================
void startCameraServer();
void setupLedFlash();
void blinkFlash(int times, int delayMs);
void controlLEDs(bool known, bool unknown);
void captureAndSendFrame();
String getFaceRecognitionStats();

// ===========================
// Función para hacer parpadear el flash
// ===========================
void blinkFlash(int times, int delayMs) {
  pinMode(LED_FLASH, OUTPUT);
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_FLASH, HIGH);
    delay(delayMs);
    digitalWrite(LED_FLASH, LOW);
    if (i < times - 1) {
      delay(delayMs);
    }
  }
}

// Helper function to parse GET requests, copied from app_httpd.cpp
static esp_err_t parse_get(httpd_req_t *req, char **obuf) {
  char *buf = NULL;
  size_t buf_len = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      *obuf = buf;
      return ESP_OK;
    }
    free(buf);
  }
  httpd_resp_send_404(req);
  return ESP_FAIL;
}

// ===========================
// Endpoint para recibir comandos de la API Python
// ===========================
extern "C" esp_err_t face_command_handler(httpd_req_t *req) {
  char* buf = NULL;
  
  if (parse_get(req, &buf) != ESP_OK) {
    return ESP_FAIL;
  }
  
  char command[32];
  if (httpd_query_key_value(buf, "type", command, sizeof(command)) == ESP_OK) {
    Serial.printf("🎯 Comando recibido desde Python: %s\n", command);
    
    if (strcmp(command, "known") == 0) {
      Serial.println("✅ ROSTRO CONOCIDO detectado");
      controlLEDs(true, false);
      knownFacesCount++;
      
      // Parpadear flash 5 veces
      Serial.println("💡 Flash parpadeando por rostro conocido");
      blinkFlash(5, 200);
      
    } else if (strcmp(command, "unknown") == 0) {
      Serial.println("🚨 ROSTRO DESCONOCIDO detectado");
      controlLEDs(false, true);
      unknownFacesCount++;
      
    } else if (strcmp(command, "no_face") == 0) {
      Serial.println("👀 No se detectaron rostros");
      controlLEDs(false, false);
    }
    
    facesDetectedCount++;
  }
  
  free(buf);
  
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, "OK", 2);
}
// Función para controlar LEDs
void controlLEDs(bool known, bool unknown) {
  digitalWrite(LED_KNOWN, known);
  digitalWrite(LED_UNKNOWN, unknown);
  Serial.printf("💡 LEDs - Conocido: %d, Desconocido: %d\n", known, unknown);
}

// Función para manejar comandos de la API Python
void handleFaceRecognitionCommand(const char* command) {
  Serial.printf("🎯 Comando recibido: %s\n", command);
  
  if (strcmp(command, "known") == 0) {
    Serial.println("✅ ROSTRO CONOCIDO detectado");
    controlLEDs(true, false); // Verde ON, Rojo OFF
    knownFacesCount++;
    
    // Parpadear flash 5 veces cuando detecta rostro conocido
    Serial.println("💡 Flash parpadeando por rostro conocido");
    blinkFlash(5, 200); // 5 veces, 200ms entre parpadeos
  } 
  else if (strcmp(command, "unknown") == 0) {
    Serial.println("🚨 ROSTRO DESCONOCIDO - INTRUSO");
    controlLEDs(false, true); // Verde OFF, Rojo ON
    unknownFacesCount++;
  }
  else if (strcmp(command, "no_face") == 0) {
    Serial.println("👀 No se detectaron rostros");
    controlLEDs(false, false); // Ambos LEDs OFF
  }
  
  facesDetectedCount++;
}

// Función para capturar y enviar frame a API Python
void captureAndSendFrame() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi no conectado");
    return;
  }
  
  // Capturar frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Error capturando frame");
    return;
  }
  
  // Verificar que sea formato JPEG
  if (fb->format != PIXFORMAT_JPEG) {
    Serial.println("❌ Formato no JPEG");
    esp_camera_fb_return(fb);
    return;
  }
  
  Serial.printf("📸 Enviando frame (%d bytes) a API Python...\n", fb->len);
  
  // Enviar a API Python para reconocimiento
  HTTPClient http;
  http.begin(python_api_url);
  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(10000); // 10 segundos timeout
  
  // Enviar imagen JPEG
  int httpCode = http.POST(fb->buf, fb->len);
  
  if (httpCode > 0) {
    String response = http.getString();
    Serial.printf("✅ API Response: %d - %s\n", httpCode, response.c_str());
    
    // Procesar respuesta de la API Python
    if (response == "known" || response == "unknown" || response == "no_face") {
      handleFaceRecognitionCommand(response.c_str());
    }
  } else {
    Serial.printf("❌ API Error: %s\n", http.errorToString(httpCode).c_str());
  }
  
  http.end();
  esp_camera_fb_return(fb);
}

// ===========================
// Función para obtener estadísticas (útil para el dashboard)
// ===========================
String getFaceRecognitionStats() {
  String stats = "{";
  stats += "\"faces_detected\": " + String(facesDetectedCount) + ",";
  stats += "\"known_faces\": " + String(knownFacesCount) + ",";
  stats += "\"unknown_faces\": " + String(unknownFacesCount) + ",";
  stats += "\"face_recognition_enabled\": " + String(faceRecognitionEnabled ? "true" : "false");
  stats += "}";
  return stats;
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("🚀 Iniciando ESP32-CAM con Sistema de Reconocimiento Facial...");

  // ===========================
  // INICIALIZAR LEDs
  // ===========================
  pinMode(LED_KNOWN, OUTPUT);
  pinMode(LED_UNKNOWN, OUTPUT);
  pinMode(LED_FLASH, OUTPUT);
  controlLEDs(false, false); // Apagar ambos al inicio
  
  // Parpadeo de flash 5 veces al inicio
  Serial.println("💡 Flash parpadeando 5 veces al inicio...");
  blinkFlash(5, 300); // 5 veces, 300ms entre parpadeos
  
  // Parpadeo de LEDs para indicar inicio
  Serial.println("💡 Probando LEDs con parpadeo inicial...");
  for (int i = 0; i < 3; i++) {
    controlLEDs(true, true); // Ambos ON
    delay(250);
    controlLEDs(false, false); // Ambos OFF
    delay(250);
  }

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
  config.frame_size = FRAMESIZE_SVGA; // Balance calidad-rendimiento
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 14;
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
  
  // Configurar resolución para streaming
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_VGA);
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
  // Iniciar Servidor Web
  // ===========================
  startCameraServer();
  Serial.println("🚀 Servidor web iniciado");

  Serial.println("🎯 Sistema listo - Use la IP mostrada arriba para acceder al dashboard");
  Serial.println("📡 ESP32-CAM funcionando en modo streaming + API");
}

void loop() {
  // Capturar y enviar frames cada CAPTURE_INTERVAL si hay WiFi
  if (WiFi.status() == WL_CONNECTED && faceRecognitionEnabled) {
    if (millis() - lastCaptureTime >= CAPTURE_INTERVAL) {
      lastCaptureTime = millis();
      captureAndSendFrame();
    }
  }
  
  delay(1000);
}