#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include "DHTesp.h"
#include <LiquidCrystal_I2C.h>
#include <WiFiClientSecure.h>
#include <FastBot.h>
#include <GyverBME280.h>                      // Подключение библиотеки
#define BOT_TOKEN "5439018659:AAFP87_LfgVMh6m-yVHhXp0QcvPFlTNDdNU"
GyverBME280 bme;                              // Создание обьекта bme

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
FastBot bot(BOT_TOKEN);

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

int32_t menuID = 0;
byte depth = 0;

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
  bot.attach(newMsg);

  timeClient.begin();
  lcd.print("#");
  timeClient.update();
  lcd.print("#");
  delay(100);
  lcd.print("#");
  bme.begin();

  uint32_t Pressure = bme.readPressure();
  for (byte i = 0; i < 6; i++) {   // счётчик от 0 до 5
    pressure_array[i] = Pressure;  // забить весь массив текущим давлением
    time_array[i] = i;             // забить массив времени числами 0 - 5
  }
  lcd.print("#");
  Humidity = dht.getHumidity();       // получить значение влажности
  Temperature = bme.readTemperature();

  lcd.print("#");
  lcd.clear();
}

void loop() 
{
  bot.tick();
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) 
  {
    // сохранить последнее внемя
    previousMillis = currentMillis;
    if (WiFi.status() == WL_CONNECTED)
    {
      timeClient.update();
    }

    Humidity = dht.getHumidity();       // получить значение влажности
    Temperature = bme.readTemperature();
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
 lcd.print(Temperature);
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


// обработчик сообщений
void newMsg(FB_msg& msg) {
  Serial.println(msg.toString());
  String prosent(" %");
  String gradus(" °C");
  if(msg.OTA && msg.chatID == "631472433") 
  {
    bot.update();
  }
  else if (msg.text == "Закрити") 
  {
    bot.closeMenu();
  }
  else if(msg.text == "Температура")
  {
    String send_text(Temperature);
    bot.sendMessage(send_text + gradus, msg.chatID);
  }
  else if(msg.text == "Вологість")
  {
    String send_text(Humidity);
    bot.sendMessage(send_text + prosent, msg.chatID);
  }
  else if(msg.text == "Повна Інформація")
  {
    String send_text = "Повна інформація:";
    send_text += String("\nТемпература: ");
    send_text += String(Temperature);
    send_text += gradus;
    send_text += String("\nВолога: ");
    send_text += String(Humidity);
    send_text += prosent;
    send_text += String("\nАтмосферний тиск: ");
    send_text += String(pressure_array[5]/100);
    send_text += String(" КПа або ");
    send_text += String(pressureToMmHg(pressure_array[5]));
    send_text += String(" мм рт. стовпчика");
    send_text += String("\nЙмовірність дощу: ");
    send_text += String(dispRain);
    send_text += prosent;
    
    bot.sendMessage(send_text , msg.chatID);
  }
  else
  {
    bot.showMenu("Температура \t Вологість \t Повна Інформація \n Закрити",msg.chatID);
  }

}

