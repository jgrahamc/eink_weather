// eink_weather: this project uses an Inkplate 10 display and the 
// Pirate Weather API to get a weather forecast for a given latitude
// and longitude and display it.
//
// Copyright (c) 2023 John Graham-Cumming

#include <Inkplate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <driver/rtc_io.h>
#include <ArduinoJson.h>

#include "params.h"

#include "fonts/gfx/Roboto_Regular_7.h"
#include "fonts/gfx/Roboto_Regular_10.h"
#include "fonts/gfx/Roboto_Regular_24.h"
#include "fonts/gfx/Roboto_Bold_12.h"

const GFXfont *fontSmall  = &Roboto_Regular7pt8b;
const GFXfont *fontMedium = &Roboto_Bold12pt8b;
const GFXfont *fontLarge  = &Roboto_Regular24pt8b;
const GFXfont *fontFooter = &Roboto_Regular10pt8b;

Inkplate ink(INKPLATE_3BIT);

// This structure and the array are used to blend the historical weather
// forecast with an up to date forecast. That way the hourly data shown
// for 48 hours is a mix of what was predicted at midnight today and what
// is predicted now. 

#define ICON_SIZE 64
struct hour_slot {
  uint32_t when;
  float    temperature;
  char     icon[ICON_SIZE];
  
};

struct hour_slot hours[48];

// setup runs the entire program and then goes to sleep.
void setup() {
  ink.begin();

  // Used to record the time when the program woke up. This is used to adjust
  // the sleep time so that the sleep takes into account how long the board
  // was awake for an working.

  uint32_t started_at = 0;

  if (connectWiFi(wifi_network, wifi_password)) {
    setRTC();
    started_at = getRtcNow();

    if (ink.sdCardInit() != 0) {
      showWeather();
    } else {
      fatal("SD card failed to initialize");
    }
    disconnectWiFi();
  }

  deepSleep(started_at);
}

// loop contains nothing because the entire sketch will be woken up 
// in setup(), do work and then go to sleep again.
void loop() {}

// connectWiFi connects to the passed in WiFi network and returns true
// if successful.
bool connectWiFi(const char *ssid, const char *pass) {
  bool success = ink.joinAP(ssid, pass);
  if (!success) {
    fatal("Failed to connect to WiFi " + String(ssid));
  }

  return success;
}

// disconnectWiFi cleans up the result of connecting via connectWiFi
void disconnectWiFi() {
  ink.disconnect();
}

// setRTC sets the RTC via NTP
void setRTC() {

  // First 0 means no offset from GMT, second 0 means no daylight savings time.

  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.cloudflare.com");
  
  while (true) {
    delay(2000);
    uint32_t fromntp = time(NULL);

    // If time from NTP doesn't look like it's been set then wait
    
    if (fromntp < 1681141968) {
      continue;
    }
    
    ink.rtcSetEpoch(fromntp);

    // The default time when not set is January 1, 2066 00:00
    
    if (getRtcNow() < 3029529605) {
      break;
    }
  }
}

// gtcRtcNow returns the current epoch time from the RTC
uint32_t getRtcNow() {
  ink.rtcGetRtcData();
  return ink.rtcGetEpoch();
}

#define SECONDS_PER_HOUR (60*60)
#define SECONDS_PER_DAY (24*SECONDS_PER_HOUR)

const char days[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

// day converts a Unix epoch to the current day of the week taking into 
// account the timezone offset in hours.
const char *day(uint32_t when, float offset) {
  when += int(offset * SECONDS_PER_HOUR);
  int dow = int(floor(when/SECONDS_PER_DAY)+4)%7;
  return days[dow];
}

#define HHMM_SIZE 6

// hhmm converts a Unix epoch to hh:mm taking into account the timezone 
// offset in hours. out just be at least HHMM_SIZE.
void hhmm(uint32_t when, float offset, char *out) {
  when += int(offset * SECONDS_PER_HOUR);
  int hh = int(floor(when/SECONDS_PER_HOUR))%24;
  int mm = int(floor(when/60))%60;
  sprintf(out, "%02d:%02d", hh, mm);
}

// prHelper does the actual printing of text. It takes an (x, y) position
// of the text, the text and font. Plus two parameters wm and hm which
// are multipliers to apply to the width and height of the text being
// printed. If the width and height are w and h then this will calculate
// x + wm*w, y + hm*h. 
void prHelper(int16_t x, int16_t y, const char *text, const GFXfont *f,
  float wm = 0, float hm = 0) {
  int16_t x1;
  int16_t y1;
  uint16_t w;
  uint16_t h;
  ink.setFont(f);
  ink.setTextSize(1);
  ink.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  ink.setCursor(x+w*wm, y+h*hm);
  ink.print(text);  
}

// pr writes text at (x, y) with font f.
void pr(int16_t x, int16_t y, const char *text, const GFXfont *f) {
  prHelper(x, y, text, f);
}

 // centre centres a piece of text at the x, y position.
void centre(int16_t x, int16_t y, const char *text, const GFXfont *f) {
  prHelper(x, y, text, f, -0.5);
}

// right right-justifies text x, y position.
void right(int16_t x, int16_t y, const char *text, const GFXfont *f) {
  prHelper(x, y, text, f, -1);
}

// flushRight right-justifies text at the y position with size s.
void flushRight(int16_t y, const char *text, const GFXfont *f) {
  prHelper(ink.width()-20, y, text, f, -1);
}

#define SMALL_IMAGE 43
#define LARGE_IMAGE 128

// centreIcon draws an icon cenetred at the x, y position. If the
// icon name is partly-cloudy-day and the size s is 43 then this
// will load partly-cloudy-day-42.png.
void centreIcon(int16_t x, int16_t y, const char *img, int16_t s) {
  char tmp[100];
  sprintf(tmp, "%s-%d.png", img, s);

  // This assumes that icon are squares
  
  ink.drawImage(tmp, x-s/2, y-s/2);
}

// drawRectangle draws a rectangle given the top left corner and 
// width and height. We don't use the built in ink.drawRect because
// the lines are thinner than a one pixel line drawn by drawThickLine.
void drawRectangle(int16_t x0, int16_t y0, int16_t w, int16_t h) {
  int16_t x1 = x0 + w;
  int16_t y1 = y0 + h;
  ink.drawThickLine(x0, y0, x1, y0, 0, 1);
  ink.drawThickLine(x0, y1, x1, y1, 0, 1);
  ink.drawThickLine(x0, y0, x0, y1, 0, 1);
  ink.drawThickLine(x1, y0, x1, y1, 0, 1);
}

#define TEMP_SIZE 6

// roundTemp rounds a floating point temperature and write to a string
// that contains the temperature. out must be at least TEMP_SIZE. 
void roundTemp(float t, char *out) {

  // Temperatures are rounded up if above 0 and down if below 0. 
  // Example: 1.4C becomes 1C, 1.6C becomes 2C, -0.4C becomes 0C, 
  // -1.7C becomes -2C.
  
  if (t >= 0) {
    t += 0.5; 
  } else {
    t -= 0.5;
  }

  sprintf(out, "%d", int(t));
}

// CA certificate for the domain being connected to using TLS. Obtained
// using the openssl s_client as follows:
//
// openssl s_client -showcerts -servername api.pirateweather.net \
//     -connect api.pirateweather.net:443
//
// The certificate below is the last certificate output by openssl as it
// is the CA certificate of the domain above.
const char *cacert = \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIIEdTCCA12gAwIBAgIJAKcOSkw0grd/MA0GCSqGSIb3DQEBCwUAMGgxCzAJBgNV\n" \
  "BAYTAlVTMSUwIwYDVQQKExxTdGFyZmllbGQgVGVjaG5vbG9naWVzLCBJbmMuMTIw\n" \
  "MAYDVQQLEylTdGFyZmllbGQgQ2xhc3MgMiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0\n" \
  "eTAeFw0wOTA5MDIwMDAwMDBaFw0zNDA2MjgxNzM5MTZaMIGYMQswCQYDVQQGEwJV\n" \
  "UzEQMA4GA1UECBMHQXJpem9uYTETMBEGA1UEBxMKU2NvdHRzZGFsZTElMCMGA1UE\n" \
  "ChMcU3RhcmZpZWxkIFRlY2hub2xvZ2llcywgSW5jLjE7MDkGA1UEAxMyU3RhcmZp\n" \
  "ZWxkIFNlcnZpY2VzIFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n" \
  "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDVDDrEKvlO4vW+GZdfjohTsR8/\n" \
  "y8+fIBNtKTrID30892t2OGPZNmCom15cAICyL1l/9of5JUOG52kbUpqQ4XHj2C0N\n" \
  "Tm/2yEnZtvMaVq4rtnQU68/7JuMauh2WLmo7WJSJR1b/JaCTcFOD2oR0FMNnngRo\n" \
  "Ot+OQFodSk7PQ5E751bWAHDLUu57fa4657wx+UX2wmDPE1kCK4DMNEffud6QZW0C\n" \
  "zyyRpqbn3oUYSXxmTqM6bam17jQuug0DuDPfR+uxa40l2ZvOgdFFRjKWcIfeAg5J\n" \
  "Q4W2bHO7ZOphQazJ1FTfhy/HIrImzJ9ZVGif/L4qL8RVHHVAYBeFAlU5i38FAgMB\n" \
  "AAGjgfAwge0wDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMCAYYwHQYDVR0O\n" \
  "BBYEFJxfAN+qAdcwKziIorhtSpzyEZGDMB8GA1UdIwQYMBaAFL9ft9HO3R+G9FtV\n" \
  "rNzXEMIOqYjnME8GCCsGAQUFBwEBBEMwQTAcBggrBgEFBQcwAYYQaHR0cDovL28u\n" \
  "c3MyLnVzLzAhBggrBgEFBQcwAoYVaHR0cDovL3guc3MyLnVzL3guY2VyMCYGA1Ud\n" \
  "HwQfMB0wG6AZoBeGFWh0dHA6Ly9zLnNzMi51cy9yLmNybDARBgNVHSAECjAIMAYG\n" \
  "BFUdIAAwDQYJKoZIhvcNAQELBQADggEBACMd44pXyn3pF3lM8R5V/cxTbj5HD9/G\n" \
  "VfKyBDbtgB9TxF00KGu+x1X8Z+rLP3+QsjPNG1gQggL4+C/1E2DUBc7xgQjB3ad1\n" \
  "l08YuW3e95ORCLp+QCztweq7dp4zBncdDQh/U90bZKuCJ/Fp1U1ervShw3WnWEQt\n" \
  "8jxwmKy6abaVd38PMV4s/KCHOkdp8Hlf9BRUpJVeEXgSYCfOn8J3/yNTd126/+pZ\n" \
  "59vPr5KW7ySaNRB6nJHGDn2Z9j8Z3/VyVOEVqQdZe4O/Ui5GjLIAZHYcSNPYeehu\n" \
  "VsyuLAOQ1xk4meTKCRlb/weWsKh/NEnfVqn3sF/tM+2MR7cwA130A4w=\n" \
  "-----END CERTIFICATE-----\n";

// callAPI calls the Pirate Weather API with a set of excluded sections (which
// must not be empty) and an epoch time for when the weather forecast should
// start. Either returns the HTTP body as a String or an empty string for an
// error. If when is zero then gets the current forecast.
String callAPI(char *exclude, uint32_t when) {
  WiFiClientSecure tls;
  tls.setCACert(cacert);
  HTTPClient http;

  // The API URL. A number of elements of the response can be filtered
  // using exclude to minimize the size of the JSON response.

  const char *api_format = \
    "https://api.pirateweather.net/forecast/" \
    "%s/%s,%s%s"                              \
    "?units=%s"                               \
    "&tz=precise"                             \
    "&exclude=%s";

  char api[232];
  char whens[32] = "";
  if (when != 0) {
    sprintf(whens, ",%d", when);
  }
  sprintf(api, api_format, api_key, lat, lon, whens, units, exclude);

  int retries = 0;

  // Retry API calls at most five times with two seconds between
  // retries.

  while (retries < 5) {
    if (http.begin(tls, api)) {
      int code = http.GET();
      if (code == 200) {
        return http.getString();
      }
    }
    
    retries += 1;
    delay(2000);
  }

  return "";
}

// showWeather gets and displays the weather forecast on screen.
void showWeather() {

  // Step 1.
  //
  // Since the hourly section of the Pirate Weather API returns the next
  // 48 hours and we want today and tomorrow we need to ask for midnight
  // today. This is done by retrieving the time now from the RTC. But there's
  // a hitch: we want local time so... we first make an API call to the
  // Pirate Weather API excluding all sections so we can just extract the UTC
  // offset.

  uint32_t now = getRtcNow();

  // This means that clock hasn't been set
  
  if (now >= 3029529605) {
    return;
  }
  
  String response = callAPI("currently,minutely,hourly,daily,alerts", 0);
  if (response == "") {
    return;
  }

  // This isn't efficient because the return from http.getString() is
  // a String which will get duplicated by deserializeJson. Size
  // determined using https://arduinojson.org/v6/assistant/#/step1

  StaticJsonDocument<768> jsonTiming;
  DeserializationError err = deserializeJson(jsonTiming, response);

  if (err) {
    fatal("Deserialize JSON failed " + String(err.c_str()));
    return;
  }

  // This will be the delta (in hours) between UTC and the local time. For example,
  // in the UK during the summer this will be 1 and to go from UTC to local time
  // you must add 1. Note that this is a float because the offset could be 1.5 hours
  // or similar (e.g. India is 5.5 hours from UTC).

  float offset = jsonTiming["offset"];
  const char *tz = jsonTiming["timezone"];

  // Figure out the epoch equivalent of midnight local time. First round to the nearest day.
  // Since the epoch starts on a midnight boundary this will give us UTC midnight for today.
  // The subtract the offset hours to get the UTC time that corresponds to local midnight.

  uint32_t seconds_since_midnight = now;

  // Do not be tempted to remove these two lines. This is integer division so this is used
  // for rounding!
  
  now /= SECONDS_PER_DAY;
  now *= SECONDS_PER_DAY;
  
  uint32_t midnight = now - int(offset * SECONDS_PER_HOUR);
  seconds_since_midnight -= midnight;

  // Step 2.
  //
  // Now get the historial weather forecast from the calculated local midnight and insert
  // it into the hours array.

  response = callAPI("currently,daily,minutely,alerts", midnight);
  if (response == "") {
    return;
  }

  DynamicJsonDocument doc(32768);
  err = deserializeJson(doc, response);
  
  if (err) {
    fatal("Deserialize JSON failed " + String(err.c_str()));
    return;
  }

  int i = 0;
  
  JsonObject hourly = doc["hourly"];
  for (JsonObject hourly_data_item : hourly["data"].as<JsonArray>()) {
    hours[i].when = hourly_data_item["time"];
    hours[i].temperature = hourly_data_item["temperature"];
    strcpy(hours[i].icon, hourly_data_item["icon"]);
    i += 1;
  }

  // Step 3.
  //
  // Then get the current forecast and overwrite entries in the hours
  // array so we blend the historical forecast and the current one.

  response = callAPI("currently,minutely,alerts", 0);
  if (response == "") {
    return;
  }
  
  doc.clear();
  err = deserializeJson(doc, response);
  
  if (err) {
    fatal("Deserialize JSON failed " + String(err.c_str()));
    return;
  }

  hourly = doc["hourly"];
  for (JsonObject hourly_data_item : hourly["data"].as<JsonArray>()) {
    for (i = 0; i < 48; i++) {
      if (hours[i].when == hourly_data_item["time"]) {
        hours[i].temperature = hourly_data_item["temperature"];
        strcpy(hours[i].icon, hourly_data_item["icon"]);
      }
    }
  }

  // Step 4. 
  //
  // Draw the hourly data on screen and since the last API call included the data 
  // for the next 7 days draw the daily forecast as well.

  int16_t bar_start_x = 50;
  int16_t bar_start_y = 200;

  // This ensure that the bar_width is a multiple of 24 so that the hours are spaced
  // at an integer number of pixels.
  
  int16_t bar_width = ink.width() - 2*bar_start_x;
  int16_t temp_bar_width = bar_width;
  bar_width /= 24;
  int16_t bar_hour_spacing = bar_width;
  bar_width *= 24;

  bar_start_x += (temp_bar_width - bar_width)/2;

  int16_t bar_height = 100;
  int16_t bar_gap = 100;

  drawRectangle(bar_start_x, bar_start_y,                    bar_width, bar_height);
  drawRectangle(bar_start_x, bar_start_y+bar_height+bar_gap, bar_width, bar_height);

  // This draws a marker showing the current time relative to the today weather forecast

  uint32_t now_offset = ((uint32_t)bar_width * seconds_since_midnight) / SECONDS_PER_DAY;
  int16_t now_x = bar_start_x + now_offset;
  ink.fillTriangle(now_x-3, bar_start_y-7, now_x+3, bar_start_y-6, now_x, bar_start_y-1, 1);
    
  ink.setTextColor(0, 7);
  pr(bar_start_x, bar_start_y-7, "Today", fontMedium);
  pr(bar_start_x, bar_start_y+bar_height+bar_gap-7, "Tomorrow", fontMedium);
  
  int16_t bar_y = bar_start_y;
  int16_t bar_x = bar_start_x;
  int16_t short_tick = 5;
  int16_t long_tick = 10;

  int16_t last_x = -1;
  char last[128];  
  for (i = 0; i < 48; i++) {
    ink.drawThickLine(bar_x, bar_y+bar_height, bar_x,
      bar_y+bar_height+((i%2==0)?short_tick:long_tick), 0, 1);
    
    if ((i % 2) == 0) {  
      char hour[HHMM_SIZE];
      hhmm(hours[i].when, offset, &hour[0]);
      char temp[TEMP_SIZE];
      roundTemp(hours[i].temperature, &temp[0]);
      if (i == 0) {
        pr(bar_x, bar_y+bar_height+short_tick+12, hour, fontSmall);        
        pr(bar_x, bar_y+bar_height+short_tick+36, temp, fontMedium);
      } else {
        centre(bar_x, bar_y+bar_height+short_tick+12, hour, fontSmall);
        centre(bar_x, bar_y+bar_height+short_tick+36, temp, fontMedium);
      }
      ink.print("\xba");
    }  

    if (last_x == -1) {
      strcpy(last, hours[i].icon);
      last_x = bar_x;
    } else {
      if (strcmp(last, hours[i].icon) != 0) {
        ink.drawThickLine(bar_x, bar_y, bar_x, bar_y+bar_height, 0, 1);
        centreIcon(last_x + (bar_x - last_x)/2, bar_y+bar_height/2, last, SMALL_IMAGE);
        strcpy(last, hours[i].icon);
        last_x = bar_x;
      }
    }

    bar_x += bar_hour_spacing;
    if ((i%24) == 23) {
      centreIcon(last_x + (bar_x - last_x)/2, bar_y+bar_height/2, last, SMALL_IMAGE);      
      bar_x = bar_start_x;
      bar_y += bar_height+bar_gap;
      last_x = -1;
    }
  }

  bar_width /= 2;
  bar_width /= 7;
  int16_t bar_day_spacing = bar_width;
  bar_width *= 7;

  bar_x = bar_start_x;
  drawRectangle(bar_x, bar_y, bar_width, bar_height);
  pr(bar_x, bar_y-7, "Next 7 Days", fontMedium);

  int count = 0;
  JsonObject daily = doc["daily"];
  for (JsonObject daily_data_item : daily["data"].as<JsonArray>()) {
    ink.drawThickLine(bar_x, bar_y, bar_x, bar_y+bar_height, 0, 1);
    
    const char* icon = daily_data_item["icon"];
    centreIcon(bar_x + bar_day_spacing/2, bar_y+bar_height/2, icon, SMALL_IMAGE);

    uint32_t when = daily_data_item["time"];
    centre(bar_x + bar_day_spacing/2, bar_y+bar_height+22, day(when, offset), fontMedium);

    char high[TEMP_SIZE];
    roundTemp(daily_data_item["temperatureHigh"], &high[0]);
    char low[TEMP_SIZE];
    roundTemp(daily_data_item["temperatureLow"], &low[0]);

    // The reason the degree symbol is printed separately from the actual temperature is
    // that this causes the temperature digits to be centered and looks better. If the
    // symbol is included in the centred string it doesn't look as good on screen.

    centre(bar_x + bar_day_spacing/2, bar_y+21, high, fontMedium);
    ink.print("\xba");
    centre(bar_x + bar_day_spacing/2, bar_y+bar_height-5, low, fontMedium);
    ink.print("\xba");
    
    bar_x += bar_day_spacing;
    count += 1;
    if (count == 7) {
      break;
    }
  }

  // Step 5.
  //
  // Now make another API call to just get the minute-by-minute rain for the current
  // hour and draw that on screen.

  response = callAPI("daily,hourly,alerts", 0);
  if (response == "") {
    return;
  }

  doc.clear();
  err = deserializeJson(doc, response);
  
  if (err) {
    fatal("Deserialize JSON failed " + String(err.c_str()));
    return;
  }

  int16_t bar_spacing = 50;
  int16_t new_width = bar_width - bar_spacing;
  new_width /= 60;
  new_width *= 60;  
  bar_x = bar_start_x + bar_width + bar_spacing + (bar_width - new_width - bar_spacing);
  bar_width = new_width;
  int16_t rain_width = bar_width/60;
  drawRectangle(bar_x, bar_y, bar_width, bar_height);
  pr(bar_x, bar_y-6, "Rain Next 60 Minutes", fontMedium);

  float max_rain = 4;

  count = 0;
  int16_t centre_x;
  JsonObject minutely = doc["minutely"];
  for (JsonObject minutely_data_item : minutely["data"].as<JsonArray>()) {
    float rain = minutely_data_item["precipIntensity"];
    if (rain > max_rain) {
      rain = max_rain;
    }

    if (rain > 0) {
      ink.drawThickLine(bar_x, bar_y, bar_x, bar_y+(bar_height*(rain/max_rain)), 0, 1);
    }

    if ((count % 10) == 0) {
      int when = minutely_data_item["time"];
      char temp[TEMP_SIZE];
      hhmm(when, offset, &temp[0]);
      if (count == 0) {
        pr(bar_x, bar_y+bar_height+short_tick+11, temp, fontSmall);
      } else if (count == 60) {
        right(bar_x, bar_y+bar_height+short_tick+11, temp, fontSmall);
      } else {
        centre(bar_x, bar_y+bar_height+short_tick+11, temp, fontSmall);
      }
      if (count == 30) {
        centre_x = bar_x;
      }
      ink.drawThickLine(bar_x, bar_y+bar_height, bar_x, bar_y+bar_height+short_tick, 0, 1);        
    }

    count += 1;
    bar_x += rain_width;
  }

  // Step 6.
  //
  // Using data about the current forecast show the title and when the forecast
  // was last checked and local observations (weather and temperature).

  JsonObject currently = doc["currently"];
  const char *icon = currently["icon"]; 
  float c = currently["temperature"];
  uint32_t when = currently["time"];

  char temp[TEMP_SIZE];
  roundTemp(c, &temp[0]);
  pr(200, 80, title, fontLarge);
  char hm[HHMM_SIZE];
  hhmm(getRtcNow(), offset, &hm[0]);
  char subtitle[80];
  sprintf(subtitle, "Forecast checked at %s. It's %s\xba right now.", hm, temp);
  
  pr(200, 150, subtitle, fontLarge);
  centreIcon(bar_start_x+64, 100, icon, LARGE_IMAGE);

  // Step 7.
  //
  // Add the status bar at the bottom

  uint32_t timenow = getRtcNow();
  char hmnow[HHMM_SIZE];
  hhmm(timenow, offset, &hmnow[0]);
  char hmnext[HHMM_SIZE];
  hhmm(timenow+sleep_time, offset, &hmnext[0]);

  JsonObject flags = doc["flags"];
  const char *version = flags["version"];
  
  char status_bar[255];
  sprintf(status_bar, "Updated: %s - Next update: %s - Time zone: %s (%.1f) - Temperature: %d\xba - Battery: %.1fv - API %s", 
    hmnow, hmnext, tz, offset, ink.readTemperature(), ink.readBattery(), version);
  flushRight(ink.height()-20, status_bar, fontSmall);

  show();
}

// clear clears the display
void clear() {
  ink.clearDisplay();
  ink.display();
}

// show displays whatever has been written to the display on the
// e-ink screen itself.
void show() {
  ink.display();
}

// fatal is used to show a fatal error on screen
void fatal(String s) {
  clear();
  ink.setTextColor(0, 7);
  ink.setTextSize(4);
  ink.setCursor(100, 100);
  ink.print(s);
  show();
}

// deepSleep puts the device into deep sleep mode for sleep_time
// seconds. When it wakes up setup() will be called.
void deepSleep(uint32_t started_at) {

  // This is needed for Inkplate 10's that use the ESP32 WROVER-E
  // in order to reduce power consumption during sleep.
  
  rtc_gpio_isolate(GPIO_NUM_12);

  // Calculate how many seconds the sleep time should be adjusted
  // by by figuring out how long the board was awake.

  uint32_t time_taken = 0;

  if (started_at != 0) {
    time_taken = getRtcNow() - started_at;
  }

  // The following sets the wake up timer to run after the appropriate
  // interval (in microseconds) and then goes to sleep.

  esp_sleep_enable_timer_wakeup((sleep_time - time_taken) * 1000000);
  esp_deep_sleep_start();
}
