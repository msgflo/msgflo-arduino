#pragma once

#include <Arduino.h>

// The min/max macros in Arduino breaks functional, which uses std::min and std::max
// This may cause them to be undefined in user sketch.
// Can then change the sketch to use _min() and _max(), or re-define it below the Msgflo.h include
// https://github.com/esp8266/Arduino/issues/398
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <functional>

namespace msgflo {

class Publisher {
  public:
    virtual void send(const String &queue, const String &payload) = 0;
};

class OutPort {
  public:
    String id = "";
    String type;
    String queue;
    Publisher *publisher = nullptr;

  public:
    virtual void send(const String &payload) {
      if (publisher) {
        publisher->send(queue, payload);
      }
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

    bool valid() const {
        return id.length() && type.length();
    }
};

typedef std::function<void (byte*, int)> InPortCallback;

class InPort {
  public:
    String id = "";
    String type;
    String queue;
    InPortCallback callback;

  public:
    void toJson(String &s) const {
      s += "{\"id\": \"";
      s += id;
      s += "\", \"type\": \"";
      s += type;
      s += "\", \"queue\": \"";
      s += queue;
      s += "\"}";
    }

    bool valid() const {
        return id.length() && type.length() && callback ? true : false;
    }

};

#ifndef MSGFLO_MAX_PORTS
#define MSGFLO_MAX_PORTS 5
#endif

#ifndef MSGFLO_MAX_PARTICIPANTS
#define MSGFLO_MAX_PARTICIPANTS 20
#endif

struct Participant {

public:
  const String component;
  const String role;

  // Optional
  String label; // human readable description
  String icon; // name from http://fontawesome.io/icons/

  // id: the unique per-instance identifier. Set if there can be multiple devices of the same role 
  // Participants using the same role must be equivalent, meaning it does not matter which one gets input message or produces output
  String id;

  std::array<OutPort, MSGFLO_MAX_PORTS> outPorts;
  int8_t lastOutPort = 0;
  std::array<InPort, MSGFLO_MAX_PORTS> inPorts;
  int8_t lastInPort = 0;

public:
  Participant() 
    : component("")
    , role("")
  {
  }

  Participant(const String &c, const String &r)
    : component(c)
    , role(r)
    , lastOutPort(0)
    , lastInPort(0)
  {
  }

  OutPort* outport(const String &id, const String &type);
  InPort* inport(const String &id, const String &type, InPortCallback callback);

  bool valid() const {
    return (lastOutPort or lastInPort) and role; 
  }
};



class Engine {
  protected:
    virtual ~Engine() {};
  public:
    virtual OutPort* addOutPort(const String &id, const String &type, const String &queue) = 0;
    virtual InPort* addInPort(const String &id, const String &type, const String &queue, InPortCallback callback) = 0;

    virtual bool addParticipant(Participant &part) = 0;

    virtual void setClientId(const char *id) = 0;
    virtual void setCredentials(const char *user, const char *pw) = 0;

    virtual void setTopicPrefix(const char *prefix) = 0;

    virtual void loop() = 0;
};


class RunnableParticipant : public Participant {
public:
  RunnableParticipant(const String &c, const String &r)
    : Participant(c, r)
  {
  }

public:
  // Called during Arduino setup()
  virtual void setup() = 0;
  // Called during Arduino loop()
  virtual void loop() = 0;

public:
  virtual ~RunnableParticipant() {};
};

}; // namespace msgflo

#include <PubSubClient.h>

namespace msgflo {
namespace pubsub {

Engine* createPubSubClientEngine(const Participant &p, PubSubClient *client,
    const char *clientId, const char *username, const char *password);

Engine* createPubSubClientEngine(const Participant &p, PubSubClient *client,
    const char *clientId);

Engine* createPubSubClientEngine(PubSubClient &client);

}; // namespace pubsub
}; // namespace msgflo

