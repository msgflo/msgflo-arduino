# msgflo-arduino

[Arduino](https://www.arduino.cc/) library for easily exposing devices on [MQTT](https://en.wikipedia.org/wiki/MQTT),
with [MsgFlo](https://msgflo.org) device discovery support.
MsgFlo allows you to visually connect devices using [Flowhub](https://flowhub.io), to easily build "IoT" systems
like home or factory automation, interactive art installations or similar.

## Status

*Minimally useful*

* Tested on several ESP8266 boards, like NodeMCU, Wemos D1, Olimex MOD-WIFI-ESP8266.
* Should work on all devices with `EthernetClient` support
* Supports Arduino IDE 1.5+
* Note: API may still change

## Prerequisites

[PubSubClient](https://github.com/knolleary/pubsubclient).

Note: For discovery message you have to increase `MQTT_MAX_PACKET_SIZE` to 512 or 1024.
This can only be done inside `PubSubClient.h`, until new [API is added](https://github.com/knolleary/pubsubclient/pull/282) for changing.

## Installing

1. [Download](https://github.com/msgflo/msgflo-arduino/archive/master.zip)
2. Unzip the archive
3. Move the `msgflo-arduino-master` directory to your `Arduino/libraries` folder
4. Rename the `msgflo-arduino-master` to `Msgflo`

## Usage

1. Open your Arduino IDE
2. Open one of the included example sketches. `File -> Examples -> Msgflo -> button`

## License
`MIT`
