import cv2
import face_recognition
import paho.mqtt.client as mqtt
import time
import requests

# MQTT Settings - Updated to EMQX
MQTT_BROKER = "broker.emqx.io"
MQTT_PORT = 1883
MQTT_TOPIC = "hadi_door_control"
MQTT_CLIENT_ID = "hadi_detector_pc"

# Telegram Bot settings
TELEGRAM_BOT_TOKEN = "7492296974:AAEdCpH2j0_R8ZkvWhZW0PqZ1hLY7N7R_5M"
TELEGRAM_CHAT_ID = "7844226118"
TELEGRAM_API_URL = f"https://api.telegram.org/bot{TELEGRAM_BOT_TOKEN}/sendMessage"

# Initialize MQTT Client with API version
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, MQTT_CLIENT_ID)
mqtt_client.connect(MQTT_BROKER, MQTT_PORT)

# Load Hadi's face and encode
hadi_img = face_recognition.load_image_file("images/hadi.jpg")
hadi_img_encoding = face_recognition.face_encodings(hadi_img)[0]

# Webcam stream
url = "http://192.168.137.156:4747/video"
cap = cv2.VideoCapture(url)

if not cap.isOpened():
    print("❌ Could not open webcam.")
    exit()

last_detected_time = 0

def send_telegram_message(message):
    try:
        payload = {
            'chat_id': TELEGRAM_CHAT_ID,
            'text': message
        }
        response = requests.post(TELEGRAM_API_URL, data=payload)
        if response.status_code == 200:
            print("📡 Telegram message sent successfully")
        else:
            print(f"❌ Failed to send Telegram message. Status code: {response.status_code}")
    except Exception as e:
        print(f"❌ Error sending Telegram message: {e}")

while True:
    ret, frame = cap.read()
    if not ret:
        print("❌ Failed to grab frame")
        break

    small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
    rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)

    face_locations = face_recognition.face_locations(rgb_small_frame)
    face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)

    for face_encoding, face_location in zip(face_encodings, face_locations):
        match = face_recognition.compare_faces([hadi_img_encoding], face_encoding, tolerance=0.4)[0]
        face_distance = face_recognition.face_distance([hadi_img_encoding], face_encoding)[0]

        top, right, bottom, left = [v * 4 for v in face_location]
        if match:
            label = f"Hadi ✅ ({face_distance:.2f})"
            color = (0, 255, 0)

            if time.time() - last_detected_time > 10:
                # Send via MQTT
                mqtt_client.publish(MQTT_TOPIC, "open_door")
                # Send Telegram notification
                send_telegram_message("Hadi detected - Door opened automatically")
                print("📡 Sent Hadi detection via MQTT and Telegram")
                last_detected_time = time.time()
        else:
            label = f"Unknown ❌ ({face_distance:.2f})"
            color = (0, 0, 255)

        cv2.rectangle(frame, (left, top), (right, bottom), color, 2)
        cv2.putText(frame, label, (left, top - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.9, color, 2)

    cv2.imshow("Webcam Feed", frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()