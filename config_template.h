// Wifi Config
char *ssid = "";
const char *password = "";
WiFiClient espClient;
// MQTT Config
const char *mqttServerIp = "";
const short mqttServerPort = ;
const char *mqttUsername = "";
const char *mqttPassword = "";
const char *mqttClientName = "";
const char *water_level_topic = "/json";
const char *mqtt_log_topic = "/logs";
const char *mqtt_response_topic = "/response";
const char *json_command_topic = "/command";
#define willTopic "/status"
#define willMessage "Offline"
int mqtt_failed_connection_attempts;
int willQoS = 1;
boolean willRetain = 1;
PubSubClient mqtt_client(espClient);
bool wasConnected = 0;
//-----------
int polling_rate = 5;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "", 3600, 60000);
//UltraSonic Conifg
const int triggerPin = 27;
const int bounceBackPin = 26;
#define SPEED_OF_SOUND 0.034
#define CM_TO_INCH 0.393701
long duration;
float distanceCm;
float distanceInch;
