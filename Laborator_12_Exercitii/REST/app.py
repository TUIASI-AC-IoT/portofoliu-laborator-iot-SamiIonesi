from flask import Flask, jsonify, send_file, request
import os
import random
from datetime import datetime

app = Flask(__name__)

SENSOR_DIR = 'sensors'
os.makedirs(SENSOR_DIR, exist_ok=True)

KNOWN_SENSOR_RANGES = {
    'temperature': (15.0, 30.0),
    'humidity': (30.0, 90.0),
    'pressure': (980.0, 1050.0)
}

def simulate_sensor_value(sensor_type):
    if sensor_type in KNOWN_SENSOR_RANGES:
        low, high = KNOWN_SENSOR_RANGES[sensor_type]
        return round(random.uniform(low, high), 2)
    else:
        return round(random.uniform(0, 100), 2)

@app.route("/sensors/<sensor_id>/value", methods=['GET'])
def get_sensor_value(sensor_id):
    config_path = os.path.join(SENSOR_DIR, f"{sensor_id}.cfg")

    if not os.path.exists(config_path):
        return jsonify({'error': f'Config file for sensor {sensor_id} not found'}), 404

    sensor_type = None
    with open(config_path, 'r') as f:
        for line in f:
            if line.lower().startswith("type:"):
                sensor_type = line.split(":", 1)[1].strip()
                break

    if not sensor_type:
        return jsonify({'error': f'Type not found in config file for {sensor_id}'}), 400

    value = simulate_sensor_value(sensor_type)

    return jsonify({
        'sensor_id': sensor_id,
        'type': sensor_type,
        'value': value
    }), 200

@app.route("/sensors/<sensor_id>", methods=['POST'])
def create_sensor(sensor_id):
    config_path = os.path.join(SENSOR_DIR, f"{sensor_id}.cfg")

    if os.path.exists(config_path):
        return jsonify({
            'error': f'Configuration for sensor "{sensor_id}" already exists.',
            'hint': 'Use PUT to update or choose a different sensor ID.'
        }), 409

    sensor_type = request.json.get('type', 'temperature')
    timestamp = datetime.now().isoformat()

    with open(config_path, 'w') as f:
        f.write(f"# Config for sensor {sensor_id}\n")
        f.write(f"Type: {sensor_type}\n")
        f.write(f"Created: {timestamp}\n")

    return jsonify({
        'message': f'Sensor "{sensor_id}" created.',
        'sensor': {
            'id': sensor_id,
            'type': sensor_type,
            'created': timestamp
        }
    }), 201

@app.route("/sensors", methods=['GET'])
def list_sensors():
    try:
        files = [f for f in os.listdir(SENSOR_DIR) if f.endswith('.cfg')]
        sensor_ids = [os.path.splitext(f)[0] for f in files]
        return jsonify({'sensors': sensor_ids}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500
    
@app.route("/sensors/<sensor_id>", methods=['PUT'])
def update_sensor(sensor_id):
    config_path = os.path.join(SENSOR_DIR, f"{sensor_id}.cfg")

    if not os.path.exists(config_path):
        return jsonify({
            'error': f'Config file for sensor "{sensor_id}" not found. Crearea nu este permisă prin PUT.'
        }), 404

    sensor_type = request.json.get('type')
    if not sensor_type:
        return jsonify({'error': 'Tipul senzorului ("type") este necesar pentru actualizare.'}), 400

    from datetime import datetime
    timestamp = datetime.now().isoformat()

    with open(config_path, 'w') as f:
        f.write(f"# Config for sensor {sensor_id}\n")
        f.write(f"Type: {sensor_type}\n")
        f.write(f"Updated: {timestamp}\n")

    return jsonify({
        'message': f'Senzorul "{sensor_id}" a fost actualizat.',
        'sensor': {
            'id': sensor_id,
            'type': sensor_type,
            'updated': timestamp
        }
    }), 200

@app.route("/")
def homepage():
    return send_file("index.html")

if __name__ == "__main__":
    app.run(debug=True)

