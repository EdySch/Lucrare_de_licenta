#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "DHT.h"
#include <HTTPClient.h>
#include <time.h>

// DHT sensor pin and type
#define DHTPIN 14
#define DHTTYPE DHT11

// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Initialize TFT display
TFT_eSPI tft = TFT_eSPI();

// WiFi credentials
const char* ssid = "WI-FI";
const char* password = "1234554321";
WiFiServer server(80);
// Target server IP
const char* host = "192.168.1.128";
const char* host1 = "192.168.1.129";
IPAddress local_IP("192.168.1.130"); // Set your desired static IP address
IPAddress gateway("192.168.1.1"); 
IPAddress subnet("255.255.255.0"); // Subnet mask for iPhone hotspot
IPAddress primaryDNS("8.8.8.8"); // Optional: Google DNS
IPAddress secondaryDNS("8.8.4.4"); // Optional: Google DNS
// NTP server details
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600; // UTC+2 for Eastern European Time (EET)
const int daylightOffset_sec = 3600; // Daylight saving time offset (1 hour)
bool microphoneOn = false;
unsigned long lastMessageTime = 0;
String receivedMessage = "";
// Initialize NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, 60000); // Update interval set to 60 seconds

String currentDate = ""; // Variable to store the current date
bool dateUpdatedToday = false; // Flag to track if the date has been updated today

// Button and LED pins
const int buttonPin1 = 32;
const int ledPin1 = 12;
const int buttonPin2 = 33;
const int ledPin2 = 27;
const int buttonPin3 = 25;
const int ledPin3 = 26;
const int buzzerPin = 21; // Pin for the buzzer

// Variables for message display
String message = "";
unsigned long lastButtonPressTime = 0;
const unsigned long messageDuration = 6000; // 6 seconds
bool buttonsActive = false;
unsigned long lastScreenUpdateTime = 0; // Track the last screen update time
unsigned long lastMessageUpdateTime = 0; // Track the last message update time
uint16_t messageBackgroundColor = TFT_BLACK; // Variable to store the background color for messages
const unsigned long WiFiCheckInterval = 5000;
unsigned long lastWiFiCheckTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(buzzerPin, OUTPUT);

if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize TFT display
  tft.begin();
  tft.setRotation(0); // Adjust Rotation of your screen (0-3)
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
   connectToWiFi();
  // Initialize NTP client
  timeClient.begin();
  timeClient.update();
  server.begin();
  Serial.println("Server started");
  // Get the initial date
  currentDate = getDateString();

  // Initialize button and LED pins
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(ledPin1, OUTPUT);
  digitalWrite(ledPin1, LOW);
  
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin2, LOW);

  pinMode(buttonPin3, INPUT_PULLUP);
  pinMode(ledPin3, OUTPUT);
  digitalWrite(ledPin3, LOW);
}

void loop() {
  unsigned long currentMillis = millis();

  // Read humidity and temperature
  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();

  // Update the display only if 1 second has passed since the last update
  if (currentMillis - lastScreenUpdateTime >= 1000) {
    tft.fillScreen(TFT_BLACK);

    tft.setTextSize(1);
    if (isnan(humidity) || isnan(temperatureC)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      // Display humidity with color gradient
      tft.setCursor(74, 12);
      if (humidity < 30) {
        tft.setTextColor(TFT_RED);
      } else if (humidity >= 30 && humidity < 50) {
        tft.setTextColor(TFT_YELLOW);
      } else if (humidity >= 50 && humidity < 70) {
        tft.setTextColor(TFT_GREEN);
      } else {
        tft.setTextColor(TFT_BLUE);
      }
      tft.print("Humidity: ");
      tft.print(humidity);
      tft.print("%");

      // Display temperature with color gradient
      tft.setCursor(62, 23);
      if (temperatureC < 15) {
        tft.setTextColor(TFT_BLUE);
      } else if (temperatureC >= 15 && temperatureC < 20) {
        tft.setTextColor(TFT_CYAN);
      } else if (temperatureC >= 20 && temperatureC < 25) {
        tft.setTextColor(TFT_YELLOW);
      } else if (temperatureC >= 25 && temperatureC < 30) {
        tft.setTextColor(TFT_ORANGE);
      } else {
        tft.setTextColor(TFT_RED);
      }
      tft.print("Temperature: ");
      tft.print(temperatureC);
      tft.print((char)247);
      tft.print("C ");
    }

    // Update time from NTP server
    timeClient.update();
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);

    // Get current time
    int hours = timeClient.getHours();
    int minutes = timeClient.getMinutes();
    int seconds = timeClient.getSeconds();

    // Construct the time string
    String timeStr = (hours < 10 ? "0" : "") + String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);

    // Calculate text width
    int textWidth = timeStr.length() * 18; // 18 is the approximate width of each character with text size 3

    // Set the cursor to the center of the screen
    tft.setCursor((tft.width() - textWidth) / 2, (tft.height() - 32) / 4);

    // Print the time
    tft.print(timeStr);

    // Print the date
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(60, 90); // Adjust the position according to your display
    tft.print(currentDate);

    // Update date only at midnight
    if (hours == 0 && minutes == 0 && seconds == 0 && !dateUpdatedToday) {
      currentDate = getDateString();
      dateUpdatedToday = true;
    } else if (hours != 0 || minutes != 0 || seconds != 0) {
      dateUpdatedToday = false;
    }

    lastScreenUpdateTime = currentMillis; // Update the last screen update time
  }

  // Check if a WiFi client is available
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");

    // Read the request
    String request = client.readStringUntil('\r');
    Serial.print("Request: ");
    Serial.println(request);

    receivedMessage = request;
    lastMessageTime = millis(); // Update the last message time

    // Send a response to the client
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html><body><h1>Message received</h1></body></html>");
    client.stop(); // Close the connection

    // Activate buttons if "Calling..." message is received
    if (receivedMessage.indexOf("Calling...") >= 0) {
      
      buttonsActive = true;
      lastMessageTime = millis(); // Reset the last message time when "Calling..." is received
      messageBackgroundColor = TFT_BLACK; // Default background for "Calling..." message
    }
  }

  // If 30 seconds have passed since the last "Calling..." message
  if (buttonsActive && (millis() - lastMessageTime > 30000)) {
    handleNoResponse(); // Automatically trigger NO response
  }

  // Check button presses only if buttons are active
  if (buttonsActive) {
    if (digitalRead(buttonPin1) == LOW) {
      blinkLED(ledPin1);
      handleYesResponse();
    }

    if (digitalRead(buttonPin2) == LOW) {
      blinkLED(ledPin2);
      handleNoResponse();
    }

    if (digitalRead(buttonPin3) == LOW) {
      blinkLED(ledPin3);
      microphoneOn = !microphoneOn;
      message = microphoneOn ? " Microphone\n\n    ON" : " Microphone\n\n   OFF";
      messageBackgroundColor = TFT_BLUE; // Blue background for microphone message
      lastButtonPressTime = millis();
    }
  }

  // Update message display only if 1 second has passed since the last update
  if (currentMillis - lastMessageUpdateTime >= 1000) {
    updateDisplay();
    lastMessageUpdateTime = currentMillis; // Update the last message update time
  }
  if (millis() - lastMessageTime > 60000) {
    Serial.println("No message received in the last minute. Restarting...");
    ESP.restart(); // Restart the ESP
  }
  // Periodically check WiFi connection
  if (millis() - lastWiFiCheckTime >= WiFiCheckInterval) {
    lastWiFiCheckTime = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi connection lost. Restarting...");
      ESP.restart();
    }
  }
   delay(1000);
}

void updateDisplay() {
  // Clear previous message if the display is updated and the message duration has passed
  if (millis() - lastButtonPressTime > messageDuration) {
    message = "";
  }

  // Display the message or the received message if buttons are not active
  if (message != "") {
    tft.fillRect(0, 120, tft.width(), tft.height() - 120, messageBackgroundColor); // Use the appropriate background color
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(20, 150); // Adjust the position of the message
    tft.println(message);
  } else if (receivedMessage != "") {
    tft.fillRect(0, 120, tft.width(), tft.height() - 120, TFT_GREEN);
    int messageYPos = 150; // Adjust the Y position for the message
    tft.setCursor(20, messageYPos); // Set the cursor to a specific position
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    buzzCalling();
    tft.println(receivedMessage);
  }
 
  
}

String getDateString() {
  // Create HTTP client object
  HTTPClient http;

  // Your target URL
  String url = "http://worldtimeapi.org/api/timezone/Europe/Bucharest";

  // Send HTTP GET request
  http.begin(url);

  // Check HTTP response code
  int httpResponseCode = http.GET();

  // Initialize date string
  String formattedDate = "Date not available";

  if (httpResponseCode > 0) { // Check for a valid response
    if (httpResponseCode == HTTP_CODE_OK) { // Check for successful response
      String payload = http.getString(); // Get the payload
      int index = payload.indexOf("datetime"); // Find the index of the "datetime" string
      if (index != -1) {
        formattedDate = payload.substring(index + 11, index + 21); // Extract the date string
      }
    }
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
  }

  // Close connection
  http.end();

  return formattedDate;
}

// Function to blink an LED
void blinkLED(int ledPin) {
  digitalWrite(ledPin, HIGH);
  delay(500); // Keep the LED on for 500 milliseconds
  digitalWrite(ledPin, LOW);
}

// Function to handle YES response
void handleYesResponse() {
  microphoneOn = false;
  message = "    YES";
  messageBackgroundColor = TFT_GREEN; // Green background for YES message
  lastButtonPressTime = millis();
  Serial.println("YES");
  
  sendMessageToESP32(host, "   Door unlocked\n\n     (Interior)\r\n");
  sendMessageToESP32(host1, "   Door unlocked\n\n     (Interior)\r\n");

  resetCallState();
}

// Function to handle NO response
void handleNoResponse() {
  microphoneOn = false;
  message = "     NO";
  messageBackgroundColor = TFT_RED; // Red background for NO message
  lastButtonPressTime = millis();
  Serial.println("NO");
  sendMessageToESP32(host, " Request rejected\n\n      (Invalid)\r\n");
  

  resetCallState();
}

// Function to reset call state
void resetCallState() {
  buttonsActive = false;
  receivedMessage = "";
  tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_BLACK); // Clear the message area
}

void buzzCalling() {
  unsigned long currentMillis = millis();
  static unsigned long lastBuzzTime = 0;
  const unsigned long buzzInterval = 2000; // Buzz interval in milliseconds

  if (currentMillis - lastBuzzTime >= buzzInterval) {
    digitalWrite(buzzerPin, HIGH);
    delay(100); // Buzzer on for 100 milliseconds
    digitalWrite(buzzerPin, LOW);
    lastBuzzTime = currentMillis; // Update last buzz time
  }
}
void sendMessageToESP32(const char* targetHost, const String& message) {
  bool messageSent = false;
  while (!messageSent) {
    WiFiClient client;
    if (client.connect(targetHost, 80)) {
      client.print(message);
      client.print("Host: ");
      client.print(targetHost);
      client.print("\r\n");
      client.print("Connection: close\r\n\r\n");
      delay(10);

      Serial.print("Message sent to ");
      Serial.println(targetHost);
      client.stop();
      messageSent = true;
    } else {
      Serial.print("Connection to ");
      Serial.print(targetHost);
      Serial.println(" failed, retrying...");
      delay(1000); // Wait 1 second before retrying
    }
  }
}
void connectToWiFi() {
  int attempts = 0;
 
  Serial.print("Connecting to WiFi..");
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, tft.height() / 2 - 20);
  tft.print("Connecting to WiFi");
  
  WiFi.begin(ssid, password);
  tft.setCursor(80, tft.height() / 2 );
  while (WiFi.status() != WL_CONNECTED && attempts < 7) {
    delay(500);
    Serial.print(".");
    tft.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Connected to WiFi!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    tft.fillScreen(TFT_BLACK);
  } else {
    Serial.println("Failed to connect to WiFi.");
    displayNoInternetMessage();
  }
}

void displayNoInternetMessage() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(60, tft.height() / 2 - 20);
  tft.println("NO INTERNET");
  tft.setCursor(40, tft.height() / 2 + 20);
  tft.println("RESTARTING IN:");

  for (int i = 30; i >= 0; i--) {
    tft.fillRect(10, tft.height() / 2 + 40, tft.width() - 20, 20, TFT_BLACK);
    tft.setCursor(60, tft.height() / 2 + 40);
    tft.println(String(i) + " seconds");
    delay(1000);
  }

  ESP.restart();
}