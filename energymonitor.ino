/*
* By Hadley Rich 
* Released under the MIT license.
* Some ideas taken from arduino.cc example code.
*/

#include <stdio.h>
#include <SPI.h>
#include <Ethernet.h>
#include <PubSubClient.h>

// MAC address from Ethernet shield sticker under board
byte mac[6] = { 0x90, 0xA2, 0xDA, 0x0E, 0xA8, 0x54 };
IPAddress ip(192, 168, 1, 200); // IP address, may need to change depending on network

char macstr[18];

const int chan1_pin = 1;
//My meter flashes 1000 times per kWh
const float w_per_pulse = 1;
const unsigned long ms_per_hour = 3600000UL;

unsigned int chan1_count = 0;
unsigned long report_time = 0;
unsigned long chan1_first_pulse = 0;
unsigned long chan1_last_pulse = 0;
volatile byte chan1_pulse = 0;


EthernetClient ethClient;
PubSubClient client("192.168.1.110", 1883, callback, ethClient);

void callback(char* topic, byte* payload, unsigned int length) {
}

void chan1_isr() {
  chan1_pulse = 1;
}

boolean connect_mqtt() {
  Serial.print("MQTT...");
  if (client.connect(macstr)) {
    Serial.println("connected");
    char topic[35];
    snprintf(topic, 35, "house/node/%s/state", macstr);
    client.publish(topic, "start");
    return true;
  }
  Serial.println("Failed.");
  return false;
}

void setup() {
  pinMode(chan1_pin, INPUT);
  attachInterrupt(1, chan1_isr, FALLING);
  Serial.begin(9600);
  delay(1000);µ
  snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(macstr);
  Serial.print(")...");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed.");
    while(true);
  }
  Serial.println(Ethernet.localIP());
}

void loop() {
  char topic[35];
  char value[6];
  static unsigned int chan1_watts;
  unsigned long chan1_delta; // Time since last pulse

  if (!client.connected() && !connect_mqtt()) {
    return;
  }
  client.loop();

  if (millis() - chan1_last_pulse > 60000) {
    chan1_watts = 0;
  }


  if (millis() - report_time > 5000) {
    chan1_delta = chan1_last_pulse - chan1_first_pulse;
    if (chan1_delta && chan1_count) {
      chan1_watts = (chan1_count - 1) * w_per_pulse * ms_per_hour / chan1_delta;
      chan1_count = 0;
    }

    if (!(report_time == 0 && chan1_watts == 0)) {
      snprintf(topic, 35, "house/power/meter/1/current");
      snprintf(value, 6, "%d", chan1_watts);
      client.publish(topic, value);
      Serial.print("Chan1 ");
      Serial.println(chan1_watts);
 
      report_time = millis();
    }
  }

  if (chan1_pulse == 1) {
    chan1_count++;
    chan1_pulse = 0;

    chan1_last_pulse = millis();
    if (chan1_count == 1) { // was reset
      chan1_first_pulse = chan1_last_pulse;
    }

    snprintf(topic, 35, "house/power/meter/1/usage");
    client.publish(topic, "1");
  }
}
