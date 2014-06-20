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
byte ip[] = { 192, 168, 1, 200 }; //IP addrs

char macstr[18];

/*
* husets mätare sitter på kanal 1 chan1_pin
*  värmepumpen sitter på kanal 2 chan2_pin  
*/


const int chan1_pin = 1;
//const int chan2_pin = 0; // STOFFE rätt pinne
//My meter flashes 1000 times per kWh            
const float w_per_pulse = 1;   

todo: number of pulses for chan2 /kwh...is not correct

const unsigned long ms_per_hour = 3600000UL;

const int update_time = 5000; //in ms

unsigned int chan1_count = 0;
unsigned int chan2_count = 0;
unsigned long report_time = 0;
unsigned long chan1_first_pulse = 0;
unsigned long chan1_last_pulse = 0;
volatile byte chan1_pulse = 0;
unsigned long chan2_first_pulse = 0;
unsigned long chan2_last_pulse = 0;
volatile byte chan2_pulse = 0;


EthernetClient ethClient;
PubSubClient client("192.168.1.110", 1883, callback, ethClient);

void callback(char* topic, byte* payload, unsigned int length) {
}

void chan1_isr() {
  chan1_pulse = 1;
}

void chan2_isr() {
  chan2_pulse = 1;
}


boolean connect_mqtt() {
  Serial.print("MQTT...");
  if (client.connect(macstr)) {
    Serial.println("connected");
    char topic[35];
    snprintf(topic, 35, "house/node/%s/state", macstr);
    client.publish(topic, "start");
   Serial.println("OK");
    return true;
  }
  Serial.println("Failed.");
  return false;
}

void setup() {
 pinMode(chan1_pin, INPUT);
 // pinMode(chan2_pin, INPUT);
  attachInterrupt(0, chan1_isr, FALLING);
  attachInterrupt(1, chan2_isr, FALLING); //STOFFE kolla upp vilken pinns
  Serial.begin(9600);
  delay(1000);
  snprintf(macstr, 18, "%02x:%02x:%02x:%02x:%02x:%02x",
    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.print(macstr);
  Serial.print(").version:0.5 ..");
  Ethernet.begin(mac,ip);
//  Serial.println(Ethernet.localIP());
}

void loop() {
  char topic[35];
  char value[6];
  static unsigned int chan1_watts;
  static unsigned int chan2_watts;
  unsigned long chan1_delta; // Time since last pulse on chan1
  unsigned long chan2_delta; // Time since last pulse on chan2

  if (!client.connected() && !connect_mqtt()) {
    return;
  }
  client.loop();

  if (millis() - chan1_last_pulse > 60000) {
    chan1_watts = 0;
    chan2_watts = 0;
  }


  if (millis() - report_time > 5000) {

    chan1_delta = chan1_last_pulse - chan1_first_pulse;
    if (chan1_delta && chan1_count) {
      chan1_watts = (chan1_count - 1) * w_per_pulse * ms_per_hour / chan1_delta;
//      chan1_kwh=(chan1_watts*(update_time/1000))/(1000*3600);  /* räknar i sekunder */
      chan1_count = 0;
    }

    chan2_delta = chan2_last_pulse - chan2_first_pulse;
    if (chan2_delta && chan2_count) {
      chan2_watts = (chan2_count - 1) * w_per_pulse * ms_per_hour / chan2_delta;
//      chan2_kwh=(chan2_watts*(update_time/1000))/(1000*3600);  /* räknar i sekunder */
      chan2_count = 0;
    }

    if (!(report_time == 0 && chan1_watts == 0 && chan2_watts == 0)) 
    {
      snprintf(topic, 35, "house/power/meter/all/current");
      snprintf(value, 6, "%d", chan1_watts);
      client.publish(topic, value);

      snprintf(topic, 35, "house/power/meter/heat/current");
      snprintf(value, 6, "%d", chan2_watts);
      client.publish(topic, value);

       Serial.print("Chan1 ");
      Serial.println(chan1_watts);
      Serial.print("Chan2 ");
      Serial.println(chan2_watts);
      report_time = millis();
    }
  }

  if (chan1_pulse == 1) {
     Serial.println("chan1 puls");
    chan1_count++;
    chan1_pulse = 0;

    chan1_last_pulse = millis();
    if (chan1_count == 1) { // was reset
      chan1_first_pulse = chan1_last_pulse;
    }

    snprintf(topic, 35, "house/power/meter/all/pulse");
    client.publish(topic, "1");
  }
  if (chan2_pulse == 1) {
    Serial.println("chan2 puls");
    chan2_count++;
    chan2_pulse = 0;

    chan2_last_pulse = millis();
    if (chan2_count == 1) { // was reset
      chan2_first_pulse = chan2_last_pulse;
    }

    snprintf(topic, 35, "house/power/meter/heat/pulse");
    client.publish(topic, "1");
  }
  int dhcp_status = Ethernet.maintain();
  /*
    returns:
    0: nothing happened
    1: renew failed
    2: renew success
    3: rebind fail
    4: rebind success
  */
}
