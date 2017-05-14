#include <Process.h>
#include <Time.h>
#include <TimeAlarms.h>

// TODO: way to turn it off

int ALARM_TIME = -1;
int HOUR_BEFORE_ALARM_TIME = -1;

Process date;

int FLUX_HOUR_START = 18;
int FINAL_LIGHT_CHANGE_HOUR = 23;
int FLUX_HOUR_END = 3;

int STARTING_SATURATION = 63;
int FINAL_SATURATION = 254;
float STARTING_X = 0.4113;
float STARTING_Y = 0.3946;
float FINAL_X = 0.6528; // https://www.ledtuning.nl/en/cie-convertor
float FINAL_Y = 0.3445;
int BRIGHT_LEVEL = 254;
int DIM_LEVEL = 70;

int SECONDS_PER_PHASE = 120;
int NUM_FLUX_ITER = (FINAL_LIGHT_CHANGE_HOUR - FLUX_HOUR_START) * 3600 / SECONDS_PER_PHASE;
int NUM_MORNING_LIGHT_ITER = 1 * 3600 / SECONDS_PER_PHASE;

void setup() {
  Bridge.begin();        // initialize Bridge
  // Serial.begin(9600);    // initialize serial
  // while (!Serial);              // wait for Serial Monitor to open
  // Serial.println(F("Time Check"));  // Title of sketch

  Alarm.timerRepeat(SECONDS_PER_PHASE, runStuff);
}

void runStuff() {
  setTime();
  lightFlux();
  morningLight();
}

void loop() {
  // digitalClockDisplay();
  Alarm.delay(10 * 1000);
}

void setTime() {
  String timeString = getTimeString();
  while (getTime(timeString, getHour) == 0) {
    timeString = getTimeString();
  }
  String dateString = getDateString();
  setTime(
    getDate(timeString, getHour),
    getDate(timeString, getMin),
    getDate(timeString, getSec),
    getDate(dateString, getDay),
    getDate(dateString, getMonth),
    getDate(dateString, getYear));
}

void lightFlux() {
  if (!withinTime(hour(), FLUX_HOUR_START, FLUX_HOUR_END)) {
    return;
  }

  int secondsSinceStart;
  if (withinTime(hour(), FINAL_LIGHT_CHANGE_HOUR, FLUX_HOUR_END)) {
    secondsSinceStart = getSeconds(FINAL_LIGHT_CHANGE_HOUR - FLUX_HOUR_START, 0, 0);
  } else {
    secondsSinceStart = getSeconds(hour() - FLUX_HOUR_START, minute(), second());
  }

  int currentIter = secondsSinceStart / SECONDS_PER_PHASE + 1;
  int currentSaturation = incrementInt(STARTING_SATURATION, FINAL_SATURATION, currentIter, NUM_FLUX_ITER);
  float currentX = incrementFloat(STARTING_X, FINAL_X, currentIter, NUM_FLUX_ITER);
  float currentY = incrementFloat(STARTING_Y, FINAL_Y, currentIter, NUM_FLUX_ITER);
  int currentBrightness = incrementInt(BRIGHT_LEVEL, DIM_LEVEL, currentIter, NUM_FLUX_ITER);

  alterLight(lightSettings(currentX, currentY, currentSaturation, currentBrightness));
}

void morningLight() {
  int currTime = getSeconds(hour(), minute(), second());

  if (ALARM_TIME < currTime) {
    // only check within a period of time to reduce calls to amazon
    if (!withinTime(currTime, 2, 6)) {
      return;
    }

    // if check to get alarmTime failed
    if (!getAlarmTime()) {
      return;
    }
  }

  int secondsSinceStart = getSeconds(hour() , minute(), second()) - HOUR_BEFORE_ALARM_TIME;
  int currentIter = secondsSinceStart / SECONDS_PER_PHASE + 1;
  int currentBrightness = incrementInt(0, BRIGHT_LEVEL, currentIter, NUM_MORNING_LIGHT_ITER);

  alterLight(lightSettings(STARTING_X, STARTING_Y, STARTING_SATURATION, currentBrightness));
}

String lightSettings(float x, float y, int saturation, int brightness) {
  return "{\"xy\":[" + String(x, 4) +
        ", " + String(y, 4) +
        "], \"sat\": " + String(saturation) +
        ", \"bri\": " + String(brightness) + "}";
}

boolean getAlarmTime() {
  Process client;
  client.runShellCommand(F("cd ~; python -c 'from utils import getAlarmTime; getAlarmTime();'"));
  unsigned long alarmTime = 0;
  while (client.available()) {
    char c = client.read();
    if (!(c >= '0' && c <= '9')) {
      break;
    }
    alarmTime *= 10;
    alarmTime += c - '0';
  }

  if (alarmTime == 0) {
    return false;
  }

  ALARM_TIME = alarmTime / 1000;
  HOUR_BEFORE_ALARM_TIME = ALARM_TIME - getSeconds(1, 0, 0);
  return true;
}

void alterLight(String json) {
  Process client;
  client.runShellCommand("cd ~; python -c 'from utils import setLights; setLights(\"" + json + "\");'");
}

//************************//
// PRINT UTILS
//************************//

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  Serial.print(F(":"));
  printDigits(minute());
  Serial.print(F(":"));
  printDigits(second());
  Serial.print(F(" "));
  printDigits(day());
  Serial.print(F("-"));
  printDigits(month());
  Serial.print(F("-"));
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits) {
  if (digits < 10)
    Serial.print(F("0"));
  Serial.print(digits);
}

//************************//
// GENERAL UTILS
//************************//

int incrementInt(int starting, int ending, int iter, int numIter) {
  return incrementFloat((float) starting, (float) ending, iter, numIter);
}

float incrementFloat(float starting, float ending, int iter, int numIter) {
  return (ending - starting) / numIter * iter + starting;
}

//************************//
// TIME UTILS
//************************//

bool withinTime(int curr, int s, int e) {
  if (e < s) {
    return curr >= s || curr <= e;
  } else {
    return curr >= s && curr <= e;
  }
}

int getSeconds(int h, int m, int s) {
  return h * 3600 + m * 60 + s;
}

String callDateProcess(String param) {
  date.begin(F("date"));
  date.addParameter(param);
  date.run();

  String timeString;
  //if there's a result from the date process, parse it:
  while (date.available() > 0) {
    timeString = date.readString();
  }
  return timeString;
}

int getDate(String timeStamp, int (*func)(String, int, int)) {
  int firstSlash = timeStamp.indexOf(F("-"));
  int secondSlash = timeStamp.lastIndexOf(F("-"));
  return func(timeStamp, firstSlash, secondSlash);
}

String getDateString() {
  return callDateProcess(F("+%F"));
}

int getTime(String timeStamp, int (*func)(String, int, int)) {
  int firstIndex = timeStamp.indexOf(F(":"));
  int secondIndex = timeStamp.lastIndexOf(F(":"));
  return func(timeStamp, firstIndex, secondIndex);
}

String getTimeString() {
  return callDateProcess(F("+%T"));
}

int getDay(String string, int firstIndex, int secondIndex) {
  return string.substring(secondIndex + 1).toInt();
}

int getMonth(String string, int firstIndex, int secondIndex) {
  return string.substring(firstIndex + 1, secondIndex).toInt();
}

int getYear(String string, int firstIndex, int secondIndex) {
  return string.substring(0, firstIndex).toInt();
}

int getHour(String string, int firstIndex, int secondIndex) {
  return string.substring(0, firstIndex).toInt();
}

int getMin(String string, int firstIndex, int secondIndex) {
  return string.substring(firstIndex + 1, secondIndex).toInt();
}

int getSec(String string, int firstIndex, int secondIndex) {
  return string.substring(secondIndex + 1).toInt();
}
