/*
  * edMeteo v.1.0
  * Copyright © 2024 Chernov Maksim
  * Released under the MIT License.
  * GitHub - https://github.com/eternaldevru
  * Telegram - https://t.me/eternaldev_ru
  * Website - https://eternaldev.ru 
*/

#include <DS_raw.h>
#include <microDS18B20.h>
#include <microOneWire.h>

#include <GyverBME280.h>
#include <LiquidCrystal_I2C.h>

#include <bufferfiller.h>
#include <enc28j60.h>
#include <EtherCard.h>
#include <net.h>
#include <stash.h>

#define FUNC_BTN   2    // номер пина кнопки    
#define BUZZER_PIN 3    // номер пина зуммера
#define DALLAS_PIN 4    // номер пина датчика DS18B20
#define LED_PIN    5    // номер пина светодиода
#define BME_ADDR   0x76 // адрес датчика BME280
#define LCD_ADDR   0x27 // адрес дисплея

byte check_mark_char[]  = {B00000, B00000, B00001, B00010, B10100, B01000, B00000, B00000}; // символ галочки
byte temperature_char[] = {B00100, B01010, B01010, B01110, B01110, B11111, B11111, B01110}; // символ температуры
byte humidity_char[]    = {B00100, B00100, B01010, B01010, B10001, B10011, B10111, B01110}; // сивол влажности
byte pressure_char[]    = {B00000, B00100, B00100, B00100, B10101, B01110, B00100, B11111}; // символ атм. давления

float current_temp_dallas;
float current_temp_bme;
float current_humidity;
float current_pressure;

uint32_t previous_update_millis = 0;
uint8_t update_interval = 5;              // интервал опроса датчиков (сек.)      

uint32_t previous_next_screen_millis = 0;
int8_t next_screen_interval = 5;          // интервал переключения экранов с показаниями (сек.)
uint8_t next_screen_number = 1;

uint32_t previous_backlight_millis = 0;
uint8_t backlight_interval = 15;          // время горения подсветки (сек.)

uint64_t previous_sending_millis = 0;
uint32_t sending_interval = 360;          // интервал передачи данных на narodmon.ru (сек.) Не чаще 1 раза в 5 минуту!

bool backlight_flag;
bool backlight_btn_flag;
bool backlight_btn_state;
uint32_t backlight_btn_timer = 0;

bool led_state = true;
uint32_t previous_errors_blink_millis;

uint8_t eth_errors = 0;

uint16_t beepFrequency[3] = {1700, 1800, 1900};

byte mymac[] = {0x3E, 0x75, 0x83, 0x0A, 0x8C, 0x5D}; // MAC-адрес. Заменить на свой! Сгенерировать можно на https://ciox.ru/mac-address-generator 
const char website[] PROGMEM = "narodmon.ru";
static byte session;
byte Ethernet::buffer[500];
Stash stash;

MicroDS18B20<DALLAS_PIN> dallas;
GyverBME280 bme;
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);

void setup() {
  
  pinMode(FUNC_BTN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(9600);
  Serial.println("\n[edMeteo]");

  digitalWrite(LED_PIN, HIGH);

  dallas.setResolution(12);
  bme.setStandbyTime(STANDBY_1000MS);
  bme.begin(BME_ADDR);

  lcd.init();
  lcd.backlight();
  lcd.createChar(1, check_mark_char);
  lcd.createChar(2, temperature_char);
  lcd.createChar(3, humidity_char);
  lcd.createChar(4, pressure_char);

  lcd.setCursor(4, 0);
  lcd.print(F("edMeteo"));
  lcd.setCursor(4, 1);
  lcd.print(F("ver.1.0"));

  for (uint8_t i = 0; i < 3; i++) {
    tone(BUZZER_PIN, beepFrequency[i]);
    delay(150);
    noTone(BUZZER_PIN);
    delay(5);
  }

  delay(3000);
  lcd.clear();

  // проверяем доступность DS18B20
  lcd.setCursor(0, 0);
  lcd.print(F("DS18B20"));
  dallas.requestTemp();

  if(dallas.readTemp()) {
    lcd.setCursor(13, 0);
    lcd.print(F("[\1]"));
    Serial.println("\n[DS18B20 - ok]");  
  }
  else {
    lcd.setCursor(13, 0);
    lcd.print("[x]");
    Serial.println("\n[DS18B20 - connection error]");    
  }

  delay(1000);

  // проверяем доступность BME280
  lcd.setCursor(0, 1);
  lcd.print(F("BME280"));

  if(bme.begin(BME_ADDR)) {
    lcd.setCursor(13, 1);
    lcd.print(F("[\1]"));
    Serial.println("[BME280 - ok]");  
  }
  else {
    lcd.setCursor(13, 1);
    lcd.print(F("[x]"));
    Serial.println("[BME280 - connection error]");   
  }

  delay(1000);

  // инициализируем ethernet-модуль и аодключаемся к интернету
  initialize_ethernet();

  previous_sending_millis = millis();;

  backlightOn();

}

void loop() {

   ether.packetLoop(ether.packetReceive());

    const char* reply = ether.tcpReply(session);
    if (reply != 0) {
      Serial.println("[got a response]");
      Serial.println(reply);
    }

   uint32_t current_millis = millis();

  // опрос датчиков
   if ((current_millis - previous_update_millis) >= (update_interval * 1000)) {
     
    dallas.requestTemp();
    current_temp_dallas = dallas.getTemp();

    current_temp_bme = bme.readTemperature();
    current_humidity = bme.readHumidity();
    current_pressure = bme.readPressure();
    current_pressure = pressureToMmHg(current_pressure);

    Serial.print("\nt1 = "); 
    Serial.println(current_temp_dallas); 
    Serial.print("t2 = "); 
    Serial.println(current_temp_bme); 
    Serial.print("humidity = "); 
    Serial.println(current_humidity); 
    Serial.print("pressure = "); 
    Serial.println(current_pressure);

    previous_update_millis = current_millis;
    
  }

  // смена экранов с показаниями
  if ((current_millis - previous_next_screen_millis) >= (next_screen_interval * 1000)) {

    switch (next_screen_number) {
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(F("\32\32\32\32\32\32\32\32\32\32\32"));
        lcd.setCursor(0, 1);
        lcd.print(F("\32\32\32\32\32\32\32\32\32\32\32"));
        
        lcd.setCursor(0, 0);
        lcd.print(F("\2 "));
        lcd.print(formatMeasurement(current_temp_dallas, 0));
        lcd.write(223);
        lcd.print("C");
  
        lcd.setCursor(0, 1);
        lcd.print(F("\2 "));
        lcd.print(formatMeasurement(current_temp_bme, 0));
        lcd.write(223);
        lcd.print("C");
        next_screen_number = 2;
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print(F("\32\32\32\32\32\32\32\32\32\32\32"));
        lcd.setCursor(0, 1);
        lcd.print(F("\32\32\32\32\32\32\32\32\32\32\32"));

        lcd.setCursor(0, 0);
        lcd.print(F("\3 "));
        lcd.print(formatMeasurement(current_humidity, 0));
        lcd.print("%");
  
        lcd.setCursor(0, 1);
        lcd.print(F("\4 "));
        lcd.print(formatMeasurement(current_pressure, 1));
        lcd.print(" mm Hg");
        next_screen_number = 1;
     
        break;
    }

    previous_next_screen_millis = current_millis;
    
  }

  // отработка нажатия кнопки
  backlight_btn_state = digitalRead(FUNC_BTN);
  
  if (backlight_btn_state && (current_millis - backlight_btn_timer) > 100) {
      
      if(!backlight_flag) {
        beep();
        backlightOn();  
      }
      
      backlight_btn_timer = current_millis;
      
  }

  // выключение подсветки по истечению заданного времени
  if (backlight_flag && (current_millis - previous_backlight_millis) >= (backlight_interval * 1000)) {
    backlight_flag = false;
    lcd.noBacklight();
    previous_backlight_millis = current_millis;
  }

  // отправка данных на narodmon.ru
  if ((current_millis - previous_sending_millis) >= (sending_interval * 1000)) {

    if(eth_errors > 0) {
      led_state = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println("\n[reconnect...]");
      initialize_ethernet();  
    }
    
    if(eth_errors == 0) {
      lcd.clear();
      lcd.setCursor(4, 0);
      lcd.print(F("Sending"));
      lcd.setCursor(4, 1);
      lcd.print(F("data..."));
      
      send_to_narodmon();
      delay(1000);
  
      lcd.clear();
      
      next_screen_number = 1;
      previous_next_screen_millis = 0;  
    }

    previous_sending_millis = millis();
    
  }

  if(eth_errors == 0) {
    led_state = true;
    digitalWrite(LED_PIN, HIGH);  
  }

  if ((eth_errors > 0) && (current_millis - previous_errors_blink_millis) >= 500) {
     
    led_state = !led_state;
    led_state ? digitalWrite(LED_PIN, HIGH) : digitalWrite(LED_PIN, LOW);
    
    previous_errors_blink_millis = current_millis;
    
  }

}

// функция включение подсветки
void backlightOn() {
  
  backlight_flag = true;
  previous_backlight_millis = millis();
  lcd.backlight();
  
}

// функция для форматирования полученных значений
String formatMeasurement(float sensor_data, uint8_t mode) {
  
  int32_t increased;
  int32_t whole_part;
  int32_t fractional_part;
  String return_string;

  increased = floor(sensor_data * 10);
  whole_part = increased / 10;
  fractional_part = abs(increased % 10);
  
  if(mode == 0) {
    return_string = return_string + whole_part + "." + fractional_part;
    return return_string;     
  }
  else if(mode == 1) {
    return_string = whole_part; 
    return return_string;  
  }
}

// функция инициализации ethernet-модуля
void initialize_ethernet() {

  eth_errors = 0;

  backlightOn();
  lcd.clear();
  lcd.setCursor(4, 0);
  lcd.print(F("Ethernet"));
  lcd.setCursor(1, 1);
  lcd.print(F("initialization"));

  Serial.println("\n[ethernet initialization...]");

  delay(1000);

  lcd.clear();
  lcd.setCursor(5, 0);
  lcd.print(F("Please"));
  lcd.setCursor(5, 1);
  lcd.print(F("wait..."));

  delay(1000);
  
  if (ether.begin(sizeof Ethernet::buffer, mymac, SS) == 0) {
    eth_errors++;
    Serial.println(F("[error] failed to access Ethernet controller"));  
  }

  if (!ether.dhcpSetup()){
    eth_errors++;
    Serial.println(F("[error] DHCP failed"));  
  }

  ether.printIp("IP:  ", ether.myip);
  ether.printIp("GW:  ", ether.gwip);
  ether.printIp("DNS: ", ether.dnsip);

  if (!ether.dnsLookup(website)) {
    eth_errors++;
    Serial.println(F("[error] DNS failed"));
  }
    
  ether.printIp("SRV: ", ether.hisip);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("ethernet"));

  if(eth_errors > 0) {
    lcd.setCursor(13, 0);
    lcd.print(F("[x]"));
    lcd.setCursor(0, 1);
    lcd.print(F("errors: "));
    lcd.setCursor(8, 1);
    lcd.print(eth_errors); 
  }
  else {
    lcd.setCursor(13, 0);
    lcd.print(F("[\1]"));   
  }

  delay(3000);
  lcd.clear();
     
}

// функция для отправки данных на narodmon.ru
void send_to_narodmon() {

  Serial.println("\nstart sending...");
  byte sd = stash.create();

  stash.print("ID=");
  stash.print(mac2String(mymac));
  stash.print("&T1=");
  stash.println(current_temp_dallas);
  stash.print("&T2=");
  stash.println(current_temp_bme);
  stash.print("&BMPP=");
  stash.println(current_pressure);
  stash.save();
  
  int stash_size = stash.size();

  Stash::prepare(PSTR("POST http://$F/post HTTP/1.0" "\r\n"
    "Host: $F" "\r\n"
    "Content-Type: application/x-www-form-urlencoded" "\r\n"
    "Content-Length: $D" "\r\n" "\r\n"
    "$H"),
  website, website, stash_size, sd);
  
  session = ether.tcpSend();  
  
}

// функция для преобразования MAC-адреса в строку
String mac2String(byte ar[]){
  String s;
  for (byte i = 0; i < 6; ++i)
  {
    char buf[3];
    sprintf(buf, "%2X", ar[i]);
    s += buf;
    if (i < 5) s += ':';
  }
  return s;
}

// функция для короткого пика зуммером
void beep() {
  tone(BUZZER_PIN, 1700, 70);
}
