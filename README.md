# Sistema de Control de Acceso con Reconocimiento Facial (ESP32-CAM + Python)

## 1. Descripción General

Este proyecto es un prototipo de un sistema de control de acceso que utiliza una cámara ESP32-CAM para capturar y transmitir video a una API de backend desarrollada en Python con Flask. La API procesa las imágenes para detectar rostros y determinar si una persona es "conocida" o "desconocida", enviando una señal de vuelta al microcontrolador.

El objetivo es demostrar cómo se puede construir un sistema de validación biométrica simple y escalable. Como prueba de concepto, el ESP32-CAM enciende un LED verde para un rostro conocido y un LED rojo para uno desconocido.

### Audiencia
Esta documentación está dirigida a desarrolladores con conocimientos básicos en:
- Python (nivel junior).
- Lenguaje C (para entender el código de Arduino/ESP32).
- Fundamentos de electrónica analógica y digital.

## 2. Flujo de Funcionamiento

El sistema opera siguiendo estos pasos:

1.  **Captura de Imagen**: El ESP32-CAM se conecta a la red WiFi, captura una imagen en formato JPEG cada 5 segundos.
2.  **Solicitud de Verificación (ESP32 -> API)**: El ESP32-CAM envía la imagen capturada a través de una solicitud `HTTP POST` al endpoint `/recognize` de la API de Python.
3.  **Procesamiento en la API**:
    - La API recibe la imagen.
    - Utiliza la librería OpenCV para detectar si hay rostros en la imagen.
    - **Simula el reconocimiento facial**: De forma aleatoria, determina si el rostro es "conocido" o "desconocido".
4.  **Respuesta de Confirmación (API -> ESP32)**: La API envía el resultado (`known`, `unknown` o `no_face`) de vuelta al ESP32-CAM a través de una solicitud `HTTP GET` a un endpoint específico que corre en el propio microcontrolador.
5.  **Acción en el Hardware**: El ESP32-CAM recibe la respuesta y activa la salida correspondiente:
    - **Rostro Conocido**: Enciende un LED verde.
    - **Rostro Desconocido**: Enciende un LED rojo.
    - **Sin Rostro**: Mantiene los LEDs apagados.

## 3. Análisis del Código

A continuación se detallan las partes más importantes del código.

### API en Python (`main.py`)

Este archivo contiene la lógica del servidor que recibe y procesa las imágenes.

- **Endpoint de Reconocimiento**: `POST /recognize`
  - Es la ruta principal que recibe los datos de la imagen del ESP32-CAM.
  - Llama a la función `detect_faces()` para realizar el análisis.

- **Función de Detección**: `detect_faces(image_data)`
  - **Detección de Rostros**: Utiliza un clasificador pre-entrenado de OpenCV (`haarcascade_frontalface_default.xml`) para encontrar las coordenadas de los rostros en la imagen.
  - **Simulación de Reconocimiento**: Es **importante** notar que el reconocimiento real no está implementado. La línea `is_known = np.random.random() > 0.3` simula el resultado, asignando aleatoriamente si un rostro es conocido o no. En un sistema real, aquí es donde se implementaría un algoritmo para comparar características faciales contra una base de datos.

- **Función de Comunicación con ESP32**: `send_face_command(face_type, esp32_ip)`
  - Esta función es la encargada de enviar el resultado del reconocimiento de vuelta al ESP32.
  - Construye una URL como `http://{IP_DEL_ESP32}/face_command?type={resultado}` y realiza una solicitud `GET`.

### Firmware del ESP32-CAM (`esp32cam/`)

El código del microcontrolador está dividido principalmente en dos archivos.

- **Lógica Principal (`WebCamServer.ino`)**:
  - **Configuración**: Aquí se definen las credenciales WiFi (`ssid`, `password`), la IP de la API de Python (`python_api_url`) y los pines de los LEDs (`LED_KNOWN`, `LED_UNKNOWN`).
  - **Envío de Imágenes (`captureAndSendFrame()`)**: Esta función se ejecuta en el `loop()` principal cada 5 segundos. Realiza una solicitud `HTTP POST` a la API de Python, enviando la imagen JPEG en el cuerpo de la solicitud.

- **Servidor HTTP y Control de Salidas (`app_httpd.cpp`)**:
  - **Recepción de Comandos**: Este archivo configura un servidor web en el propio ESP32. La función clave es `face_command_handler`, que está asociada a la ruta `/face_command`.
  - **Control de LEDs**: `face_command_handler` lee el parámetro `type` de la URL (ej. `?type=known`). Dependiendo del valor, utiliza `gpio_set_level()` para encender o apagar los LEDs conectados a los **GPIO 12 (verde)** y **GPIO 13 (rojo)**. Aquí es donde la "señal de respuesta" se materializa en una acción física.

## 4. Endpoints y Rutas URL

A continuación se listan las rutas más relevantes del proyecto.

### API de Python (`main.py`)

| Método | Ruta                  | Descripción                                               |
|--------|-----------------------|-----------------------------------------------------------|
| `POST` | `/recognize`          | Recibe una imagen y la procesa para el reconocimiento.     |
| `GET`  | `/status`             | Devuelve el estado de funcionamiento de la API.           |
| `POST` | `/admin/add_face`     | (Simulado) Endpoint para agregar un nuevo rostro conocido. |
| `GET`  | `/admin/known_faces`  | (Simulado) Lista los rostros actualmente conocidos.       |

### Servidor Web del ESP32-CAM

| Método | Ruta             | Descripción                                                                 |
|--------|------------------|-----------------------------------------------------------------------------|
| `GET`  | `/`              | Muestra una página web con el streaming de video y controles de la cámara.  |
| `GET`  | `/capture`       | Captura y devuelve una única imagen estática en formato JPG.                |
| `GET`  | `/stream`        | Inicia un streaming de video en formato MJPEG.                              |
| `GET`  | `/status`        | Devuelve un JSON con el estado y configuración actual de la cámara.         |
| `GET`  | `/control`       | Permite cambiar parámetros de la cámara (ej. `?var=framesize&val=8`).       |
| `GET`  | `/face_command`  | **Ruta clave**: Recibe el comando desde la API de Python (ej. `?type=known`). |

## 5. Configuración y Puesta en Marcha

### Hardware
1.  Un módulo ESP32-CAM.
2.  Un LED verde conectado al `GPIO 12`.
3.  Un LED rojo conectado al `GPIO 13`.
4.  Un programador FTDI para cargar el código al ESP32-CAM.

### Software
1.  **API de Python**:
    - Se recomienda crear un entorno virtual.
    - Instala las dependencias: `pip install Flask opencv-python numpy requests`.
    - Ejecuta la API con: `python main.py`.
2.  **Firmware ESP32-CAM**:
    - Abre el proyecto en el IDE de Arduino o PlatformIO.
    - En `WebCamServer.ino`, modifica las credenciales WiFi y la variable `python_api_url` para que apunte a la IP de la máquina donde se ejecuta la API de Python.
    - Carga el código al ESP32-CAM.
3.  **Verificación**:
    - Abre el Monitor Serie en el IDE de Arduino para ver los logs del ESP32-CAM. Deberías ver cómo se conecta al WiFi y empieza a enviar imágenes.
    - En la terminal donde ejecutas la API, verás los logs de las imágenes recibidas y los rostros detectados.

## 5. Escalabilidad y Pasos Futuros

Este proyecto es una base que puede escalarse fácilmente a un sistema de control de acceso real:

- **Control de Cerraduras**: La lógica en `face_command_handler` que enciende los LEDs puede ser reemplazada para activar un relé o un servomotor que controle una cerradura electromagnética.
- **Base de Datos Real**: La simulación de reconocimiento en `main.py` puede sustituirse por una librería como `face_recognition` o `DeepFace` para comparar los rostros detectados con una base de datos de usuarios autorizados (ej. almacenada en SQL, MongoDB, etc.).
- **Integración Contable**: La API podría consultar otra base de datos para verificar el estado de cuenta de un cliente o la membresía de un usuario, permitiendo o denegando el acceso según si su pago está al día.

Este enfoque modular (hardware -> API -> base de datos) permite que cada parte del sistema evolucione de forma independiente.
