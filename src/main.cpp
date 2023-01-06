#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include "ACS712.h"
#include "Fonts/FreeSerifBold12pt7b.h"
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>  //Ticker Library

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
// We have 30 amps version sensor connected to A0 pin of arduino
// Replace with your version if necessary
ACS712 sensor(ACS712_30A, A0);
AsyncWebServer server(80);
Ticker timer1;

#define NUMFLAKES     10 // Number of snowflakes in the animation example

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

float d_volts, d_amps, d_wats, d_amp_hours, d_wat_hours, d_volts_s1,d_volts_s2,d_volts_s3;
uint32_t d_seconds;

const char* ssid = "Tenda_B01928";
const char* password = "22071982";

const char* PARAM_MESSAGE = "message";

String message_text = "";
String message_size = "";
String message_x = "";
String message_y = "";
String message = "";
String message_font = "";
boolean display_show = false;
boolean display_clear = false;

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

String IntToString(int counter){
    return (counter < 10 ? "0" : "") + String(counter);
}

void TimerHandler1()
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
    display.println(IntToString(h)+":"+IntToString(m)+":"+IntToString(s));

    // show s1,s2,s3 volts
    display.setCursor(80, 35);
    display.println("s1 " + String(d_volts_s1,2));
    display.setCursor(80, 45);
    display.println("s2 " + String(d_volts_s2,2));
    display.setCursor(80, 55);
    display.println("s3 " + String(d_volts_s3,2));

    display.display();
    //display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void setup() {
    Serial.begin(9600);

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
    // HTML SERVER
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.printf("WiFi Failed!\n");
    } else {
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
            request->send(200, "text/html", "<form method=post><input type='submit'></form>");
        });
        // Send a POST request to <IP>/post with a form field message set to <message>
        server.on("/", HTTP_POST, [](AsyncWebServerRequest *request){
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
            message = "<form method=post><label>X</label><input name='x' value='"+message_x+"'/>"
                      +"<br><label>Y</label><input name='y' value='"+message_y+"'/>"
                      +"<br><label>size</label><input name='message_size' value='"+message_size+"'/><br>"
                      +"<br><label>font</label><input name='message_font' value='"+message_font+"'/><br>"
                      +"<br><label>text</label><input name='message_text' value='"+message_text+"'/>"
                      +"<br><input type='submit' name='clear' value='clear'/>"
                      +"<input type='submit' name='show' value='show'/><br><input type='submit'/></form><br>DShow:"+String(display_show)+"<br>DClear:" + String(display_clear);
            request->send(200, "text/html", message);
        });
        server.onNotFound(notFound);
        server.begin();
    }


    Serial.println("Getting single-ended readings from AIN0..3");
    Serial.println("ADC Range: +/- 6.144V (1 bit = 0.1875mV/ADS1115)");
    // The ADC input range (or gain) can be changed via the following
    // functions, but be careful never to exceed VDD +0.3V max, or to
    // exceed the upper and lower limits if you adjust the input range!
    // Setting these values incorrectly may destroy your ADC!
    //                                                                ADS1015  ADS1115
    //                                                                -------  -------
    // ads.setGain(GAIN_TWOTHIRDS);  // 2/3x gain +/- 6.144V  1 bit = 3mV      0.1875mV (default)
    // ads.setGain(GAIN_ONE);        // 1x gain   +/- 4.096V  1 bit = 2mV      0.125mV
    // ads.setGain(GAIN_TWO);        // 2x gain   +/- 2.048V  1 bit = 1mV      0.0625mV
    // ads.setGain(GAIN_FOUR);       // 4x gain   +/- 1.024V  1 bit = 0.5mV    0.03125mV
    // ads.setGain(GAIN_EIGHT);      // 8x gain   +/- 0.512V  1 bit = 0.25mV   0.015625mV
    // ads.setGain(GAIN_SIXTEEN);    // 16x gain  +/- 0.256V  1 bit = 0.125mV  0.0078125mV

    if (!ads.begin()) {
        Serial.println("Failed to initialize ADS.");
        while (1);
    }
    // Show initial display buffer contents on the screen --
    // the library initializes this with an Adafruit splash screen.
    display.display();
    delay(1000); // Pause for 2 seconds

    // Clear the buffer
    display.clearDisplay();
    // calibrate() method calibrates zero point of sensor,
    // It is not necessary, but may positively affect the accuracy
    // Ensure that no current flows through the sensor at this moment
    // If you are not sure that the current through the sensor will not leak during calibration - comment out this method
    Serial.println("Calibrating... Ensure that no current flows through the sensor at this moment");
    int zero = sensor.calibrate();
    Serial.println("Done!");
    Serial.println("Zero point for this sensor = " + String(zero));

    //Initialize Ticker every 0.5s
    d_seconds = 1;
    d_amp_hours = d_wat_hours = 0;
    timer1.attach(1, TimerHandler1); //Use attach_ms if you need time in ms
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

//    delay(1000);
    float I = sensor.getCurrentDC();

    // Send it to serial
    Serial.println(String("I = ") + I + " A");

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
    d_volts = volts2;
    d_volts_s1 = volts1;
    d_volts_s2 = volts2;
    d_volts_s3 = volts3;
    d_amps = I;
    d_wats = d_amps * d_volts;
    showDataOnDisplay();
    // Wait a second before the new measurement
    delay(1000);
}