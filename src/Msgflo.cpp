#include "Msgflo.h"

#include <vector>

namespace msgflo {

OutPort* Participant::outport(const String &id, const String &type) {

  if (lastOutPort < MSGFLO_MAX_PORTS) {
    const int port = lastOutPort++;
    outPorts[port].id = id;
    outPorts[port].type = type;
    return &outPorts[port];
  } else {
    return NULL;
  }
}

InPort* Participant::inport(const String &id, const String &type, InPortCallback callback) {

  if (lastInPort < MSGFLO_MAX_PORTS) {
    const int port = lastInPort++;
    inPorts[port].id = id;
    inPorts[port].type = type;
    inPorts[port].queue = "";
    inPorts[port].callback = callback;
    return &inPorts[port];
  } else {
    return NULL;
  }
}

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

// Crack inherited from PubSubClient
#ifdef ESP8266
static void globalCallback(const char* topic, byte* payload, unsigned int length);
#else
static void globalCallback(char* topic, byte* payload, unsigned int length);
#endif


static
const char *discoveryTopic = "fbp";


String defaultTopic(const char *prefix, const String &role, const String &portId) {
  String topic = "";
  if (prefix) {
    topic += prefix;
  }
  topic += role + "/" + portId;

  return topic;
}


class PubSubClientEngine : public Engine, public Publisher {

private:
    PubSubClient *mqtt = NULL;
    const char *clientId = NULL;
    const char *username = NULL;
    const char *password = NULL;
    const char *topicPrefix = NULL;

    int discoveryPeriod = 60; // seconds
    long lastDiscoverySent = 0; // milliseconds

    std::array<Participant *, MSGFLO_MAX_PARTICIPANTS> participants;
    int lastParticipant = 0;

    // Legacy    
    Participant defaultParticipant;

public:
    PubSubClientEngine()
    {
        addParticipant(defaultParticipant);
    }

    PubSubClientEngine(const Participant &p, PubSubClient *_mqtt, const char *id,
                    const char *username, const char *password)
      : defaultParticipant(p)
      , clientId(id)
      , username(username)
      , password(password)
      , lastDiscoverySent(0)
    {
      addParticipant(defaultParticipant);
      setClient(*_mqtt);
    }

public:
    void setClient(PubSubClient &client) {
      mqtt = &client;
      mqtt->setCallback(&globalCallback);
    }

    void setCredentials(const char *user, const char *pw) {
        username = user;
        password = pw;
    }
    void setClientId(const char *id) {
        clientId = id;
    }

    void setTopicPrefix(const char *prefix) {
        topicPrefix = prefix;
    }

    bool addParticipant(Participant &part) {
      if (lastParticipant < MSGFLO_MAX_PARTICIPANTS) {
        const int idx = lastParticipant++;
        participants[idx] = &part;
        return true;
      }
      return false;
    }

    // Legacy API
    OutPort* addOutPort(const String &id, const String &type, const String &queue) {
      OutPort *p = defaultParticipant.outport(id, type);
      p->queue = queue;
      return p;
    }

    // Legacy API
    InPort* addInPort(const String &id, const String &type, const String &queue, InPortCallback callback) {
      InPort *p = defaultParticipant.inport(id, type, callback);
      p->queue = queue;
      return p;
    }

public:
    void send(const String &queue, const String &payload) {
      if (!mqtt) {
        return;
      }
      mqtt->publish(queue.c_str(), payload.c_str());
    }

    void loop() {
      if (!mqtt) {
        return;
      }
      mqtt->loop();
      const long currentTime = millis();
      const long secondsSinceLastDiscovery = (currentTime - lastDiscoverySent)/1000;
      if (secondsSinceLastDiscovery > discoveryPeriod/3) {
        for (const auto part : participants ) {
          if (!part or !part->valid()) {
            continue;
          }
          sendDiscovery(part);
        }
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

    void callback(const char* topic, byte* payload, unsigned int length) {
      for (const auto part : participants) {
        if (!part or !part->valid()) {
          continue;
        }
        for (const auto &p : part->inPorts) {
            if (p.queue == topic) {
                p.callback(payload, length);
            }
        }
      }
    }

private:
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
      for (int i=0; i < p->outPorts.size(); i++) {
        const auto &port = p->outPorts[i];
        if (port.valid()) {
            if (i > 0) {
                discoveryMessage += ",";
            }
            port.toJson(discoveryMessage);
        }
      }
      discoveryMessage += "],";

      discoveryMessage += "\"inports\": [";
      for (int i=0; i < p->inPorts.size(); i++) {
        const auto &port = p->inPorts[i];
        if (port.valid()) {
            if (i > 0) {
                discoveryMessage += ",";
            }
            port.toJson(discoveryMessage);
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
      for (const auto part : participants) {
        if (!part or !part->valid()) {
          continue;
        }
        for (const auto &p : part->inPorts) {
            if (p.valid() and p.queue.length()) {
                mqtt->subscribe(p.queue.c_str());
            }
        }
      }
    }

    void registerParticipants() {
        for (auto part : participants) {
          if (!part or !part->valid()) {
            continue;
          }
          for (auto &p : part->inPorts) {
            if (!p.valid()) {
              break;
            }
            if(!p.queue.length()) {
              p.queue = defaultTopic(topicPrefix, part->role, p.id);
            }
          }

          for (auto &p : part->outPorts) {
            if (!p.valid()) {
              break;
            }
            p.publisher = this;
            if(!p.queue.length()) {
              p.queue = defaultTopic(topicPrefix, part->role, p.id);
            }
          }
        }
    }

    void onConnected() {
      registerParticipants();
      subscribeInPorts();

      for (const auto part : participants) {
        if (!part or !part->valid()) {
          continue;
        }
        const bool success = sendDiscovery(part);
        if (!success) {
            Serial.println("Failed to send Msgflo discovery");
            Serial.print("MQTT_MAX_PACKET_SIZE = ");
            Serial.println(MQTT_MAX_PACKET_SIZE);
        }
      }
    }

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

#ifdef ESP8266
static void globalCallback(const char* topic, byte* payload, unsigned int length) {
  instance->callback(topic, payload, length);
}
#else
static void globalCallback(char* topic, byte* payload, unsigned int length) {
  instance->callback(topic, payload, length);
}
#endif

Engine *createPubSubClientEngine(PubSubClient &mqtt) {
  if (instance != nullptr) {
    Serial.println("Double initialization of msgflo engine.");
    return instance;
  }

  instance = new (instanceBytes) PubSubClientEngine();
  instance->setClient(mqtt);
  return instance;
}

Engine *createPubSubClientEngine(const Participant &part, PubSubClient* mqtt,
    const char *clientId) {

  auto instance = createPubSubClientEngine(*mqtt);
  instance->setClientId(clientId);
  return instance;
}

Engine *createPubSubClientEngine(const Participant &part, PubSubClient* mqtt,
    const char *clientId, const char *username, const char *password) {

  auto instance = createPubSubClientEngine(*mqtt);
  instance->setClientId(clientId);
  instance->setCredentials(username, password);
  return instance;
}


};
};

