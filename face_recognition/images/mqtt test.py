import paho.mqtt.publish as publish

publish.single("hadi-2760/control", "open", hostname="broker.hivemq.com", port=1883)
