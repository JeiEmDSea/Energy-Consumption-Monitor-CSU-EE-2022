#include "EmonLib.h"
#include "LiquidCrystal_I2C.h"

#define VOLTAGE_SENSOR_PIN A0
#define CURRENT_SENSOR_PIN A1

const float vCal = 700;
const float iCal = 5; // ? 5.405 -> ACS712|5A|185mV/A -> https://community.openenergymonitor.org/t/emonlib-at-no-load-doesnt-give-zero-current-readings/3986#:~:text=I%20used%20current%20callibration%20at%205.405%20because%20ACS712%20has%20185mV/A%20and%20from%20the%20openenergy%20I%20found%20that%20for%20a%20voltage%20type%20sensor%20Current/VOltage%20will%20give%20the%20current%20callibration%20constant.

double energy_kWh = 0;
unsigned long startTime;

EnergyMonitor emon;
LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup()
{
    Serial.begin(9600);

    startTime = millis();

    emon.voltage(VOLTAGE_SENSOR_PIN, vCal, 1.7);
    emon.current(CURRENT_SENSOR_PIN, iCal);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(3, 0);
    lcd.print("Hello, world!");
    delay(2000);
    lcd.clear();
}

void loop()
{
    calculateValues();
    showValuesToLCD();

    delay(1000);
}

void calculateValues()
{
    emon.calcVI(20, 2000);
    // ? float realPower = emon.realPower;          // ? extract Real Power into variable
    // ? float apparentPower = emon.apparentPower;  // ? extract Apparent Power into variable
    // ? float powerFActor = emon.powerFactor;      // ? extract Power Factor into Variable
    // ? float supplyVoltage = emon.Vrms;           // ? extract Vrms into Variable
    // ? float Irms = emon.Irms;                    // ? extract Irms into Variable}

    unsigned long time = (millis() - startTime) - startTime;
    energy_kWh = energy_kWh + (emon.apparentPower * (0.1 / 3600));
}

void showValuesToLCD()
{
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("V:" + String(emon.Vrms, 1) + "V");
    lcd.setCursor(9, 0);
    lcd.print("P:" + String(emon.apparentPower, 2) + "VA");
    lcd.setCursor(0, 1);
    lcd.print("I:" + String(emon.Irms, 2) + "A");
    lcd.setCursor(9, 1);
    lcd.print("E:" + String(energy_kWh * 1000, 1) + "mWh");
}
