#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include "Fonts/FreeSerifBold12pt7b.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
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

#define TIMER1_INTERVAL_MS        20
#define DEBOUNCING_INTERVAL_MS    100
#define LONG_PRESS_INTERVAL_MS    5000

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &SPI, OLED_DC, OLED_RESET, OLED_CS);
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */
//ACS712 sensor(ACS712_30A, A0);
AsyncWebServer server(80);
Ticker timer1;

#define NUMFLAKES     10 // Number of snowflakes in the animation example
#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

float d_volts, d_amps, d_wats, d_amp_hours, d_wat_hours, d_volts_s1,d_volts_s2,d_volts_s3;
uint32_t d_seconds;
float amps_zero;

struct configData
{
    char wifi_ssid[20];
    char wifi_password[20];
    char iot_server[50];
    float correct_v0;
    float correct_v1;
    float correct_v2;
//    bool sleep;
//    uint16_t brightness;
};

configData appConfig =
        {
                "MALOK2",
                "aposum1982",
                "https://www.example-api.com",
                1,
                1,
                1
        };

String ssid = "Tenda_B01928";
String password = "22071982";

const char* PARAM_MESSAGE = "message";

String message_text = "";
String message_size = "";
String message_x = "";
String message_y = "";
String message = "";
String message_font = "";
boolean display_show = false;
boolean display_clear = false;

void configSave(){
    EEPROM.put(0, appConfig);
    if (EEPROM.commit()) {
        Serial.println("EEPROM successfully committed!");
    } else {
        Serial.println("ERROR! EEPROM commit failed!");
    }
}
void configLoad(){
    EEPROM.get(0,appConfig);
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
        display_clear = true;
    }
    if (request->hasParam("show", true) && request->getParam("show", true)->value()) {
        display_show = true;
    }
    if (request->hasParam("wifi_set", true) && request->getParam("wifi_set", true)->value()) {
       request->getParam("wifi_ssid", true)->value().toCharArray(appConfig.wifi_ssid,20);
       request->getParam("wifi_password", true)->value().toCharArray(appConfig.wifi_password,20);
        configSave();
    }
    message = String("<form method=post><label>WIFI SSID</label><input name='wifi_ssid' value='")+appConfig.wifi_ssid+"'/>"
              +"<br><label>WIFI SSID</label><input name='wifi_password' value='"+appConfig.wifi_password+"'/>"
              +"<br><label>X</label><input name='x' value='"+message_x+"'/>"
              +"<br><label>Y</label><input name='y' value='"+message_y+"'/>"
              +"<br><label>size</label><input name='message_size' value='"+message_size+"'/><br>"
              +"<br><label>font</label><input name='message_font' value='"+message_font+"'/><br>"
              +"<br><label>text</label><input name='message_text' value='"+message_text+"'/>"
              +"<br><input type='submit' name='wifi_set' value='wifi_set'/><input type='submit' name='clear' value='clear'/>"
              +"<input type='submit' name='show' value='show'/><br><input type='submit'/></form><br>DShow:"+String(display_show)+"<br>DClear:" + String(display_clear);
    request->send(200, "text/html", message);
}

String intToString(int counter){
    return (counter < 10 ? "0" : "") + String(counter);
}

String eepromGet(int addr){
    String strText;
    for(int i=0;i<20;i++)
    {
        uint8_t _char = EEPROM.read(addr+i);
        if (_char == 0) break;
        strText = strText + char(_char);
    }
    //EEPROM.get(addr,strText);
    return strText;
}



void eepromSet(int addr, String value){
    for(int i=0;i<int(value.length());i++)
    {
        EEPROM.write(addr+i, value[i]); //Write one by one with starting address of 0x0F
    }
    int i = addr + int(value.length());
    EEPROM.write(i,0);
    //EEPROM.put(addr, value);
    if (EEPROM.commit()) {
        Serial.println("EEPROM successfully committed!");
    } else {
        Serial.println("ERROR! EEPROM commit failed!");
    }
}

void timerHandler1()
{
    d_seconds++;
    d_amp_hours = d_amp_hours + d_amps / 3600;
    d_wat_hours = d_wat_hours + (d_amps * d_volts) / 3600;
}

void showDataOnDisplay() {
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
    display.println("1 " + String(d_volts_s1,3));
    display.setCursor(80, 45);
    display.println("2 " + String(d_volts_s2,3));
    display.setCursor(80, 55);
    display.println("3 " + String(d_volts_s3,3));

    display.display();
    //display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void setup() {
    Serial.begin(9600);
    EEPROM.begin(150);
    //delay(5000);
    Serial.println(F("init battery_indicator"));
    //eepromSet(0,"Tenda_B01928");
    //eepromSet(20,"22071982");
    //configSave();
    configLoad();
    Serial.println(appConfig.wifi_ssid);
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
//    ssid = eepromGet(0);
//    password = eepromGet(20);

    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.clearDisplay();
    delay(1000); // Pause for 2 seconds
    display.setTextSize(1);
    display.setCursor(0, 10);
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
        delay(3000);
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){requestGet(request);});
        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){requestPost(request);});
        server.onNotFound(requestNotFound);
        server.begin();
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

    Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
    int16_t adc0 = ads.readADC_SingleEnded(3);
    amps_zero = ads.computeVolts(adc0);

    //int zero = sensor.calibrate();
    Serial.println("Done!");
    Serial.println("Zero point for this sensor = " + String(amps_zero));

    //Initialize Ticker every 0.5s
    d_seconds = 1;
    d_amp_hours = d_wat_hours = 0;
    timer1.attach(1, timerHandler1); //Use attach_ms if you need time in ms
}

void loop() {
    int16_t adc0, adc1, adc2, adc3;
    float volts0, volts1, volts2, volts3;

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
    if (display_show){
        display_show = false;
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


    if (display_clear){
        display_clear = false;
        display.clearDisplay();
        display.display();
    }
    // show data on display
    d_volts = volts0;
    d_volts_s1 = volts0;
    d_volts_s2 = volts1;
    d_volts_s3 = volts3;
    d_amps = (amps_zero - volts3) * 10;
    d_wats = d_amps * d_volts;
    showDataOnDisplay();
    // Wait a second before the new measurement
    delay(1000);
}