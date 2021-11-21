#include <ArduinoJson.h>
//https://arduinojson.org/
//6.18.4
#include <PubSubClient.h>
//https://pubsubclient.knolleary.net/
//V 2.8.0
#include <NTPClient.h>
// https://github.com/arduino-libraries/NTPClient
// v 3.2.0
#include "uptime.h"
//https://github.com/YiannisBourkelis/Uptime-Library
// V 1.0.0
#include <WiFiUdp.h>
#include <WiFiClient.h>
#include <WiFi.h>


#include <NewPing.h>
// https://bitbucket.org/teckel12/arduino-new-ping/wiki/Home
// V 1.9.1

#include "version.h" // Auto Incremented Version store
#include "config.h" // Global vars and Secrets

// https://randomnerdtutorials.com/esp32-hc-sr04-ultrasonic-arduino/
// https://www.arduino.cc/reference/en/language/functions/advanced-io/pulsein/

// Max expected distance is about 51-52 inches

void mqttConnect() {
  Serial.print("Attempting MQTT connection...");
  char mqtt_log_message[256];
  long unsigned int mqtt_log_time;
  if (mqtt_client.connect(mqttClientName, mqttUsername, mqttPassword, willTopic, willQoS, willRetain, willMessage)) {
      mqtt_log_time = timeClient.getEpochTime();
      if (wasConnected == 1){
      // We where connected but lost connection for some reason.
      sprintf(mqtt_log_message, "Lost Connection to mqtt, attempted %d times to reconnect, new connection made at %d", mqtt_failed_connection_attempts, mqtt_log_time);
      }else{
      sprintf(mqtt_log_message, "Made first connection, clearing previous responses but not logs.");
      mqtt_client.publish(mqtt_response_topic, "");

      }
      mqttLog(mqtt_log_message);
      mqtt_failed_connection_attempts = 0;
      mqtt_client.publish(willTopic, "online", true);
      Serial.println(willTopic);
      // ...
      // Subcribe here.
      mqtt_client.subscribe(json_command_topic);
      // ...
      mqtt_client.loop();
  }else{
      mqtt_failed_connection_attempts++;
      Serial.print("failed, rc = ");
      Serial.print(mqtt_client.state());
      Serial.println("Waiting for 5 seconds before trying again.");
      delay(5000);
  }
}

void mqttLog(const char* str) {
  if (mqtt_client.connected()){
    DynamicJsonDocument doc(256);
    timeClient.update();
    int mqtt_log_time = timeClient.getEpochTime();
    doc["time"] = mqtt_log_time;
    doc["msg"] = str;
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    mqtt_client.publish(mqtt_log_topic, buffer, n);
  }else{
    // print to serial
    // also figure out a way to store.
  }
}

void callback(char *topic, byte *payload, unsigned int length) {
  StaticJsonDocument<1024> r_doc;
  deserializeJson(r_doc, payload, length);
  timeClient.update();
  DynamicJsonDocument doc(256);
  doc["rx_t"] = timeClient.getEpochTime();
  //Overwrite if we *do* understand the command received.
  doc["msg"] = "Received Command but didn't understand";

  if (r_doc["set_polling"]){
    polling_rate = r_doc["set_polling"].as<int>()*1000;
    char mqtt_log_message[64];
    sprintf(mqtt_log_message, "Setting the polling rate at %d seconds" , polling_rate/1000);
    doc["msg"] = mqtt_log_message;
  }

  if (r_doc["set_info"]) {
    // Validate Input as int
    if (r_doc["set_info"].as<int>() != 0){
      send_info_rate = r_doc["set_info"].as<int>()*1000;
      char mqtt_log_message[64];
      sprintf(mqtt_log_message, "Setting the info send rate at %d seconds" , send_info_rate/1000); //38+10
      doc["msg"] = mqtt_log_message;
    }
  }

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  mqtt_client.publish(mqtt_response_topic, buffer, n);
}

//Takes and sends readings
void take_ultrasonic_reading(){
  if (currentMillis - last_poll_Millis >= polling_rate ) {
    last_poll_Millis = currentMillis;
    timeClient.update();
    int epochDate = timeClient.getEpochTime();
    Serial.println(epochDate);

    digitalWrite(triggerPin, LOW);
    delayMicroseconds(20);
    digitalWrite(triggerPin, HIGH);
    // Min delay here is 10 seems like a higher number gives more stable readings
    delayMicroseconds(100);
    digitalWrite(triggerPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    // Calculate the distance
    distanceCm = duration * SPEED_OF_SOUND/2;
    // Convert to inches
    distanceInch = distanceCm * CM_TO_INCH;

    DynamicJsonDocument doc(256);
    doc["t"] = epochDate;
    doc["duration"] = duration;
    // doc["duration"] = sonar.ping_median(25);
    doc["CM"] = distanceCm;
    // doc["CM"] = sonar.convert_cm(doc["duration"]);
    doc["Inch"] = distanceInch;
    // doc["Inch"] = sonar.convert_in(doc["duration"]);
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    mqtt_client.publish(water_level_topic, buffer, n);
  }
}
//Sends info about device and network connection
void send_info(){
  if (currentMillis - last_info_Millis >= send_info_rate){
    last_info_Millis = currentMillis;
    timeClient.update();
    int epochDate = timeClient.getEpochTime();
    DynamicJsonDocument doc(512);
    doc["t"] = epochDate; //1+10
    doc["Wifi IP"] = ip_char; //7+16
    doc["Code Version"] = _VERSION; //12+21
    doc["polling_rate"] = polling_rate/1000; //12+8 1000 days is 8 digits in seconds
    doc["info_rate"] = send_info_rate/1000; //9+8t
    uptime::calculateUptime();
    char readable_time[12];
    sprintf(readable_time, "%02d:%02d:%02d:%02d", uptime::getDays(),uptime::getHours(),uptime::getMinutes(),uptime::getSeconds());
    doc["Uptime"] = readable_time; //6+12
    //155
    char buffer[256];
    size_t n = serializeJson(doc, buffer);
    mqtt_client.publish(mqtt_info_topic, buffer, n);
    mqtt_client.loop();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.print("Connecting to: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" WiFi Connected!");
  // Generate some info once for info topic
  IPAddress ip = WiFi.localIP();
  Serial.print(ip);
  Serial.println("/");
  sprintf(ip_char, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);

  pinMode(triggerPin, OUTPUT);
  pinMode(echoPin, INPUT);

  mqtt_client.setServer(mqttServerIp, mqttServerPort);
  mqtt_client.setCallback(callback);
  while (!mqtt_client.connected()) {
    mqttConnect();
  }

}

void loop() {
  if (!mqtt_client.connected()) {
    mqttConnect();
  }
  currentMillis = millis();
  take_ultrasonic_reading();
  send_info();
  mqtt_client.loop();
}
