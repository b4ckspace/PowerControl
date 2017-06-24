#include <Ethernet.h>
#include <TinyWebServer.h>
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

bool relaisOnHandler(TinyWebServer& web_server);
bool relaisOffHandler(TinyWebServer& web_server);

TinyWebServer::PathHandler handlers[] = {
  {"/relais/" "*", TinyWebServer::POST, &relaisOnHandler },
  {"/relais/" "*", TinyWebServer::DELETE, &relaisOffHandler },
  {NULL},
};

TinyWebServer webServer = TinyWebServer(handlers, NULL);
EthernetClient ethernetClient;

typedef struct {
  const char* route;
  const uint8_t pin;
} output_t;

output_t outputs[] = {
  { "kitchen_light", 32 },
  { "podest_light", 34 },
  { "canvas_light", 26 },
  { "entry_light", 24 },
  { "oven", 22 },
  //{ "powersockets", 36 },
  { "storage_light", 28 },
  { "bar_light", 30 },
};

typedef struct {
  uint8_t pin;
  const char* mqttTopic;
  const char* valueUp;
  const char* valueDown;
  uint16_t debounceMs;
  Bounce *debouncer;
} t_pinConfiguration;

t_pinConfiguration pins[] = {
  {48, "sensor/button/kitchen/main", "released", "pressed", 42, NULL},
  {49, "sensor/button/kitchen/storage", "released", "pressed", 42, NULL}
};


output_t* determineOutput(char* route) {
  for (uint8_t i = 0; i < ARRAY_SIZE(outputs); i++) {    
    if (strcasecmp(route, outputs[i].route) == 0) { 
      return &outputs[i];
    }
  }

  return NULL;
}

void sendHttpOk(TinyWebServer& web_server) {
  web_server.send_error_code(200);
  web_server.send_content_type("text/plain");
  web_server.end_headers();  
  web_server << F("OK");
}

bool relaisOnHandler(TinyWebServer& web_server) {
  char* filename = TinyWebServer::get_file_from_path(web_server.get_path());
  output_t* output = determineOutput(filename);
  
  if (output) {
    digitalWrite(output->pin, HIGH);
  }
  
  sendHttpOk(web_server);
}

bool relaisOffHandler(TinyWebServer& web_server) {
  char* filename = TinyWebServer::get_file_from_path(web_server.get_path());
  output_t* output = determineOutput(filename);

  if (output) {
    digitalWrite(output->pin, LOW);
  }

  sendHttpOk(web_server);
}

void connectMqtt() {
  while (!mqttClient.connected()) {
    if (mqttClient.connect(HOSTNAME, MQTT_TOPIC_LAST_WILL, 1, true, "disconnected")) {
      Serial.println("MQTT connected");
      mqttClient.publish(MQTT_TOPIC_LAST_WILL, "connected", true);
    } else {
      delay(1000);
    }
  }
}

void setup() {

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
  
  webServer.begin();
}

void loop() {
  
  for (uint8_t i = 0; i < ARRAY_SIZE(pins); i++) {
    pins[i].debouncer->update();

    if (pins[i].debouncer->rose()) {
      mqttClient.publish(pins[i].mqttTopic, pins[i].valueUp, true);
    } else if (pins[i].debouncer->fell()) {
      mqttClient.publish(pins[i].mqttTopic, pins[i].valueDown, true);
    }
  }

  connectMqtt();
  mqttClient.loop();
  webServer.process();
}
