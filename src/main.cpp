#include <LiquidCrystal_I2C.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define led_pin 13
#define tmp_pin 4
#define pH_pin 32

#define pubish_cycle 2000 //ms

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;

const char* mqttID = "ESP32Client_test2303";

const char* Topic = "aqua/sensor";

const int oneWireBus = 4;

OneWire oneWire(oneWireBus);

DallasTemperature sensors(&oneWire);

LiquidCrystal_I2C lcd(0x27, 16, 2);

float pH_upper_Limit = 8.5;
float pH_lower_Limit = 7;
float tmp_upper_Limit = 32;
float tmp_lower_Limit = 25;

float pH;
float tmp;

bool warning = false;

WiFiClient espClient;
PubSubClient client(espClient);

void connectToWiFi();
void connectToMQTT();
void readInfo();
void sendToMQTT();

void connectToWiFi() {
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

void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect(mqttID)) {
      Serial.println("connected");
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
  if (warning) {
    digitalWrite(led_pin, HIGH);
  }
  else {
    digitalWrite(led_pin, LOW);
  }
}

void displayInfo(){
  readInfo();
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(2, 1);
  lcd.print(pH);
  lcd.setCursor(8, 1);
  lcd.print(tmp);

}

void sendToMQTT() {
  if (!client.connected()) {
    connectToMQTT();
  }
  char temperatureString[7];
  dtostrf(tmp, 1, 2, temperatureString);

  char pHString[7];
  dtostrf(pH, 1, 2, pHString);

  char combinedString[16];
  snprintf(combinedString, sizeof(combinedString), "%s,%s", temperatureString, pHString);

  client.publish(Topic, combinedString, 0);

  Serial.println(combinedString);
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
    sendToMQTT();
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{ 
    char msg = 0;
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");

    for(int i = 0 ; i < length; i++){ msg = (char)payload[i]; }
    Serial.println(msg);
    
    // if('1' == msg){ digitalWrite(LedPin, HIGH); }
    // else if('2' == msg){ digitalWrite(LedPin, LOW); }
}
