#include <Ethernet.h>
#include <Bounce2.h>
#include <Flash.h>
#include <PubSubClient.h>
#include "settings.h"

// RELAIS
//
// GPIO 22, 24, 26, 28, 30, 32, 34,36

// DIGITAL IO
//       1  2  3  4  5  6  7
// GPIO 52,50,49,46,44,42,40

PubSubClient mqttClient;
EthernetClient ethernetClient;

typedef struct {
  const char* name;
  const uint8_t pin;
} output_t;

output_t outputs[] = {
  { "kitchen", 32 },
  { "podest", 34 },
  { "ceiling", 26 },
  { "entry", 24 },
  { "oven", 22 },
  //{ "powersockets", 36 },
  { "storage", 28 },
  { "bar", 30 },
};

typedef struct {
  uint8_t pin;
  const char* mqttTopic;
  const char* valueUp;
  const char* valueDown;
  uint16_t debounceMs;
  Bounce *debouncer;
  const char* output;
} t_pinConfiguration;

t_pinConfiguration pins[] = {
  {48, "sensor/button/kitchen/main",    "released", "pressed", 20, NULL, "kitchen"},
  {49, "sensor/button/kitchen/storage", "released", "pressed", 20, NULL, "storage"}
};

output_t* determineOutput(char* name) {
  for (uint8_t i = 0; i < ARRAY_SIZE(outputs); i++) {
    if (strcasecmp(name, outputs[i].name) == 0) {
      return &outputs[i];
    }
  }

  return NULL;
}

void mqttConnect() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(HOSTNAME, MQTT_TOPIC_LAST_WILL, 1, true, "disconnected")) {
      mqttClient.subscribe(MQTT_TOPIC_POWERCONTROL);
      mqttClient.publish(MQTT_TOPIC_LAST_WILL, "connected", true);
      Serial.println("Connected!");
    } else {
      Serial.println("MQTT connect failed!");
      delay(1000);
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {

  const char* onOff = (char*) payload;
  const char* lastSegment = strrchr((char*) topic, '/');

  if (lastSegment == NULL) {
    return;
  }

  output_t* output = determineOutput(lastSegment + 1);
  if (output == NULL) {
    return;
  }

  if (strncasecmp(onOff, "ON", length) == 0) {
    digitalWrite(output->pin, HIGH);
  } else if (strncasecmp(onOff, "OFF", length) == 0) {
    digitalWrite(output->pin, LOW);
  }
}

void setup() {

  delay(3000);

  Serial.begin(115200);

  pinMode(10, OUTPUT); // set the SS pin as an output (necessary!)
  digitalWrite(10, HIGH); // but turn off the W5100 chip!

  for (uint8_t i = 0; i < ARRAY_SIZE(pins); i++) {
    pinMode(pins[i].pin, INPUT_PULLUP);

    pins[i].debouncer = new Bounce();
    pins[i].debouncer->attach(pins[i].pin);
    pins[i].debouncer->interval(100);
  }

  for (uint8_t i = 0; i < ARRAY_SIZE(outputs); i++) {
    pinMode(outputs[i].pin, OUTPUT);
  }

  Serial.println("Setup eth0");
  Ethernet.begin(mac);

  Serial.println("IP");
  Serial.println(Ethernet.localIP());

  mqttClient.setClient(ethernetClient);
  mqttClient.setServer(MQTT_HOST, 1883);
  mqttClient.setCallback(mqttCallback);
}

void toggleOutput(const char* name) {
  const output_t* output = determineOutput(name);

  if (output == NULL) {
    return;
  }
  
  digitalWrite(output->pin, (digitalRead(output->pin) == HIGH) ? LOW : HIGH);
}

void loop() {
  Ethernet.maintain();
    
  for (uint8_t i = 0; i < ARRAY_SIZE(pins); i++) {
    pins[i].debouncer->update();

    if (pins[i].debouncer->rose()) {
      mqttClient.publish(pins[i].mqttTopic, pins[i].valueUp, true);
    } else if (pins[i].debouncer->fell()) {     
      if (pins[i].output) {
        toggleOutput(pins[i].output);
      }
      
      mqttClient.publish(pins[i].mqttTopic, pins[i].valueDown, true);
    }
  }

  mqttConnect();
  mqttClient.loop();
}

