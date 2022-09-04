#include "EmonLib.h"
#include "LiquidCrystal_I2C.h"
#include "DS3231.h"
#include "SPI.h"
#include "Sim800L.h"
#include "SoftwareSerial.h"
#include "SdFat.h"
#include "Arduino.h"
#include "U8g2lib.h"
#include "Wire.h"

#define VOLTAGE_SENSOR_PIN A0
#define CURRENT_SENSOR_PIN A1
#define RX 10
#define TX 11

const float vCal = 700;
const float iCal = 5; // ? 5.405 -> ACS712|5A|185mV/A -> https://community.openenergymonitor.org/t/emonlib-at-no-load-doesnt-give-zero-current-readings/3986#:~:text=I%20used%20current%20callibration%20at%205.405%20because%20ACS712%20has%20185mV/A%20and%20from%20the%20openenergy%20I%20found%20that%20for%20a%20voltage%20type%20sensor%20Current/VOltage%20will%20give%20the%20current%20callibration%20constant.

double energy_kWh = 0;
double pricePerkWh = 10;

String number = "+639355276219";
String codeMessage = "#monitor";

bool redrawGraph = true;
double ox, oy;
double lastReading = 0;
double yLength = 15;
double x = 0;
double xLength = 60;
double xInterval = 20;

EnergyMonitor emon;
LiquidCrystal_I2C lcd(0x27, 20, 4);
DS3231 rtc(SDA, SCL);
SdFat sdCard;
SdFile sdFile;
SoftwareSerial gsm(RX, TX);
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, SCL, SDA);

void setup()
{
  Serial.begin(9600);
  emon.voltage(VOLTAGE_SENSOR_PIN, vCal, 1.7);
  emon.current(CURRENT_SENSOR_PIN, iCal);
  initLCD();
  initSDcard();
  rtc.begin();
  initGSM();
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  loadCurrentConsumption();
  lastReading = energy_kWh * 1000;
}

void loop()
{
  calculateValues();
  showValuesToLCD();
  saveDataToSDcard();

  drawGraph(
      x++,                   // second
      energy_kWh * 1000,     // consumption
      30,                    // x origin
      50,                    // y origin
      75,                    // graph width
      30,                    // graph height
      0,                     // start second
      xLength,               // end second
      xInterval,             // x-axis division
      lastReading,           // y-axis lower bound
      lastReading + yLength, // y-axis upper bound
      lastReading + yLength, // y-axis division
      0,
      redrawGraph);

  if (x == xLength)
  {
    x = 0;
    lastReading = energy_kWh * 1000;
    redrawGraph = true;
  }

  delay(500);

  parseMessages();

  delay(500);
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

void parseMessages()
{
  String message = "";

  while (gsm.available() > 0)
  {
    char ch = (char)gsm.read();
    message += ch;

    if (!message.startsWith("#"))
    {
      message = "";
    }
  }

  message.trim();

  if (message.length() > 0)
  {
    Serial.println(message);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Message: " + message);
    delay(2000);

    if (message == codeMessage)
    {
      double foreCast = ((energy_kWh * pricePerkWh) / rtc.getTime().hour) * 24;
      String data = String("total mWh: " + String(energy_kWh * 1000, 1) + "\ncurrent price: P" + String(energy_kWh * pricePerkWh, 2) + "\nfuture price: P" + String(foreCast, 2));
      sendMessage(data);
    }
  }
}

void sendMessage(String mess)
{
  gsm.println("AT+CMGS=\"" + number + "\"\r");
  delay(200);
  gsm.println(mess);
  delay(100);
  gsm.println((char)26); // ? ASCII code of CTRL+Z
  delay(200);
}

void initGSM()
{
  gsm.begin(9600);
  delay(1000);

  gsm.println("AT+CMGF=1");
  delay(200);
  gsm.println("AT+CNMI=1,2,0,0,0");
  delay(200);

  sendMessage("GSM Initialized");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("GSM Initialized");
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

void drawGraph(double x, double y, double gx, double gy, double w, double h, double xlo, double xhi, double xinc, double ylo, double yhi, double yinc, double dig, boolean &redraw)
{
  double i = 0;
  double temp = 0;

  if (redraw == true)
  {
    redraw = false;
    u8g2.clearBuffer();

    ox = (x - xlo) * (w) / (xhi - xlo) + gx;
    oy = (y - ylo) * (gy - h - gy) / (yhi - ylo) + gy;

    // draw y scale
    for (i = ylo; i <= yhi; i += yinc)
    {
      // compute the transform
      // note my transform funcition is the same as the map function, except the map uses long and we need doubles
      temp = (i - ylo) * (gy - h - gy) / (yhi - ylo) + gy;
      if (i == 0)
      {
        u8g2.drawHLine(gx - 3, temp, w + 3);
      }
      else
      {
        u8g2.drawHLine(gx - 3, temp, 3);
      }
      u8g2.setCursor(gx - 27, temp - 3);
      u8g2.println(i, dig);
    }

    // draw x scale
    for (i = xlo; i <= xhi; i += xinc)
    {
      // compute the transform
      temp = (i - xlo) * (w) / (xhi - xlo) + gx;
      if (i == 0)
      {
        u8g2.drawVLine(temp, gy - h, h + 3);
      }
      else
      {
        u8g2.drawVLine(temp, gy, 3);
      }
      u8g2.setCursor(temp, gy + 6);
      u8g2.println(i, dig);
    }
  }

  // graph drawn now plot the data
  // the entire plotting code are these few lines...

  x = (x - xlo) * (w) / (xhi - xlo) + gx;
  y = (y - ylo) * (gy - h - gy) / (yhi - ylo) + gy;
  u8g2.drawLine(ox, oy, x, y);
  u8g2.drawLine(ox, oy - 1, x, y - 1);
  ox = x;
  oy = y;

  // up until now print sends data to a video buffer NOT the screen
  // this call sends the data to the screen
  u8g2.sendBuffer();
}