/*
 Name:		LapTimer.ino
 Created:	1/18/2020 1:53:41 PM
 Author:	john
*/
#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"
#include <chrono>
#include <cmath>
#include <Wire.h>
#include <string>
#include <sstream>

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
constexpr int MAX_DISTANCE = 5;
constexpr int LOOP_DELAY_MS = 10;
constexpr int THIRD_OF_A_SECOND_MS = 333;
constexpr int QUARTER_OF_A_SECOND_MS = 250;
constexpr long MAX_THRESHOLD_DISTANCE = 45; // lower to prevent false detects from resetting the first car detection
constexpr int64_t LAP_LOCKOUT_DURATION_NS = 2000000000;
constexpr int64_t LAP_COUNT_DISPLAY_DELAY_MS = 1000;


int lapCount = 1;
bool raceHasStarted = false;
bool isFirstCarDetection = false;
std::chrono::high_resolution_clock::time_point lapStart, lapEnd;

Adafruit_7segment display = Adafruit_7segment();

void setup(void)
{
	pinMode(TRIGGER_ADDRESS, OUTPUT);
	pinMode(ECHO_ADDRESS, INPUT);
	display.begin(DISPLAY_ADDRESS);
	display.writeDigitNum(0, 0);
	display.writeDigitNum(1, 0);
	display.writeDigitNum(3, 0);
	display.writeDigitNum(4, 0);
	display.drawColon(true);
	display.writeDisplay();
}

long GetDistanceCentimeters(long duration)
{
	return (duration / 2.0) / 29.1;
}

bool IsDistanceWithinThreshold(long distance)
{
	return (distance > MIN_DISTANCE&& distance < MAX_DISTANCE);
}

void DisplayMinutesAndSeconds(int64_t minutes, int64_t secondsWithMinutes)
{
	int64_t seconds = secondsWithMinutes % 60; // modulo out the minutes
	// Display minutes on left two locations
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

	// Display seconds on right two locations
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
	// Display seconds on left two locations
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

	// Display milliseconds on right two locations
	int twoDigits = (millis % 1000) / 10;
	int thirdDigit = millis % 10;

	if (thirdDigit >= 5) // round two digits
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

void loop()
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
				DisplayLapCount(lapCount);
			}
		}
	}
	else if (distance < MAX_THRESHOLD_DISTANCE)
	{
		isFirstCarDetection = true;
	}

	if (raceHasStarted)
	{
		DisplayTime(lapTime);
	}

	delay(LOOP_DELAY_MS);
}
