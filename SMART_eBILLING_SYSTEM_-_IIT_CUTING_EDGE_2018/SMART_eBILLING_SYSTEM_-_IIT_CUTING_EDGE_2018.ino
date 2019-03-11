/*****************************************************************************
 *
 * SMART eBILLING SYSTEM - IIT CUTING EDGE 2018 :P
 *
 *****************************************************************************
 *****************************************************************************/

#define BLYNK_PRINT Serial

// Выбираем модем:
#define TINY_GSM_MODEM_SIM900

#define pinBOOT 9  

//pinMode(9,OUTPUT);

 pinMode(9,OUTPUT);
 digitalWrite(9,HIGH);
 delay(1000);
 digitalWrite(9,LOW);


 // pinMode(10,OUTPUT);
 // digitalWrite(10,HIGH);
 // delay(1000);
 // digitalWrite(10,LOW);
 ////////////////////////////////////////////////////////////

 // The default GSM connection confirmation time is 60
 // If you want to override this value, uncomment and set this option:
 //
//#define BLYNK_HEARTBEAT 30

#include <TinyGsmClient.h>
#include <BlynkSimpleSIM800.h>

// Ваш токен
char auth[] = "5d76227f0ccf40e29225a1d4a491bf8e";

// Данные GPRS подключения
// оставить пустым если нет Логина и Пароля

char apn[]  = "ppwap";
char user[] = "";
char pass[] = "";


// Аппаратный Serial on Mega, Leonardo, Micro
//#define SerialAT Serial1

// Hardware Serial on Uno, Nano (but the serial of the monitor will be blocked)
// Since feet 0 and 1 are connected to the modem
//#define SerialAT Serial

// или Софтварный Software Serial on Uno, Nano
#include <SoftwareSerial.h>
SoftwareSerial SerialAT(2, 3); // RX, TX

TinyGsm modem(SerialAT);

void setup()
{

  delay(3000);
// Следующие три строки нужны для запуска модема
// Равносильно нажатию кнопки Power на шилде
    digitalWrite(pinBOOT, HIGH);
    delay(1000);
    digitalWrite(pinBOOT, LOW);

  // Debug console
  Serial.begin(9600);

  delay(10);

  // Set GSM module baud rate (вставьте свою скорость)
  SerialAT.begin(9600);
  delay(3000);


  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();

  // Unlock your SIM card with a PIN
  //modem.simUnlock("1234");

  Blynk.begin(auth, modem, apn, user, pass);
}

void loop()
{
  Blynk.run();
}
