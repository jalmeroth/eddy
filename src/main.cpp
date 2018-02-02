
extern "C" {
    #include "user_interface.h"             // os_timer_t
}
#include <time.h>
#include <Arduino.h>
#include <myconfig.h>                       // General Setup
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Neopixel.h>

// Device Setup
#define DEVICE_ID           "eddy"
#define FW_VERSION          "0.0.2"
#define FW_NAME             "envelope"
#define NUM_LEDS            3
#define PIN_LEDS            D5
#define TZ                  1               // UTC+1
#define DST_MIN             0               // One hour for summer time
#define HOUR_START          6               // 06:00
#define HOUR_STOP           21              // 21:59
#define LEN_DATETIME        20              // 2018-01-26 20:54:42
#define DELAY_RETRY         500
// MQTT Setup
#define MQTT_UPDATE_RATE    60 * 1000       // 60 seconds (milliseconds)
#define MQTT_SEP            "/"
#define MQTT_WILD           "+"
#define MQTT_SET            "set"
#define MQTT_PREFIX         "iot"
#define MQTT_META           "meta"
#define MQTT_ONLINE         "online"
#define MQTT_FW_VERSION     "fwversion"
#define MQTT_FW_NAME        "fwname"
#define MQTT_LAST_SEEN      "lastseen"
#define MQTT_COMMAND        "command"
#define MQTT_COLOR          "color"
#define MQTT_OTA            "ota"
#define MQTT_RSSI           "rssi"
#define MQTT_LOCAL_IP       "localip"
#define MQTT_TOP_BASE       MQTT_PREFIX MQTT_SEP DEVICE_ID MQTT_SEP
#define MQTT_TOP_META       MQTT_TOP_BASE MQTT_META MQTT_SEP
#define MQTT_TOP_ONLINE     MQTT_TOP_META MQTT_ONLINE
#define MQTT_TOP_FWVERSION  MQTT_TOP_META MQTT_FW_VERSION
#define MQTT_TOP_FWNAME     MQTT_TOP_META MQTT_FW_NAME
#define MQTT_TOP_LASTSEEN   MQTT_TOP_META MQTT_LAST_SEEN
#define MQTT_TOP_OTA        MQTT_TOP_META MQTT_OTA
#define MQTT_TOP_RSSI       MQTT_TOP_META MQTT_RSSI
#define MQTT_TOP_LOCAL_IP   MQTT_TOP_META MQTT_LOCAL_IP
#define MQTT_TOP_SETTER     MQTT_TOP_BASE MQTT_WILD MQTT_SEP MQTT_SET
#define MQTT_TOP_COMMAND    MQTT_TOP_BASE MQTT_COMMAND
#define MQTT_TOP_COLOR      MQTT_TOP_BASE MQTT_COLOR
#define MQTT_TOP_CMD_SET    MQTT_TOP_COMMAND MQTT_SEP MQTT_SET
#define MQTT_TOP_CLR_SET    MQTT_TOP_COLOR MQTT_SEP MQTT_SET
// OTA Setup
#ifndef OTA_PW_HASH
    #define OTA_PW_HASH     "97c209ca505f2958db30a42135afab5c"  // md5("iot")
#endif

// General Setup
extern const char* ssid;                    // see #include <myconfig.h>
extern const char* pass;
extern const char* mqtt_host;               // der einzig wahre Broker
extern uint16_t mqtt_port;
bool ota_activated = false;

// ========== Stop editing config here!1! ==========

time_t now = 0;
struct tm * timeinfo;
char datetime[LEN_DATETIME];
os_timer_t mqtt_update_timer;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
ArduinoOTAClass ota_client;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, PIN_LEDS, NEO_GRB + NEO_KHZ800);

uint32_t red = pixels.Color(255, 0, 0);
uint32_t yellow = pixels.Color(255, 255, 0);
uint32_t green = pixels.Color(0, 255, 0);
uint32_t black = pixels.Color(0, 0, 0);

char* get_datetime() {
    time(&now);
    timeinfo = localtime(&now);
    strftime(datetime, LEN_DATETIME, "%Y-%m-%d %H:%M:%S", timeinfo);
    return datetime;
}

bool mqtt_pub(const char* topic, const char* payload, bool retained = false) {
    if(!mqtt_client.connected()) { return false; }
    Serial.print("PUB: "); Serial.print(topic); Serial.print(" = "); Serial.println(payload);
    return mqtt_client.publish(topic, payload, retained);
}

void serial_comm() {
    // enable Serial communication
    while (Serial.available()) {
        String inputString = Serial.readString();
        inputString.trim();
        inputString.toLowerCase();
        if(inputString == "datetime") {
            get_datetime();
        } else {
            Serial.print("Ignoring: "); Serial.println(inputString);
        }
    }
}

void pixels_show(uint32_t color) {
    for(int i=0; i<NUM_LEDS; i++) {
        pixels.setPixelColor(i, color);
    }
    pixels.show();
}

void pixels_setup() {
    pixels.begin();
    pixels.show();
}

void ota_setup() {
    if(!ota_activated) {
        ota_client.setHostname(WiFi.hostname().c_str());
        ota_client.setPasswordHash(OTA_PW_HASH);
        ota_client.begin();
        Serial.println("OTA-Service up.");
        Serial.print("  Hostname: "); Serial.println(ota_client.getHostname());
        ota_activated = true;
    }
}

bool mqtt_sub(const char* topic) {
    if(!mqtt_client.connected()) { return false; }
    Serial.print("SUB: "); Serial.println(topic);
    return mqtt_client.subscribe(topic);
}

void mqtt_message(char* topic, byte* payload, unsigned int length) {
    Serial.println("New message arrived.");
    payload[length] = '\0';
    char* message = (char*)payload;
    Serial.print("Topic: "); Serial.println(topic);
    Serial.print("Message: '"); Serial.print(message); Serial.println("'");

    if(strcmp(topic, MQTT_TOP_CMD_SET) == 0) {
        Serial.println("COMMAND");
        if(strcmp(message, "ota") == 0) {
            ota_setup();
            mqtt_pub(MQTT_TOP_OTA, ota_activated ? "true" : "false", true);
        } else if(strcmp(message, "restart") == 0) {
            ESP.restart();
        } else {
            Serial.print("Ignoring: "); Serial.println(message);
        }
    } else if(strcmp(topic, MQTT_TOP_CLR_SET) == 0) {
        Serial.println("COLOR");
        if(strcmp(message, "red") == 0) {
            pixels_show(red);
        } else if(strcmp(message, "yellow") == 0) {
            pixels_show(yellow);
        } else if(strcmp(message, "green") == 0) {
            pixels_show(green);
        } else if(strcmp(message, "black") == 0) {
            pixels_show(black);
        } else if(strcmp(message, "test") == 0) {
            pixels.setPixelColor(2, green);
            pixels.setPixelColor(1, yellow);
            pixels.setPixelColor(0, red);
            pixels.show();
        } else {
            Serial.print("Ignoring: "); Serial.println(message);
        }
    } else {
        Serial.println("UNKNOWN");
    }
}

void mqtt_update() {
    os_timer_disarm(&mqtt_update_timer);
    char rssi[5];
    snprintf(rssi, 5, "%d", WiFi.RSSI());
    mqtt_pub(MQTT_TOP_RSSI, rssi);
    mqtt_pub(MQTT_TOP_LOCAL_IP, WiFi.localIP().toString().c_str(), true);
    if(mqtt_pub(MQTT_TOP_LASTSEEN, get_datetime(), true)) {
        os_timer_setfn(&mqtt_update_timer, (os_timer_func_t *)mqtt_update, (void *)0);
        os_timer_arm(&mqtt_update_timer, MQTT_UPDATE_RATE, false);
    }
}

void mqtt_init() {
    mqtt_pub(MQTT_TOP_ONLINE, "true", true);
    mqtt_pub(MQTT_TOP_FWVERSION, FW_VERSION, true);
    mqtt_pub(MQTT_TOP_FWNAME, FW_NAME, true);
    mqtt_pub(MQTT_TOP_OTA, ota_activated ? "true" : "false", true);
    mqtt_update();
    mqtt_sub(MQTT_TOP_SETTER);
}

void mqtt_connect() {
    // Loop until we're reconnected
    Serial.print("Connecting to MQTT");
    while (!mqtt_client.connected()) {
        mqtt_client.connect(WiFi.macAddress().c_str(), MQTT_TOP_ONLINE, 1, true, "false");
        delay(DELAY_RETRY);
        Serial.print(".");
    }
    Serial.println(" connected.");
    mqtt_init();
}

void mqtt_setup() {
    mqtt_client.setServer(mqtt_host, mqtt_port);
    mqtt_client.setCallback(mqtt_message);
}

void time_setup() {
    configTime(TZ * 3600, DST_MIN * 60, "pool.ntp.org", "time.nist.gov");
    Serial.print("Synchronizing time");
    // While it's before 2018, let's wait
    while (((2018 - 1970) * 365 * 24 * 3600) > now) {
        delay(DELAY_RETRY);
        Serial.print(".");
        now = time(nullptr);
    }
    Serial.println(" done.");
    Serial.print("  Time now: "); Serial.print(ctime(&now));
}

void wifi_setup() {
    Serial.print("Connecting to: "); Serial.print(ssid);
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
    WiFi.mode(WIFI_STA);
    WiFi.hostname(DEVICE_ID);
    WiFi.begin(ssid, pass);
    while(WiFi.status() != WL_CONNECTED) {
        delay(DELAY_RETRY);
        Serial.print(".");
    }
    Serial.println(" connected.");
    Serial.print("  IP address: "); Serial.println(WiFi.localIP());
    Serial.print("  MAC address: "); Serial.println(WiFi.macAddress().c_str());
    Serial.print("  Hostname: "); Serial.println(WiFi.hostname().c_str());
}

void setup() {
    Serial.begin(115200); Serial.println();
    Serial.setDebugOutput(true);
    Serial.print("Device Id: "); Serial.println(DEVICE_ID);
    Serial.print("FW Version: "); Serial.println(FW_VERSION);
    pixels_setup();
    wifi_setup();
    time_setup();
    mqtt_setup();
    if(ota_activated) {
        ota_setup();
    }
}

void loop() {
    serial_comm();

    if(WiFi.status() != WL_CONNECTED) {
        wifi_setup();
    }

    if(!mqtt_client.connected()) {
        mqtt_connect();
    }

    mqtt_client.loop();

    if(ota_activated) {
        ota_client.handle();
    }
}
