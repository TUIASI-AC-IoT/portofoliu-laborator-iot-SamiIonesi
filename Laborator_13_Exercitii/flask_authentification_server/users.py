
users_db = {
    "SamiBoss": {"password": "samica", "role": "Admin"},
    "AndreiCretu": {"password": "12345678", "role": "Owner"},
    "WhyMe": {"password": "hello", "role": "Owner"},
    "test": {"password": "test", "role": "Owner"},
}

def authenticate_user(username, password):
    user = users_db.get(username)
    if user and user["password"] == password:
        return {"username": username, "role": user["role"]}
    return None
