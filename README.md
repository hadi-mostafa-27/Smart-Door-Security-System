# Smart-Door-Security-System
Smart Door Security System
This project is a sophisticated 

Smart Door Security System that combines several technologies to provide secure and convenient access control. It uses an ultrasonic sensor to detect visitors, a Python script with OpenCV for face recognition, and a Telegram bot for remote alerts and control. The system communicates between the Python script and an ESP8266 microcontroller using the MQTT protocol.



Features

Visitor Detection: An ultrasonic sensor detects visitors near the door.


Real-time Alerts: The system sends real-time alerts to the owner's Telegram app when a visitor is detected.


Remote Control: The owner can reply to the Telegram bot to either grant ("yes") or deny ("no") access.



Face Recognition: The system can recognize authorized users (e.g., "Hadi") using Python and OpenCV.



Visual and Audio Feedback: RGB LEDs and a buzzer provide visual and audio cues for system status, such as "access granted" or "denied".


Physical Button Access: Recognized users can press a button to open the door.



System Reset: The system automatically resets if there is no response from the owner within 30 seconds.

Components Used
Hardware

ESP8266 (NodeMCU): The main microcontroller for WiFi and control logic.


Ultrasonic Sensor (HC-SR04): Used to detect visitors near the door.


Servo Motor (SG90): Simulates the door's lock/unlock mechanism.


RGB LED: Provides visual feedback.


Buzzer: Used for audio alerts.


Push Button: Allows for manual override.

Software & Libraries

Arduino IDE (C++): For programming the ESP8266 firmware.


Python (OpenCV, face_recognition): For face detection and recognition.


Telegram Bot API: For remote notifications and control.


MQTT (HiveMQ Broker): Facilitates communication between the Python script and the ESP8266.

How the Project Works
Visitor Detection: An ultrasonic sensor checks for visitors. If a person is within 50cm, the ESP8266 triggers an alert.


Notification: A notification is sent to the owner's Telegram with the message "Visitor detected! Reply 'yes' or 'no'".

Owner Response:

If the owner replies "yes," the door opens for 5 seconds.

If the owner replies "no," access is denied, and the buzzer sounds with a red LED.


Face Recognition Mode: The Python script continuously monitors for the presence of an authorized user. When "Hadi" is detected, it sends an 

open_door command via MQTT.


Manual Override: Upon receiving the MQTT command, the ESP8266 activates a green LED and waits for the button to be pressed. Pressing the button then opens the door.



