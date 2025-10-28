from flask import Flask, request, jsonify
from datetime import datetime

app = Flask(__name__)

@app.route('/recognize', methods=['POST'])
def recognize_face():
    try:
        data = request.get_json()
        face_type = data.get('face_type', 'unknown')
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        if face_type == 'known':
            print(f"🟢 [{timestamp}] ROSTRO CONOCIDO detectado - Acceso permitido")
        else:
            print(f"🔴 [{timestamp}] ROSTRO DESCONOCIDO - INTRUSO detectado!")
        
        return jsonify({
            "status": "success", 
            "message": f"Face type: {face_type}",
            "timestamp": timestamp
        }), 200
        
    except Exception as e:
        print(f"❌ Error en API: {e}")
        return jsonify({"status": "error"}), 500

@app.route('/status', methods=['GET'])
def status():
    return jsonify({"status": "API funcionando"}), 200

if __name__ == '__main__':
    print("🚀 Iniciando API de Reconocimiento Facial...")
    print("📡 Esperando notificaciones del ESP32-CAM...")
    app.run(host='0.0.0.0', port=5000, debug=True)