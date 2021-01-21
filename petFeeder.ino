//Kütüphaneler
#include <ESP8266_Lib.h>
#include <BlynkSimpleShieldEsp8266.h>
#include <SoftwareSerial.h>
#include "PIR.h"
#include "Servo.h"
#include "PiezoSpeaker.h"
#include <TimeLib.h>
#include <WidgetRTC.h>

//Blynk auth token
char auth[] = "M8ev0EwmriOsY2TQKzE7ZaBya0sBv_xj";


// WiFi bilgisi
char ssid[] = "ANTALYA NİKSARLILAR DERNEĞİ";
char pass[] = "6060niksar6060";


//Pin yerleri
SoftwareSerial EspSerial(10, 11); // RX, TX
#define PIR_PIN_SIG 4
#define SERVOSM_PIN_SIG 3
#define THINSPEAKER_PIN_POS 2


//ESP8266 Baud
#define ESP8266_BAUD 9600


//Global değişkenler
ESP8266 wifi(&EspSerial);
PIR pir(PIR_PIN_SIG);
Servo servoSM;
PiezoSpeaker PiezoSpeaker(THINSPEAKER_PIN_POS);

int pirCounter = 0;
int countdown = 0;
int pirCooldown = 240; // İlk açılışta 5 dakika bekleme süresi.

long morningFeedTime = 0;
long eveningFeedTime = 0;


//Hoparlör melodileri
unsigned int comeLength          = 3;
unsigned int comeMelody[]        = {NOTE_C5, NOTE_G5, NOTE_C5};
unsigned int comeNoteDurations[] = {8      , 8      , 8};


//Blynk değişkenleri
BlynkTimer timer;
WidgetRTC rtc;
WidgetTerminal terminal(V10);

//Şimdi Besle butonu. Eğer gelen değer 1 yani true ise, rotateServo metoduyla servo döndürülüyor.
BLYNK_WRITE(V1)
{
  if(param.asInt()){
      rotateServo("MANUEL");
    }
}

//Sabah besleme saati
BLYNK_WRITE(V2)
{
  morningFeedTime = param[0].asLong();
}

//Akşam besleme saati
BLYNK_WRITE(V3)
{
  eveningFeedTime = param[0].asLong();
}

//Hoparlör çalma tuşu
BLYNK_WRITE(V5)
{
  if(param.asInt()){
    PiezoSpeaker.playMelody(comeLength, comeMelody, comeNoteDurations);
  }
}

//Blynk'e bağlanıp, widgetleri başlatıyoruz ve uygulamada bulunan değerleri senkronize ediyoruz (Sabah, akşam besleme saati vs.). 
//Ardından terminali sıfırlayıp, Arduino'nun başladığını uygulama üzerinden haber veriyoruz.
BLYNK_CONNECTED() {
  rtc.begin();
  Blynk.syncAll();
  Blynk.virtualWrite(V4, pirCounter);
  terminal.clear();
  terminal.flush();
  write(hourToString() + ":" + minToString() + " - " +  "petFeeder basladi!");
}

void checkPIRandFeed(){

  //PIR sensörünü okuyor ve sayacı arttırıyor. pirCooldown değişkeniyle bekletmemizin sebebi, PIR sensörün ısınmadığı için ilk 3-4 dakikada sürekli true değerini döndürmesi ve sürekli motoru çalıştırması.
  if(pirCooldown == 0) {
    if(pir.read()){
    pirCounter++;
    Blynk.virtualWrite(V4, pirCounter);
  } 
  }else{
    pirCooldown--;
  }

  //PIR sensördeki değer 10 ve katı olursa, evcil hayvana yemek veriyor.
  if(pirCounter % 10 == 0 && pirCounter != 0){
    rotateServo("SENSOR");
    pirCounter++;
  }

  //Eğer countdown 0 değilse, her saniye 1 azaltıyoruz. Bu sayede toplamda 1 dakika beklemiş oluyor ve yemek fonksiyonu birden fazla çalışmıyor.
  //Delay yerine countdown değişkeni kullanmamızın sebebi, delay fonksiyonunun tüm Arduino'yu 1 dakikalığına durdurması.
  if(countdown != 0){
      countdown--;
  }

  //Sabah yemeği kontrolü
  if(shouldFeed(morningFeedTime) && countdown == 0){
      write("Sabah yemeği vakti!");
      rotateServo("OTOMATIK");
      countdown = 60;
  }

  //Akşam yemeği kontrolü
  if(shouldFeed(eveningFeedTime) && countdown == 0){
      write("Akşam yemeği vakti!");
      rotateServo("OTOMATIK");
      countdown = 60;
  }
}

//Evcil hayvanın beslenme saatinin gelip gelmediğini hesaplayıp true veya false olarak sonuç döndürüyor.
bool shouldFeed(long feedTime){
  return feedTime / 3600 == hour() && (feedTime % 3600) / 60 == minute() ? true : false;
}

void setup()
{
  //Debug için:
  //Serial.begin(9600);

  //ESP8266'yı belirlenen baud değerinde başlatıyoruz.
  EspSerial.begin(ESP8266_BAUD);
  delay(10);

  //Blynk ile wifiye bağlanıp, ana timer komutumuzu çalıştırıyoruz.
  Blynk.begin(auth, wifi, ssid, pass);
  timer.setInterval(1000L, checkPIRandFeed);

}

void loop()
{
  Blynk.run();
  timer.run();
}

//Melodi çalıp yemek verileceğini haber veriyoruz, ardından da motoru döndürüp uygulama konsoluna bilgi yolluyoruz.
void rotateServo(String type){
    PiezoSpeaker.playMelody(comeLength, comeMelody, comeNoteDurations);
    delay(500);
    servoSM.attach(SERVOSM_PIN_SIG);
    delay(300);
    servoSM.write(150);
    delay(1000);
    servoSM.write(-150);
    delay(1000);
    servoSM.detach();
    write(String(day()) + "/" + String(month()) + "/" + String(year()) + " - " + "[" + type + "]" + "Evcil hayvan su zamanda beslendi: " + hourToString() + ":" + minToString());
}

//Saati okunabilir bir String'e çeviren fonksiyon.
String hourToString(){
  return hour() < 10 ? "0" + String(hour()) : String(hour());
}

//Dakikayı okunabilir bir String'e çeviren fonksiyon.
String minToString(){
  return minute() < 10 ? "0" + String(minute()) : String(minute());
}

//Terminale mesaj yolla.
void write(String message){
  terminal.println(message);
  terminal.flush();
}
