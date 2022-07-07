/*
  Made by Jonathan Halim

  Reference:
*/

//Include Libraries
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#elif defined(ESP8266)
  #include <ESP8266WiFi.h>
#endif
#include <Firebase_ESP_Client.h>
#include <SPI.h>
#include <MFRC522.h>

//Provide token generation process info.
#include "addons/TokenHelper.h"
//Provide RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

//Insert network credentials
#define WIFI_SSID "Wifi_SSID" //Change Wifi_SSID to tour own Wifi Username
#define WIFI_PASSWORD "Wifi_Password" //Change Wifi_Password to tour own Wifi Password

//Insert Firebase project API Key
#define API_KEY "Firebase_APIKEY" //Change Firebase_APIKEY to your Firebase Project API Key

// Insert RTDB URL
#define DATABASE_URL "Firebase_URL" //Change Firebase_URL to your Firebase Realtime Database URL

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

//Define RFID and Relay GPIO Pin
#define SS_PIN    21  // ESP32 pin GPIO
#define RST_PIN   22 // ESP32 pin GPIO
#define RELAY_PIN 5 // ESP32 pin GPIO connects to relay

MFRC522 rfid(SS_PIN, RST_PIN);

String LockValue;
bool signupOK = false;

byte keyTagUID[4] = {0x00, 0x00, 0x00, 0x00}; //Input registered RFID UID tag here

void setup() {
  Serial.begin(115200);
  while (!Serial);

  SPI.begin(); //Initialize SPI bus
  rfid.PCD_Init(); //Initialize MFRC522 RFID
  pinMode(RELAY_PIN, OUTPUT); //Initialize pin as an output.
  digitalWrite(RELAY_PIN, HIGH); //Lock the door. Use HIGH if relay active low, use LOW if relay active high

  //Connecting to Wi-Fi Protocol
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  //Assign the registered API KEY
  config.api_key = API_KEY;

  //Assign the registered RTDB URL
  config.database_url = DATABASE_URL;

  //Sign up to Firebase (For Anonymous)
  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("ok");
    signupOK = true;
  }
  else {
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  //Assign the callback function for the long running token generation task 
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
}

void loop() {
  //Get data from Firebase
  Firebase.RTDB.getString(&fbdo, "esp1"); //Get String data from "esp1" in firebase realtime database
  LockValue = fbdo.stringData();
  Serial.println(LockValue);
  if (LockValue == "0"){
    digitalWrite(RELAY_PIN, HIGH); //If ESP1 Lock Value is 0, then door is locked
  }
  if (LockValue == "1"){
    digitalWrite(RELAY_PIN, LOW); //If ESP1 Lock Value is 1, then door is unlocked
  }

  //RFID Card is Present
  if (rfid.PICC_IsNewCardPresent()) { //New tag is available
    if (rfid.PICC_ReadCardSerial()) { //NUID has been readed
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);

      if (rfid.uid.uidByte[0] == keyTagUID[0] &&
          rfid.uid.uidByte[1] == keyTagUID[1] &&
          rfid.uid.uidByte[2] == keyTagUID[2] &&
          rfid.uid.uidByte[3] == keyTagUID[3] ) {
          Serial.println("Access is granted");
          Firebase.RTDB.setString(&fbdo, "esp1", 1); 
          digitalWrite(RELAY_PIN, LOW);  //Unlock the door for 3 seconds
          delay(3000);
          digitalWrite(RELAY_PIN, HIGH); //Lock the door
          Firebase.RTDB.setString(&fbdo, "esp1", 0);
      }
      else
      {
        Serial.print("Access denied, UID:");
        for (int i = 0; i < rfid.uid.size; i++) {
          Serial.print(rfid.uid.uidByte[i] < 0x10 ? " 0" : " ");
          Serial.print(rfid.uid.uidByte[i], HEX);
        }
        Serial.println();
      }
    }
  }
}
