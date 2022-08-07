#include "Arduino.h"
#include "U8g2lib.h"
#include "Wire.h"
#include "ArduinoJson.h"

bool redrawGraph = true;
double ox, oy;
double lastReading = 0;
double yLength = 15;
double x = 0;
double xLength = 60;
double xInterval = 20;

double energy_kWh = 0;

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

void setup()
{
  Serial.begin(9600);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
}

void loop()
{
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
}

void parseData()
{
  if (Serial.available())
  {
    StaticJsonDocument<100> settings;
    DeserializationError err = deserializeJson(settings, Serial);

    if (err == DeserializationError::Ok)
    {
      // ! Serial.println(settings.as<String>());
      energy_kWh = settings["energy_kWh"].as<String>().toDouble();
    }
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