#include "Msgflo.h"

#include <vector>

namespace msgflo {
namespace pubsub {

static
void printMqttState(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT:
      Serial.println("Disconnected: connection timeout");
      break;
    case MQTT_CONNECTION_LOST:
      Serial.println("Disconnected: connection lost");
      break;
    case MQTT_CONNECT_FAILED:
      Serial.println("Disconnected: connect failed");
      break;
    case MQTT_DISCONNECTED:
      Serial.println("Disconnected: disconnected");
      break;
    case MQTT_CONNECTED:
      Serial.println("Disconnected: connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      Serial.println("Disconnected: bad protocol");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      Serial.println("Disconnected: bad client id");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      Serial.println("Disconnected: unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      Serial.println("Disconnected: bad credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      Serial.println("Disconnected: unauthorized");
      break;
  }
}

static void globalCallback(const char* topic, byte* payload, unsigned int length);

class Publisher {
  public:
    virtual void send(const String &queue, const String &payload) = 0;
};

struct PubSubOutPort : public OutPort {
  public:
    String id;
    String type;
    String queue;
    Publisher *publisher;

    PubSubOutPort& operator=(PubSubOutPort&& p) = default;

    PubSubOutPort()
        : publisher(nullptr)
    {        
    }

    const bool valid() {
        return publisher;
    }

    PubSubOutPort(const String &id, const String &type, const String &queue, Publisher *publisher) :
      id(id), type(type), queue(queue), publisher(publisher) {
    }

    void toJson(String &s) const {
      s += "{\"id\": \"";
      s += id;
      s += "\", \"type\": \"";
      s += type;
      s += "\", \"queue\": \"";
      s += queue;
      s += "\"}";
    }

    void send(const String &payload) {
      publisher->send(queue, payload);
    }
};


struct PubSubInPort : public InPort {
  public:
    String id;
    String type;
    String queue;
    InPortCallback callback;
    bool _valid;

    PubSubInPort(PubSubInPort&& p) = default;
    PubSubInPort& operator=(PubSubInPort&& p) = default;

    PubSubInPort& operator=(PubSubInPort& p) = default;

    PubSubInPort()
        : _valid(false) {
    }

    const bool valid() {
        return _valid;
    }

    PubSubInPort(const String &id, const String &type, const String &queue, InPortCallback callback) :
      id(id), type(type), queue(queue), callback(callback), _valid(true) {
    }

    void toJson(String &s) const {
      s += "{\"id\": \"";
      s += id;
      s += "\", \"type\": \"";
      s += type;
      s += "\", \"queue\": \"";
      s += queue;
      s += "\"}";
    }

};

static
const char *discoveryTopic = "fbp";

#define MAX_PORTS 5

class PubSubClientEngine : public Engine, public Publisher {

    PubSubClient *mqtt;
    const char *clientId;
    const char *username;
    const char *password;

    const Participant participant;
    std::array<PubSubOutPort, MAX_PORTS> outPorts;
    int8_t lastOutPort;
    std::array<PubSubInPort, MAX_PORTS> inPorts;
    int8_t lastInPort;

    long lastDiscoverySent; // milliseconds

  public:
    void send(const String &queue, const String &payload) {
      mqtt->publish(queue.c_str(), payload.c_str());
    }

    PubSubClientEngine(const Participant &p, PubSubClient *mqtt, const char *id, const char *username, const char *password):
        participant(p),
        mqtt(mqtt),
        clientId(id),
        username(username),
        password(password),
        lastDiscoverySent(0),
        lastOutPort(0),
        lastInPort(0)
    {
      mqtt->setCallback(&globalCallback);
    }

    OutPort* addOutPort(const String &id, const String &type, const String &queue) {
      if (lastOutPort < MAX_PORTS) {
        const int port = lastOutPort++;
        outPorts[port] = PubSubOutPort(id, type, queue, this);
        return &outPorts[port];
      } else {
        return NULL;
      }
    }

    InPort* addInPort(const String &id, const String &type, const String &queue, InPortCallback callback) {
      if (lastInPort < MAX_PORTS) {
        const int port = lastInPort++;
        inPorts[port] = PubSubInPort(id, type, queue, callback);
        return &inPorts[port];
      } else {
        return NULL;
      }
    }

    void callback(const char* topic, byte* payload, unsigned int length) {
        for (auto &p : inPorts) {
            if (p.queue == topic) {
                p.callback(payload, length);
            }
        }
    }

    bool sendDiscovery(const Participant *p) {
      // fbp {"protocol":"discovery","command":"participant","payload":{"component":"dlock13/DoorLock","label":"Open the door","icon":"lightbulb-o","inports":[{"queue":"/bitraf/door/boxy4/open","type":"object","id":"open"}],"outports":[],"role":"boxy4","id":"boxy4"}}
      String discoveryMessage =
        "{"
        "\"protocol\": \"discovery\","
        "\"command\": \"participant\","
        "\"payload\": {\"component\": \"";

      discoveryMessage += p->component;
      discoveryMessage += "\", \"label\": \"";
      discoveryMessage += p->label;
      discoveryMessage += "\", \"icon\": \"";
      discoveryMessage += p->icon;

      discoveryMessage += "\", \"id\": \"";
      discoveryMessage += p->id;
      discoveryMessage += "\", \"role\": \"";
      discoveryMessage += p->role;

      discoveryMessage += "\", \"outports\": [";
      for (int i=0; i < outPorts.size(); i++) {
        auto &p = outPorts[i];
        if (p.valid()) {
            if (i > 0) {
                discoveryMessage += ",";
            }
            p.toJson(discoveryMessage);
        }
      }
      discoveryMessage += "],";

      discoveryMessage += "\"inports\": [";
      for (int i=0; i < inPorts.size(); i++) {
        auto &p = inPorts[i];
        if (p.valid()) {
            if (i > 0) {
                discoveryMessage += ",";
            }
            p.toJson(discoveryMessage);
        }
      }
      discoveryMessage += "]";

      discoveryMessage += "}}";
      Serial.print("MsgFlo discovery message bytes: ");
      Serial.println(discoveryMessage.length());

      // FIXME: due to https://github.com/knolleary/pubsubclient/issues/110 this is pretty likely,
      // needs a proper fix for when compiling with Arduino IDE..
      const bool success = mqtt->publish(discoveryTopic, (const uint8_t*)discoveryMessage.c_str(), discoveryMessage.length(), false);
      return success;
    }

    void subscribeInPorts() {
        for (auto &p : inPorts) {
            if (p.valid()) {
                mqtt->subscribe(p.queue.c_str());
            }
        }
    }

    void onConnected() {
        subscribeInPorts();
        const bool success = sendDiscovery(&participant);
        if (!success) {
            Serial.println("failed to send Msgflo discovery");
            Serial.printf("limit = %d\n", MQTT_MAX_PACKET_SIZE);
        }
    }

    void loop() {
      mqtt->loop();
      const long currentTime = millis();
      const long secondsSinceLastDiscovery = (currentTime - lastDiscoverySent)/1000;
      if (secondsSinceLastDiscovery > participant.discoveryPeriod/3) {
        sendDiscovery(&participant);
        lastDiscoverySent = currentTime;
      }

      if (!mqtt->connected()) {
        int state = mqtt->state();

        Serial.print("Connecting...");
        // This is blocking
        if (connect()) {
          Serial.println("ok");
          onConnected();
        } else {
          Serial.println("failed");
          printMqttState(mqtt->state());
        }
      }
    }
private:
    bool connect() {
        if (username) {
            return mqtt->connect(clientId, username, password);
        } else {
            return mqtt->connect(clientId);
        }
    }
};

uint8_t instanceBytes[sizeof(PubSubClientEngine)];
static PubSubClientEngine *instance = nullptr;

static void globalCallback(const char* topic, byte* payload, unsigned int length) {
  instance->callback(topic, payload, length);
}

Engine *createPubSubClientEngine(const Participant &part, PubSubClient* mqtt,
    const char *clientId, const char *username, const char *password) {

  if (instance != nullptr) {
    Serial.println("Double initialization of msgflo engine.");
    return instance;
  }

  instance = new (instanceBytes) PubSubClientEngine(part, mqtt, clientId, username, password);
  return instance;
}

Engine *createPubSubClientEngine(const Participant &part, PubSubClient* mqtt,
    const char *clientId) {

    return createPubSubClientEngine(part, mqtt, clientId, NULL, NULL);
}

};
};

