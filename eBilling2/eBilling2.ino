#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define BLYNK_PRINT Serial

// Выбираем модем:
#define TINY_GSM_MODEM_SIM900
#include <TinyGsmClient.h>
#include <BlynkSimpleSIM800.h>

SoftwareSerial GSM(2, 3); // RX, TX
#define pinBOOT 9  

// или Софтварный Software Serial on Uno, Nano
#include <SoftwareSerial.h>
//SoftwareSerial SerialAT(2, 3); // RX, TX

TinyGsm modem(GSM);

LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the LCD I2C address
const int currentSensor = A0; // voltage pin
const int voltageSensor = A1; // voltage pin , not use voltagee sensor :(
int mVperAmp = 100; // use 100 for 20A Module and 66 for 30A Module
double sumKWH = 0.00000;
float WH = 0;//energy consumption in watt hour
float KWH = 0;
double sumRupees = 0.00000;//Total energy consumption in rupees
float rupees = 0;//energy consumption in rupees
double Voltage = 0;//AC supply peak voltage
double vrms = 0;//AC supply rms voltage
double current = 0;//load peak current
double irms = 0;//load rms current
double power = 0;//
#define RELAY1 4

// Ваш токен
char auth[] = "5d76227f0ccf40e29225a1d4a491bf8e";

// Данные GPRS подключения
// оставить пустым если нет Логина и Пароля

char apn[]  = "ppwap";
char user[] = "";
char pass[] = "";

WidgetLCD lcdd(V1);

////////////////////// GSM ///////////////////////

enum _parseState {
  PS_DETECT_MSG_TYPE,

  PS_IGNORING_COMMAND_ECHO,

  PS_HTTPACTION_TYPE,
  PS_HTTPACTION_RESULT,
  PS_HTTPACTION_LENGTH,

  PS_HTTPREAD_LENGTH,
  PS_HTTPREAD_CONTENT
};

enum _actionState {
  AS_IDLE,
  AS_WAITING_FOR_RESPONSE
};

byte actionState = AS_IDLE;
unsigned long lastActionTime = 0;

byte parseState = PS_DETECT_MSG_TYPE;
char buffer[80];
byte pos = 0;

int contentLength = 0;

void resetBuffer() {
  memset(buffer, 0, sizeof(buffer));
  pos = 0;
}

void sendGSM(const char* msg, int waitMs = 500) {
  GSM.println(msg);
  while (GSM.available()) {
    parseATText(GSM.read());
  }
  delay(waitMs);
}


//////////////////////////////////////////////////

void setup() {
   delay(3000);
// Следующие три строки нужны для запуска модема
// Равносильно нажатию кнопки Power на шилде
    digitalWrite(pinBOOT, HIGH);
    delay(1000);
    digitalWrite(pinBOOT, LOW);

  // Debug console
  Serial.begin(9600);

  delay(10);
  
  ////////////
  GSM.begin(9600);
  Serial.begin(9600);

//  sendGSM("AT+SAPBR=3,1,\"APN\",\"dialogbb\"");
//  sendGSM("AT+SAPBR=1,1", 3000);
//  sendGSM("AT+HTTPINIT");
//  sendGSM("AT+HTTPPARA=\"CID\",1");
//  sendGSM("AT+HTTPPARA=\"URL\",\"http://www.iforce2d.net/test.php\"");
  //sendGSM("AT+HTTPPARA=\"URL\",\"http://www.cebebilling.000webhostapp.com/add.php?acid=AC00001&unit=22\"");
  
  ////////////

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  modem.restart();

  // Unlock your SIM card with a PIN
  //modem.simUnlock("1234");

  Blynk.begin(auth, modem, apn, user, pass);
  
  //delay(3000); 
  // set up the LCD's number of columns and rows:
  lcd.init(); //initialize the lcd
  lcd.backlight(); //open the backlight 
  welcome();
  pinMode(RELAY1, OUTPUT);
}

void loop() {
  Blynk.run();
  if (Serial.available())
  {
    int state = Serial.parseInt();
    if (state == 0)
    {
      digitalWrite(RELAY1, 1);          // Turns Off Relays 1
      Serial.println("Light OFF");
    }
    if (state == 1)
    {
      digitalWrite(RELAY1, 0);         // Turns Relay On
      Serial.println("Light ON");
    }
  }
  energyCalculations();
  
  //Serial.println("SubmitHttpRequest - started" );
  //sendData();
  //Serial.println("SubmitHttpRequest - finished" );
  //delay(10000);

  gsm();
}

void energyCalculations() {

  // getting voltage from Input PIN
  //Voltage = getVPP(0);
  vrms = 230; //find total voltage
  // getting current from Input PIN
  current = getVPP(1);
  irms = (current / 2.0) * 0.707 * 1000 / mVperAmp;
  power = (vrms * irms * 0.9297);
  if (power < 30) {
    power = 0;
    irms = 0;
  }

  //WH = (power / 3600);
  WH = (power / 60);
  KWH = (WH / 1000);
  sumKWH = sumKWH + KWH;
  rupees = getReading();
  sumRupees = sumRupees + rupees ;

  lcd.setCursor(1, 0);
  lcd.print(vrms);
  lcd.print("v  ");
  lcd.print(irms);
  lcd.print("A");
  lcd.setCursor(1, 1);
  lcd.print(power);
  lcd.print("w  ");
  lcd.print(WH);
  lcd.print("Wh ");
  lcd.setCursor(1, 2);
  lcd.print("Total ");
  lcd.print(sumKWH);
  lcd.print("kWh");
  lcd.setCursor(1, 3);
  lcd.print("Total Rs.");
  lcd.print(sumRupees);

  

     lcdd.clear(); //Use it to clear the LCD Widget
  lcdd.print(4, 0, WH); // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
  lcdd.print(4, 1, "World");
//  lcdd.print("Total Rs.");
//  lcdd.print(sumRupees);

}

float getVPP(int pinValue)
{
  // pinValue = 0 means it is Voltage Input , pinValue = 1 means it is Current Input
  float result;

  int readValue;             // value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here

  uint32_t start_time = millis();
  while ((millis() - start_time) < 1000) //sample for 1 Sec
  {
    if (pinValue == 0)
    {
      // reading Voltage Input PIN
      readValue = analogRead(voltageSensor);
    }
    else if (pinValue == 1)
    {
      // reading Current Input PIN
      readValue = analogRead(currentSensor);
    }

    // see if you have a new maxValue
    if (readValue > maxValue)
    {
      /*record the maximum sensor value*/
      maxValue = readValue;
    }
    if (readValue < minValue)
    {
      /*record the maximum sensor value*/
      minValue = readValue;
    }
  }

  // Subtract min from max
  result = ((maxValue - minValue) * 5.0) / 1024.0;

  return result;
}


float getReading()
{

  float solution;

  if (sumKWH <= 30)
    solution = (KWH * 2.50);
  if (( sumKWH > 31 ) && ( sumKWH <= 60 ))
    solution = ( KWH * 4.85 );
  if (( sumKWH > 61 ) && (sumKWH <= 90))
    solution = (KWH * 10.00);
  if (( sumKWH > 91 ) && (sumKWH <= 120))
    solution = (KWH * 27.75);
  if (( sumKWH > 121 ) && (sumKWH <= 180))
    solution = (KWH * 32.00);
  if (sumKWH > 180)
    solution = (KWH * 45.00);

  return solution;

}

void welcome() {

  lcd.setCursor(5, 0);
  lcd.print("CEYLON");
  lcd.setCursor(3, 1);
  lcd.print("ELECTRICITY");
  lcd.setCursor(6, 2);
  lcd.print("BOARD");
  lcd.setCursor(1, 3);
  lcd.print("eBILLING SYSTEM");
  delay(5000);
  lcd.clear();

  lcd.setCursor(6, 0);
  lcd.print("IIT");
  lcd.setCursor(4, 1);
  lcd.print("SOFTWARE");
  lcd.setCursor(3, 2);
  lcd.print("ENGINEERING");
  lcd.setCursor(2, 3);
  lcd.print("GROUP PROJECT");
  delay(3000);
  lcd.clear();

  lcd.setCursor(5, 1);
  lcd.print("GROUP");
  lcd.setCursor(7, 2);
  lcd.print("8");
  delay(2000);

  lcd.home();

}


/////////////////////////////

void gsm(){
  unsigned long now = millis();

  if ( actionState == AS_IDLE ) {
    if ( now > lastActionTime + 5000 ) {
      sendGSM("AT+HTTPACTION=0");
      lastActionTime = now;
      actionState = AS_WAITING_FOR_RESPONSE;
    }
  }

  while (GSM.available()) {
    lastActionTime = now;
    parseATText(GSM.read());
  }
}

void parseATText(byte b) {

  buffer[pos++] = b;

  if ( pos >= sizeof(buffer) )
    resetBuffer(); // just to be safe

  /*
    // Detailed debugging
    Serial.println();
    Serial.print("state = ");
    Serial.println(state);
    Serial.print("b = ");
    Serial.println(b);
    Serial.print("pos = ");
    Serial.println(pos);
    Serial.print("buffer = ");
    Serial.println(buffer);*/

  switch (parseState) {
    case PS_DETECT_MSG_TYPE:
      {
        if ( b == '\n' )
          resetBuffer();
        else {
          if ( pos == 3 && strcmp(buffer, "AT+") == 0 ) {
            parseState = PS_IGNORING_COMMAND_ECHO;
          }
          else if ( b == ':' ) {
            //Serial.print("Checking message type: ");
            //Serial.println(buffer);

            if ( strcmp(buffer, "+HTTPACTION:") == 0 ) {
              Serial.println("Received HTTPACTION");
              parseState = PS_HTTPACTION_TYPE;
            }
            else if ( strcmp(buffer, "+HTTPREAD:") == 0 ) {
              Serial.println("Received HTTPREAD");
              parseState = PS_HTTPREAD_LENGTH;
            }
            resetBuffer();
          }
        }
      }
      break;

    case PS_IGNORING_COMMAND_ECHO:
      {
        if ( b == '\n' ) {
          Serial.print("Ignoring echo: ");
          Serial.println(buffer);
          parseState = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPACTION_TYPE:
      {
        if ( b == ',' ) {
          Serial.print("HTTPACTION type is ");
          Serial.println(buffer);
          parseState = PS_HTTPACTION_RESULT;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPACTION_RESULT:
      {
        if ( b == ',' ) {
          Serial.print("HTTPACTION result is ");
          Serial.println(buffer);
          parseState = PS_HTTPACTION_LENGTH;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPACTION_LENGTH:
      {
        if ( b == '\n' ) {
          Serial.print("HTTPACTION length is ");
          Serial.println(buffer);

          // now request content
          GSM.print("AT+HTTPREAD=0,");
          GSM.println(buffer);

          parseState = PS_DETECT_MSG_TYPE;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPREAD_LENGTH:
      {
        if ( b == '\n' ) {
          contentLength = atoi(buffer);
          Serial.print("HTTPREAD length is ");
          Serial.println(contentLength);

          Serial.print("HTTPREAD content: ");

          parseState = PS_HTTPREAD_CONTENT;
          resetBuffer();
        }
      }
      break;

    case PS_HTTPREAD_CONTENT:
      {
        // for this demo I'm just showing the content bytes in the serial monitor
        Serial.write(b);

        contentLength--;

        if ( contentLength <= 0 ) {

          // all content bytes have now been read

          parseState = PS_DETECT_MSG_TYPE;
          resetBuffer();

          Serial.print("\n\n\n");

          actionState = AS_IDLE;
        }
      }
      break;
  }
}


////////////////////////////


// Used to send Total Energy Consumption Billing to Customer
void sendBilling()
{
  GSM.println("AT+CLIP=1\r");
  GSM.println("AT+CMGF=1");    // Setting the GSM Module in Text mode
  delay(1000);  
  GSM.println("AT+CMGS=\"0772088783\"\r"); // Sending Energy Consumption to Customer's Mobile Number
  delay(1000);
  
    GSM.print("Dear Customer, Your Energy Consumption is :");
    GSM.print(sumKWH);
    GSM.print(" and Total Billing is Rs. ");
    GSM.print(sumRupees);
  
  delay(100);
  GSM.println((char)26); // ASCII code of CTRL+Z
  delay(100);
}



////////////////////////////
