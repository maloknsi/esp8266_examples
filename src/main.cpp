#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include "Fonts/FreeSerifBold12pt7b.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <EEPROM.h>

//  Arduino UNO has 5.0 volt with a max ADC value of 1023 steps
//  ACS712 5A  uses 185 mV per A
//  ACS712 20A uses 100 mV per A
//  ACS712 30A uses  66 mV per A

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

#define OLED_DC     0
#define OLED_CS     16
#define OLED_RESET  2

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
AsyncWebServer server(80);
Ticker timer1;

#define NUMFLAKES     10 // Number of snowflakes in the animation example
#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

float d_volts, d_amps, d_wats, d_amp_hours, d_wat_hours, d_volts_s1,d_volts_s2,d_volts_s3;
float volts0, volts1, volts2, volts3;
uint32_t d_seconds;
boolean isShowDataOnDisplay = true;

struct configData
{
    char wifi_ssid[20];
    char wifi_password[20];
    char iot_server[50];
    float correct_v0;
    float correct_v1;
    float correct_v2;
    float correct_v3;
    float correct_amp;
};

configData appConfig =
        {
                "Mi Phone",
                "22071982",
                "https://www.example-api.com",
                8.42,
                8.48,
                4.46,
                2.04,
                1.0
        };

String message_text = "";
String message_size = "";
String message_x = "";
String message_y = "";
String message = "";
String message_font = "";
boolean set_display_show = false;
boolean set_display_clear = false;
boolean set_config = false;

void appConfigSave(){
    isShowDataOnDisplay = false;
    display.clearDisplay();
    display.setCursor(0, 0);
    delay(1000);
    Serial.println("appConfig:");
    display.println("appConfig:");

    Serial.println("wifi" + String(appConfig.wifi_ssid) + " / " + String(appConfig.wifi_password));
    display.println("W:" + String(appConfig.wifi_ssid));
    display.println(appConfig.wifi_password);
    display.display();
    Serial.println("correct (v0,v1,v2,v3,amp):");
    display.println("CT (v0,v1,v2,v3,amp):");
    display.println("");

    Serial.println(String(appConfig.correct_v0) + "/" + String(appConfig.correct_v1) + "/" + String(appConfig.correct_v2) + "/" + String(appConfig.correct_v3)+ "/" + String(appConfig.correct_amp));
    display.print(String(appConfig.correct_v0) + "/" + String(appConfig.correct_v1) + "/" + String(appConfig.correct_v2) + "/" + String(appConfig.correct_v3)+ "/" + String(appConfig.correct_amp));

    display.display();
    delay(1000);
    Serial.println("save eeprom");
    display.println("save eeprom");
    display.display();
    EEPROM.put(0, appConfig);
    if (EEPROM.commit()) {
        Serial.println("EEPROM successfully committed!");
        display.println("EEPROM success!");
    } else {
        Serial.println("ERROR! EEPROM commit failed!");
        display.println("EEPROM failed!");
    }
    display.display();
    delay(2000);
    isShowDataOnDisplay = true;
}

void appConfigLoad(){
    EEPROM.get(0,appConfig);
    if (isnan(appConfig.correct_v0)) appConfig.correct_v0 = 1;
    if (isnan(appConfig.correct_v1)) appConfig.correct_v1 = 1;
    if (isnan(appConfig.correct_v2)) appConfig.correct_v2 = 1;
    if (isnan(appConfig.correct_v3)) appConfig.correct_v3 = 1;
    if (isnan(appConfig.correct_amp)) appConfig.correct_amp = 1;
}

String intToString(int counter){
    return (counter < 10 ? "0" : "") + String(counter);
}

void timerHandler1()
{
    d_seconds++;
    d_amp_hours = d_amp_hours + d_amps / 3600;
    d_wat_hours = d_wat_hours + (d_amps * d_volts) / 3600;
}

void showDataOnDisplay() {
    if (isShowDataOnDisplay){
        display.clearDisplay();
        display.setFont(&FreeSerifBold12pt7b);
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 18);
        display.print(String(d_amps,2)+"A");
        display.setCursor(0, 38);
        display.print(String(d_volts,2)+"v");
        display.setCursor(0, 62);
        display.print(String(d_wats,2)+"w");

        display.setFont();
        display.setTextSize(1);
        display.setCursor(80, 0);
        display.println(String(d_amp_hours,3)+"ah");
        display.setCursor(80, 10);
        display.println(String(d_wat_hours,3)+"wh");
        // show time
        display.setCursor(80, 20);
        uint16_t h,m,s;
        uint32_t t = d_seconds;
        s = t % 60;
        t = (t - s)/60;
        m = t % 60;
        t = (t - m)/60;
        h = t;
        display.println(intToString(h) + ":" + intToString(m) + ":" + intToString(s));

        // show s1,s2,s3 volts
        display.setCursor(80, 35);
        display.println("1 " + String(d_volts_s1 - d_volts_s2,3));
        display.setCursor(80, 45);
        display.println("2 " + String(d_volts_s2 - d_volts_s3,3));
        display.setCursor(80, 55);
        display.println("3 " + String(d_volts_s3,3));

        display.display();
        //display.cp437(true);         // Use full 256 char 'Code Page 437' font
    }
}

void calculateDataToDisplay(){
    if (isnan(appConfig.correct_amp)) appConfig.correct_amp = 1;
    d_volts = volts0 * appConfig.correct_v0;
    d_volts_s1 = volts1 * appConfig.correct_v1;
    d_volts_s2 = volts2 * appConfig.correct_v2;
    d_volts_s3 = volts3 * appConfig.correct_v3;
    d_amps = (d_volts_s1 - d_volts) * 1000 * appConfig.correct_amp;// (amps_zero - volts3) * 1000 / 66;
    if (isnan(d_amps)) d_amps = 0;
    d_wats = d_amps * d_volts;
}


void requestNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void requestGet(AsyncWebServerRequest *request) {
    request->send(200, "text/html", "<form method=post><input type='submit'></form>");
}

void requestPost(AsyncWebServerRequest *request) {
    if (request->hasParam("x", true)) {
        message_x = request->getParam("x", true)->value();
    }
    if (request->hasParam("y", true)) {
        message_y = request->getParam("y", true)->value();
    }
    if (request->hasParam("message_size", true)) {
        message_size = request->getParam("message_size", true)->value();
    }
    if (request->hasParam("message_font", true)) {
        message_font = request->getParam("message_font", true)->value();
    }
    if (request->hasParam("message_text", true)) {
        message_text = request->getParam("message_text", true)->value();
    }
    if (request->hasParam("clear", true) && request->getParam("clear", true)->value()) {
        set_display_clear = true;
    }
    if (request->hasParam("show", true) && request->getParam("show", true)->value()) {
        set_display_show = true;
    }
    if (request->hasParam("wifi_set", true) && request->getParam("wifi_set", true)->value()) {
        if (request->hasParam("wifi_ssid", true) && request->getParam("wifi_ssid", true)->value()) {
            request->getParam("wifi_ssid", true)->value().toCharArray(appConfig.wifi_ssid,20);
        }
        if (request->hasParam("wifi_password", true) && request->getParam("wifi_password", true)->value()) {
            request->getParam("wifi_password", true)->value().toCharArray(appConfig.wifi_password,20);
        }
        set_config = true;
    }
    if (request->hasParam("correct_set", true) && request->getParam("correct_set", true)->value()) {
        if (request->hasParam("v0", true) && request->getParam("v0", true)->value()) {
            float v0 = request->getParam("v0", true)->value().toFloat();
            appConfig.correct_v0 = v0 / volts0;
        }
        if (request->hasParam("v1", true) && request->getParam("v1", true)->value()) {
            float v1 = request->getParam("v1", true)->value().toFloat();
            appConfig.correct_v1 = v1 / volts1;
        }
        if (request->hasParam("v2", true) && request->getParam("v2", true)->value()) {
            float v2 = request->getParam("v2", true)->value().toFloat();
            appConfig.correct_v2 = v2 / volts2;
        }
        if (request->hasParam("v3", true) && request->getParam("v3", true)->value()) {
            float v3 = request->getParam("v3", true)->value().toFloat();
            appConfig.correct_v3 = v3 / volts3;
        }
        if (request->hasParam("amps", true) && request->getParam("amps", true)->value()) {
            float amps = request->getParam("amps", true)->value().toFloat();
            appConfig.correct_amp = amps / d_amps;
        }
        calculateDataToDisplay();
        set_config = true;
    }
    message = String("<form method=post><label>WIFI SSID</label><input name='wifi_ssid' value='")+appConfig.wifi_ssid+"'/>"
              +"<br><label>WIFI SSID</label><input name='wifi_password' value='"+appConfig.wifi_password+"'/>"
              +"<br><input type='submit' name='wifi_set' value='wifi_set'/>"
              +"<br><label>v0 ["+appConfig.correct_v0+"]</label><input name='v0' value='"+d_volts+"'/>"
              +"<br><label>v1 ["+appConfig.correct_v1+"]</label><input name='v1' value='"+d_volts_s1+"'/>"
              +"<br><label>v2 ["+appConfig.correct_v2+"]</label><input name='v2' value='"+d_volts_s2+"'/>"
              +"<br><label>v3 ["+appConfig.correct_v3+"]</label><input name='v3' value='"+d_volts_s3+"'/>"
              +"<br><label>amps ["+appConfig.correct_amp+"]</label><input name='amps' value='"+d_amps+"'/>"
              +"<br><input type='submit' name='correct_set' value='correct_set'/>"
               +"<br><input type='submit' name='correct_refresh' value='correct_refresh'/>"
              +"<br><label>X</label><input name='x' value='"+message_x+"'/>"
              +"<br><label>Y</label><input name='y' value='"+message_y+"'/>"
              +"<br><label>size</label><input name='message_size' value='"+message_size+"'/><br>"
              +"<br><label>font</label><input name='message_font' value='"+message_font+"'/><br>"
              +"<br><label>text</label><input name='message_text' value='"+message_text+"'/>"
              +"<br><input type='submit' name='clear' value='clear'/>"
              +"<input type='submit' name='show' value='show'/><br><input type='submit'/></form>"
              +"<br>DShow:"+String(set_display_show)+"<br>DClear:" + String(set_display_clear);
    request->send(200, "text/html", message);
}
void setup() {
    Serial.begin(9600);
    Serial.println(F("init battery_indicator"));
    Serial.println(F("init display"));
    // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Don't proceed, loop forever
    }
    if (false){
        // test fonts
        display.setFont(&FreeSerifBold12pt7b);
        display.setTextColor(SSD1306_WHITE); // Draw white text
        for(int16_t j=0; j<50; j++) {
            display.clearDisplay();
            display.setTextSize(1);
            display.setCursor(0, j);
            for (int16_t i = 0; i < 10; i++) {
                display.println(String(j) + " 12.34V");
            }
            display.display();
            delay(2000);
        }
        return;
    }

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.clearDisplay();
    delay(1000); // Pause for 2 seconds
    display.setTextSize(1);
    display.setCursor(0, 10);
    //------------------------------
    Serial.println(F("init eeprom"));
    EEPROM.begin(sizeof(appConfig));
    //appConfigSave();
    appConfigLoad();
    Serial.println(appConfig.wifi_ssid);
    //------------------------------
    Serial.println(String("init WIFI:") + appConfig.wifi_ssid + "|" + appConfig.wifi_password);
    display.println("init wifi");
    display.println(appConfig.wifi_ssid);
    display.println(appConfig.wifi_password);
    display.display();
    // HTML SERVER
    WiFi.mode(WIFI_STA);
    WiFi.begin(appConfig.wifi_ssid, appConfig.wifi_password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        display.println("WiFi Failed!");
        display.display();
        Serial.println("WiFi Failed!");
        //----- INIT SOFT AP
        WiFi.softAP("battery_indicator");
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){requestGet(request);});
        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){requestPost(request);});
        server.onNotFound(requestNotFound);
        server.begin();
        display.println("SoftAp" + WiFi.softAPIP().toString());
        display.display();
        Serial.println("SoftAp" + WiFi.softAPIP().toString());
        delay(10000);
    } else {
        display.clearDisplay();
        display.println("WiFi connected!");
        Serial.println("WiFi connected!");
        display.println(String("IP: "));
        display.print(WiFi.localIP());
        display.display();
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){requestGet(request);});
        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){requestPost(request);});
        server.onNotFound(requestNotFound);
        server.begin();
        //delay(3000);
    }


    //                                                                ADS1015  ADS1115
    //                                                                -------  -------
    // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
     ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
    // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
    // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
    // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
    // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

    Serial.println("init adc");
    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        while (1);
    }

//    Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
//    int16_t adc0 = ads.readADC_SingleEnded(3);
//    amps_zero = ads.computeVolts(adc0);
//
//    Serial.println("Done!");
//    Serial.println("Zero point for this sensor = " + String(amps_zero));

    //Initialize Ticker every 0.5s
    d_seconds = 1;
    d_amp_hours = d_wat_hours = 0;
    timer1.attach(1, timerHandler1); //Use attach_ms if you need time in ms
}

void loop() {
    int16_t adc0, adc1, adc2, adc3;

    adc0 = ads.readADC_SingleEnded(0);
    adc1 = ads.readADC_SingleEnded(1);
    adc2 = ads.readADC_SingleEnded(2);
    adc3 = ads.readADC_SingleEnded(3);

    volts0 = ads.computeVolts(adc0);
    volts1 = ads.computeVolts(adc1);
    volts2 = ads.computeVolts(adc2);
    volts3 = ads.computeVolts(adc3);

    Serial.println("-----------------------------------------------------------");
    Serial.print("AIN0: "); Serial.print(adc0); Serial.print("  "); Serial.print(volts0); Serial.println("V");
    Serial.print("AIN1: "); Serial.print(adc1); Serial.print("  "); Serial.print(volts1); Serial.println("V");
    Serial.print("AIN2: "); Serial.print(adc2); Serial.print("  "); Serial.print(volts2); Serial.println("V");
    Serial.print("AIN3: "); Serial.print(adc3); Serial.print("  "); Serial.print(volts3); Serial.println("V");

    // show message from WEB SERVER
    if (set_display_show){
        set_display_show = false;
        if (message_font == "1"){
            display.setFont(&FreeSerifBold12pt7b);
        } else {
            display.setFont();
        }
        display.setTextSize(message_size.toInt());
        display.setCursor(message_x.toInt(), message_y.toInt());
        display.println(message_text);
        display.display();
    }


    if (set_display_clear){
        set_display_clear = false;
        display.clearDisplay();
        display.display();
    }
    if (set_config){
        set_config = false;
        appConfigSave();
    }
    // show data on display
    calculateDataToDisplay();
    showDataOnDisplay();
    // Wait a second before the new measurement
    delay(1000);
}