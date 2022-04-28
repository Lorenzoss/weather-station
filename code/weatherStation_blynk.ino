// Fill-in information from your Blynk Template here
#define BLYNK_TEMPLATE_ID ""
#define BLYNK_DEVICE_NAME ""

#define BLYNK_FIRMWARE_VERSION        "2.0.0"

#define BLYNK_PRINT Serial
// #define BLYNK_DEBUG
// #define APP_DEBUG
#define USE_WEMOS_D1_MINI

#include "BlynkEdgent.h"

// Pins:
#define voltageAnalogPin A0
// Virtual pins:
// V0 Temperature
// V1 Humidity
// V2 Pressure
// V3 Luminance
// V4 Battery Voltage
// V6 Dew point

#include <Wire.h>
// GY-49
#include <Max44009.h>
// BME280/BMP280
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

bool statusBME;
bool statusGY;

Max44009 myLux(0x4A);
Adafruit_BME280 bme;

BlynkTimer timer;

float currentTime;
String hourMinute;

bool minMaxReset;

BLYNK_WRITE(InternalPinRTC) {
    long t = param.asLong();
    float tempValue = t%86400;
    float dayTime = tempValue/86400;

    int hour = dayTime*24;
    int minute = dayTime*1440 - hour*60;

    currentTime = (hour * 60 + minute);
    currentTime = currentTime/1440;

    if(minute<10){
        hourMinute = String(hour) + ":0" + String(minute);
    }
    else{
        hourMinute = String(hour) + ":" + String(minute);
    }
}

float dewPointFormula(float temp, float humidity){
    float dewPoint;
    dewPoint = (237.3*(log(humidity/100) + ((17.27*temp)/(237.3+temp)))) / (17.27-(log(humidity/100)-((17.27*temp)/(237.3+temp))));
    return dewPoint;
}

float getRealTemp(float rawTemperature, float illuminance){
    // Simple correction for overheating
    float realTemp;
    float temperatureFactor = 10;
    float illuminanceFactor = illuminance/100000;
    realTemp = rawTemperature - temperatureFactor * illuminanceFactor * currentTime;
    return realTemp;
}

void SendData(){
    float rawTemperature;
    float humidity;
    float illuminance;

    // Pressure and humidity
    if(statusBME){
        Blynk.sendInternal("rtc", "sync"); 
        rawTemperature = bme.readTemperature();
        humidity = bme.readHumidity();
        float pressure = (bme.readPressure() / 100.0F)+28.77;

        Blynk.virtualWrite(V1, humidity);
        Blynk.virtualWrite(V2, pressure);
    }

    // Illuminance
    if(statusGY){
        illuminance = myLux.getLux();
        if(illuminance >= 0){
            Blynk.virtualWrite(V3, illuminance);
        }
        else{
            String desc = "GY49 error: " + String(illuminance);
            Blynk.logEvent("error_on_data_reading", desc);

        }
    }

    // Temperature and dewpoint
    if(statusGY && statusBME){
        if(illuminance > 0){
            float temperature = getRealTemp(rawTemperature, illuminance);
            Blynk.virtualWrite(V0, temperature);
            
            float dewPoint = dewPointFormula(temperature, humidity);
            Blynk.virtualWrite(V6, dewPoint);  

            Blynk.virtualWrite(V8, hourMinute);
        }
    }

    // Battery voltage
    float rawVoltage = analogRead(voltageAnalogPin);
    float voltage = (rawVoltage/1024)*5.05 ;
    if(voltage < 3.65){
        Blynk.logEvent("battery_voltage_low");
    }

    Blynk.virtualWrite(V5, rawVoltage);
    Blynk.virtualWrite(V4, voltage);

    ESP.deepSleep(120e6);
}

void setup()
{
    // Serial.begin(115200);
    BlynkEdgent.begin();

    // If the timer interval is too low ( < ~10 seconds) blynk does not have the time to start the firmware update before the esp goes in deep sleep
    timer.setInterval(10000L, SendData);

    Wire.begin();
    Wire.setClock(100000);
    delay(150);

    statusBME = bme.begin(0x76);
    statusGY = myLux.isConnected();

    if(!statusBME){
        Blynk.logEvent("sensor_offline", "BME280");
    }

    if(!statusGY){
        Blynk.logEvent("sensor_offline", "GY49");
    }
}

void loop() {
    BlynkEdgent.run();
    timer.run();
}

