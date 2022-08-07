#include "EmonLib.h"
#include "LiquidCrystal_I2C.h"
#include "DS3231.h"
#include "SPI.h"
#include "Sim800L.h"
#include "SoftwareSerial.h"
#include "SdFat.h"
#include "ArduinoJson.h"

#define VOLTAGE_SENSOR_PIN A0
#define CURRENT_SENSOR_PIN A1
#define RX 6
#define TX 5

const float vCal = 700;
const float iCal = 5; // ? 5.405 -> ACS712|5A|185mV/A -> https://community.openenergymonitor.org/t/emonlib-at-no-load-doesnt-give-zero-current-readings/3986#:~:text=I%20used%20current%20callibration%20at%205.405%20because%20ACS712%20has%20185mV/A%20and%20from%20the%20openenergy%20I%20found%20that%20for%20a%20voltage%20type%20sensor%20Current/VOltage%20will%20give%20the%20current%20callibration%20constant.

double energy_kWh = 0;
double pricePerkWh = 10;

char num[] = "+639355276219";

EnergyMonitor emon;
LiquidCrystal_I2C lcd(0x27, 20, 4);
DS3231 rtc(SDA, SCL);
SdFat sdCard;
SdFile sdFile;
Sim800L gsm(RX, TX);
SoftwareSerial dataPlotter(2, 3);

void setup()
{
  Serial.begin(9600);
  emon.voltage(VOLTAGE_SENSOR_PIN, vCal, 1.7);
  emon.current(CURRENT_SENSOR_PIN, iCal);
  initLCD();
  initSDcard();
  rtc.begin();
  gsm.begin(9600);

  loadCurrentConsumption();
}

void loop()
{
  calculateValues();
  showValuesToLCD();
  saveDataToSDcard();
  sendDataToUser(17, 0); // ? every 5:00PM

  delay(1000);
}

void sendDataToPlotter()
{
  StaticJsonDocument<100> sensorData;

  sensorData["energy_kWh"] = energy_kWh;
  dataPlotter.print(sensorData.as<String>());
}

void calculateValues()
{
  emon.calcVI(20, 2000);
  /** Value map
   * ? float realPower = emon.realPower;          // ? extract Real Power into variable
   * ? float apparentPower = emon.apparentPower;  // ? extract Apparent Power into variable
   * ? float powerFActor = emon.powerFactor;      // ? extract Power Factor into Variable
   * ? float supplyVoltage = emon.Vrms;           // ? extract Vrms into Variable
   * ? float Irms = emon.Irms;                    // ? extract Irms into Variable}
   */

  energy_kWh = energy_kWh + (emon.apparentPower * (0.1 / 3600));
}

void showValuesToLCD()
{
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("V:" + String(emon.Vrms, 1) + "V");
  lcd.setCursor(0, 1);
  lcd.print("I:" + String(emon.Irms, 2) + "A");
  lcd.setCursor(0, 2);
  lcd.print("P:" + String(emon.apparentPower, 2) + "VA");

  lcd.setCursor((energy_kWh < 1 ? 10 : 9), 0);
  lcd.print("E:" + String(energy_kWh * 1000, 1) + "mWh");
  lcd.setCursor(10, 1);
  lcd.print("C:P" + String(energy_kWh * pricePerkWh, 2));

  double foreCast = ((energy_kWh * pricePerkWh) / rtc.getTime().hour) * 24;
  lcd.setCursor(10, 2);
  lcd.print("F:P" + String(foreCast, 2));

  lcd.setCursor(0, 3);
  lcd.print("T:" + String(rtc.getTimeStr()));
}

void saveDataToSDcard()
{
  String logFile = String(String(rtc.getDateStr()) + ".log.txt");
  String consumptionFile = String(String(rtc.getDateStr()) + ".consumption.txt");

  if (sdFile.open(logFile.c_str(), O_CREAT | O_WRITE | O_APPEND))
  {
    sdFile.print(rtc.getDateStr());
    sdFile.print(",");
    sdFile.print(rtc.getTimeStr());
    sdFile.print(",");
    sdFile.println(String(energy_kWh));
    sdFile.close();
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error logging data");
    delay(2000);
  }

  if (sdFile.open(consumptionFile.c_str(), O_READ))
  {
    sdFile.close();
    sdFile.remove(consumptionFile.c_str());
  }

  if (sdFile.open(consumptionFile.c_str(), O_CREAT | O_WRITE))
  {
    sdFile.print(String(energy_kWh * 1000, 1));
    sdFile.close();
  }
  else
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Error logging consumption");
    delay(2000);
  }
}

void loadCurrentConsumption()
{
  String consumptionFile = String(String(rtc.getDateStr()) + ".consumption.txt");

  if (sdFile.open(consumptionFile.c_str(), O_READ))
  {
    String myString = "";

    while (sdFile.available())
    {
      myString += mapByteToString(sdFile.read());
    }

    energy_kWh = myString.toDouble() / 1000;
    sdFile.close();
  }
}

void sendDataToUser(int hr, int min)
{
  if (rtc.getTime().hour == hr && rtc.getTime().min == min && rtc.getTime().sec == 0)
  // ! if (rtc.getTime().min % 10 == 0) // ? every 10 minutes
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sending data");
    String message = String("Your energy consumption is P" + String(energy_kWh * pricePerkWh, 1));
    gsm.sendSms(num, message.c_str());
    delay(5000);
  }
}

void initLCD()
{
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Energy Consumption");
  lcd.setCursor(0, 1);
  lcd.print("Monitoring System");
  delay(2000);
  lcd.clear();
}

void initSDcard()
{
  if (sdCard.begin())
  {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SD Card initalized");
    delay(2000);
  }

  /** Connection error handling
   * *else
   * *{
   * *  lcd.clear();
   * *  lcd.setCursor(0, 0);
   * *  lcd.print("SD card initialization failed");
   * *  delay(2000);
   * *  return;
   * *}
   * *
   * *myFile = SD.open("DATA.txt", FILE_WRITE);
   * *
   * *if (myFile)
   * *{
   * *  lcd.clear();
   * *  lcd.setCursor(0, 0);
   * *  lcd.print("Data file ready");
   * *  delay(2000);
   * *  myFile.close();
   * *}
   */
}

String mapByteToString(int byte)
{
  switch (byte)
  {
  case 48:
    return "0";
  case 49:
    return "1";
  case 50:
    return "2";
  case 51:
    return "3";
  case 52:
    return "4";
  case 53:
    return "5";
  case 54:
    return "6";
  case 55:
    return "7";
  case 56:
    return "8";
  case 57:
    return "9";
  default:
    return ".";
  }
}