#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <PubSubClient.h>  
#include <Servo.h>

#define TRIG_PIN D1
#define ECHO_PIN D2
#define RED_PIN D5
#define GREEN_PIN D6
#define BLUE_PIN D7
#define BUZZER_PIN D4
#define SERVO_PIN D3
#define BUTTON_PIN D8

const char* ssid = "hadi";
const char* password = "12345678900";

#define BOT_TOKEN "7492296974:AAEdCpH2j0_R8ZkvWhZW0PqZ1hLY7N7R_5M"
#define CHAT_ID "7844226118"

const char* mqtt_server = "broker.emqx.io"; 
const char* mqtt_topic = "hadi_door_control";
const char* mqtt_client_id = "esp8266_door_controller";

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WiFiClient espClient;
PubSubClient mqttClient(espClient);  

Servo doorServo;

enum SystemMode { OWNER_MODE_ON, OWNER_MODE_OFF };
SystemMode currentMode = OWNER_MODE_OFF;
bool waitingResponse = false;
unsigned long detectionStartTime = 0;
const unsigned long responseTimeout = 30000;
bool visitorDetected = false;
unsigned long lastMessageCheckTime = 0;
String lastCommand = "";

const int MOTOR_OPEN_TIME = 5000;
unsigned long motorStartTime = 0;
bool motorActive = false;
bool hadiDetected = false;

bool buttonPressed = false;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  doorServo.attach(SERVO_PIN);
  doorServo.write(0);

  connectToWiFi();
  
  mqttClient.setServer(mqtt_server, 1883  );
  mqttClient.setCallback(mqttCallback);
  
  setRGBColor(0, 0, 255);
  Serial.println("System Ready. Send /owner_on or /owner_off");

  bot.sendMessage(CHAT_ID, "Security System Online. Current mode: " + getModeString(), "");
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  checkButton();

  if (millis() - lastMessageCheckTime > 1000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastMessageCheckTime = millis();
  }

  if (waitingResponse && (millis() - detectionStartTime > responseTimeout)) {
    handleTimeout();
  }

  if (motorActive && (millis() - motorStartTime > MOTOR_OPEN_TIME)) {
    doorServo.write(0);
    motorActive = false;
    Serial.println("🚪 Motor turned off after 5 seconds");
    if (hadiDetected) {
      bot.sendMessage(CHAT_ID, "Door closed after Hadi access.", "");
      hadiDetected = false;
    }
  }

  if (buttonPressed && hadiDetected) {
    buttonPressed = false;
    openDoorForHadi();
  }

  if (currentMode == OWNER_MODE_ON) {
    checkForHadiDetection();
  } else {
    checkUltrasonicSensor();
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT Message [");
  Serial.print(topic);
  Serial.print("] ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (message == "open_door") {
    if (currentMode == OWNER_MODE_ON) {
      handleHadiDetected();
      openDoorForHadi();
    }
  }
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(mqtt_client_id)) {
      Serial.println("connected");
      mqttClient.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void checkButton() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      buttonPressed = true;
    }
  }

  lastButtonState = reading;
}

void openDoorForHadi() {
  Serial.println("🔘 Button pressed for Hadi access");
  bot.sendMessage(CHAT_ID, "Button pressed - opening door for Hadi", "");
  
  setRGBColor(0, 255, 0); 
  doorServo.write(90);   
  playTone("granted");
  
  motorStartTime = millis();
  motorActive = true;
  hadiDetected = false;
}

void checkUltrasonicSensor() {
  long distance = measureDistance();
  
  if (distance < 50) { 
    if (!visitorDetected && !waitingResponse && !motorActive) {
      visitorDetected = true;
      handleVisitorDetected();
    }
  } else {
    if (visitorDetected && !waitingResponse) {
      visitorDetected = false;
    }
  }
}

void checkForHadiDetection() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  for (int i = 0; i < numNewMessages; i++) {
    String text = bot.messages[i].text;
    if (text == "hadi_detected") {
      handleHadiDetected();
    }
  }
}

void handleHadiDetected() {
  Serial.println("🟢 Hadi detected! Press button to open door.");
  bot.sendMessage(CHAT_ID, "🟢 Hadi detected! Press the button to open door.", "");
  
  setRGBColor(0, 255, 0); 
  playTone("granted");
  
  hadiDetected = true;
}

void handleVisitorDetected() {
  Serial.println("🔔 Visitor detected!");
  bot.sendMessage(CHAT_ID, "🚪 Visitor detected at your door!\n"
                           "Please reply with:\n"
                           "'yes' - to open the door for 5 seconds\n"
                           "'no' - to deny access", "");

  setRGBColor(255, 165, 0);
  playTone("notify");

  waitingResponse = true;
  detectionStartTime = millis();
  visitorDetected = true;
}

void handleTimeout() {
  Serial.println("⛔ Timeout. No reply received.");
  bot.sendMessage(CHAT_ID, "⏳ Access timed out. Door remains closed.", "");
  setRGBColor(255, 0, 0); 
  playTone("denied");
  delay(3000);
  resetSystem();
}

void handleNewMessages(int numNewMessages) {
  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID) {
      bot.sendMessage(chat_id, "⚠️ Unauthorized access.", "");
      continue;
    }

    String text = bot.messages[i].text;
    text.toLowerCase();
    lastCommand = text;

    if (text == "/owner_on") {
      currentMode = OWNER_MODE_ON;
      visitorDetected = false;
      waitingResponse = false;
      bot.sendMessage(chat_id, "✅ Owner Mode: ON\nAutomatic Hadi detection enabled.", "");
      setRGBColor(0, 255, 0); 
    }
    else if (text == "/owner_off") {
      currentMode = OWNER_MODE_OFF;
      waitingResponse = false;
      bot.sendMessage(chat_id, "✅ Owner Mode: OFF\nUltrasonic visitor detection enabled.", "");
      setRGBColor(0, 0, 255); 
    }
    else if (waitingResponse) {
      if (text == "yes") {
        grantAccess();
      }
      else if (text == "no") {
        denyAccess();
      }
      else {
        bot.sendMessage(chat_id, "❌ Invalid response. Please reply 'yes' or 'no'.", "");
      }
    }
    else if (text == "/status") {
      String statusMsg = "🔍 System Status:\n";
      statusMsg += "Mode: " + getModeString() + "\n";
      statusMsg += "Distance: " + String(measureDistance()) + " cm\n";
      if (hadiDetected) {
        statusMsg += "🟢 Hadi detected - press button to open\n";
      }
      if (waitingResponse) {
        statusMsg += "⏳ Waiting for response (" + String((responseTimeout - (millis() - detectionStartTime)) / 1000) + "s remaining)";
      } else if (motorActive) {
        statusMsg += "🚪 Door open (" + String((MOTOR_OPEN_TIME - (millis() - motorStartTime)) / 1000) + "s remaining)";
      } else {
        statusMsg += "✅ Ready";
      }
      bot.sendMessage(chat_id, statusMsg, "");
    }
    else if (text == "/help") {
      String helpMsg = "🆘 Available Commands:\n\n";
      helpMsg += "/owner_on - Enable Hadi detection\n";
      helpMsg += "/owner_off - Enable visitor detection\n";
      helpMsg += "/status - Check system status\n";
      helpMsg += "/help - Show this message\n\n";
      helpMsg += "When Hadi is detected:\n";
      helpMsg += "Press the physical button to open door\n\n";
      helpMsg += "When visitor is detected:\n";
      helpMsg += "yes - Open door for 5 seconds\n";
      helpMsg += "no - Deny access";
      bot.sendMessage(chat_id, helpMsg, "");
    }
    else if (text != "hadi_detected") {
      bot.sendMessage(chat_id, "❌ Unknown command. Send /help for options.", "");
    }
  }
}

String getModeString() {
  return currentMode == OWNER_MODE_ON ? "Owner ON (Hadi detection)" : "Owner OFF (Visitor detection)";
}

void grantAccess() {
  Serial.println("✅ Access granted. Opening door for 5 seconds...");
  bot.sendMessage(CHAT_ID, "🔓 Access granted. Opening door for 5 seconds...", "");
  setRGBColor(0, 255, 0); 
  doorServo.write(90);     
  playTone("granted");
  motorStartTime = millis();
  motorActive = true;
  waitingResponse = false;
  visitorDetected = false;
}

void denyAccess() {
  Serial.println("🚫 Access denied.");
  bot.sendMessage(CHAT_ID, "🔒 Access denied. Door remains closed.", "");
  setRGBColor(255, 0, 0);  
  playTone("denied");
  delay(3000);
  resetSystem();
}

void resetSystem() {
  waitingResponse = false;
  visitorDetected = false;
  setRGBColor(0, 0, 255); 
  Serial.println("System reset.");
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  secured_client.setInsecure();
  Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
}

long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.0344 / 2;
  return distance;
}

void setRGBColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void playTone(String type) {
  if (type == "granted") {
    tone(BUZZER_PIN, 800, 150);
    delay(200);
    tone(BUZZER_PIN, 1000, 200);
  } else if (type == "denied") {
    for (int i = 0; i < 3; i++) {
      tone(BUZZER_PIN, 300, 200);
      delay(300);
    }
  } else if (type == "notify") {
    tone(BUZZER_PIN, 1000, 100);
    delay(150);
    tone(BUZZER_PIN, 1500, 100);
  }
}