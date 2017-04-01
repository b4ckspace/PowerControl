#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define HOSTNAME "Arduino-PowerControl"

#define MQTT_HOST "mqtt.core.bckspc.de"
#define MQTT_TOPIC_LAST_WILL "state/powercontrol"


static uint8_t mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01 };
