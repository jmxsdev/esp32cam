from flask import Flask, request, jsonify
import cv2
import numpy as np
import os
from datetime import datetime
import base64
import requests

def send_face_command(face_type, esp32_ip):
    try:
        url = f"http://{esp32_ip}/face_command?type={face_type}"
        response = requests.get(url, timeout=5)
        return response.status_code == 200
    except:
        return False

# Llamar esta función cuando detectes un rostro
send_face_command("known", "192.168.1.100")  # Ejemplo para rostro conocido
app = Flask(__name__)

# Cargar clasificadores Haar Cascade para detección facial
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# Base de datos simple de rostros "conocidos" (en producción usarías face_recognition o similar)
known_faces = {
    "user1": "Descripción de características faciales del usuario 1",
    "user2": "Descripción de características faciales del usuario 2"
}

def detect_faces(image_data):
    """Detecta rostros en la imagen y determina si son conocidos"""
    try:
        # Convertir bytes a numpy array
        nparr = np.frombuffer(image_data, np.uint8)
        img = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        
        if img is None:
            return {"error": "No se pudo decodificar la imagen"}
        
        # Convertir a escala de grises para detección
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        
        # Detectar rostros
        faces = face_cascade.detectMultiScale(
            gray, 
            scaleFactor=1.1, 
            minNeighbors=5, 
            minSize=(30, 30)
        )
        
        print(f"🔍 Rostros detectados: {len(faces)}")
        
        if len(faces) == 0:
            return {"face_type": "no_face", "faces_detected": 0}
        
        # Aquí iría el reconocimiento facial real
        # Por ahora, simulamos reconocimiento basado en características simples
        
        # SIMULACIÓN: 70% conocido, 30% desconocido
        is_known = np.random.random() > 0.3
        confidence = np.random.uniform(0.7, 0.95) if is_known else np.random.uniform(0.3, 0.6)
        
        face_type = "known" if is_known else "unknown"
        
        result = {
            "status": "success",
            "face_type": face_type,
            "confidence": round(confidence, 2),
            "faces_detected": len(faces),
            "timestamp": datetime.now().isoformat()
        }
        
        # Log en consola
        if face_type == "known":
            print(f"✅ [{datetime.now().strftime('%H:%M:%S')}] ROSTRO CONOCIDO - Confianza: {confidence:.2f}")
        else:
            print(f"🚨 [{datetime.now().strftime('%H:%M:%S')}] ROSTRO DESCONOCIDO - Confianza: {confidence:.2f}")
            
        return result
        
    except Exception as e:
        print(f"❌ Error en detección facial: {str(e)}")
        return {"error": str(e)}

@app.route('/recognize', methods=['POST'])
def recognize_face():
    """Endpoint principal para reconocimiento facial"""
    try:
        if not request.data:
            return jsonify({"error": "No image data"}), 400
        
        # Procesar imagen
        result = detect_faces(request.data)
        
        if "error" in result:
            return jsonify(result), 500
            
        return jsonify(result), 200
        
    except Exception as e:
        print(f"❌ Error en endpoint: {str(e)}")
        return jsonify({"error": "Internal server error"}), 500

@app.route('/admin/add_face', methods=['POST'])
def add_face():
    """Endpoint para agregar nuevos rostros a la base de datos"""
    try:
        data = request.get_json()
        face_name = data.get('name')
        face_data = data.get('data')  # En producción, aquí irían las características faciales
        
        if not face_name:
            return jsonify({"error": "Name required"}), 400
            
        known_faces[face_name] = face_data or f"Face data for {face_name}"
        
        print(f"✅ Rostro agregado: {face_name}")
        return jsonify({"status": "success", "message": f"Face {face_name} added"}), 200
        
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route('/admin/known_faces', methods=['GET'])
def get_known_faces():
    """Endpoint para listar rostros conocidos"""
    return jsonify({
        "status": "success",
        "known_faces": list(known_faces.keys()),
        "count": len(known_faces)
    }), 200

@app.route('/status', methods=['GET'])
def status():
    return jsonify({
        "status": "running",
        "service": "Facial Recognition API",
        "timestamp": datetime.now().isoformat()
    }), 200

if __name__ == '__main__':
    print("🚀 Iniciando API de Reconocimiento Facial...")
    print("📡 Endpoints disponibles:")
    print("   POST /recognize     - Reconocimiento facial")
    print("   POST /admin/add_face - Agregar nuevo rostro")
    print("   GET  /admin/known_faces - Listar rostros conocidos")
    print("   GET  /status        - Estado del servicio")
    print("\n🔍 Esperando imágenes del ESP32-CAM...")
    
    app.run(host='0.0.0.0', port=5000, debug=True)