
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#else
#include <Wifi.h>
#endif

#include <PubSubClient.h>
#include <Msgflo.h>

// Expose an LED to MQTT
class Led : public msgflo::RunnableParticipant {
public:
    // Config
    const int pin;
    bool inverted = false;

    // State
    msgflo::InPort *onPort;

public:
    Led(const char *role, int _pin)
        : msgflo::RunnableParticipant("arduino/Led", role)
        , pin(_pin)
    {
      
      onPort = this->inport("on", "string", [this](byte * data, int length) -> void {
        const std::string in((char *)data, length);
        const boolean requestOn = (in == "true");
        const boolean requestOff = (in == "false");
        if (requestOn) {
          digitalWrite(pin, inverted ? LOW : HIGH);
        } else if (requestOff) {
          digitalWrite(pin, inverted ? HIGH : LOW);
        } else {
          // ignored
        }
      });
    }

    void setup() {
      pinMode(pin, OUTPUT);
      digitalWrite(pin, inverted ? HIGH : LOW);
    }

    void loop() {
    }
};

// Exposes a button connected on an I/O pin to MQTT
// Polling-based (so works on any digital pin)
class Button : public msgflo::RunnableParticipant {
public:
    // Config
    const int pin;
    int pollPeriod = 20; // milliseconds
    int updatePeriod = 30000; // milliseconds
    bool invertPress = false;
    bool pullup = true;

    // State
    long nextButtonCheck = 0;
    long nextUpdate = 0;
    bool wasPressed = false;

    msgflo::OutPort *pressedPort;

public:
    Button(const char *role, int _pin)
        : msgflo::RunnableParticipant("arduino/Button", role)
        , pin(_pin)
    {
      pressedPort = this->outport("pressed", "string");
    }

    void setup() {
      pinMode(pin, pullup ? INPUT_PULLUP : INPUT);
    }

    void loop() {
        const long currentTime = millis(); 
        if (currentTime >= nextButtonCheck) {
            const bool pressed = readPressed();
            if (pressed != wasPressed) {
              sendState(pressed);
            }
            wasPressed = pressed;
            nextButtonCheck += pollPeriod;
        }
        if (currentTime >= nextUpdate) {
            const bool pressed = readPressed();
            wasPressed = pressed;
            sendState(pressed);
            nextUpdate += updatePeriod;
        }
    }

    bool readPressed() {
        const bool read = digitalRead(pin);
        return (invertPress) ? !read : read;
    }
    void sendState(bool pressed) {
        pressedPort->send((pressed) ? "true" : "false");
    }
};


WiFiClient wifiClient;
PubSubClient mqttClient;
msgflo::Engine *engine;

// Declare the Participant instances we got
Led ledOne("led", BUILTIN_LED);
Button buttonOne("button", D3);
msgflo::RunnableParticipant *participants[] = {
    &ledOne,
    &buttonOne,
};

struct Config {
  const String group = "multiple";
  const String prefix = "msgflo/arduino/multiple/";
  // MQTT topics will be on form $prefix$role/$port
  // Example: msgflo/arduino/multiple/button/pressed

  const char *wifiSsid = "CONFIGURE-ME";
  const char *wifiPassword = "CONFIGURE-ME";

  const char *mqttHost = "test.mosquitto.org";
  const int mqttPort = 1883;

  const char *mqttUsername = NULL;
  const char *mqttPassword = NULL;
} cfg;



void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();

  ledOne.inverted = true;

  // Setup WIFI and MQTT
  WiFi.mode(WIFI_STA);
  Serial.printf("Configuring wifi: %s\r\n", cfg.wifiSsid);
  WiFi.begin(cfg.wifiSsid, cfg.wifiPassword);

  mqttClient.setServer(cfg.mqttHost, cfg.mqttPort);
  mqttClient.setClient(wifiClient);

  String clientId = cfg.group;
  clientId += WiFi.macAddress();

  engine = msgflo::pubsub::createPubSubClientEngine(mqttClient);
  engine->setClientId(clientId.c_str());
  engine->setCredentials(cfg.mqttUsername, cfg.mqttPassword);
  engine->setTopicPrefix(cfg.prefix.c_str());

  // Setup participants
  for (auto p : participants) {
    engine->addParticipant(*p);
    p->setup();
  }
}

void loop() {
  static bool connected = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (!connected) {
      Serial.printf("Wifi connected: ip=%s\r\n", WiFi.localIP().toString().c_str());
    }
    connected = true;
    engine->loop();
  } else {
    if (connected) {
      connected = false;
      Serial.println("Lost wifi connection.");
    }
  }

  for (auto p : participants) {
    p->loop();
  }
}

