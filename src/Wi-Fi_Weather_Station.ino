#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include "DHTesp.h"
#include <LiquidCrystal_I2C.h>
#include <GyverBME280.h>                      // Подключение библиотеки
GyverBME280 bme;                              // Создание обьекта bme
AsyncWebServer server(80);

// датчик DHT
#define DHTPin 2

DHTesp dht;

float Temperature;
float Humidity;
int delta;
uint32_t pressure_array[6];
uint32_t sumX, sumY, sumX2, sumXY;
float a, b;
byte time_array[6];
int dispRain;

LiquidCrystal_I2C lcd(0x27, 16, 2);


byte gragus[8]
{
  0b01100,
  0b10010,
  0b10010,
  0b01100,
  0b00000,
  0b00000,
  0b00000,
  0b00000,
};


const char *ssid     = "MyWi-Fi";
const char *password = "11223344";

const long utcOffsetInSeconds = 10800;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

unsigned long previousMillis = 0;  // хранит время последнего обновления светодиода
unsigned long timer_for_barometr = 0;
const long interval = 2000;        // интервал мигания (миллисекунды)
// Определение NTP-клиента для получения времени
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup()
{
  Serial.begin(115200);
  lcd.begin(16,2);
  lcd.init();
  lcd.setCursor(4, 0);
  lcd.print("STARTING");
  lcd.createChar(1,gragus);
  lcd.setCursor(0, 1);
  lcd.print("#");
  dht.setup(DHTPin, DHTesp::DHT11);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print("#");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! This is a sample response.");
  });
  
  AsyncElegantOTA.begin(&server);    // Start AsyncElegantOTA
  server.begin();
  lcd.print("#");
  Serial.println("HTTP server started");
  timeClient.begin();
  lcd.print("#");
  timeClient.update();
  lcd.print("#");
  delay(100);
  lcd.print("#");
  bme.begin();

 // bme.takeForcedMeasurement();
  uint32_t Pressure = bme.readPressure();
  for (byte i = 0; i < 6; i++) {   // счётчик от 0 до 5
    pressure_array[i] = Pressure;  // забить весь массив текущим давлением
    time_array[i] = i;             // забить массив времени числами 0 - 5
  }
  lcd.print("#");
  lcd.clear();
}

void loop() 
{
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) 
  {
    // сохранить последнее внемя
    previousMillis = currentMillis;
    if (WiFi.status() == WL_CONNECTED)
    {
      timeClient.update();
      //Serial.print(daysOfTheWeek[timeClient.getDay()]);
      //Serial.print(", ");
      //Serial.print(timeClient.getHours());
      //Serial.print(":");
      //Serial.print(timeClient.getMinutes());
    }

    Humidity = dht.getHumidity();       // получить значение влажности

    if(!Humidity)
    {
      Humidity = 50;
    }
    else if(Humidity>100)
    {
      Humidity = 50;
    }
    else if(Humidity<0)
    {
      Humidity = 50;
    }

    //Serial.print("Temperature:");
    //Serial.println(Temperature);
    //Serial.print("Humidity:");
    //Serial.println(Humidity);

    //float pressure = bme.readPressure();        // Читаем давление в [Па]
    //Serial.print("Pressure: ");
    //Serial.print(pressure / 100.0F);            // Выводим давление в [гПа]
    //Serial.print(" hPa , ");

     //Serial.print("Temperature: ");
    //Serial.print(bme.readTemperature());        // Выводим темперутуру в [*C]
    //Serial.println(" *C");

    print_time();
    print_temp();
    print_Humidity();
    print_dispRain();
    print_Pressure();
  }

  if(currentMillis-timer_for_barometr>=600000)
  {
    lcd.clear();
    timer_for_barometr = currentMillis;

       // тут делаем линейную аппроксимацию для предсказания погоды
    long averPress = 0;
    for (byte i = 0; i < 10; i++) {
      //bme.takeForcedMeasurement();
      averPress += bme.readPressure();
      delay(1);
    }
    averPress /= 10;

    for (byte i = 0; i < 5; i++)
    {                   // счётчик от 0 до 5 (да, до 5. Так как 4 меньше 5)
      pressure_array[i] = pressure_array[i + 1];     // сдвинуть массив давлений КРОМЕ ПОСЛЕДНЕЙ ЯЧЕЙКИ на шаг назад
    }
    pressure_array[5] = averPress;                    // последний элемент массива теперь - новое давление
    sumX = 0;
    sumY = 0;
    sumX2 = 0;
    sumXY = 0;
    for (int i = 0; i < 6; i++) 
    {                    // для всех элементов массива
      sumX += time_array[i];
      sumY += (long)pressure_array[i];
      sumX2 += time_array[i] * time_array[i];
      sumXY += (long)time_array[i] * pressure_array[i];
    }
    a = 0;
    a = (long)6 * sumXY;             // расчёт коэффициента наклона приямой
    a = a - (long)sumX * sumY;
    a = (float)a / (6 * sumX2 - sumX * sumX);
    delta = a * 6;      // расчёт изменения давления
    dispRain = map(delta, -250, 250, 100, -100);  // пересчитать в проценты
    //Serial.println(String(pressure_array[5]) + " " + String(delta) + " " + String(dispRain));   // дебаг
  }

}

void print_time()
{
 lcd.setCursor(0, 0);
 lcd.print(timeClient.getHours());
 lcd.setCursor(2, 0);
 lcd.print(":");
 lcd.setCursor(3, 0);
 lcd.print(timeClient.getMinutes());
}

void print_temp()
{
 lcd.setCursor(0, 1);
 lcd.print(bme.readTemperature());
 lcd.setCursor(4, 1);
 lcd.print(char(1));
 lcd.setCursor(5, 1);
}


void print_Humidity()
{
 lcd.setCursor(6, 1);
 lcd.print((int)Humidity);
 lcd.setCursor(8, 1);
 lcd.print("%");
}

void print_Pressure()
{
 lcd.setCursor(10, 1);
 lcd.print((int)pressure_array[5]/100);
 lcd.print("hPa");
}

void print_dispRain()
{
 lcd.setCursor(7, 0);
 lcd.print("Rain:");
 lcd.setCursor(14, 0);
 lcd.print(dispRain);
 lcd.print("%");
}

