#pragma once
#include <chrono>
#include <cmath>
#include <string>
#include <sstream>
#include <WString.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "ArduinoJson-v6.15.1.h"
#include "NetworkConfiguration.h"

// Seven segment bit mask
//  _
// |_|
// |_|
//
//B00000001 Top
//B00000010 Top Right
//B00000100 Bottom Right
//B00001000 Bottom
//B00010000 Bottom Left
//B00100000 Top Left
//B01000000 middle bar

// bit mask letters
constexpr uint8_t LETTER_L = B00111000;
constexpr uint8_t LETTER_A = B01110111;
constexpr uint8_t LETTER_P = B01110011;
constexpr uint8_t DASH = B01000000;
constexpr uint8_t BLANK = B00000000;

constexpr uint8_t TRIGGER_ADDRESS = 12;
constexpr uint8_t ECHO_ADDRESS = 14;
constexpr uint8_t DISPLAY_ADDRESS = 0x70;
constexpr int MIN_DISTANCE = 1;
constexpr int MAX_DISTANCE = 55;
constexpr int TENTH_OF_A_SECOND_MS = 10;
constexpr int THIRD_OF_A_SECOND_MS = 333;
constexpr int QUARTER_OF_A_SECOND_MS = 250;
constexpr int ONE_SECOND_MS = 1000;
constexpr int64_t ONE_SECOND_NS = 1000000000;
constexpr int64_t TWO_SECONDS_NS = 2000000000;
constexpr int64_t FIVE_SECONDS_NS = 5000000000;

constexpr NetworkConfiguration _netConfig = NetworkConfiguration();
constexpr int QUICK_BLINK_DURATION = 150;
constexpr int LONG_BLINK_DURATION = 750;
constexpr int BLINK_INTERVAL = 250;

ESP8266WiFiMulti _wifiMulti;
int lapCount = 1;
bool _ledOn;
bool raceHasStarted = false;
bool isFirstCarDetection = false;
std::chrono::high_resolution_clock::time_point lapStart, lapEnd;
Adafruit_7segment display = Adafruit_7segment();

struct HttpResponse
{
    int httpCode;
    String body; // only set if httpCode is non-negative
};

struct JsonResponse
{
    bool deserializationSuccess = false;
    StaticJsonDocument<1028> document;
};
