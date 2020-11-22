/*
 Name:		LapTimer.ino
 Created:	1/18/2020 1:53:41 PM
 Author:	john
*/
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <Wire.h>
#include "LapTimer.h"

void setup(void)
{
    Serial.begin(9600);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(TRIGGER_ADDRESS, OUTPUT);
    pinMode(ECHO_ADDRESS, INPUT);
    display.begin(DISPLAY_ADDRESS);
    display.writeDigitNum(0, 0);
    display.writeDigitNum(1, 0);
    display.writeDigitNum(3, 0);
    display.writeDigitNum(4, 0);
    display.drawColon(true);
    display.writeDisplay();
    TurnLedOff();

    WiFi.mode(WIFI_STA);
    _wifiMulti.addAP(_netConfig.ssid, _netConfig.key);
}

void TurnLedOn()
{
    digitalWrite(LED_BUILTIN, LOW);
    _ledOn = true;
}

void TurnLedOff()
{
    digitalWrite(LED_BUILTIN, HIGH);
    _ledOn = false;
}

void BlinkLED(int onDuration, int blinkCount)
{
    BlinkLED(onDuration, BLINK_INTERVAL, blinkCount);
}

void BlinkLED(int onDuration, int blinkInterval, int blinkCount)
{
    if (onDuration < 100)
    {
        onDuration = 100; // 100ms is shortest blink duration
    }

    for (int i = 0; i < blinkCount; i++)
    {
        TurnLedOn();
        delay(onDuration);
        TurnLedOff();
        delay(blinkInterval);
    }
}

long GetDistanceCentimeters(long duration)
{
    return (duration / 2.0) / 29.1;
}

bool IsDistanceWithinThreshold(long distance)
{
    return (distance > MIN_DISTANCE && distance < MAX_DISTANCE);
}

void DisplayMinutesAndSeconds(int64_t minutes, int64_t secondsWithMinutes)
{
    int64_t seconds = secondsWithMinutes % 60; // modulo out the minutes
    // Display minutes on left two digits
    if (minutes < 10)
    {
        display.writeDigitNum(0, 0);
        display.writeDigitNum(1, minutes);
    }
    else
    {
        display.writeDigitNum(0, (minutes / 10));
        display.writeDigitNum(1, (minutes % 10));
    }

    // Display seconds on right two digits
    if (seconds < 10)
    {
        display.writeDigitNum(3, 0);
        display.writeDigitNum(4, seconds);
    }
    else
    {
        display.writeDigitNum(3, (seconds / 10));
        display.writeDigitNum(4, (seconds % 10));
    }

    display.drawColon(true);
    display.writeDisplay();
}

void DisplaySecondsAndMilliseconds(int64_t seconds, int64_t millis)
{
    // Display seconds on left two digits
    if (seconds < 10)
    {
        display.writeDigitNum(0, 0);
        display.writeDigitNum(1, seconds, true);
    }
    else if (seconds < 100)
    {
        display.writeDigitNum(0, seconds / 10);
        display.writeDigitNum(1, seconds % 10, true);
    }

    // Display milliseconds on right two digits
    int twoDigits = (millis % 1000) / 10;
    int thirdDigit = millis % 10;

    if (thirdDigit >= 5) // round to two digits
    {
        twoDigits += 1;
    }

    display.writeDigitNum(3, twoDigits / 10);
    display.writeDigitNum(4, twoDigits % 10);

    display.drawColon(false);
    display.writeDisplay();
}

void DisplayTime(std::chrono::nanoseconds time)
{
    int64_t lapTimeMinutes = std::chrono::duration_cast<std::chrono::minutes>(time).count();
    int64_t lapTimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(time).count();
    int64_t lapTimeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(time).count() % 1000; // modulo out the seconds

        // Display seconds
    if (lapTimeSeconds < 60)
    {
        DisplaySecondsAndMilliseconds(lapTimeSeconds, lapTimeMillis);
    }
    else if (lapTimeMinutes < 100)
    {
        DisplayMinutesAndSeconds(lapTimeMinutes, lapTimeSeconds);
    }
    else
    {
        display.writeDigitRaw(0, DASH);
        display.writeDigitRaw(1, DASH);
        display.writeDigitRaw(3, DASH);
        display.writeDigitRaw(4, DASH);
        display.writeDisplay();
    }
}

void DisplayLapCount(int lapCount)
{
    if (lapCount >= 1000)
    {
        display.print(lapCount);
    }
    else
    {
        display.writeDigitRaw(0, LETTER_L);

        if (lapCount < 10)
        {
            display.writeDigitRaw(1, LETTER_A);
            display.writeDigitRaw(3, LETTER_P);
            display.writeDigitNum(4, lapCount);
        }
        else if (lapCount >= 10 && lapCount < 100)
        {
            display.writeDigitRaw(1, LETTER_P);
            display.writeDigitNum(3, (lapCount / 10));
            display.writeDigitNum(4, (lapCount % 10));
        }
        else if (lapCount >= 100 && lapCount < 1000)
        {
            display.writeDigitNum(1, (lapCount / 100));
            display.writeDigitNum(2, (lapCount / 10));
            display.writeDigitNum(3, (lapCount % 10));
        }
    }

    display.drawColon(false);
    display.writeDisplay();
    delay(LAP_COUNT_DISPLAY_DELAY_MS);
}

void DisplayLapTime(std::chrono::nanoseconds lapTime)
{
    for (int flashCount = 0; flashCount < 3; flashCount++) {
        DisplayTime(lapTime);
        delay(THIRD_OF_A_SECOND_MS);
        display.writeDigitRaw(0, BLANK);
        display.writeDigitRaw(1, BLANK);
        display.writeDigitRaw(3, BLANK);
        display.writeDigitRaw(4, BLANK);
        display.writeDisplay();
        delay(QUARTER_OF_A_SECOND_MS);
    }
}

void PostLapTimeToServer(std::chrono::nanoseconds lapTime)
{
    int64_t lapTimeMillis = std::chrono::duration_cast<std::chrono::milliseconds>(lapTime).count();

    StaticJsonDocument<256> jsonDoc;
    jsonDoc["lapTime"] = lapTimeMillis;
    String jsonString;
    serializeJson(jsonDoc, jsonString);

    String announceLapTimeEndPoint = "/AnnounceLapTime";
    String endpoint = (String)_netConfig.endpointBase + announceLapTimeEndPoint;
    WiFiClient wifiClient;
    HTTPClient httpClient;
    HttpResponse httpResponse;

    Serial.println("Endpoint: ");
    Serial.println(endpoint);
    Serial.println("json: ");
    Serial.println(jsonString);

    if (httpClient.begin(wifiClient, endpoint))
    {
        httpClient.addHeader("Content-Type", "application/json");
        httpResponse.httpCode = httpClient.POST(jsonString);

        if (httpResponse.httpCode > 0)
        { /// HTTP client errors are negative
            Serial.printf("HTTP POST success, code: %d\n", httpResponse.httpCode);
            httpResponse.body = httpClient.getString();
            auto size = sizeof(httpResponse.body);
            Serial.print("response Body size: ");
            Serial.println(size * 8);
            Serial.print("response Body: ");
            Serial.println(httpResponse.body);
        }
        else
        {
            Serial.printf("HTTP POST failed, error: %s\n", httpClient.errorToString(httpResponse.httpCode).c_str());
            BlinkLED(QUICK_BLINK_DURATION, 2);
            BlinkLED(LONG_BLINK_DURATION, 1);
        }

        httpClient.end();
    }
}

void loop()
{
    if (_wifiMulti.run() == WL_CONNECTED)
    {
        long duration, distance;
        digitalWrite(TRIGGER_ADDRESS, LOW);
        delayMicroseconds(5);

        digitalWrite(TRIGGER_ADDRESS, HIGH);
        delayMicroseconds(10);

        digitalWrite(TRIGGER_ADDRESS, LOW);

        duration = pulseIn(ECHO_ADDRESS, HIGH);
        lapEnd = std::chrono::high_resolution_clock::now();
        std::chrono::nanoseconds lapTime = lapEnd - lapStart;
        distance = GetDistanceCentimeters(duration);

        if (IsDistanceWithinThreshold(distance))
        {
            if (lapTime.count() > LAP_LOCKOUT_DURATION_NS)
            {
                if (!raceHasStarted)
                {
                    raceHasStarted = true;
                    isFirstCarDetection = false;
                    lapStart = std::chrono::high_resolution_clock::now();
                    DisplayLapCount(lapCount);
                }
                else if (isFirstCarDetection)
                {
                    isFirstCarDetection = false;
                    lapStart = std::chrono::high_resolution_clock::now();
                    lapCount += 1;

                    DisplayLapTime(lapTime);
                    PostLapTimeToServer(lapTime);
                    DisplayLapCount(lapCount);
                }
            }
        }
        else if (distance > MAX_DISTANCE)
        {
            isFirstCarDetection = true;
        }

        if (raceHasStarted)
        {
            DisplayTime(lapTime);
        }
    }
    else
    {
        Serial.println("NOT connected");
        BlinkLED(QUICK_BLINK_DURATION, 2);
        delay(LAP_COUNT_DISPLAY_DELAY_MS);
    }
    delay(LOOP_DELAY_MS);
}