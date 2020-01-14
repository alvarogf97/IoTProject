#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <cppQueue.h>
#include <ArduinoJson.h>

#include <Wire.h>
#include "SSD1306Wire.h"

#define DATA_QUEUE_SIZE 10

SSD1306Wire display(0x3c, SDA, SCL);

/* Define our data struct and our class queue */
struct sensor {
  int value;
  const char* magnitude;
};

struct screen {
  const char* display_type;
};

DataQueue<sensor> temperature_queue(DATA_QUEUE_SIZE);
DataQueue<sensor> humidity_queue(DATA_QUEUE_SIZE);
DataQueue<sensor> wind_queue(DATA_QUEUE_SIZE);
DataQueue<screen> screen_queue(DATA_QUEUE_SIZE);

/* Define JSON document 

It is necessary to define the JSONDocument capcity, we
can use JSON_OBJECT_SIZE macro to get this capacity, based
on the number of objects and fields in the expected JSON.
So ``JSON_OBJECT_SIZE(X)` returns the needed capacity for
store a JSON object with single object which has X fields.

Our JSON structures will be:

  - temperature || humidity || wind topics:
    {
      "data": {
        "value": "int",
        "magnitude": "string"
      }
    }
  - display topic:
    {
      "data": {
        "display_type": "string"
      }
    }
  - ack topic:
    {
      "data": {
        "ack_type": "string",
        "last_value": "string"
      }
    }
*/
const int sensor_json_capacity = JSON_OBJECT_SIZE(1)*3;
const int display_json_capacity = JSON_OBJECT_SIZE(1)*2;
StaticJsonDocument<sensor_json_capacity> temperature_json_doc;
StaticJsonDocument<sensor_json_capacity> humidity_json_doc;
StaticJsonDocument<sensor_json_capacity> wind_json_doc;
StaticJsonDocument<display_json_capacity> screen_json_doc;

/* Define Wifi settings */
const char* wifi_ssid = "MASTER";
const char* wifi_password = "malagaiot";

/* Define MQTT settings */
const char* mqtt_server = "iot.ac.uma.es";
const char* mqtt_user = "master";
const char* mqtt_pass = "malagaiot";  
const char* mqtt_topic_ack = "master/GRUPOE/ack"
const char* mqtt_topic_temperature = "master/GRUPOE/temperatura";
const char* mqtt_topic_humidity = "master/GRUPOE/humidity";
const char* mqtt_topic_wind = "master/GRUPOE/wind";
const char* mqtt_topic_screen = "master/GRUPOE/screen";
const char* mqtt_topic_device = "master/GRUPOE/device";
const char* will_message = "Godbye";
char mqtt_message[100];
char mqtt_ack[100];

/* Define PubSub classes */
WiFiClient espClient;
PubSubClient client(espClient);

/* Latest data saved */
String lastTemperature;
String lastHumidity;
String lastWind;

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(handle_msg);
  snprintf(mqtt_message, 50, "ESP_%d", ESP.getChipId());

  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
}

void loop() {
 /*
  *  Get the data of every queue and display them in each iteration,
  *  If there is no data to display, the Oled will be turned off in 
  *  order to save battery.
  */
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if(!temperature_queue.isEmpty()){
    struct sensor temperature = temperature_queue.dequeue();
    /* Draw the sensor data */
    Serial.print("Temperatura: ");
    Serial.print(temperature.value);
    Serial.println(temperature.magnitude);
    lastTemperature = String(temperature.value)+String(temperature.magnitude);
    send_ack("temperature", lastTemperature);
  }

  if(!humidity_queue.isEmpty()){
    struct sensor humidity = humidity_queue.dequeue();
    /* Draw the sensor data */
    Serial.print("Humedad: ");
    Serial.print(humidity.value);
    Serial.println(humidity.magnitude);
    lastHumidity = String(humidity.value)+String(humidity.magnitude);
    send_ack("humidity", lastHumidity);
  }

  if(!wind_queue.isEmpty()){
    struct sensor wind = wind_queue.dequeue();
    /* Draw the sensor data */
    Serial.print("Velocidad del viento: ");
    Serial.print(wind.value);
    Serial.println(wind.magnitude);
    lastWind = String(wind.value)+String(wind.magnitude);
    send_ack("wind", lastWind);
  }

  if(!screen_queue.isEmpty()){
    struct screen display = screen_queue.dequeue();
    /* Change the display type */
    Serial.println("La screen del nintendo: ");
    Serial.println(display.display_type);
    if(strcmp(display.display_type, "0") == 0){
      String headers[] = {"Temperatura: ","Humedad: ","Viento: "};
      String values[] = {lastTemperature,lastHumidity,lastWind};
      drawAllDataFullScreen(headers, values);
    }else if(strcmp(display.display_type, "1") == 0){
      String header = "Temperatura: ";
      String value = lastTemperature;
      drawSingleDataFullScreen(header, value);
    }else if(strcmp(display.display_type, "2") == 0){
      String header = "Humedad: ";
      String value = lastHumidity;
      drawSingleDataFullScreen(header, value);
    }else if(strcmp(display.display_type, "3") == 0){
      String header = "Viento: ";
      String value = lastWind;
      drawSingleDataFullScreen(header, value);
    }
  }

  String headers[] = {"Temperatura: ","Humedad: ","Viento: "};
  String values[] = {lastTemperature,lastHumidity,lastWind};
  drawAllDataFullScreen(headers, values);
}


/****************************
 *           WIFI           * 
 **************************** 
*/
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

/****************************
 *        MQTT BROKER       * 
 **************************** 
*/
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_message, mqtt_user, mqtt_pass, mqtt_topic_device, 0, 1, will_message, false)) {
      client.subscribe(mqtt_topic_temperature);
      client.subscribe(mqtt_topic_humidity);
      client.subscribe(mqtt_topic_wind);
      client.subscribe(mqtt_topic_screen);
      Serial.println("connected");
      snprintf(mqtt_message, 50, "connected");
      client.publish(mqtt_topic_device, mqtt_message, true);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

/****************************
 *     Send ACK message     * 
 **************************** 
*/
void send_ack(char* type, char* last_value){
  snprintf(mqtt_ack, 100, "{\"data\": {\"ack_type\":\"%s\", \"last_value\": \"%s\"}}", type, last_value);
  client.publish(mqtt_topic_ack, payload);
}

/****************************
 *    Handle MQTT msgs      * 
 **************************** 
*/
void handle_msg(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  char *input = (char*)payload;
  if(strcmp(topic, mqtt_topic_temperature) == 0){
    deserializeJson(temperature_json_doc, input);
    struct sensor temperature;
    temperature.value = temperature_json_doc["data"]["value"];
    temperature.magnitude = temperature_json_doc["data"]["magnitude"];
    if(temperature_queue.isFull()){
      temperature_queue.dequeue();
    }
    temperature_queue.enqueue(temperature);
  } else if (strcmp(topic, mqtt_topic_humidity) == 0) {
    deserializeJson(humidity_json_doc, input);
    struct sensor humidity;
    humidity.value = humidity_json_doc["data"]["value"];
    humidity.magnitude = humidity_json_doc["data"]["magnitude"];
    if(humidity_queue.isFull()){
      humidity_queue.dequeue();
    }
    humidity_queue.enqueue(humidity);
  } else if (strcmp(topic, mqtt_topic_wind) == 0) {
    deserializeJson(wind_json_doc, input);
    struct sensor wind;
    wind.value = wind_json_doc["data"]["value"];
    wind.magnitude = wind_json_doc["data"]["magnitude"];
    if(wind_queue.isFull()){
      wind_queue.dequeue();
    }
    wind_queue.enqueue(wind);
  } else if (strcmp(topic, mqtt_topic_screen) == 0) {
    deserializeJson(screen_json_doc, input);
    struct screen data;
    data.display_type = screen_json_doc["data"]["display_type"];
    if(screen_queue.isFull()){
      screen_queue.dequeue();
    }
    screen_queue.enqueue(data);
  }
}

/****************************
 *    Show Topic data       * 
 **************************** 
*/
void drawSingleDataFullScreen(String headerData, String valueData) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 10, headerData);
    display.setFont(ArialMT_Plain_24);
    display.drawString(64, 26, valueData);
    display.display();
}

void drawAllDataFullScreen(String headersData[],String values[]) {
    display.clear();
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_10);
    display.drawString(64, 0, headersData[0]);
    display.drawString(64, 10, values[0]);
    display.drawString(64, 20, headersData[1]);
    display.drawString(64, 30, values[1]);
    display.drawString(64, 40, headersData[2]);
    display.drawString(64, 50, values[2]);
    display.display();
}
