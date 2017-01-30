#pragma once

#include <Arduino.h>
#include <functional>

namespace msgflo {

struct Participant {
    const String component;
    const String label;
    const String icon;
    const String role;
    const String id;
};

class OutPort {
  public:
    virtual void send(const String &payload) = 0;
};

typedef std::function<void (byte*, int)> InPortCallback;

class InPort {
  public:
    // 
};

class Engine {
  protected:
    virtual ~Engine() {};
  public:
    virtual OutPort* addOutPort(const String &id, const String &type, const String &queue) = 0;
    virtual InPort* addInPort(const String &id, const String &type, const String &queue, InPortCallback callback) = 0;

    virtual void loop() = 0;
};

}; // namespace msgflo

#include <PubSubClient.h>

namespace msgflo {
namespace pubsub {

Engine* createPubSubClientEngine(const Participant &p, PubSubClient *client,
    const char *clientId, const char *username, const char *password);

}; // namespace pubsub
}; // namespace msgflo

