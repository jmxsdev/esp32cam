# Sistema de Control de Acceso con Reconocimiento Facial (ESP32-CAM + Python)

## 1. Descripción General

Este proyecto es un prototipo de un sistema de control de acceso que utiliza una cámara ESP32-CAM para capturar y transmitir imágenes a una API de backend desarrollada en Python. La API procesa las imágenes para detectar rostros y determinar si una persona es "conocida" o "desconocida", enviando una señal de vuelta al microcontrolador.

El objetivo es demostrar cómo se puede construir un sistema de validación biométrica simple y escalable. Como prueba de concepto, el ESP32-CAM realiza las siguientes acciones:
- **Rostro Conocido**: Enciende un LED verde y hace parpadear el flash de la cámara 5 veces.
- **Rostro Desconocido**: Enciende un LED rojo.
- **Inicio del Sistema**: Al arrancar, el flash parpadea 5 veces para indicar que el sistema se ha iniciado correctamente.

### Audiencia
Esta documentación está dirigida a desarrolladores con conocimientos básicos en:
- Python (nivel junior).
- Lenguaje C (para entender el código de Arduino/ESP32).
- Fundamentos de electrónica y conexión en protoboard.

## 2. Flujo de Funcionamiento

El sistema opera siguiendo estos pasos:

1.  **Captura de Imagen**: El ESP32-CAM se conecta a la red WiFi y captura una imagen periódicamente.
2.  **Solicitud de Verificación (ESP32 -> API)**: El ESP32-CAM envía la imagen capturada a través de una solicitud `HTTP POST` al endpoint `/recognize` de la API de Python.
3.  **Procesamiento en la API**:
    - La API recibe la imagen.
    - Utiliza la librería `face_recognition` para detectar rostros y compararlos con una base de datos de rostros conocidos (`known_faces_encodings.json`).
4.  **Respuesta de Confirmación (API -> ESP32)**: La API envía el resultado (`known`, `unknown` o `no_face`) de vuelta al ESP32-CAM.
5.  **Acción en el Hardware**: El ESP32-CAM recibe la respuesta y activa la salida correspondiente:
    - **Rostro Conocido**: Enciende un LED verde y activa 5 parpadeos del flash.
    - **Rostro Desconocido**: Enciende un LED rojo.
    - **Sin Rostro**: Mantiene los LEDs apagados.

## 3. Configuración y Puesta en Marcha

### Hardware y Conexiones

**Materiales Necesarios:**
- 1x Módulo ESP32-CAM con cámara.
- 1x Programador FTDI (para cargar el código).
- 1x LED color verde.
- 1x LED color rojo.
- 2x Resistencia de 330 Ohm (un valor cercano como 320 Ohm también funciona).
- 1x Protoboard.
- Cables de conexión (jumpers).

**Instrucciones de Conexión en Protoboard:**

Sigue estos pasos para conectar los componentes correctamente:

1.  **Alimentación**: Conecta los pines `5V` y `GND` de la ESP32-CAM a los rieles de alimentación de la protoboard.
2.  **Modo Flash/Programación**: Para cargar el código, es necesario conectar el pin `GPIO 0` a `GND`. Recuerda desconectarlo para la operación normal.
3.  **Conexión de LEDs**:
    - **Tierra Común (GND)**: Conecta un pin `GND` de la ESP32-CAM al riel negativo (azul) de la protoboard.
    - **LED Verde (Rostro Conocido)**:
        - Conecta el pin `GPIO 12` de la ESP32-CAM a una pata de una resistencia de 330Ω.
        - Conecta la otra pata de la resistencia a la pata larga (ánodo) del LED verde.
        - Conecta la pata corta (cátodo) del LED verde al riel de tierra (GND).
    - **LED Rojo (Rostro Desconocido)**:
        - Conecta el pin `GPIO 13` de la ESP32-CAM a una pata de la otra resistencia de 330Ω.
        - Conecta la otra pata de la resistencia a la pata larga (ánodo) del LED rojo.
        - Conecta la pata corta (cátodo) del LED rojo al riel de tierra (GND).

**Diagrama ASCII de Conexión de LEDs:**

```
           +------------------+
           |    ESP32-CAM     |
           +------------------+
                  |      |
 (GND) -----------+      +----------- (GPIO 12)
   |                     |
   |               [Resistencia 330Ω]
   |                     |
   |                (+)--|>|--(-) (LED Verde)
   |                     |
   |                     +------------------+
   |                                        |
   +----------------------------------------+ (Riel GND de la Protoboard)


           +------------------+
           |    ESP32-CAM     |
           +------------------+
                  |      |
 (GND) -----------+      +----------- (GPIO 13)
   |                     |
   |               [Resistencia 330Ω]
   |                     |
   |                 (+)--|>|--(-) (LED Rojo)
   |                      |
   |                      +-----------------+
   |                                        |
   +----------------------------------------+ (Riel GND de la Protoboard)
```
*(Nota: El diagrama muestra las conexiones por separado para mayor claridad. Ambos LEDs comparten el mismo riel de tierra.)*

### Software

1.  **API de Python (`main.py`)**:
    - Se recomienda crear un entorno virtual: `python -m venv .venv` y activarlo.
    - Instala las dependencias: `pip install Flask face_recognition numpy opencv-python`.
    - Para registrar un rostro conocido, coloca una imagen llamada `nombre.jpg` en la raíz del proyecto y ejecuta `python main.py --add-face nombre.jpg`. Esto actualizará el archivo `known_faces_encodings.json`.
    - Ejecuta la API con: `python main.py`.

2.  **Firmware ESP32-CAM (`esp32cam/WebCamServer.ino`)**:
    - Abre el proyecto en el IDE de Arduino.
    - Asegúrate de tener el soporte para placas ESP32 instalado.
    - Selecciona la placa "AI Thinker ESP32-CAM".
    - En `WebCamServer.ino`, modifica las credenciales WiFi (`ssid`, `password`).
    - Actualiza la variable `python_api_url` para que apunte a la dirección IP de la máquina donde se ejecuta la API de Python.
    - Carga el código a la ESP32-CAM usando el programador FTDI.

3.  **Verificación**:
    - Abre el Monitor Serie en el IDE de Arduino (baud rate a 115200) para ver los logs del ESP32-CAM. Deberías ver cómo se conecta al WiFi y empieza a enviar imágenes.
    - En la terminal donde ejecutas la API, verás los logs de las imágenes recibidas y los resultados del reconocimiento.

## 4. Análisis del Código

### API en Python (`main.py`)

- **Endpoint de Reconocimiento (`POST /recognize`)**: Recibe la imagen del ESP32-CAM, la decodifica y utiliza `face_recognition` para encontrar rostros y compararlos con los encodings guardados.
- **Endpoint de Administración (`GET /admin`)**: Muestra una página simple para ver los rostros conocidos y añadir nuevos.
- **Lógica de Reconocimiento**: La comparación se realiza calculando la distancia euclidiana entre el encoding del rostro detectado y los encodings de los rostros conocidos.

### Firmware del ESP32-CAM (`esp32cam/WebCamServer.ino`)

- **Lógica Principal**: En el `loop()`, la función `captureAndSendFrame()` se encarga de tomar una foto y enviarla a la API de Python.
- **Recepción de Comandos**: La función `face_command_handler` (asociada a la ruta `/face_command`) se activa por una petición de la API y es la responsable de accionar los LEDs y el flash según el resultado.

## 5. Escalabilidad y Pasos Futuros

Este proyecto es una base que puede escalarse a un sistema de control de acceso real:

- **Control de Cerraduras**: La lógica que enciende los LEDs puede ser reemplazada para activar un relé o un servomotor que controle una cerradura.
- **Base de Datos Robusta**: Los rostros conocidos pueden almacenarse en una base de datos más completa (SQL, MongoDB) en lugar de un archivo JSON.
- **Mejoras en la Interfaz**: El frontend de administración puede mejorarse para gestionar usuarios, ver registros de acceso y más.