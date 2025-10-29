from flask import Flask, request, jsonify, render_template, redirect, url_for
import cv2
import numpy as np
import os
from datetime import datetime
import requests
import face_recognition
import json

# --- CONFIGURACIÓN ---
KNOWN_FACES_FILE = "known_faces_encodings.json"
app = Flask(__name__)

# --- BASE DE DATOS LOCAL (EN MEMORIA) ---
known_face_encodings = []
known_face_names = []

# --- FUNCIONES DE GESTIÓN DE ROSTROS ---

def save_known_faces():
    """Guarda los encodings y nombres en el archivo JSON."""
    with open(KNOWN_FACES_FILE, "w") as f:
        data = {
            "names": known_face_names,
            "encodings": [enc.tolist() for enc in known_face_encodings]
        }
        json.dump(data, f)
    print("✅ Base de datos de rostros guardada.")

def load_known_faces():
    """Carga los rostros conocidos desde el archivo JSON al iniciar."""
    global known_face_encodings, known_face_names
    try:
        with open(KNOWN_FACES_FILE, "r") as f:
            data = json.load(f)
            known_face_names = data["names"]
            known_face_encodings = [np.array(enc) for enc in data["encodings"]]
            print(f"✅ {len(known_face_names)} rostros conocidos cargados desde la base de datos.")
    except FileNotFoundError:
        print("⚠️ No se encontró archivo de rostros conocidos. Se creará uno nuevo.")
        save_known_faces() # Crea un archivo vacío si no existe
    except Exception as e:
        print(f"❌ Error cargando la base de datos de rostros: {e}")

# --- LÓGICA DE RECONOCIMIENTO FACIAL ---

def recognize_and_detect_faces(image_data):
    """
    Detecta y reconoce rostros en una imagen.
    Devuelve la imagen con los rostros marcados y el resultado del reconocimiento.
    """
    try:
        nparr = np.frombuffer(image_data, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        if img is None:
            return None, {"error": "No se pudo decodificar la imagen"}

        # Convertir a RGB (face_recognition usa RGB)
        rgb_frame = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)

        # Encontrar todas las caras y sus encodings en el frame actual
        face_locations = face_recognition.face_locations(rgb_frame)
        face_encodings = face_recognition.face_encodings(rgb_frame, face_locations)

        result = {"face_type": "no_face", "faces_detected": 0, "name": "N/A"}

        if not face_encodings:
            # Mostrar imagen sin rostros
            cv2.imshow('ESP32-CAM Stream', img)
            cv2.waitKey(1)
            return img, result

        result["faces_detected"] = len(face_encodings)
        
        # Por defecto, asumimos que el rostro es desconocido
        result["face_type"] = "unknown" 

        for face_encoding, face_location in zip(face_encodings, face_locations):
            # Ver si la cara coincide con algún rostro conocido
            matches = face_recognition.compare_faces(known_face_encodings, face_encoding, tolerance=0.6)
            name = "Desconocido"

            # Encontrar la mejor coincidencia
            face_distances = face_recognition.face_distance(known_face_encodings, face_encoding)
            if len(face_distances) > 0:
                best_match_index = np.argmin(face_distances)
                if matches[best_match_index]:
                    name = known_face_names[best_match_index]
                    result["face_type"] = "known"
                    result["name"] = name

            # Dibujar rectángulo y nombre
            top, right, bottom, left = face_location
            color = (0, 255, 0) if name != "Desconocido" else (0, 0, 255)
            cv2.rectangle(img, (left, top), (right, bottom), color, 2)
            cv2.putText(img, name, (left + 6, bottom - 6), cv2.FONT_HERSHEY_DUPLEX, 0.8, (255, 255, 255), 1)
        
        print(f"🔍 Reconocimiento: {result}")
        
        # Mostrar la imagen en una ventana
        cv2.imshow('ESP32-CAM Stream', img)
        cv2.waitKey(1)

        return img, result

    except Exception as e:
        print(f"❌ Error en reconocimiento facial: {e}")
        return None, {"error": str(e)}

# --- COMUNICACIÓN CON ESP32-CAM ---

def send_face_command(face_type, esp32_ip):
    """Envía el resultado del reconocimiento al ESP32-CAM."""
    try:
        url = f"http://{esp32_ip}/face_command?type={face_type}"
        response = requests.get(url, timeout=2) # Timeout corto para no bloquear
        if response.status_code == 200:
            print(f"✅ Comando '{face_type}' enviado a {esp32_ip}")
            return True
        return False
    except requests.exceptions.RequestException as e:
        print(f"⚠️ No se pudo enviar el comando a {esp32_ip}: {e}")
        return False

# --- ENDPOINTS DE LA API ---

@app.route('/recognize', methods=['POST'])
def recognize_face_endpoint():
    """Endpoint principal para recibir imagen y realizar reconocimiento."""
    if not request.data:
        return jsonify({"error": "No image data"}), 400
    
    img, result = recognize_and_detect_faces(request.data)
    
    if "error" in result:
        return jsonify(result), 500

    # Reactivamos el envío de comandos al ESP32
    esp32_ip = request.remote_addr
    face_type = result.get("face_type", "no_face")
    send_face_command(face_type, esp32_ip)

    return jsonify(result), 200

# --- ENDPOINTS DEL PANEL DE ADMINISTRACIÓN ---

@app.route('/admin')
def admin_panel():
    """Muestra el panel de administración."""
    return render_template('admin.html', known_faces=known_face_names)

@app.route('/admin/add_face', methods=['POST'])
def add_face():
    """Añade un nuevo rostro a la base de datos desde el panel."""
    if 'face_image' not in request.files or 'name' not in request.form:
        return redirect(url_for('admin_panel'))
    
    file = request.files['face_image']
    name = request.form['name']

    if file and name:
        try:
            # Cargar la imagen y obtener el encoding
            image = face_recognition.load_image_file(file)
            encodings = face_recognition.face_encodings(image)

            if encodings:
                # Añadir a la base de datos en memoria
                known_face_encodings.append(encodings[0])
                known_face_names.append(name)
                # Guardar en el archivo JSON
                save_known_faces()
                print(f"✅ Rostro '{name}' agregado.")
            else:
                print(f"⚠️ No se encontró un rostro en la imagen para '{name}'.")

        except Exception as e:
            print(f"❌ Error procesando la imagen para agregar rostro: {e}")

    return redirect(url_for('admin_panel'))

@app.route('/admin/delete_face', methods=['POST'])
def delete_face():
    """Elimina un rostro de la base de datos."""
    data = request.get_json()
    name_to_delete = data.get('name')

    if name_to_delete in known_face_names:
        indices = [i for i, name in enumerate(known_face_names) if name == name_to_delete]
        # Eliminar de atrás hacia adelante para no afectar los índices
        for i in sorted(indices, reverse=True):
            del known_face_names[i]
            del known_face_encodings[i]
        
        save_known_faces()
        print(f"✅ Rostro '{name_to_delete}' eliminado.")
        return jsonify({"status": "success"})
    
    return jsonify({"status": "error", "message": "Face not found"}), 404


if __name__ == '__main__':
    load_known_faces()
    print("🚀 Iniciando API de Reconocimiento Facial...")
    print(" J. Gzuz")
    print("==============================================")
    print("📡 Endpoints disponibles:")
    print("   POST /recognize          - Endpoint para el ESP32-CAM")
    print("   GET  /admin             - Panel de administración web")
    print("   POST /admin/add_face    - Añadir nuevo rostro")
    print("   POST /admin/delete_face - Eliminar rostro")
    print("==============================================")
    
    app.run(host='0.0.0.0', port=5000, threaded=False)
