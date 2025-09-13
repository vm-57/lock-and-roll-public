// Defining the board
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#define SerialAT Serial1
#define SerialMon Serial

#include <TinyGsmClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_PN532.h>
#include <ESP_SSLClient.h>
#include <Ticker.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Board Constants
#define LED_PIN 12
#define ALARM_PIN 26
#define ALARM_DURATION 1000
#define PWR_PIN 4
#define uS_TO_S_FACTOR 1000000ULL  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP 60           // Time ESP32 will go to sleep (in seconds)
#define UART_BAUD 115200
#define PIN_DTR 25
#define PIN_TX 27
#define PIN_RX 26
#define PWR_PIN 4
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13
#define LED_PIN 12
#define I2C_SDA 21
#define I2C_SCL 22


// Server Constants
#define SERVER_IP 0
#define SERVER_PORT 443  // 443 HTTPS and 80 HTTP
#define APN "hologram"
#define DEVICE_ID 3236926  // Would be replaced per device

// Communication
#define PIN_TX 27
#define PIN_RX 26

TinyGsm modem(SerialAT);
TinyGsmClient basic_client(modem);
ESP_SSLClient ssl_client;

#define SPEED 2000
#define FULL_ROTATION 200

// Server details
const char server[] = "https://lock-and-roll-sms.azurewebsites.net";
const int port = 443;
const char apn[] = "hologram";
const char gprsUser[] = "";
const char gprsPass[] = "";


// LIS3DH setup
Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// PN532 NFC setup
#define PN532_SS 5
Adafruit_PN532 nfc(PN532_SS);

// Stepper motor setup
#define DIR_PIN 33
#define STEP_PIN 32
#define STEP_ENABLE 25

// GPS Setup
float lat = -1;
float lon = -1;
bool gpsFlag = true;


bool _isLocked = false;
int numNFCFail = 0;

String A2PComply(String message_str) {
  Serial.println(message_str);
  String A2P = "[Lock and Roll]: " + message_str + " Reply STOP to cancel.";
  Serial.println(A2P);
  return A2P;
}

void enableGPS(void) {
  modem.sendAT("+CGPIO=0,48,1,1");
  if (modem.waitResponse(10000L) != 1) {
    DBG("Set GPS Power HIGH Failed");
  }
  modem.enableGPS();
}

void flashLED(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
}


void checkGyroscope() {
  sensors_event_t event;
  lis.getEvent(&event);

  // Print the values to the serial monitor
  Serial.print("X: ");
  Serial.print(event.acceleration.x);
  Serial.print(" ");
  Serial.print("Y: ");
  Serial.print(event.acceleration.y);
  Serial.print(" ");
  Serial.print("Z: ");
  Serial.println(event.acceleration.z);

  if (abs(event.acceleration.x) > 15 || abs(event.acceleration.y) > 15 || abs(event.acceleration.z) > 15) {
    Serial.println("ALARM!!!");
    flashLED(3);
    soundAlarm();
  }
}

void checkNFC() {
  uint8_t success;
  uint8_t uid[7];
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100);

  if (success) {
    Serial.print("NFC UID: ");
    /*
        numNFCFail += 1;
        if (numNFCFail % 5 == 0) {
            soundAlarm();
    } 
    PUT THIS FOR WHEN THE NFC ID ISN'T CORRECT
    */


    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    numNFCFail = 0;
    if (!lock()) unlock();
  }
}

bool lock() {
  if (_isLocked == false) {
    digitalWrite(STEP_ENABLE, LOW);
    _isLocked = true;
    digitalWrite(DIR_PIN, LOW);
    Serial.println("Locking...");
    for (int i = 0; i < FULL_ROTATION; i++) {
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(SPEED);
      digitalWrite(STEP_PIN, LOW);
      delayMicroseconds(SPEED);
    }
    //digitalWrite(STEP_ENABLE, HIGH);
    return true;
  }
  return false;
}

bool unlock() {
  if (_isLocked == true) {
    digitalWrite(STEP_ENABLE, LOW);
    _isLocked = false;
    digitalWrite(DIR_PIN, HIGH);
    Serial.println("Unlocking...");
    for (int i = 0; i < FULL_ROTATION; i++) {
      digitalWrite(STEP_PIN, HIGH);
      delayMicroseconds(SPEED);
      digitalWrite(STEP_PIN, LOW);
      delayMicroseconds(SPEED);
    }
    digitalWrite(STEP_ENABLE, HIGH);
    return true;
  }
  return false;
}

void soundAlarm() {
  String payload = "{\"deviceid\":\"" + String(DEVICE_ID) + "\", \"location\":\"" + getGPSInfo(0) + "\"}";
  //digitalWrite(ALARM_PIN, HIGH);
  //delay(ALARM_DURATION);
  //digitalWrite(ALARM_PIN, LOW);
  sendHTTP(payload, "/alarm");
  //for (int i=0; i < 5; i++) {
  //  digitalWrite(ALARM_PIN, HIGH);
  //  delay(ALARM_DURATION);
  //  digitalWrite(ALARM_PIN, LOW);
  //}
}

void connectToNetwork() {
  SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);

  Serial.println("\nWait...");

  delay(1000);

  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  // Restart takes quite some time
  // To skip it, call init() instead of restart()
  Serial.println("Initializing modem...");
  if (!modem.init()) {
    Serial.println("modem init fail. restarting instead");
    modem.restart();
  }

  String name = modem.getModemName();
  delay(500);
  Serial.println("Modem Name: " + name);

  String modemInfo = modem.getModemInfo();
  delay(500);
  Serial.println("Modem Info: " + modemInfo);

  modem.sendAT("+CFUN=0 ");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" +CFUN=0  false ");
  }
  delay(200);

  String res;
  res = modem.setNetworkMode(2);
  if (res != "1") {
    DBG("setNetworkMode  false ");
    return;
  }
  delay(200);

  res = modem.setPreferredMode(1);
  if (res != "1") {
    DBG("setPreferredMode  false ");
    return;
  }
  delay(200);
  delay(200);

  modem.sendAT("+CFUN=1 ");
  if (modem.waitResponse(10000L) != 1) {
    DBG(" +CFUN=1  false ");
  }
  delay(200);

  SerialAT.println("AT+CGDCONT?");
  delay(500);

  Serial.println("\n\n\nWaiting for network...");
  if (!modem.waitForNetwork()) {
    delay(10000);
    return;
  }

  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  }

  getGPSInfo(1);  // turning the GPS on

  Serial.println("Connecting to: " + String(apn));
  if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
    delay(10000);
    return;
  }

  Serial.print("GPRS status: ");
  if (modem.isGprsConnected()) {
    Serial.println("connected");
  } else {
    Serial.println("not connected");
  }

  String ccid = modem.getSimCCID();
  Serial.println("CCID: " + ccid);

  String imei = modem.getIMEI();
  Serial.println("IMEI: " + imei);

  String cop = modem.getOperator();
  Serial.println("Operator: " + cop);

  IPAddress local = modem.localIP();
  Serial.println("Local IP: " + String(local));

  int csq = modem.getSignalQuality();
  Serial.println("Signal quality: " + String(csq));

  SerialAT.println("AT+CPSI?");  // Get connection type and band
  delay(500);
  if (SerialAT.available()) {
    String r = SerialAT.readString();
    Serial.println(r);
  }

  modem.sendAT("+CMGF=1");  // Set the message format to text mode
  modem.waitResponse(1000L);
  modem.sendAT("+CNMI=2,2,0,0,0\r");
  delay(100);
}

void sendHTTP(String payload, String destination) {
  if (modem.isGprsConnected()) {
    Serial.println("GPRS connected");
  } else {
    SerialMon.println("Connecting to GPRS...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
      delay(10000);
    }
  }
  if (modem.isNetworkConnected()) {
    Serial.println("Network connected");
  } else {
    Serial.println("\n\n\nWaiting for network...");
    if (!modem.waitForNetwork()) {
      delay(10000);
      return;
    }
  }

  // ignore server ssl certificate verification
  ssl_client.setInsecure();

  // Set the receive and transmit buffers size in bytes for memory allocation (512 to 16384).
  ssl_client.setBufferSizes(1024 /* rx */, 512 /* tx */);

  ssl_client.setDebugLevel(1);

  // Assign the basic client
  ssl_client.setClient(&basic_client);

  Serial.println("---------------------------------");
  Serial.print("Connecting to server...\n");

  if (ssl_client.connect("lock-and-roll-sms.azurewebsites.net", 443)) {
    Serial.println("ok");
    Serial.println("Send POST request...");
    ssl_client.print("POST " + destination + " HTTP/1.1\r\n");
    ssl_client.print("Host: lock-and-roll-sms.azurewebsites.net\r\n");
    ssl_client.print("Connection: close\r\n");
    ssl_client.print("Content-Type: application/json\r\n");
    ssl_client.print("Content-Length: ");
    ssl_client.print(payload.length());
    ssl_client.print("\r\n\r\n");
    ssl_client.print(payload);
  } else {
    Serial.println("failed\n");
  }
  ssl_client.stop();
  Serial.println();
}

void Read_SMS() {

  modem.sendAT("+CMGL=\"ALL\"");
  modem.waitResponse(5000L);

  String data = SerialAT.readString();
  Serial.println(data);

  if (data.indexOf("+CMT:") > 0)

  {
    parseSMS(data);
  }

  modem.sendAT("+cmgd=,4");
  modem.waitResponse(1000L);
}

void parseSMS(String data) {

  data.replace(",,", ",");
  data.replace("\r", ",");
  data.replace("\"", "");
  Serial.println(data);

  String for_mess = data;

  char delimiter = ',';
  String date_str = parse_SMS_by_delim(for_mess, delimiter, 2);
  String time_str = parse_SMS_by_delim(for_mess, delimiter, 3);
  String number_str = parse_SMS_by_delim(for_mess, delimiter, 4);
  String message_str = parse_SMS_by_delim(for_mess, delimiter, 5);

  Serial.println("************************");
  Serial.println(date_str);
  Serial.println(time_str);
  Serial.println(number_str);
  Serial.println(message_str);
  Serial.println("************************");

  // if the SMS message corresponds to certain actions, we want to act upon them
  handleSMS(message_str, number_str);
}

String getGPSInfo(bool init) {
  enableGPS();
  int i = 0;
  int GPSLimit = 100;  // how many times it'll query for GPS info. Change from 1 if it's 1.
  while (i < GPSLimit && gpsFlag == true) {
    if (modem.getGPS(&lat, &lon)) {
      Serial.printf("GPS Info Obtained: {%d}\n", i);
      Serial.printf("Lat: {%f}, Lon: {%f}\n", lat, lon);
      String latitude = String(lat, 6);
      String longitude = String(lon, 6);
      return "https://maps.google.com/?q=" + latitude + "," + longitude;
    } else if (init == 0) {
      Serial.print("Unable to determine GPS Information");
      break;
    }
    Serial.printf("Trying to Determine GPS Information: {%d}\n", i);
    delay(100);
    i += 1;
  }
  if (i >= 99) {
    Serial.print("Could not determine GPS Information.\n");
    gpsFlag == false;
  }
  return "Unable to obtain GPS data";
}

String parse_SMS_by_delim(String sms, char delimiter, int targetIndex) {
  // Tokenize the SMS content using the specified delimiter
  int delimiterIndex = sms.indexOf(delimiter);
  int currentIndex = 0;

  while (delimiterIndex != -1) {
    if (currentIndex == targetIndex) {

      String targetToken = sms.substring(0, delimiterIndex);
      targetToken.replace("\"", "");
      targetToken.replace("\r", "");
      targetToken.replace("\n", "");
      return targetToken;
    }

    // Move to the next token
    sms = sms.substring(delimiterIndex + 1);
    delimiterIndex = sms.indexOf(delimiter);
    currentIndex++;
  }

  // If the target token is not found, return an empty string
  return "";
}

void handleSMS(String message_str, String number_str) {
  message_str.replace(" ", "");
  message_str.toUpperCase();
  if (message_str == "" || message_str == "STOP" || message_str == "CANCEL" || message_str == "END" || message_str == "QUIT" || message_str == "STOPALL" || message_str == "UNSUBSCRIBE" || message_str == "START" || message_str == "INFO" || message_str == "HELP") {
    return;  // handles empty strings and opt-outs/ins...
  }
  if (message_str == "ON") {
    Serial.println("Turning LED Pin ON");
    digitalWrite(LED_PIN, LOW);
    String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("I am responding to an ON command!") + "\"}";
    sendHTTP(payload, "/outbound");
  } else if (message_str == "OFF") {
    Serial.println("Turning LED Pin OFF");
    digitalWrite(LED_PIN, HIGH);
    String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("I am responding to an OFF command!") + "\"}";
    sendHTTP(payload, "/outbound");
  } else if (message_str == "LOCATION") {
    Serial.println("Getting location information");
    String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("Your bike's current location: " + getGPSInfo(1)) + "\"}";
    sendHTTP(payload, "/outbound");
  } else if (message_str == "LOCK") {
    Serial.println("Locking...");
    if (lock() == true) {
      String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("Your bike is now LOCKED. To unlock, reply UNLOCK. Msg&Data rates may apply.") + "\"}";
      Serial.println(payload);
      sendHTTP(payload, "/outbound");
    } else {
      String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("ALERT! Your bike is already locked!") + "\"}";
      sendHTTP(payload, "/outbound");
    }
  } else if (message_str == "UNLOCK") {
    Serial.println("Unlocking...");
    if (unlock() == true) {
      String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("Your bike is now UNLOCKED. To lock, reply LOCK. Msg&Data rates may apply.") + "\"}";
      sendHTTP(payload, "/outbound");
    } else {
      String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("ALERT! Your bike is already unlocked!") + "\"}";
      sendHTTP(payload, "/outbound");
    }
  } else {
    Serial.printf("Command passed in: %s\n", message_str);
    String payload = "{\"recipient\":\"" + number_str + "\", \"message\":\"" + A2PComply("ALERT! Invalid command passed in. Type HELP to see a list of all valid commands!") + "\"}";
    sendHTTP(payload, "/outbound");
  }
}

void setup() {
  Serial.begin(115200);
  SerialAT.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);

  // Power on SIM7000G
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(1000);
  digitalWrite(PWR_PIN, LOW);

  // LED and alarm setup
  pinMode(LED_PIN, OUTPUT);
  pinMode(ALARM_PIN, OUTPUT);

  digitalWrite(ALARM_PIN, LOW);

  // LIS3DH initialization
  digitalWrite(0, HIGH);   // 3v input pin for LIS3DH
  if (!lis.begin(0x18)) {  // 0x18 is the I2C address
    Serial.println("Could not start LIS3DH");
  }
  lis.setRange(LIS3DH_RANGE_2_G);

  // NFC initialization
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Failed to find NFC reader");
  } else {
    Serial.println("Successfully registered NFC reader");
  }

  // Stepper initialization
  pinMode(DIR_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(STEP_ENABLE, OUTPUT);
  digitalWrite(STEP_ENABLE, HIGH);  // Disable battery drain during setup

  // Network Stuff
  connectToNetwork();
  flashLED(3);
}

void loop() {
  if (_isLocked) checkGyroscope();
  checkNFC();
  Read_SMS();
  delay(100);
}