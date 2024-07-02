#include <Adafruit_Fingerprint.h>
#include <Keypad.h>
#include <NewPing.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
String receivedMessage = "";

// const char* ssid = "iPhone - Eduard";
// const char* password = "idk12345";
const char* ssid = "WI-FI";
const char* password = "1234554321";
WiFiServer server(80);
const char* host = "192.168.1.128"; // IP address of the receiver ESP32
//const char* host = "172.20.10.7";
IPAddress local_IP("192.168.1.129"); // Set your desired static IP address
IPAddress gateway("192.168.1.1"); 
IPAddress subnet("255.255.255.0"); // Subnet mask for iPhone hotspot
IPAddress primaryDNS("8.8.8.8"); // Optional: Google DNS
IPAddress secondaryDNS("8.8.4.4"); // Optional: Google DNS
const byte ROW_NUM = 4;
const byte COLUMN_NUM = 3;
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'}
};
byte pin_rows[ROW_NUM] = {26, 16, 17, 33}; // Updated pin numbers for ESP32
byte pin_column[COLUMN_NUM] = {25, 27, 32}; // Updated pin numbers for ESP32
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);
unsigned long lastKeyPressTime = 0;
const unsigned long debounceDelay = 200; // Increased debounce delay
#define mySerial Serial2 // Use Serial2 for fingerprint sensor on ESP32
const unsigned long WiFiCheckInterval = 30000;
unsigned long lastWiFiCheckTime = 0;

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

const int RELAY_PIN = 4;
const int trigPin = 13; // HC-SR04 Trig pin
const int echoPin = 14; // HC-SR04 Echo pin
const int extled = 15;

NewPing sonar(trigPin, echoPin); // Define the HC-SR04 sensor

#define SS_PIN 5  // RFID SS pin
#define RST_PIN 0 // RFID RST pin
#define SCK_PIN 18  // RFID SCK pin
#define MOSI_PIN 23 // RFID MOSI pin
#define MISO_PIN 19 // RFID MISO pin

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

void setup() {
  Serial.begin(115200);
  
  for (int i = 0; i < ROW_NUM; i++) {
    pinMode(pin_rows[i], INPUT_PULLUP);
  }
  for (int i = 0; i < COLUMN_NUM; i++) {
    pinMode(pin_column[i], INPUT_PULLUP);
  }

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(extled, OUTPUT); // Initialize 27 pin as output for the LED
  
  // Initialize HC-SR04 sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Initialize fingerprint sensor
  mySerial.begin(57600, SERIAL_8N1, 22, 21);  // RX on GPIO 16, TX on GPIO 17
  finger.begin(57600);
  unsigned long start = millis();
  while (!finger.verifyPassword()) {
    if (millis() - start > 5000) {  // Wait for 5 seconds to find the sensor
      Serial.println(F("Did not find fingerprint sensor, restarting..."));
      ESP.restart();  // Reset the ESP32 board
    }
    delay(100);
  }
  Serial.println(F("Found fingerprint sensor!"));

  // Initialize RFID reader
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);
  rfid.PCD_Init();

  unsigned long lastDistanceCheckTime = 0;
  const unsigned long distanceCheckInterval = 1000; // Check distance every second

  bool fingerprintAuthenticating = false;
  bool rfidAuthenticating = false;
  bool passwordAuthenticating = false;

  // Connect to WiFi
  Serial.println();
  Serial.println("Connecting to WiFi...");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(ssid, password);

  int attempts = 0;
  const int maxAttempts = 5;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
    Serial.println("Server started");
  } else {
    Serial.println("Failed to connect to WiFi after 5 attempts");
    // Continue with the rest of your setup code even if not connected
  }
  do {

    WiFiClient client = server.available();
    if (client) {
      Serial.println("Client connected");

      // Read the request
      String request = client.readStringUntil('\r');
      Serial.println(request);
     
      receivedMessage = request;
      Serial.println(receivedMessage);

      // Send a response to the client
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println();
      client.println("<html><body><h1>Message received</h1></body></html>");
      client.stop(); // Close the connection

    }
    if (millis() - lastWiFiCheckTime >= WiFiCheckInterval) {
    lastWiFiCheckTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Restarting...");
      ESP.restart();
    }
  }
    if (receivedMessage.indexOf("unlocked") >= 0) {
      digitalWrite(extled, LOW); // Turn off exterior LED
      controlRelay();
      ESP.restart();
    }
    // Check distance every second
    if (millis() - lastDistanceCheckTime >= distanceCheckInterval) {
      lastDistanceCheckTime = millis(); // Update last check time

      // Read distance from HC-SR04 sensor
      unsigned int distance = sonar.ping_cm();
     
      // If distance is less than 30 cm, turn on LED
      if (distance < 30) {
        digitalWrite(extled, HIGH); // Turn on LED connected to 27
      } else {
        digitalWrite(extled, LOW); // Turn off LED connected to 27
      }
    }
    
    if (!fingerprintAuthenticating) {
      // If not already authenticating with fingerprint, attempt fingerprint authentication
      fingerprintAuthenticating = fingerprintAuthenticate();
    }

    if (!rfidAuthenticating) {
      // If not already authenticating with RFID, attempt RFID authentication
      rfidAuthenticating = rfidAuthenticate();
    }

    // If any authentication method is successful, break out of the loop
    if (fingerprintAuthenticating || rfidAuthenticating) {
      break;
    }

    if (!passwordAuthenticating) {
      // If not already authenticating with password, attempt password authentication
      passwordAuthenticating = passwordAuthenticate();
    }

  } while (true); // Loop indefinitely until authentication succeeds

  // Restart the ESP32 after successful authentication
  ESP.restart();
}

bool fingerprintAuthenticate() {
  Serial.println(F("\nAttempting fingerprint authentication..."));

  // Capture fingerprint image
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    Serial.println(F("\nFingerprint sensor error. Please try again."));
    return false;
  }

  // Convert fingerprint image to template
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) {
    Serial.println(F("\nFingerprint image conversion failed. Please try again."));
    return false;
  }

  // Search for fingerprint match
  p = finger.fingerSearch();
  if (p == FINGERPRINT_OK) {
    Serial.println(F("\nFingerprint authentication successful!"));
    
    sendMessage("   Dor unlocked\n\n    (Fingerprint)\r\n");

    digitalWrite(extled, LOW); // Turn off extled after successful authentication
    controlRelay();
    return true;
  } else {
    Serial.println(F("\nFingerprint authentication failed."));
    
    sendMessage("      Invalid \n\n     fingerprint\r\n");

    return false;
  }
}

bool passwordAuthenticate() {
  Serial.println(F("\nPlease enter password:"));
  String password = "";
  unsigned long startTime = millis();
  bool timedOut = false; // Flag to track if time limit is reached
  do {
    char key = keypad.getKey();
    if ((key >= '0' && key <= '9') || key == '*' || key == '#') {
      if (millis() - lastKeyPressTime > debounceDelay) {
        password += key;
        Serial.print(key); // Print actual key for each entered digit
        lastKeyPressTime = millis();
      }
    }
    if (millis() - startTime >= 5000) {
      timedOut = true;
    }
    delay(100); // Delay to avoid multiple key inputs
  } while (password.length() < 4 && !timedOut);

  bool success = password == "8888";

  if (success && !timedOut) { // Check if password is correct and not timed out
    Serial.println(F("\nPassword authentication successful!"));
    
    sendMessage("   Dor unlocked\n\n     (Password)\r\n");

    digitalWrite(extled, LOW); // Turn off extled after successful authentication
    controlRelay();
  } else {
    if (!timedOut) {
      Serial.println(F("\nPassword authentication failed."));
      
      sendMessage("      Invalid\n\n      Password\r\n");
    }
  }
  return success && !timedOut; // Return success only if password is correct and not timed out
}

bool rfidAuthenticate() {
  Serial.println(F("\nAttempting RFID authentication..."));
  unsigned long startTime = millis();
  while (millis() - startTime < 5000) {
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      Serial.print("UID:");
      String uid = "";
      for (byte i = 0; i < rfid.uid.size; i++) {
        uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
        uid += String(rfid.uid.uidByte[i], HEX);
      }
      Serial.println(uid);
      // Expected UID: D8 8D CA 9E
      if (rfid.uid.uidByte[0] == 0xD8 &&
          rfid.uid.uidByte[1] == 0x8D &&
          rfid.uid.uidByte[2] == 0xCA &&
          rfid.uid.uidByte[3] == 0x9E) {
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();
        Serial.println("RFID authentication successful!");

        sendMessage("   Dor unlocked\n\n       (RFID)\r\n");

        digitalWrite(extled, LOW); // Turn off extled after successful authentication
        controlRelay();
        
        return true;
      } else {
        Serial.println("RFID authentication failed! (Incorrect UID)");
        
        sendMessage("     Invalid\n\n        RFID\r\n");

        return false;
      }
    }
    delay(100); // Small delay to avoid overloading the RFID reader
  }
  Serial.println("RFID authentication failed! (Timeout)");
  return false;
}

void loop() {
  // Empty loop as main authentication is handled in setup
}

void controlRelay() {
  Serial.println("Turning relay ON for 30 seconds.");
  digitalWrite(RELAY_PIN, LOW); // Turn on the relay
  delay(30000);
  digitalWrite(RELAY_PIN, HIGH); // Turn on the relay
}

void sendMessage(String message) {
  bool messageSent = false;
  int retryCount = 0;
  const int maxRetries = 5;

  while (!messageSent && retryCount < maxRetries) {
    WiFiClient client;
    if (client.connect(host, 80)) {
      // Send the message
      client.print(message);
      client.print("Host: ");
      client.print(host);
      client.print("\r\n");
      client.print("Connection: close\r\n\r\n");
      delay(10);

      Serial.println("Message sent");
      client.stop();
      messageSent = true;
    } else {
      Serial.println("Connection failed, retrying...");
      delay(1000); // Wait 1 second before retrying
      retryCount++;
    }
  }

  if (!messageSent) {
    Serial.println("Failed to send message after 5 attempts.");
  }
}

