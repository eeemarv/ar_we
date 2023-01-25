#include <Arduino.h>
#include <.env.h>
#include <SPI.h>
#include <env_defaults.h>
#include <NTPClient.h>
#include <PubSubClient.h>
#include <EthernetENC.h>
#include <EthernetUdp.h>
#include <ArduinoJson.h>
#include <math.h>
#include <ESP8266WiFi.h>

#define REQUEST_STATUS_TIMER 0x01
#define REQUEST_STATUS_OWM 0x02
#define REQUEST_STATUS_THING 0x04
#define TIMER_RETRY 0

#define OWM_SERV "api.openweathermap.org"
#define OWM_LOC "/data/2.5/weather?units=metric&lang=nl&lat=" OWM_LAT "&lon=" OWM_LON "&appid=" OWM_APIKEY
#define THINGSPEAK_SERV "api.thingspeak.com"
#define THINGSPEAK_LOC "/update"
#define TEMP_MUL 100

#define REQUEST_STEP \
  requestInterval = REQUEST_INTERVAL_TIME; \
  lastRequest = millis(); \
  return;

#define REQUEST_RETRY \
  if (retryRequestCount){ \
    retryRequestCount--; \
    REQUEST_STEP; \
    return; \
  } \
  requestStatus = REQUEST_STATUS_TIMER; \
  return;

const byte mac[] = {0xDE, 0xAD, MAC_4_LAST};
const IPAddress mqttServerIp(MQTT_SERVER_IP);
EthernetUDP udp;
EthernetClient ethMqtt;
EthernetClient ethOwm;
EthernetClient ethThing;
NTPClient timeClient(udp, NTP_POOL, NTP_OFFSET, NTP_INTERVAL);
PubSubClient mqttClient(ethMqtt);
uint32_t mqttLastReconnectAttempt = 0;

uint8_t requestStatus = REQUEST_STATUS_TIMER;
uint32_t lastRequest = 0;
uint32_t requestInterval = 0;
uint8_t retryRequestCount;

uint8_t newOwmSample;
uint8_t lastOwmSample = -1;


DynamicJsonDocument owmDoc(2048);
StaticJsonDocument<400> owmFilter;
char postBody[1024];
char m1[12];

float airTemp;
bool waterTempReady = false;
bool airTempReady = false;
char waterTempChar[8];
int32_t waterTempAcc = 0;
int16_t waterTempBuff[WATER_TEMP_MAX_SAMPLES];
uint8_t waterTempBuffIndex = 0;
uint8_t waterTempBuffSize = 0;
uint32_t lastWaterTempSample = 0;
uint32_t lastAirTempSample = 0;
uint32_t lastWaterTempPub = 0;
uint32_t lastAirTempPub = 0;
uint32_t lastPresencePub = 0;

void mqttCallback(char *topic, uint8_t *payload, unsigned int length){
  int16_t newSample;
  uint8_t iii;

  char *msg = reinterpret_cast<char *>(payload);

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (iii = 0; iii < length; iii++){
    Serial.print(msg[iii]);
  }
  Serial.println();

  if (strcmp(topic, SUB_WATER_TEMP) == 0){
    msg[length] = '\0';
    newSample = (int16_t) floor(atof(msg) * TEMP_MUL);
    waterTempAcc += newSample;
    if (waterTempBuffSize == WATER_TEMP_MAX_SAMPLES){
      waterTempAcc -= waterTempBuff[waterTempBuffIndex];
    }
    waterTempBuff[waterTempBuffIndex] = newSample;
    waterTempBuffIndex++;
    if (waterTempBuffIndex == WATER_TEMP_MAX_SAMPLES){
      waterTempBuffIndex = 0;
    }
    if (waterTempBuffSize < WATER_TEMP_MAX_SAMPLES) {
      waterTempBuffSize++;
    }

    if (waterTempBuffSize >= WATER_TEMP_MIN_SAMPLES) {
      waterTempReady = true;
      lastWaterTempSample = millis();
    }

    return;
  }
}

inline bool mqttReconnect()
{
  uint8_t iii;
  uint8_t charPos;
  char clientId[10] = CLIENT_ID_PREFIX;
  for (iii = 0; iii < 4; iii++){
    charPos = strlen(clientId);
    clientId[charPos] = '0' + random(0, 10);
    clientId[charPos + 1] = '\0';
  }
  if (mqttClient.connect(clientId)){
    mqttClient.subscribe(SUB_WATER_TEMP);
  }
  return mqttClient.connected();
}

inline void calcWaterTempChar(){
  uint16_t div;
  float fl;
  div = waterTempBuffSize * TEMP_MUL;
  fl = ((float) waterTempAcc) / ((float) div);
  snprintf(waterTempChar, sizeof(waterTempChar), "%.2f", fl);
}

char *floatToChar(float fl){
  snprintf(m1, sizeof(m1), "%.2f", fl);
  return m1;
}

char *intToChar(int iii){
  itoa(iii, m1, 10);
  return m1;
}

void setup() {
  delay(200);
  Serial.begin(SERIAL_BAUD);

  while (!Serial){
    // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("ESP8266 Core version: ");
  Serial.println(ESP.getCoreVersion());
  Serial.println("ESP8266 CPU clock freq:");
  Serial.print(ESP.getCpuFreqMHz());
  Serial.println(" Mhz");

  Serial.println("Turning WiFi Off");
  WiFi.mode(WIFI_OFF);

  Ethernet.init(ETH_CS_PIN);

  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(SPI_FREQ);

  Serial.println("Reset Wiz Ethernet...");
  pinMode(ETH_RST_PIN, OUTPUT);
  digitalWrite(ETH_RST_PIN, HIGH);
  delay(500);
  digitalWrite(ETH_RST_PIN, LOW);
  delay(50);
  digitalWrite(ETH_RST_PIN, HIGH);
  delay(500);

  Serial.println("Initialize Ethernet with DHCP:");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
      while (true) {
        delay(1); 
      }
    }
    if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
  }

  Serial.print("  DHCP assigned IP ");
  Serial.println(Ethernet.localIP());

  timeClient.begin();

  mqttClient.setServer(mqttServerIp, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  owmFilter["main"]["temp"] = true;
  owmFilter["main"]["pressure"] = true;
  owmFilter["main"]["humidity"] = true;
  owmFilter["wind"]["speed"] = true;
  owmFilter["rain"]["1h"] = true;
  owmFilter["clouds"]["all"] = true;

  requestStatus = REQUEST_STATUS_TIMER;
}

void loop() {
  DeserializationError deserErr;
  char httpResponseStatus[32] = {0};
  char endOfHeaders[] = "\r\n\r\n";

  Ethernet.maintain();

  if (mqttClient.connected()){

    if (millis() - lastWaterTempPub > WATER_TEMP_PUB_INTERVAL){
      if (waterTempReady){
        if (millis() - lastWaterTempSample > WATER_TEMP_VALID_TIME){
          waterTempReady = false;
          waterTempBuffIndex = 0;
          waterTempBuffSize = 0;
          waterTempAcc = 0;
          Serial.println("waterTemp valid reset.");
        }
      }
      if (waterTempReady){
        calcWaterTempChar();
        Serial.print("buf: ");
        Serial.print(waterTempBuffSize);
        Serial.print(" idx: ");
        Serial.println(waterTempBuffIndex);
        Serial.print("pub we/water_temp ");
        Serial.println(waterTempChar);
        mqttClient.publish(PUB_WATER_TEMP, waterTempChar);
      }
      lastWaterTempPub = millis();
    }

    if (millis() - lastAirTempPub > AIR_TEMP_PUB_INTERVAL){
      if (airTempReady){
        if (millis() - lastAirTempSample > AIR_TEMP_VALID_TIME){
          airTempReady = false;
          Serial.println("airTemp valid reset.");
        }
      }
      if (airTempReady){
        Serial.print("pub we/air_temp ");
        Serial.println(airTemp);
        mqttClient.publish(PUB_AIR_TEMP, floatToChar(airTemp));
      }
      lastAirTempPub = millis();
    }

    if (millis() - lastPresencePub > PRESENCE_INTERVAL){
      Serial.println("pub we/p");
      mqttClient.publish(PUB_PRESENCE, "1");
      lastPresencePub = millis();
    }

    mqttClient.loop();
  } else {
    if (millis() - mqttLastReconnectAttempt > MQTT_CONNECT_RETRY_TIME) {
      if (mqttReconnect()){
        #ifdef SERIAL_EN
        Serial.println("connected");
        #endif
        mqttLastReconnectAttempt = 0;
      } else {
        #ifdef SERIAL_EN
        Serial.print("failed, rc=");
        Serial.print(mqttClient.state());
        Serial.print(" try again in ");
        Serial.print(MQTT_CONNECT_RETRY_TIME);
        Serial.println(" ms.");
        #endif
        mqttLastReconnectAttempt = millis();
      }
    }
  }

  timeClient.update();
  newOwmSample = timeClient.getMinutes() / (int)THINGSPEAK_OWM_INTERVAL_MIN;

  if (lastOwmSample < 0){
    lastOwmSample = newOwmSample;
  }

  if (newOwmSample != lastOwmSample){
    lastOwmSample = newOwmSample;
    Serial.print("Owm sample > ");
    Serial.print(timeClient.getHours());
    Serial.print(":");
    Serial.print(timeClient.getMinutes());
    Serial.print(":");
    Serial.println(timeClient.getSeconds());
    if (requestStatus != REQUEST_STATUS_TIMER){
      Serial.println("Request trigger not active.");
      return;
    }
    requestStatus = REQUEST_STATUS_OWM;
    retryRequestCount = OWM_RETRY;
    REQUEST_STEP;
  }

  if (!requestStatus){
    return;
  }

  if (requestStatus == REQUEST_STATUS_TIMER){
    return;
  }

  if (millis() - lastRequest < requestInterval){
    return;
  }

  if (requestStatus == REQUEST_STATUS_OWM){
    Serial.println(OWM_SERV OWM_LOC);

    ethOwm.setTimeout(OWM_TIMEOUT);
    if (!ethOwm.connect(OWM_SERV, 80)) {
      Serial.println("Owm Connection failed");
      REQUEST_RETRY;
    }
    Serial.println("Owm connected.");
    ethOwm.print("GET ");
    ethOwm.print(OWM_LOC);
    ethOwm.println(" HTTP/1.0");
    ethOwm.println("Connection: close");
    if (ethOwm.println() == 0) {
      Serial.println("Failed to send request");
      ethOwm.stop();
      REQUEST_RETRY;
    }

    ethOwm.readBytesUntil('\r', httpResponseStatus, sizeof(httpResponseStatus));
    if (strcmp(httpResponseStatus, "HTTP/1.1 200 OK") != 0) {
      Serial.print("Unexpected response: ");
      Serial.println(httpResponseStatus);
      ethOwm.stop();
      REQUEST_RETRY;
    }

    // Skip HTTP headers
    if (!ethOwm.find(endOfHeaders)) {
      Serial.println("Invalid response");
      ethOwm.stop();
      REQUEST_RETRY;      
    }

    // Parse JSON object
    deserErr = deserializeJson(owmDoc, ethOwm, DeserializationOption::Filter(owmFilter));
    if (deserErr) {
      Serial.print("deserializeJson() failed: ");
      Serial.println(deserErr.f_str());
      ethOwm.stop();
      REQUEST_RETRY;
    }

    ethOwm.stop();

    if (owmDoc.isNull()){
      Serial.println("owmDoc is null.");
      REQUEST_RETRY;
    }

    Serial.println("owmDoc: ");
    serializeJsonPretty(owmDoc, Serial);
    if (owmDoc["main"]["temp"] == "null"){
      Serial.println("owm air temp is null.");
      REQUEST_RETRY;
    }

    airTemp = owmDoc["main"]["temp"].as<float>();
    lastAirTempSample = millis();
    airTempReady = true;

    requestStatus = REQUEST_STATUS_THING;
    retryRequestCount = THINGSPEAK_RETRY;
    REQUEST_STEP;
  }

  if (requestStatus == REQUEST_STATUS_THING){
    if (!waterTempReady || !airTempReady){
      Serial.println("no POST to Thingspeak channel ");
      Serial.print("waterTempReady: ");
      Serial.println(waterTempReady ? "yes" : "no");
      Serial.print("airTempReady: ");
      Serial.println(airTempReady ? "yes" : "no");

      requestStatus = REQUEST_STATUS_TIMER;
      return;
    }

    calcWaterTempChar();
    strcpy(postBody, "api_key=");
    strcat(postBody, THINGSPEAK_APIKEY);
    strcat(postBody, "&field1=");
    strcat(postBody, waterTempChar);
    strcat(postBody, "&field2=");
    strcat(postBody, floatToChar(airTemp));
    strcat(postBody, "&field3=");
    strcat(postBody, intToChar(owmDoc["main"]["pressure"].as<signed int>()));
    strcat(postBody, "&field4=");
    strcat(postBody, intToChar(owmDoc["main"]["humidity"].as<signed int>()));
    strcat(postBody, "&field5=");
    strcat(postBody, floatToChar(owmDoc["wind"]["speed"].as<float>()));
    strcat(postBody, "&field6=");
    strcat(postBody, floatToChar(owmDoc["rain"]["1h"].as<float>()));
    strcat(postBody, "&field7=");
    strcat(postBody, intToChar(owmDoc["clouds"]["all"].as<signed int>()));

    Serial.println("POST to thingspeak.com channel: ");
    Serial.println(postBody);

    ethThing.setTimeout(THINGSPEAK_TIMEOUT);
    if (!ethThing.connect(THINGSPEAK_SERV, 80)) {
      Serial.println("Thingspeak Connection failed");
      REQUEST_RETRY;
    }
    Serial.println("Thingspeak connected.");
    ethThing.print("POST ");
    ethThing.print(THINGSPEAK_LOC);
    ethThing.println(" HTTP/1.0");
    ethThing.println("Connection: close");
    ethThing.println("Content-Type: application/x-www-form-urlencoded");
    ethThing.print("Content-Length:");
    ethThing.println(strlen(postBody));
    if (ethThing.println() == 0) {
      Serial.println("Failed to send request");
      ethThing.stop();
      REQUEST_RETRY;
    }
    ethThing.print(postBody);
    ethThing.flush();
    ethThing.stop();

    requestStatus = REQUEST_STATUS_TIMER;
  }
}
