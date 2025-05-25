from flask import Flask, jsonify, request
from flask_cors import CORS
import os

app = Flask(__name__)
CORS(app)

FILES_DIR = 'files'

os.makedirs(FILES_DIR, exist_ok=True) # Ne asiguram ca directorul exista

@app.route('/files', methods=['GET'])
def list_files():
    try:
        files = os.listdir(FILES_DIR)
        return jsonify(files)
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/files/<filename>', methods=['GET'])
def get_file_content(filename):
    filepath = os.path.join(FILES_DIR, filename)

    if not os.path.exists(filepath):
        return jsonify({'error': 'File not found'}), 404

    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()
        return jsonify({'filename': filename, 'content': content})
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/files', methods=['POST'])
def create_file():
    data = request.get_json()
    filename = data.get('filename')
    content = data.get('content', '')

    if not filename:
        return jsonify({'error': 'Filename is required'}), 400

    filepath = os.path.join(FILES_DIR, filename)

    if os.path.exists(filepath):
        return jsonify({'error': 'File already exists'}), 409

    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return jsonify({'message': 'File created', 'filename': filename})
    except Exception as e:
        return jsonify({'error': str(e)}), 500

    
@app.route('/files/auto', methods=['POST'])
def create_file_auto():
    data = request.get_json()
    content = data.get('content', '')

    i = 1
    while True:
        filename = f'file_{i}.txt'
        filepath = os.path.join(FILES_DIR, filename)
        if not os.path.exists(filepath):
            break
        i += 1

    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return jsonify({'message': 'File created', 'filename': filename})
    except Exception as e:
        return jsonify({'error': str(e)}), 500 

@app.route('/files/<filename>', methods=['DELETE'])
def delete_file(filename):
    filepath = os.path.join(FILES_DIR, filename)

    if not os.path.exists(filepath):
        return jsonify({'error': 'File not found'}), 404

    try:
        os.remove(filepath)
        return jsonify({'message': 'File deleted', 'filename': filename})
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@app.route('/files/<filename>', methods=['PUT'])
def update_file(filename):
    filepath = os.path.join(FILES_DIR, filename)

    if not os.path.exists(filepath):
        return jsonify({'error': 'File not found'}), 404

    data = request.get_json()
    content = data.get('content', '')

    try:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(content)
        return jsonify({'message': 'File updated', 'filename': filename})
    except Exception as e:
        return jsonify({'error': str(e)}), 500


if __name__ == '__main__':
    app.run(debug=True)
