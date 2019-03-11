#include <SoftwareSerial.h>
SoftwareSerial GSM(2, 3); // RX, TX
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4); // Set the LCD I2C address
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
  while(GSM.available()) {
    parseATText(GSM.read());
  }
  delay(waitMs);
}

void setup()
{
  GSM.begin(9600);
  Serial.begin(9600);
  
  sendGSM("AT+SAPBR=3,1,\"APN\",\"ppwap\"");  
  sendGSM("AT+SAPBR=1,1",3000);
  sendGSM("AT+HTTPINIT");  
  sendGSM("AT+HTTPPARA=\"CID\",1");
 // sendGSM("AT+HTTPPARA=\"URL\",\"http://www.iforce2d.net/test.php\"");
   sendGSM("AT+HTTPPARA=\"URL\",\"http://www.ceb-ebilling.000webhostapp.com/ceb/add.php?acid='AC00010'&unit=");
sendGSM(char(sumKWH));
sendGSM("\"");

// set up the LCD's number of columns and rows:
    lcd.init();                      // initialize the lcd 
  // Print a message to the LCD.
  lcd.backlight();
  welcome();
  pinMode(RELAY1, OUTPUT);
}

void loop()
{ 
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
  //////
  unsigned long now = millis();
    
  if ( actionState == AS_IDLE ) {
    if ( now > lastActionTime + 5000 ) {
      sendGSM("AT+HTTPACTION=0");
      lastActionTime = now;
      actionState = AS_WAITING_FOR_RESPONSE;
    }
  }
  
  while(GSM.available()) {
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

//////////

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
  lcd.print("v     ");
  lcd.print(irms);
  lcd.print("A");
  lcd.setCursor(1, 1);
  lcd.print(power);
  lcd.print("w     ");
  lcd.print(WH);
  lcd.print("Wh ");
  lcd.setCursor(1, 2);
  lcd.print("Usage     ");
  lcd.print(sumKWH);
  lcd.print("kWh");
  lcd.setCursor(1, 3);
  lcd.print("Billing  Rs.");
  lcd.print(sumRupees);

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

  lcd.setCursor(7, 0);
  lcd.print("CEYLON");
  lcd.setCursor(5, 1);
  lcd.print("ELECTRICITY");
  lcd.setCursor(8, 2);
  lcd.print("BOARD");
  lcd.setCursor(3, 3);
  lcd.print("eBILLING SYSTEM");
  delay(6000);
  lcd.clear();

//  lcd.setCursor(8, 0);
//  lcd.print("IIT");
//  lcd.setCursor(6, 1);
//  lcd.print("SOFTWARE");
//  lcd.setCursor(5, 2);
//  lcd.print("ENGINEERING");
//  lcd.setCursor(4, 3);
//  lcd.print("GROUP PROJECT");
//  delay(4000);
  lcd.clear();

  lcd.home();

}

void powerUp()
{
  pinMode(9, OUTPUT); 
  digitalWrite(9,LOW);
  delay(1000);
  digitalWrite(9,HIGH);
  delay(2000);
  digitalWrite(9,LOW);
  delay(3000);
} 
