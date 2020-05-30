/* This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be filled wherever it is marked.


  Copyright (c) [2016] [Andreas Spiess]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <YoutubeApi.h>
#include <MusicEngine.h>
#include <SNTPtime.h>
#include "config.h"

// ================================================ PIN DEFINITIONS ======================================

#define BUZ_PIN D5
#define NEO_PIN D4
#define POWER_PIN D1

#define NUMBER_OF_DIGITS 6
#define STARWARS "t112v127l12<dddg2>d2c<ba>g2d4c<ba>g2d4cc-c<a2d6dg2>d2c<ba>g2d4c<ba>g2d4cc-c<a2"

#define SUBSCRIBER_INTERVAL 30000
#define NTP_LOOP_INTERVAL 60000
#define DISP_LOOP_INTERVAL 500

#define MAX_BRIGHTNESS 80
#define MAX_DIGITS 6

#ifdef DEBUG
  #define DEBUG_PRINT(x) Serial.print (x)
  #define DEBUG_PRINTLN(x) Serial.println (x)
  // #define DEBUG_ESP true
  // #define DEBUGSERIAL Serial

#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

strDateTime timeNow;
int lastHour = 0;

unsigned long entrySubscriberLoop, entryNTPLoop, entryDispLoop;

struct subscriberStruc {
  long last;
  long actual;
  long old[24];
} subscribers;

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(8, 8, 6, 1, NEO_PIN,
                            NEO_TILE_TOP   + NEO_TILE_LEFT   + NEO_TILE_ROWS   + NEO_TILE_PROGRESSIVE +
                            NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE,
                            NEO_GRB + NEO_KHZ800);

const uint16_t colors[] = {
  matrix.Color(255, 0, 0), matrix.Color(0, 255, 0), matrix.Color(0, 0, 255), matrix.Color(255, 0 , 255)
};

MusicEngine music(BUZ_PIN);

WiFiClientSecure client;
YoutubeApi api(API_KEY, client);

SNTPtime NTPch("ch.pool.ntp.org");

// ================================================ helper functions =================================================

void beepUp() {
  music.play("T80 L40 O4 CDEFGHAB>CDEFGHAB");
  while (music.getIsPlaying() == 1) yield();
  delay(500);
}

void beepDown() {
  music.play("T1000 L40 O5 BAGFEDC<BAGFEDC<BAGFEDC");
  while (music.getIsPlaying() == 1) yield();
  delay(500);
}

void starwars() {
  music.play(STARWARS);
  while (music.getIsPlaying()) yield();
  delay(500);
}

int measureLight() {
  DEBUG_PRINT("Light measured: ");
  DEBUG_PRINTLN(analogRead(A0));
  int brightness = map(analogRead(A0), 200, 1024, 0, MAX_BRIGHTNESS);
  brightness = (brightness > MAX_BRIGHTNESS) ? MAX_BRIGHTNESS : brightness;  // clip value
  brightness = (brightness < 5) ? 0 : brightness;
 // brightness = MAX_BRIGHTNESS;

 DEBUG_PRINT("Light value calculated: ");
 DEBUG_PRINTLN(brightness);

  return brightness;
}

void debugPrint() {
  DEBUG_PRINTLN("---------Stats---------");
  DEBUG_PRINT("Subscriber Count: ");
  DEBUG_PRINTLN(subscribers.actual);
  DEBUG_PRINT("Variance: ");
  DEBUG_PRINTLN(subscribers.actual - subscribers.old[timeNow.hour]);
  DEBUG_PRINT("LastSubs: ");
  DEBUG_PRINTLN(subscribers.last);
  DEBUG_PRINT("View Count: ");
  DEBUG_PRINTLN(api.channelStats.viewCount);
  DEBUG_PRINT("Comment Count: ");
  DEBUG_PRINTLN(api.channelStats.commentCount);
  DEBUG_PRINT("Video Count: ");
  DEBUG_PRINTLN(api.channelStats.videoCount);
  // Probably not needed :)
  DEBUG_PRINT("hiddenSubscriberCount: ");
  DEBUG_PRINTLN(subscribers.actual);
  DEBUG_PRINTLN("------------------------");
}

void debugPrintSubs() {
  for (int i = 0; i < 24; i++) {
    DEBUG_PRINT(i);
    DEBUG_PRINT(" old ");
    DEBUG_PRINTLN(subscribers.old[i]);
  }
}

void displayText(String tt) {
  matrix.setTextWrap(false);
  matrix.setBrightness(40);
  matrix.fillScreen(0);
  matrix.setTextColor(colors[0]);
  matrix.setCursor(0, 0);
  matrix.print(tt);
  matrix.show();
}

void displayNeo(int subs, int variance ) {
  // DEBUG_PRINTLN("Display");

  matrix.fillScreen(0);
  int bright = measureLight();
  if (bright > -1) {
    // calc to be added spaces
    String res = String(subs);
    for (int i = 1; res.length() < MAX_DIGITS; i++) {
      res = " " + res;
    }
    matrix.setBrightness(bright);
    matrix.setTextColor(colors[2]);
    matrix.setCursor(0, 0);
    matrix.print(res);

    // Show arrow
    char arrow = (variance <= 0) ? 0x1F : 0x1E;
    if (variance > 0) matrix.setTextColor(colors[1]);
    else matrix.setTextColor(colors[0]);
    matrix.setCursor(36, 0);
    matrix.print(arrow);

    // show variance bar
    int h = map(variance, 0, 400, 0, 8);
    h = (h > 8) ? 8 : h;
    if (h > 0) matrix.fillRect(42, 8 - h,  1, h , colors[3]);
  }
  matrix.show();
}

void updateSubs() {
  if (api.getChannelStatistics(CHANNEL_ID))
  {
    // get subscribers from YouTube
    //      DEBUG_PRINTLN("Get Subs");
    subscribers.actual = api.channelStats.subscriberCount;
    displayNeo(subscribers.actual, subscribers.actual - subscribers.old[timeNow.hour]);
    DEBUG_PRINT("Subs ");
    DEBUG_PRINT(subscribers.actual);
    DEBUG_PRINT(" yesterday ");
    DEBUG_PRINTLN(subscribers.old[timeNow.hour]);
    if (subscribers.last > 0) {
      if (subscribers.actual > subscribers.last ) {
        beepUp();
        if (subscribers.actual % 10 <= subscribers.last % 10) for (int ii = 0; ii < 1; ii++) beepUp();
        if (subscribers.actual % 100 <= subscribers.last % 100) starwars();
        if (subscribers.actual % 1000 <= subscribers.last % 1000) for (int ii = 0; ii < 2; ii++) starwars();
      }
      else {
        if (subscribers.actual < subscribers.last ) beepDown();
      }
    }
    subscribers.last = subscribers.actual;
    debugPrint();
  }
}

bool wifiConnect() {
  int retryCounter = CONNECT_TIMEOUT * 10;
  // WiFi.forceSleepWake();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  // wifi_station_connect();
  DEBUG_PRINT("(Re)connecting to Wifi: ");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  // WiFi.printDiag(Serial);

  // make sure to wait for wifi connection
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    retryCounter--;
    if (retryCounter <= 0) {
      DEBUG_PRINT(" Timeout reached! Wifi Status: ");
      DEBUG_PRINTLN(WiFi.status());
      return false;
    }
    delay(100);
    DEBUG_PRINT(".");
  }
  DEBUG_PRINT(" Connected with IP address ");
  DEBUG_PRINTLN(WiFi.localIP());
  client.setInsecure();
  return true;
}

// ================================================ SETUP ================================================
void setup() {
  #ifdef DEBUG
    Serial.begin(SERIAL_BAUD); // initialize serial connection
    DEBUG_PRINTLN("Starting up");
    // delay for the serial monitor to start
    delay(3000);
  #endif

  pinMode(POWER_PIN, INPUT);

  DEBUG_PRINTLN("Initializing display");
  matrix.begin();
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  matrix.show();
  displayText("YouTube");

  pinMode(A0, INPUT);

  wifiConnect();

  DEBUG_PRINTLN("Getting NTP time");
  while (!NTPch.setSNTPtime()) DEBUG_PRINT("."); // set internal clock
  entryNTPLoop = millis() + NTP_LOOP_INTERVAL;
  entrySubscriberLoop = millis() + SUBSCRIBER_INTERVAL;
  beepUp();
  beepUp();

  DEBUG_PRINTLN("Getting initial YoutToube subscribers");
  Serial.setDebugOutput(true);
  while (subscribers.actual == 0) {
    DEBUG_PRINT(" Subscribers before: ");
    DEBUG_PRINTLN(subscribers.actual);
    if (api.getChannelStatistics(CHANNEL_ID)) {
      subscribers.actual = api.channelStats.subscriberCount;
      DEBUG_PRINT(" Subscribers after: ");
      DEBUG_PRINTLN(subscribers.actual);
    } else {
      DEBUG_PRINTLN("Unable to get stats");
    }
  }
  DEBUG_PRINTLN("a");
  for (int i = 0; i < 24; i++) {
    if (subscribers.old[i] == 0) subscribers.old[i] = subscribers.actual - 245;
  }
  DEBUG_PRINTLN("b");
  debugPrintSubs();
  DEBUG_PRINTLN("Setup done");
}

// ================================================ LOOP =================================================
void loop() {

  //-------- Your Sketch starts from here ---------------

  if (millis() - entryDispLoop > DISP_LOOP_INTERVAL) {
    entryDispLoop = millis();
    displayNeo(subscribers.actual, subscribers.actual - subscribers.old[timeNow.hour]);
  }

  if (millis() - entryNTPLoop > NTP_LOOP_INTERVAL) {
    // Check if the wifi is connected
    // wifiConnect();

    //   DEBUG_PRINTLN("NTP Loop");
    entryNTPLoop = millis();
    timeNow = NTPch.getTime(1.0, 1); // get time from internal clock
    NTPch.printDateTime(timeNow);
    if (timeNow.hour != lastHour ) {
      DEBUG_PRINTLN("New hour!!!");
      subscribers.old[lastHour] = subscribers.actual;
      subscribers.last = subscribers.actual;
      debugPrintSubs();
      lastHour = timeNow.hour;
    }
  }

  if (millis() - entrySubscriberLoop > SUBSCRIBER_INTERVAL) {
    // Check if the wifi is connected
    // wifiConnect();

    //   DEBUG_PRINTLN("Subscriber Loop");
    entrySubscriberLoop = millis();
    updateSubs();
  }

  delay(1000);
  DEBUG_PRINT(".");
}
