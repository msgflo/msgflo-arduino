
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Msgflo.h>

struct Config {
  const String component = "iot/EthernetButton";
  const String prefix = "public/";
  const String role = "testbutton";

  const int ledPin = PN_0; // Connected Tiva
  const int buttonPin = 2;

  const char *mqttHost = "10.0.0.2";
  const int mqttPort = 1883;

  const char *mqttUsername = NULL;
  const char *mqttPassword = NULL;
} cfg;

EthernetClient wifiClient;
PubSubClient mqttClient;
msgflo::Engine *engine;
msgflo::OutPort *buttonPort;
msgflo::InPort *ledPort;
long nextButtonCheck = 0;

auto participant = msgflo::Participant(cfg.component, cfg.role);

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println();
  Serial.println();

  Serial.println("Setting up Ethernet");
  Ethernet.begin(0); // Use DHCP to get IP
  Serial.println(Ethernet.localIP());

  // Provide a Font Awesome (http://fontawesome.io/icons/) icon for the component
  participant.icon = "toggle-on";

  mqttClient.setServer(cfg.mqttHost, cfg.mqttPort);
  mqttClient.setClient(wifiClient);

  String clientId = cfg.role;

  engine = msgflo::pubsub::createPubSubClientEngine(participant, &mqttClient, clientId.c_str(), cfg.mqttUsername, cfg.mqttPassword);

  buttonPort = engine->addOutPort("button", "any", cfg.prefix + cfg.role + "/button");

  ledPort = engine->addInPort("led", "boolean", cfg.prefix + cfg.role + "/led", 
  [](byte *data, int length) -> void {
      const int capacity = 10;
      char buffer[capacity] = {0};
      for (int i=0; i<length && i<capacity; i++) {
        buffer[i] = data[i];
      }
      const String in(buffer);
      const boolean on = (in == "1" || in == "true");
      digitalWrite(cfg.ledPin, on);
  });

  pinMode(cfg.buttonPin, INPUT);
  pinMode(cfg.ledPin, OUTPUT);
}

void loop() {
  static bool connected = false;

  if (true) {
    if (!connected) {
      Serial.println("Ethernet ready");
    }
    connected = true;
    engine->loop();
  } else {
    if (connected) {
      connected = false;
      Serial.println("Lost wifi connection.");
    }
  }

  // TODO: check for statechange. If changed, send right away. Else only send every 3 seconds or so
  if (millis() > nextButtonCheck) {
    const bool pressed = digitalRead(cfg.buttonPin);
    buttonPort->send(pressed ? "true" : "false");
    nextButtonCheck += 300;
  }
}

