#include <TinyGPS++.h>
#include <EEPROM.h>
#define RXD2 22
#define TXD2 23
HardwareSerial neogps(1);

TinyGPSPlus gps;

String PHONE = "";
String PASSWORD = "12345";
String smsStatus, senderNumber, receivedDate, msg;
boolean isReply = false;
boolean passwordState = false;
boolean SetLoc = false;
boolean alertOn = false;
double range, lat, lng;

void TrackDistance() {
  range = 6371 * acos(sin(lat) * sin(gps.location.lat()) + cos(lat) * cos(gps.location.lat()) * cos(gps.location.lng() - lng));
}

void SetLocation() {
  lat = gps.location.lat();
  lng = gps.location.lng();
  SetLoc = true;
}

void RemoveLocation(){
  SetLoc = false;
  alertOn = false;
  lat = 0;
  lng = 0;
}

void parseData(String buff) {
  delay(100);
  Serial.println(buff);
  unsigned int len, index;
  index = buff.indexOf("\r");
  buff.remove(0, index + 2);
  buff.trim();
  if (buff != "OK") {
    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();
    Serial.println(cmd);

    buff.remove(0, index + 2);

    if (cmd == "+CMTI") {
      index = buff.indexOf(",");
      String temp = buff.substring(index + 1, buff.length());
      temp = "AT+CMGR=" + temp + "\r";
      Serial2.println(temp);
    }else if (cmd == "+CMGR") {
      extractSms(buff);
      Serial.println("Sender Number: " + senderNumber);
      Serial.println("PHONE: " + PHONE);
      if (senderNumber == PHONE) {
        Serial.println("PHONE SAME");
        Serial.println(msg);
        if (msg == "get location") {
          sendLocation();
        } else if (msg == "set password") {
          sendSms("Input New Password:", PHONE);
          passwordState = true;
        } else if(passwordState == true){
          EEPROM.writeString(0, msg);
          EEPROM.commit();
          PASSWORD = msg;
          sendSms("Password Set!", PHONE);
        } else if(msg == "set location"){
          SetLocation();
          sendSms("Location Set!", PHONE);
        } else if(msg == "remove location"){
          RemoveLocation();
          sendSms("Location Removed!", PHONE);
        }
      } else if(passwordState == false){
        passwordState = true;
        sendSms("INPUT PASSWORD", senderNumber);
        Serial.println("Need Password");
      }else if(passwordState == true){
        if(msg == PASSWORD){
          PHONE = senderNumber;
          EEPROM.writeString(1,PHONE);
          EEPROM.commit();
          passwordState = false;
          sendSms("PHONE NUMBER SAVED, NOW YOU CAN ACCESS FREELY!", PHONE);
        }else{
          sendSms("WRONG PASSWORD, PLEASE TRY AGAIN", PHONE);
        }
      }
      Serial2.println("AT+CMGD=1,4");  //delete all saved SMS
      delay(1000);
      smsStatus = "";
      senderNumber = "";
      receivedDate = "";
      msg = "";
    }
  }else{

  }
}

void sendSms(String msg, String number){
  Serial2.print("AT+CMGF=1\r");
    delay(200);
    Serial2.print("AT+CMGS=\"" + number + "\"\r");
    delay(200);
    Serial2.print(msg);
    delay(500);
    Serial2.write(0x1A); 
    delay(100);
}

void extractSms(String buff) {
  unsigned int index;
  Serial.println(buff);

  index = buff.indexOf(",");
  smsStatus = buff.substring(1, index - 1);
  buff.remove(0, index + 2);

  senderNumber = buff.substring(0, 14);
  buff.remove(0, 19);

  receivedDate = buff.substring(0, 20);
  buff.remove(0, buff.indexOf("\r"));
  buff.trim();

  index = buff.indexOf("\n\r");
  buff = buff.substring(0, index);
  buff.trim();
  msg = buff;
  buff = "";
  msg.toLowerCase();
}
void sendLocation() {
  // Can take up to 60 seconds
  boolean newData = false;
  String msg;
  for (unsigned long start = millis(); millis() - start < 2000;) {
    while (neogps.available()) {
      if (gps.encode(neogps.read())) {
        newData = true;
      }
    }
  }
  if (newData) 
  {
    Serial.print("Latitude= ");
    Serial.print(gps.location.lat(), 6);
    Serial.print(" Longitude= ");
    Serial.println(gps.location.lng(), 6);
    newData = false;
    delay(300);
    msg = ("http://maps.google.com/maps?q=loc:"+String(gps.location.lat())+","+String(gps.location.lng()));
    delay(100);
    sendSms(msg, PHONE);
    delay(1000);
    Serial.println("GPS Location SMS Sent Successfully.");
    //*/
  }
}

void sendWarning(){
  sendSms("ALERT, The item is out of set location range!", PHONE);
  sendLocation();
  alertOn = true;
}

void smsToChar(){
  char msg;
  String sms;
  while(Serial2.available()){
    msg = Serial2.read();
    sms += String(msg);
    delay(10);
  }
  delay(100);
  parseData(sms);
}

void updateSerial() {
  while (Serial.available()) {
    Serial2.write(Serial.read());  //Forward what Serial received to Software Serial Port
    delay(10);
  }
  while (Serial2.available()) {
    // Serial.println("GSM MASUK");
    smsToChar();
  }
}

void setup() {
  delay(2000);

  Serial.begin(115200);
  Serial.println("esp32 serial initialize");

  Serial2.begin(9600);
  Serial.println("Sim800l serial initialize");

  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("GPS serial initialize");

  pinMode(18, OUTPUT); // BUZZER

  EEPROM.begin(128);
  if(EEPROM.read(0) != 255){
    PASSWORD = EEPROM.readString(0);
  }if(EEPROM.read(1) != 255){
    PHONE = EEPROM.readString(1);
  }

  smsStatus = "";
  senderNumber = "";
  receivedDate = "";
  msg = "";
  
  Serial2.println("AT+CMGF=1");  //SMS text mode
  delay(1000);
  Serial2.println("AT+CMGD=1,4");  //delete all saved SMS
  delay(1000);
}  //setup function ends

void loop() {
  updateSerial();
  // Untuk test GPS
  // if (gps.encode(neogps.read())) {
  //   Serial.print("Location: ");
  //   Serial.print(gps.location.lat());
  //   Serial.print(", ");
  //   Serial.println(gps.location.lng());
  // }
  if(SetLoc == true){
    if (gps.encode(neogps.read())) {
      TrackDistance();
      if(range * 10 >= 4){
        digitalWrite(18, HIGH);
        if(alertOn == false)sendWarning();
      }
    }
  }else digitalWrite(18, LOW);
} 
