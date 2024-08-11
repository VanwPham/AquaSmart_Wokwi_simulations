#include <LiquidCrystal_I2C.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <WiFi.h>
#include <cstring>

using namespace std;

#define led_pin 13
#define tmp_pin 4
#define pH_pin 32

#define pubish_cycle 2000 //ms

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttID = "vankt23";
const char* publishTopic = "aqua/sensor";

const int oneWireBus = 4;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// default settings
float pH_upper_Limit = 8.5;
float pH_lower_Limit = 7;
float tmp_upper_Limit = 32;
float tmp_lower_Limit = 25;

bool warning = false;

WiFiClient espClient;
PubSubClient client(espClient);

float pH;
float tmp;

void connectToWiFi();
void connectToMQTT();
void readInfo();
void publish();
void getRange(char* msg, char* topic);
void callback(char* topic, byte* payload, unsigned int length) ;

void connectToWiFi(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectToMQTT(){
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(mqttID)) {
      Serial.println("connected");
      client.subscribe("aqua/sensor/tmpLimit");
      client.subscribe("aqua/sensor/pHLimit");
      client.subscribe("aqua/servo");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(1000);
    }
  }
}

void readInfo(){
  sensors.requestTemperatures();
  tmp = sensors.getTempCByIndex(0);
  pH = analogRead(pH_pin);
  pH = pH * 14.0 / 4095.0; // Chuyển đổi giá trị ADC 12-bit sang giá trị pH

  warning = (pH < pH_lower_Limit || pH > pH_upper_Limit
             || tmp < tmp_lower_Limit || tmp > tmp_upper_Limit);
}

void ledDisplay()
{
  if (warning) digitalWrite(led_pin, HIGH);
  else digitalWrite(led_pin, LOW);
}

void displayInfo(){
  readInfo();
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(2, 1);
  lcd.print(pH);
  lcd.setCursor(8, 1);
  lcd.print(tmp);
  ledDisplay();
}

void publish() {
  if (!client.connected()) {
    connectToMQTT();
  }
  char temperatureString[7];
  dtostrf(tmp, 1, 2, temperatureString);

  char pHString[7];
  dtostrf(pH, 1, 2, pHString);

  char combinedString[16];
  snprintf(combinedString, sizeof(combinedString), "%s,%s", temperatureString, pHString);

  client.publish(publishTopic, combinedString, 0);
}

void setup(){
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(2, 0);
  lcd.print("pH");
  lcd.setCursor(8, 0);
  lcd.print("temp(C)");

  pinMode(pH_pin, INPUT);
  pinMode(tmp_pin, INPUT);
  pinMode(led_pin, OUTPUT);

  sensors.begin();

  connectToWiFi();
  client.setServer(mqttServer, mqttPort);
  connectToMQTT();
  client.setCallback(callback);
}

void loop()
{
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  static int last_sent = millis();
  int current = millis();
  if (current - last_sent > pubish_cycle)
  { 
    last_sent = current;
    displayInfo();
    publish();
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{ 
  char* msg = new char[length+1];
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  for(int i = 0 ; i < length; i++) {msg[i] = (char)payload[i];}
  msg[length] = '\0';
  Serial.println(msg);
  
  if (strcmp(topic, "aqua/servo") == 0) {

  }
  else getRange(msg, topic);
  delete [] msg;
}

void getRange(char* msg, char* topic) {
  char* token = strtok(msg, ",");
  
  if (strcmp(topic, "aqua/sensor/pHLimit") == 0) 
  {
    pH_upper_Limit = stof(token);
    token = strtok(NULL, ",");
    pH_lower_Limit = stof(token);
  }
  else if (strcmp(topic, "aqua/sensor/tmpLimit") == 0)
  {
    tmp_upper_Limit = stof(token);
    token = strtok(NULL, ",");
    tmp_lower_Limit = stof(token);
  }
}