#include <LiquidCrystal_I2C.h>
#include <DallasTemperature.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <WiFi.h>
#include <cstring>
#include <ThingSpeak.h>
#include <ESP32Servo.h>
#include <queue>  // Sử dụng queue

using namespace std;

#define led_pin 13
#define tmp_pin 4
#define pH_pin 32
#define servo_pin 16

#define pubish_cycle 2000 //ms
// cập nhật dữ liệu lên thingspeak sau 7 phút rưỡi
#define thing_speak 450000 //ms 

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
const char* mqttID = "vankt23";
const char* publishSensorTopic = "aqua/sensor's";
const char* feedTopic = "aqua/Servo";

// Thingspeak config
const unsigned long myChannelID = 2624543;
const char* myWriteAPIKey = "HSEZ9JK5G263RF5U";
const char* myReadAPIKey = "YA3LFQ0FBOO1RHIW";

// Thông tin Thingspeak cho `amount`
const unsigned long amountChannelID = 2627892;
const char* amountWriteAPIKey = "1CEPY8UDF09GUFSH";

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

Servo fishFeederServo;
int feedTimes = 0;

// Queue lưu trữ các giá trị amount cần gửi
queue<int> amountQueue;

float pH;
float tmp;

void connectToWiFi();
void connectToMQTT();
void readInfo();
void publish();
void feedFish();
void publishLimits();
void getRange(char* msg, char* topic);
void callback(char* topic, byte* payload, unsigned int length);

void connectToWiFi(){
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
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
      client.subscribe("aqua/Servo");
    } else {
      Serial.print("failed with state ");
      Serial.println(client.state());
      delay(100);
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

  client.publish(publishSensorTopic, combinedString, 0);
}

void sendAmountFromQueue() {
  if (!amountQueue.empty()) {
    int amount = amountQueue.front();  // Lấy giá trị đầu tiên từ queue

    ThingSpeak.setField(1, amount);
    int result = ThingSpeak.writeFields(amountChannelID, amountWriteAPIKey);

    if (result == 200) {
      Serial.println("Amount sent successfully from queue");
      amountQueue.pop();  // Xóa giá trị đã gửi thành công khỏi queue
    } else {
      Serial.println("Error sending amount from queue, retrying...");
    }
  }
}

void feedFish() {
  static int last_call =  0;
  static bool opening = false;
  int current = millis();
  if (current - last_call < 1000) return;
  last_call = current;
  if (!opening)
  {
    // fishFeederServo.writeMicroseconds(1000); 
    fishFeederServo.write(0);
    opening = true;
  } 
  else 
  {
    // fishFeederServo.writeMicroseconds(1500);
    fishFeederServo.write(90);
    opening = false;
    feedTimes--;
  }
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
  fishFeederServo.attach(servo_pin);

  sensors.begin();
  ThingSpeak.begin(espClient);

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

  if (feedTimes > 0) {  
    feedFish();
  }

  static int last_sent = 0;
  int current = millis();
  if (current - last_sent >= pubish_cycle)
  { 
    last_sent = current;
    displayInfo();
    publish();
  }
  
  // Gửi amount từ queue lên ThingSpeak sau mỗi 7'30s
  static int thingSpeak_last_sent = 0;
  if (current - thingSpeak_last_sent >= thing_speak)
  {
    thingSpeak_last_sent = current;
    ThingSpeak.setField(1,tmp);
    ThingSpeak.setField(2,pH);
    int msg = ThingSpeak.writeFields(myChannelID, myWriteAPIKey);
    if (msg == 200){
      Serial.println("Successful");
    }
    else {
      Serial.println("Error");
    }
    sendAmountFromQueue();
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
  
  if (strcmp(topic, "aqua/Servo") == 0) {
    int amount = int(msg[0] - '0');
    feedTimes += amount;
    Serial.println(topic);
        amountQueue.push(amount);
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
