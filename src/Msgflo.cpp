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
    const String id;
    const String type;
    const String queue;
    Publisher *publisher;

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
    const String id;
    const String type;
    const String queue;
    const InPortCallback callback;

    PubSubInPort(const String &id, const String &type, const String &queue, InPortCallback callback) :
      id(id), type(type), queue(queue), callback(callback) {
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

class PubSubClientEngine : public Engine, public Publisher {

    PubSubClient *mqtt;
    const char *clientId;
    const char *username;
    const char *password;

    const Participant participant;
    std::vector<PubSubOutPort> outPorts;
    std::vector<PubSubInPort> inPorts;

  public:
    void send(const String &queue, const String &payload) {
      mqtt->publish(queue.c_str(), payload.c_str());
    }

    PubSubClientEngine(const Participant &p, PubSubClient *mqtt, const char *id, const char *username, const char *password):
        participant(p),
        mqtt(mqtt),
        clientId(id),
        username(username),
        password(password)
    {
      mqtt->setCallback(&globalCallback);
    }

    OutPort* addOutPort(const String &id, const String &type, const String &queue) {
      outPorts.emplace_back(id, type, queue, this);
      return &outPorts[outPorts.size() - 1];
    }

    InPort* addInPort(const String &id, const String &type, const String &queue, InPortCallback callback) {
      inPorts.emplace_back(id, type, queue, callback);
      return &inPorts[inPorts.size() - 1];
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
      for (auto &p : outPorts) {
        p.toJson(discoveryMessage);
      }
      discoveryMessage += "],";

      discoveryMessage += "\"inports\": [";
      for (auto &p : inPorts) {
        p.toJson(discoveryMessage);
      }
      discoveryMessage += "]";

      discoveryMessage += "}}";
      Serial.print("discoveryMessage: ");
      Serial.print(discoveryMessage.length());
      Serial.print(" :");
      //Serial.println(discoveryMessage);

      // FIXME: due to https://github.com/knolleary/pubsubclient/issues/110 this is pretty likely,
      // needs a proper fix for when compiling with Arduino IDE..
      const bool success = mqtt->publish(discoveryTopic, (const uint8_t*)discoveryMessage.c_str(), discoveryMessage.length(), false);
      return success;
    }

    void subscribeInPorts() {
        for (auto &p : inPorts) {
            mqtt->subscribe(p.queue.c_str());
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

      if (!mqtt->connected()) {
        int state = mqtt->state();

        Serial.print("Connecting...");
        // This is blocking
        if (mqtt->connect(clientId, username, password)) {
          Serial.println("ok");
          onConnected();
        } else {
          Serial.println("failed");
          printMqttState(mqtt->state());
        }
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

};
};

