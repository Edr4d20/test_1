// V1_0   *add  BMP/BME280"sensore_temperatura-umidità" , BHI750"sensore_luce"   SDA=21  SCL=22
// V1_1   *add  ligthMaster_mqtt , OTA , state_Button

// V1_2   *fix OTA rm, availability MQTT, master state fix, device info clean

//////////////////////////////////////////////////
///////////     WIFI CONFIG      /////////////
//////////////////////////////////////////
#include <Arduino.h>
#include <WiFi.h>
extern "C" {
  #include "freertos/FreeRTOS.h"
  #include "freertos/timers.h"
}
#include <AsyncMqttClient.h>
#include <ArduinoJson.h>

#define MQTT_PORT 1883
AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

String ssid = "FASTWEB-A5XY76";
String pass = "MQTT3D9GLG";
String mqtt_ip = "192.168.68.201";
String mqtt_user = "edr4d20";
String mqtt_pass = "RockRider340";

const char* MQTT_STATUS_TOPIC = "esp32/status";
const char* DEVICE_ID = "esp32_chipid_main";
bool otaStarted = false;
///////////    END WIFI CONFIG       ////////////////

//////////////////////////////////////////////////
///////////     HELPER FUNCTIONS      /////////////
//////////////////////////////////////////
String getConfigUrl() {
  return "http://" + WiFi.localIP().toString() + "/update";
}

void addAvailability(DynamicJsonDocument &doc) {
  doc["availability_topic"] = MQTT_STATUS_TOPIC;
  doc["payload_available"] = "online";
  doc["payload_not_available"] = "offline";
}

void fillDeviceInfo(JsonObject deviceInfo, const char* name, const char* model) {
  deviceInfo["identifiers"] = DEVICE_ID;
  deviceInfo["name"] = name;
  deviceInfo["model"] = model;
  deviceInfo["manufacturer"] = "ESP32";
  if (WiFi.isConnected()) {
    deviceInfo["configuration_url"] = getConfigUrl();
  }
}

void publishAvailability(bool online) {
  mqttClient.publish(MQTT_STATUS_TOPIC, 0, true, online ? "online" : "offline");
}
///////////    END HELPER FUNCTIONS       ////////////////

//////////////////////////////////////////////////
///////////     SENSOR CONFIG  V1_0     /////////////
//////////////////////////////////////////
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <BH1750.h>

Adafruit_BME280 bme;
BH1750 lightMeter;
bool hasBME = false;
bool hasBH1750 = false;
const float TEMP_OFFSET = -2.0f;
//////////     END SENSOR CONFIG  V1_0     ////////////////

//////////////////////////////////////////////////
///////////     SENSOR FUNCTION  V1_0     /////////////
//////////////////////////////////////////
void mqtt_discoveryBME() {
  if (!hasBME) return;

  {
    DynamicJsonDocument doc(512);
    doc["name"] = "sensore_temperatura";
    doc["state_topic"] = "esp32/sensore_ambientale/state";
    doc["unit_of_measurement"] = "°C";
    doc["device_class"] = "temperature";
    doc["value_template"] = "{{ value_json.temperature }}";
    doc["unique_id"] = "esp32_sensore_temperatura";
    addAvailability(doc);
    JsonObject deviceInfo = doc.createNestedObject("device");
    fillDeviceInfo(deviceInfo, "ESP32 Sensore Ambiente", "BME280/BMP280");
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish("homeassistant/sensor/esp32_sensore_temperatura/config", 0, true, payload.c_str());
  }

  {
    DynamicJsonDocument doc(512);
    doc["name"] = "sensore_umidita";
    doc["state_topic"] = "esp32/sensore_ambientale/state";
    doc["unit_of_measurement"] = "%";
    doc["device_class"] = "humidity";
    doc["value_template"] = "{{ value_json.humidity }}";
    doc["unique_id"] = "esp32_sensore_umidita";
    addAvailability(doc);
    JsonObject deviceInfo = doc.createNestedObject("device");
    fillDeviceInfo(deviceInfo, "ESP32 Sensore Ambiente", "BME280/BMP280");
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish("homeassistant/sensor/esp32_sensore_umidita/config", 0, true, payload.c_str());
  }

  {
    DynamicJsonDocument doc(512);
    doc["name"] = "sensore_pressione";
    doc["state_topic"] = "esp32/sensore_ambientale/state";
    doc["unit_of_measurement"] = "hPa";
    doc["device_class"] = "pressure";
    doc["value_template"] = "{{ value_json.pressure }}";
    doc["unique_id"] = "esp32_sensore_pressione";
    addAvailability(doc);
    JsonObject deviceInfo = doc.createNestedObject("device");
    fillDeviceInfo(deviceInfo, "ESP32 Sensore Ambiente", "BME280/BMP280");
    String payload;
    serializeJson(doc, payload);
    mqttClient.publish("homeassistant/sensor/esp32_sensore_pressione/config", 0, true, payload.c_str());
  }
}

void mqtt_discoveryBH1750() {
  if (!hasBH1750) return;

  DynamicJsonDocument doc(512);
  doc["name"] = "sensore_luce";
  doc["state_topic"] = "esp32/sensore_luce/state";
  doc["unit_of_measurement"] = "lx";
  doc["device_class"] = "illuminance";
  doc["value_template"] = "{{ value_json.illuminance }}";
  doc["unique_id"] = "esp32_sensore_luce";
  addAvailability(doc);

  JsonObject deviceInfo = doc.createNestedObject("device");
  fillDeviceInfo(deviceInfo, "ESP32 RGBIC + Sensor", "BH1750");

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish("homeassistant/sensor/esp32_sensore_luce/config", 0, true, payload.c_str());
}

void mqtt_publishBME() {
  if (!hasBME || !mqttClient.connected()) return;

  float t = bme.readTemperature() + TEMP_OFFSET;
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0F;

  DynamicJsonDocument doc(256);
  doc["temperature"] = t;
  doc["humidity"] = h;
  doc["pressure"] = p;

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish("esp32/sensore_ambientale/state", 0, false, payload.c_str());
}

void mqtt_publishBH1750() {
  if (!hasBH1750 || !mqttClient.connected()) return;

  float lux = lightMeter.readLightLevel();
  DynamicJsonDocument doc(128);
  doc["illuminance"] = lux;

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish("esp32/sensore_luce/state", 0, false, payload.c_str());
}

void init_Sensor() {
  Wire.begin(21, 22);

  if (bme.begin(0x76)) {
    hasBME = true;
    Serial.println("BME280 trovato @0x76");
  } else if (bme.begin(0x77)) {
    hasBME = true;
    Serial.println("BME280 trovato @0x77");
  } else {
    Serial.println("BME280 non trovato");
  }

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    hasBH1750 = true;
    Serial.println("BH1750 trovato");
  } else {
    Serial.println("BH1750 non trovato");
  }
}
//////////     END SENSOR FUNCTION  V1_0     ////////////////

//////////////////////////////////////////////////
///////////     FASTLED CONFIG       /////////////
//////////////////////////////////////////
#include <FastLED.h>
#define CHIPSET   SM16703
#define COLOR_ORDER GRB

#define NUM_STRIPS 2
#define DATA_PIN_1  23
#define DATA_PIN_2  4
#define NUM_LEDS_1  76
#define NUM_LEDS_2  24

const uint8_t  ligth_dataPin[NUM_STRIPS] = { DATA_PIN_1, DATA_PIN_2 };
const uint16_t ligth_numLeds[NUM_STRIPS] = { NUM_LEDS_1, NUM_LEDS_2 };
const uint16_t ligth_numPixel[NUM_STRIPS] = { ligth_numLeds[0]*2 , ligth_numLeds[1]*2 };

CRGB light_leds1[NUM_LEDS_1];
CRGB light_leds2[NUM_LEDS_2];
CRGB* ligth_leds[NUM_STRIPS] = { light_leds1, light_leds2 };

#define NUM_LIGHT 4
const uint8_t  vr_ligth_srip[NUM_LIGHT] = { 0 , 1 , 1 , 1 };
const uint16_t vr_ligth_startIdx[NUM_LIGHT] = { 0 , 0 , 6 , 12 };
const uint16_t vr_ligth_segSize[NUM_LIGHT] = { 76 , 6 , 6 , 12 };

String ligth_masterDevice = "esp32-StrisciaLedAll";
static String ligth_device[NUM_LIGHT] = { "esp32-StrisciaLed1" , "esp32-StrisciaLed2" , "esp32-StrisciaLed3" , "esp32-StrisciaLed4" };
String ligth_currentEffect[NUM_LIGHT] = { "static", "static", "static", "static" };
bool   ligth_power[NUM_LIGHT] = { false, false, false, false };
bool   light_mode[NUM_LIGHT] = { true, true, true, true };
bool   ligth_effectReset[NUM_LIGHT] = { false, false, false, false };
uint8_t  ligth_brightness[NUM_LIGHT] = { 100, 100, 100, 100 };
uint8_t  ligth_sat[NUM_LIGHT] = { 255, 255, 255, 255 };
uint8_t  ligth_hue[NUM_LIGHT] = { 0, 0, 0, 0 };
uint8_t  ligth_effectSpeed[NUM_LIGHT] = { 20, 20, 20, 20 };
unsigned long  ligth_effectDelay[NUM_LIGHT] = { 0, 0, 0, 0 };
uint16_t ligth_colorTemp[NUM_LIGHT] = { 153, 153, 153, 153 };
unsigned long ligth_lastEffectUpdate[NUM_LIGHT] = { 0, 0, 0, 0 };
///////////     END FASTLED CONFIG       ////////////////

//////////////////////////////////////////////////
///////////     FASTLED FUNCTION       /////////////
//////////////////////////////////////////
void init_ledStrip() {
  for (int i = 0; i < NUM_LIGHT; i++) {
    ligth_power[i] = false;
    ligth_brightness[i] = 100;
    ligth_effectReset[i] = true;
  }

  FastLED.addLeds<CHIPSET, DATA_PIN_1, COLOR_ORDER>(ligth_leds[0], ligth_numLeds[0]);
  FastLED.addLeds<CHIPSET, DATA_PIN_2, COLOR_ORDER>(ligth_leds[1], ligth_numLeds[1]);
  FastLED.setBrightness(255);
  FastLED.clear();
  FastLED.show();
}

void mqtt_discoveryLigth(String dev) {
  String cmd_topic = dev + "/set";
  mqttClient.subscribe(cmd_topic.c_str(), 0);

  String discovery_topic = "homeassistant/light/" + dev + "/config";
  DynamicJsonDocument doc(2048);

  doc["name"] = dev;
  doc["unique_id"] = dev;
  doc["schema"] = "json";
  doc["state_topic"] = dev + "/state";
  doc["command_topic"] = dev + "/set";
  doc["brightness"] = true;
  doc["effect"] = true;
  doc["min_mireds"] = 153;
  doc["max_mireds"] = 500;
  doc["hs_color"] = true;
  addAvailability(doc);

  JsonArray effectList = doc.createNestedArray("effect_list");
  effectList.add("static");
  effectList.add("rainbow");
  effectList.add("fire");
  effectList.add("confetti");
  effectList.add("flash");
  effectList.add("cycle");
  effectList.add("warmfill");

  JsonArray modes = doc.createNestedArray("supported_color_modes");
  modes.add("hs");
  modes.add("color_temp");

  JsonObject deviceInfo = doc.createNestedObject("device");
  fillDeviceInfo(deviceInfo, "ESP32 Striscia LED", "SM167 LED STRIP");

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(discovery_topic.c_str(), 0, true, payload.c_str());
}

void mqtt_publishLigth(int i) {
  DynamicJsonDocument doc(512);
  doc["state"] = ligth_power[i] ? "ON" : "OFF";

  if (ligth_power[i]) {
    doc["brightness"] = ligth_brightness[i];

    if (light_mode[i]) {
      doc["color_mode"] = "hs";
      JsonObject color = doc.createNestedObject("color");
      color["h"] = (float)ligth_hue[i] * 360.0f / 255.0f;
      color["s"] = (float)ligth_sat[i] * 100.0f / 255.0f;
    } else {
      doc["color_mode"] = "color_temp";
      doc["color_temp"] = ligth_colorTemp[i];
    }

    doc["effect"] = ligth_currentEffect[i];
  }

  String stateStr;
  serializeJson(doc, stateStr);
  mqttClient.publish((ligth_device[i] + "/state").c_str(), 0, false, stateStr.c_str());
}

void mqtt_if_messageLigth(int i, String payload, String topic) {
  if (topic != ligth_device[i] + "/set") return;

  ligth_effectReset[i] = true;

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.print("JSON error: ");
    Serial.println(err.c_str());
    return;
  }

  if (doc.containsKey("state")) {
    ligth_power[i] = (doc["state"] == "ON");
  }

  if (doc.containsKey("brightness")) {
    ligth_brightness[i] = doc["brightness"].as<uint8_t>();
  }

  if ((doc.containsKey("color_mode") && doc["color_mode"] == "hs") || doc.containsKey("color")) {
    light_mode[i] = true;
    JsonObject color = doc["color"];
    if (!color.isNull()) {
      if (color.containsKey("h")) {
        ligth_hue[i] = map(color["h"].as<float>(), 0, 360, 0, 255);
      }
      if (color.containsKey("s")) {
        ligth_sat[i] = map(color["s"].as<float>(), 0, 100, 0, 255);
      }
    }
  }

  if (doc.containsKey("color_temp")) {
    ligth_colorTemp[i] = doc["color_temp"].as<uint16_t>();
    light_mode[i] = false;
  }

  if (doc.containsKey("effect")) {
    String newEffect = doc["effect"].as<String>();
    if (newEffect != ligth_currentEffect[i]) {
      ligth_currentEffect[i] = newEffect;
      Serial.println("EFFETTO CAMBIATO + RESET: " + ligth_currentEffect[i]);
    }
  }

  mqtt_publishLigth(i);
}

void mqtt_discoveryButton() {
  mqttClient.subscribe("esp32/warmfill/set", 0);

  String button_topic = "homeassistant/button/esp32_warmfill/config";
  DynamicJsonDocument doc(512);

  doc["name"] = "Warmfill Globale";
  doc["unique_id"] = "esp32_warmfill";
  doc["command_topic"] = "esp32/warmfill/set";
  addAvailability(doc);

  JsonObject deviceInfo = doc.createNestedObject("device");
  fillDeviceInfo(deviceInfo, "ESP32 Striscia LED", "SM167 LED STRIP");

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(button_topic.c_str(), 0, true, payload.c_str());
}

void mqtt_if_messageButton(String payload, String topic) {
  if (topic == "esp32/warmfill/set") {
    Serial.println("🔥 WARMFILL GLOBALE AVVIATO!");
    for (int i = 0; i < NUM_LIGHT; i++) {
      ligth_currentEffect[i] = "warmfill";
      ligth_power[i] = true;
      ligth_brightness[i] = 110;
      ligth_effectReset[i] = true;
    }
  }
}

void mqtt_discoveryLigthMaster() {
  String cmd_topic = ligth_masterDevice + "/set";
  mqttClient.subscribe(cmd_topic.c_str(), 0);

  String discovery_topic = "homeassistant/light/" + ligth_masterDevice + "/config";
  DynamicJsonDocument doc(2048);

  doc["name"] = "Tutte le luci";
  doc["unique_id"] = ligth_masterDevice;
  doc["schema"] = "json";
  doc["state_topic"] = ligth_masterDevice + "/state";
  doc["command_topic"] = ligth_masterDevice + "/set";
  doc["brightness"] = true;
  doc["effect"] = true;
  doc["min_mireds"] = 153;
  doc["max_mireds"] = 500;
  doc["hs_color"] = true;
  addAvailability(doc);

  JsonArray effectList = doc.createNestedArray("effect_list");
  effectList.add("static");
  effectList.add("rainbow");
  effectList.add("fire");
  effectList.add("confetti");
  effectList.add("flash");
  effectList.add("cycle");
  effectList.add("warmfill");

  JsonArray modes = doc.createNestedArray("supported_color_modes");
  modes.add("hs");
  modes.add("color_temp");

  JsonObject deviceInfo = doc.createNestedObject("device");
  fillDeviceInfo(deviceInfo, "ESP32 Striscia LED", "SM167 LED STRIP");

  String payload;
  serializeJson(doc, payload);
  mqttClient.publish(discovery_topic.c_str(), 0, true, payload.c_str());
}

void mqtt_publishLigthMaster() {
  DynamicJsonDocument doc(512);

  bool anyOn = false;
  for (int i = 0; i < NUM_LIGHT; i++) {
    if (ligth_power[i]) {
      anyOn = true;
      break;
    }
  }

  doc["state"] = anyOn ? "ON" : "OFF";

  if (anyOn) {
    doc["brightness"] = ligth_brightness[0];

    if (light_mode[0]) {
      doc["color_mode"] = "hs";
      JsonObject color = doc.createNestedObject("color");
      color["h"] = (float)ligth_hue[0] * 360.0f / 255.0f;
      color["s"] = (float)ligth_sat[0] * 100.0f / 255.0f;
    } else {
      doc["color_mode"] = "color_temp";
      doc["color_temp"] = ligth_colorTemp[0];
    }

    doc["effect"] = ligth_currentEffect[0];
  }

  String stateStr;
  serializeJson(doc, stateStr);
  mqttClient.publish((ligth_masterDevice + "/state").c_str(), 0, false, stateStr.c_str());
}

void mqtt_if_messageLigthMaster(String payload, String topic) {
  if (topic != ligth_masterDevice + "/set") return;

  for (int i = 0; i < NUM_LIGHT; i++) {
    mqtt_if_messageLigth(i, payload, ligth_device[i] + "/set");
  }

  mqtt_publishLigthMaster();
}

void mqtt_publishAllLights() {
  for (int i = 0; i < NUM_LIGHT; i++) {
    mqtt_publishLigth(i);
  }
  mqtt_publishLigthMaster();
}

void ligth_setCCT(int strip, int idx, uint8_t warmW, uint8_t coldC) {
  ligth_leds[strip][idx].g = warmW;
  ligth_leds[strip][idx].r = coldC;
  ligth_leds[strip][idx].b = 0;
}

uint8_t clamp8(int v) {
  return (uint8_t)constrain(v, 0, 255);
}

void vr_ligth_setCCT_helperHA(uint8_t lightId, uint16_t ct_mired, uint8_t bri) {
  if (lightId >= NUM_LIGHT) return;

  const int CT_COLD = 150;
  const int CT_WARM = 500;

  ct_mired = constrain((int)ct_mired, CT_COLD, CT_WARM);

  float warmRatio = float(ct_mired - CT_COLD) / float(CT_WARM - CT_COLD);
  float coldRatio = 1.0f - warmRatio;

  uint8_t ww = clamp8(int(warmRatio * 255.0f));
  uint8_t cw = clamp8(int(coldRatio * 255.0f));

  ww = (uint16_t(ww) * bri) / 255;
  cw = (uint16_t(cw) * bri) / 255;

  uint8_t strip = vr_ligth_srip[lightId];
  uint16_t start = vr_ligth_startIdx[lightId];
  uint16_t size  = vr_ligth_segSize[lightId];

  if (strip >= NUM_STRIPS) return;
  if (start >= ligth_numLeds[strip]) return;

  for (int i = start + 1; i < start + size; i += 2) {
    if (i >= ligth_numLeds[strip]) break;
    ligth_setCCT(strip, i, ww, cw);
  }
}

inline void applyVirtualBrightness(uint8_t lightId, uint8_t bri) {
  if (lightId >= NUM_LIGHT) return;

  uint8_t strip = vr_ligth_srip[lightId];
  if (strip >= NUM_STRIPS) return;

  uint16_t start = vr_ligth_startIdx[lightId];
  uint16_t size  = vr_ligth_segSize[lightId];
  if (start >= ligth_numLeds[strip] || size == 0) return;

  uint16_t n = size;
  if (start + n > ligth_numLeds[strip]) n = ligth_numLeds[strip] - start;

  if (bri == 0) {
    fill_solid(ligth_leds[strip] + start, n, CRGB::Black);
    return;
  }

  nscale8_video(ligth_leds[strip] + start, n, bri);
}

void vr_ligth_updateLEDs(uint8_t vr_ligth) {
  if (vr_ligth >= NUM_LIGHT) return;

  uint8_t  strip = vr_ligth_srip[vr_ligth];
  uint16_t start = vr_ligth_startIdx[vr_ligth];
  uint16_t size  = vr_ligth_segSize[vr_ligth];

  if (strip >= NUM_STRIPS) return;
  if (start >= ligth_numLeds[strip]) return;

  uint16_t n = size;
  if (start + n > ligth_numLeds[strip]) n = ligth_numLeds[strip] - start;

  if (!ligth_power[vr_ligth]) {
    fill_solid(ligth_leds[strip] + start, n, CRGB::Black);
    ligth_effectReset[vr_ligth] = true;
    return;
  }

  if (ligth_effectReset[vr_ligth]) {
    fill_solid(ligth_leds[strip] + start, n, CRGB::Black);
  }

  static uint8_t gHue[NUM_LIGHT] = {0, 0, 0, 0};
  String effect = ligth_currentEffect[vr_ligth];

  if (effect == "static") {
    if (light_mode[vr_ligth]) {
      CRGB color = CHSV(ligth_hue[vr_ligth], ligth_sat[vr_ligth], 255);
      for (int i = start; i < start + n; i += 2) {
        ligth_leds[strip][i] = color;
      }
      for (int i = start + 1; i < start + n; i += 2) {
        ligth_setCCT(strip, i, 0, 0);
      }
    } else {
      for (int i = start; i < start + n; i += 2) {
        ligth_leds[strip][i] = CRGB::Black;
      }
      vr_ligth_setCCT_helperHA(vr_ligth, ligth_colorTemp[vr_ligth], 255);
    }
  }
  else if (effect == "rainbow") {
    uint16_t numPixels = n / 2;
    for (int i = 0; i < numPixels; i++) {
      int idx = start + (i * 2);
      if (idx >= start + n) break;
      uint8_t localHue = gHue[vr_ligth] + i * 7;
      ligth_leds[strip][idx] = CHSV(localHue, 255, 255);
    }
    for (int i = start + 1; i < start + n; i += 2) {
      ligth_setCCT(strip, i, 0, 0);
    }
    gHue[vr_ligth] += 2;
  }
  else if (effect == "confetti") {
    fadeToBlackBy(ligth_leds[strip] + start, n, 20);

    uint16_t numPixels = n / 2;
    if (numPixels > 0) {
      int pixel = random16(numPixels);
      int rgbIndex = start + (pixel * 2);
      int cctIndex = rgbIndex + 1;

      if (rgbIndex < start + n) {
        ligth_leds[strip][rgbIndex] += CHSV(gHue[vr_ligth] + random8(64), 200, 255);
      }
      if (cctIndex < start + n) {
        ligth_setCCT(strip, cctIndex, 50, 0);
      }
      gHue[vr_ligth]++;
    }
  }
  else if (effect == "flash") {
    static bool flashState = false;
    static unsigned long lastFlash = 0;

    if (millis() - lastFlash > 500) {
      flashState = !flashState;
      lastFlash = millis();
    }

    CRGB color = flashState ? CRGB(CHSV(ligth_hue[vr_ligth], ligth_sat[vr_ligth], 255)) : CRGB::Black;

    for (int i = start; i < start + n; i += 2) {
      ligth_leds[strip][i] = color;
    }
    for (int i = start + 1; i < start + n; i += 2) {
      ligth_setCCT(strip, i, 0, 0);
    }
  }
  else if (effect == "cycle") {
    for (int i = start; i < start + n; i += 2) {
      ligth_leds[strip][i] = CHSV(gHue[vr_ligth], 255, 255);
    }
    for (int i = start + 1; i < start + n; i += 2) {
      ligth_setCCT(strip, i, 255, 0);
    }
    gHue[vr_ligth] += 6;
  }
  else if (effect == "warmfill") {
    static uint8_t phase = 0;
    static uint8_t cicle_number = 0;
    static uint8_t rotCount = 0;

    if (ligth_effectReset[vr_ligth]) {
      phase = 0;
      cicle_number = 0;
      rotCount = 0;
      ligth_effectReset[vr_ligth] = false;
    }

    if (!ligth_power[vr_ligth]) return;

    fill_solid(ligth_leds[strip] + start, n, CRGB::Black);

    uint16_t maxCCT = (n / 2);
    if (maxCCT == 0) return;

    if (phase == 0 && vr_ligth == 0) {
      cicle_number++;

      if (cicle_number >= maxCCT) {
        phase = 1;
        cicle_number = maxCCT - 1;
        return;
      }

      rotCount = 0;
      for (int i = 0; i < cicle_number; i++) {
        int seg_rel = cicle_number - i - 1;
        if (seg_rel < 0) continue;

        uint16_t seg_num = start + 1 + seg_rel * 2;
        if (seg_num >= start + n) continue;

        rotCount++;
        if (rotCount > 6) rotCount = 1;

        if (rotCount <= 3) {
          ligth_setCCT(strip, seg_num, 77, 0);
        } else {
          ligth_setCCT(strip, seg_num, 0, 0);
        }
      }
    }
  }

  ligth_effectReset[vr_ligth] = false;
  applyVirtualBrightness(vr_ligth, ligth_brightness[vr_ligth]);
}

void updateAllVirtualLEDs() {
  FastLED.setBrightness(255);
  for (uint8_t id = 0; id < NUM_LIGHT; id++) {
    vr_ligth_updateLEDs(id);
  }
  FastLED.show();
}
///////////     END FASTLED FUNCTION       /////////////

//////////////////////////////////////////////////
///////////     BUTTON CONFIG  V1_1    /////////////
//////////////////////////////////////////
#define BUTTON_PIN 18

bool buttonStableState = HIGH;
bool buttonLastReading = HIGH;
unsigned long buttonLastDebounceTime = 0;
const unsigned long buttonDebounceMs = 50;
//////////     END BUTTON CONFIG  V1_1    ////////////////

//////////////////////////////////////////////////
///////////     BUTTON FUNCTION  V1_1     /////////////
//////////////////////////////////////////
void init_Button() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void toggleAllLights() {
  bool newState = !ligth_power[0];

  for (int i = 0; i < NUM_LIGHT; i++) {
    ligth_power[i] = newState;
    ligth_effectReset[i] = true;
  }

  mqtt_publishAllLights();
  Serial.println(newState ? "Pulsante: TUTTE ON" : "Pulsante: TUTTE OFF");
}

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != buttonLastReading) {
    buttonLastDebounceTime = millis();
  }

  if ((millis() - buttonLastDebounceTime) > buttonDebounceMs) {
    if (reading != buttonStableState) {
      buttonStableState = reading;

      if (buttonStableState == LOW) {
        toggleAllLights();
      }
    }
  }

  buttonLastReading = reading;
}
//////////     END BUTTON FUNCTION  V1_1     ////////////////

//////////////////////////////////////////////////
///////////     WIFI FUNCTION      /////////////
//////////////////////////////////////////
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid.c_str(), pass.c_str());
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println(WiFi.localIP());

      

      connectToMqtt();
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0);
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");

  publishAvailability(true);

  mqtt_discoveryButton();

  for (int i = 0; i < NUM_LIGHT; i++) {
    mqtt_discoveryLigth(ligth_device[i]);
  }

  mqtt_discoveryLigthMaster();
  mqtt_discoveryBME();
  mqtt_discoveryBH1750();

  mqtt_publishBME();
  mqtt_publishBH1750();
  mqtt_publishAllLights();
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String payloadStr = "";
  for (int i = 0; i < len; i++) {
    payloadStr += (char)payload[i];
  }
  String topicStr(topic);
  Serial.println("MQTT: " + topicStr + " = " + payloadStr);

  mqtt_if_messageButton(payloadStr, topicStr);
  mqtt_if_messageLigthMaster(payloadStr, topicStr);

  for (int idx = 0; idx < NUM_LIGHT; idx++) {
    mqtt_if_messageLigth(idx, payloadStr, topicStr);
  }
}

void init_Wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  mqttReconnectTimer = xTimerCreate(
    "mqttTimer",
    pdMS_TO_TICKS(2000),
    pdFALSE,
    (void*)0,
    reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt)
  );

  wifiReconnectTimer = xTimerCreate(
    "wifiTimer",
    pdMS_TO_TICKS(2000),
    pdFALSE,
    (void*)0,
    reinterpret_cast<TimerCallbackFunction_t>(connectToWifi)
  );

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(mqtt_ip.c_str(), MQTT_PORT);
  mqttClient.setCredentials(mqtt_user.c_str(), mqtt_pass.c_str());
  mqttClient.setWill(MQTT_STATUS_TOPIC, 0, true, "offline");

  connectToWifi();
}
///////////     END WIFI FUNCTION      /////////////

void setup() {
  Serial.begin(115200);
  Serial.println();

  init_Button();
  init_Sensor();
  init_ledStrip();
  init_Wifi();

  Serial.println("Inizializzazione completata - V1_2");
  delay(3000);
}

unsigned long current_millis, t3s;
unsigned long tled = 0;

void loop() {
  current_millis = millis();
  handleButton();

  if ((current_millis - t3s) >= 3000) {
    Serial.println("----------3s timer ------------");
    mqtt_publishBME();
    mqtt_publishBH1750();
    mqtt_publishAllLights();
    t3s = current_millis;
  }

  if ((current_millis - tled) >= 20) {
    updateAllVirtualLEDs();
    tled = current_millis;
  }
}
