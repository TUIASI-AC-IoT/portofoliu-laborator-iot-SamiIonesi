from flask import Flask, request, jsonify, send_from_directory
from flask_jwt_extended import (
    JWTManager, create_access_token, get_jwt_identity, jwt_required
)
from users import authenticate_user
import json

app = Flask(__name__)
app.config["JWT_SECRET_KEY"] = "super-secret-key"
jwt = JWTManager(app)

# token store simplu (pentru validare)
token_store = set()

@app.route('/auth', methods=['POST'])
def login():
    if not request.is_json:
        return jsonify({"msg": "Missing JSON in request"}), 400

    username = request.json.get("username", None)
    password = request.json.get("password", None)

    if not username or not password:
        return jsonify({"msg": "Missing username or password"}), 400

    user = authenticate_user(username, password)
    if not user:
        return jsonify({"msg": "Bad username or password"}), 401

    access_token = create_access_token(identity=json.dumps({"username": username, "role": user["role"]}))
    token_store.add(access_token)
    return jsonify(access_token=access_token), 200

@app.route('/auth/jwtStore', methods=['GET'])
@jwt_required()
def validate_token():
    token = request.headers.get("Authorization", "").replace("Bearer ", "")
    
    if token not in token_store:
        return jsonify({"msg": "Token invalid sau expirat"}), 404

    identity = json.loads(get_jwt_identity())
    return jsonify({
        "msg": "Token valid",
        "username": identity["username"],
        "role": identity["role"]
    }), 200

@app.route('/auth/jwtStore', methods=['DELETE'])
@jwt_required()
def logout():
    token = request.headers.get("Authorization", "").replace("Bearer ", "")

    if token in token_store:
        token_store.remove(token)
        return jsonify({"msg": "Token invalidat (logout) cu succes"}), 200
    else:
        return jsonify({"msg": "Tokenul nu a fost găsit în sistem"}), 404
    
pir_status = {"pir": 0}  # status curent, initial 0

@app.route('/sensor/pir1', methods=['GET'])
@jwt_required()
def get_pir_status():
    token = request.headers.get("Authorization", "").replace("Bearer ", "")
    if token not in token_store:
        return jsonify({"msg": "Token invalid sau expirat"}), 404

    identity = json.loads(get_jwt_identity())
    role = identity["role"]
    if role not in ["Admin", "Owner"]:
        return jsonify({"msg": "Acces interzis pentru rolul curent"}), 403

    return jsonify(pir_status), 200

@app.route('/sensor/pir1', methods=['POST'])
@jwt_required()
def handle_pir():
    global pir_status
    token = request.headers.get("Authorization", "").replace("Bearer ", "")
    if token not in token_store:
        return jsonify({"msg": "Token invalid sau expirat"}), 404

    identity = json.loads(get_jwt_identity())
    role = identity["role"]

    if role not in ["Admin", "Owner"]:
        return jsonify({"msg": "Acces interzis pentru rolul curent"}), 403

    data = request.get_json()
    pir_status = data  # actualizezi statusul global
    print(f"[{identity['username']} - {role}] Trimite PIR: {data}")
    return jsonify({"msg": "Date primite"}), 200
    
@app.route("/")
def serve_html():
    return send_from_directory("static", "index.html")

if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0")
