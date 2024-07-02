#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include "DHT.h"
#include <time.h>

// DHT sensor pin and type
#define DHTPIN 14
#define DHTTYPE DHT11
const int PIN_RED = 32;
const int PIN_GREEN = 33;
const int PIN_BLUE = 25;
const int BUTTON_PIN = 27;
const int Interiorled = 21;
// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Initialize TFT display
TFT_eSPI tft = TFT_eSPI();
// WiFi credentials
const char* ssid = "WI-FI";
const char* password = "1234554321";
WiFiServer server(80);
const char* host = "192.168.1.130";
IPAddress local_IP("192.168.1.128"); // Set your desired static IP address
IPAddress gateway("192.168.1.1"); 
IPAddress subnet("255.255.255.0"); // Subnet mask for iPhone hotspot
IPAddress primaryDNS("8.8.8.8"); // Optional: Google DNS
IPAddress secondaryDNS("8.8.4.4"); // Optional: Google DNS
// NTP server details
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3 * 3600; // UTC+2 for Eastern European Time (EET)
const int daylightOffset_sec = 3600; // Daylight saving time offset (1 hour)

// Initialize NTP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, 60000); // Update interval set to 60 seconds

String receivedMessage = ""; // Variable to store the received message

// Buzzer pin
#define BUZZER_PIN 13
bool buzzerActivated = false; // Flag to track if the buzzer has been activated
bool dateUpdatedToday = false; // Flag to track if the date has been updated today

String currentDate = ""; // Variable to store the current date

unsigned long lastMessageTime = 0; // Variable to track the last time a message was received
unsigned long InteriorActivatedTime = 0; // Variable to track when the Interior was activated
bool InteriorActive = false; // Flag to indicate if the Interior is currently active

bool buttonDisabled = false; // Flag to disable the button
const unsigned long WiFiCheckInterval = 5000;
unsigned long lastWiFiCheckTime = 0;

void setup() {
  Serial.begin(9600);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); 
  pinMode(Interiorled, OUTPUT);
  digitalWrite(Interiorled, LOW);
  // Connect to WiFi
   if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("STA Failed to configure");
  }
  // Initialize DHT sensor
  dht.begin();
    h
  // Initialize TFT display
  tft.begin();
  tft.setRotation(2); // Adjust Rotation of your screen (0-3)
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE);
 connectToWiFi();
 
  
  // Initialize NTP client
  timeClient.begin();
  timeClient.update();
  
  // Start the server
  server.begin();
  Serial.println("Server started");

  // Initialize buzzer pin as output
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Ensure the buzzer is off initially

  // Get the initial date
  currentDate = getDateString();
}

void loop() {

  // Read humidity and temperature
  float humidity = dht.readHumidity();
  float temperatureC = dht.readTemperature();
  
  tft.setTextSize(1);
  
  if (isnan(humidity) || isnan(temperatureC)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    // Clear the screen
    tft.fillScreen(TFT_BLACK);

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

  // Clear the area where the time is displayed
  tft.fillRect(0, (tft.height() - 32) / 2, tft.width(), 32, TFT_BLACK);

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

  // Check if 30 seconds have passed since the last received message
  if (millis() - lastMessageTime > 30000) {
    // Clear the message and set background color to default
    receivedMessage = "";
    tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_BLACK);
    setColor(PIN_RED, PIN_GREEN, PIN_BLUE, 0, 0, 0); // Turn off the LED
    buttonDisabled = false; // Re-enable the button
  }

  // Change background color based on the received message
  if (receivedMessage.indexOf("Calling") >= 0) {
    tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_GREEN);

    // Print the received message if available
    int messageYPos = 150; // Adjust the Y position for the message
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(10, messageYPos); // Set the cursor to a specific position
    tft.println(receivedMessage);
  } else if (receivedMessage.indexOf("Invalid") >= 0) {
    tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_RED);
   
    setColor(PIN_RED, PIN_GREEN, PIN_BLUE, 255, 0, 0); // Turn on red LED
    delay(1000); // Keep the LED on for 1 second
    setColor(PIN_RED, PIN_GREEN, PIN_BLUE, 0, 0, 0); // Turn off the LED
  } else if (receivedMessage.indexOf("unlocked") >= 0) {
    tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_GREEN);
    buttonDisabled = true; // Disable the button while "unlocked" message is active

    if (!buzzerActivated) {
      // Buzz the buzzer for one second
      digitalWrite(BUZZER_PIN, HIGH);
      setColor(PIN_RED, PIN_GREEN, PIN_BLUE, 0, 255, 0); // Turn on green LED
      delay(1000);
      digitalWrite(BUZZER_PIN, LOW);
      setColor(PIN_RED, PIN_GREEN, PIN_BLUE, 0, 0, 0); // Turn off the LED
      controlInterior();
      buzzerActivated = true; // Set the flag to prevent further buzzing
    }
  } else {
    // If no specific keyword is found, use default background color
    tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_BLACK); // Ensure background is black
    
  }

  // Print the received message if available
  if (receivedMessage != "") {
    int messageYPos = 150; // Adjust the Y position for the message
    tft.setCursor(10, messageYPos); // Set the cursor to a specific position
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.println(receivedMessage);
  }

  // Check if a WiFi client is available
  WiFiClient client = server.available();
  if (client) {
    Serial.println("Client connected");

    // Read the request
    String request = client.readStringUntil('\r');
    Serial.println(request);

    receivedMessage = request;
    lastMessageTime = millis(); // Update the last message time

    // Send a response to the client
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println();
    client.println("<html><body><h1>Message received</h1></body></html>");
    client.stop(); // Close the connection

    // Reset the flag when a new message is received
    buzzerActivated = false;
  }

  // Check button press and send message
  if (digitalRead(BUTTON_PIN) == LOW && !buttonDisabled) {
    sendMessageToHost(host);
  }

  // Interior control with non-blocking delay
  if (InteriorActive && (millis() - InteriorActivatedTime > 30000)) {
    Serial.println("Turning Interior OFF.");
    digitalWrite(Interiorled, LOW); // Turn off the Interior
    InteriorActive = false;
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
  // Delay for one second before updating again
  delay(1000);
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

// Function to set RGB LED color
void setColor(int redPin, int greenPin, int bluePin, int redValue, int greenValue, int blueValue) {
  analogWrite(redPin, redValue);
  analogWrite(greenPin, greenValue);
  analogWrite(bluePin, blueValue);
}

void controlInterior() {
  Serial.println("Turning Interior ON for 30 seconds.");
  digitalWrite(Interiorled, HIGH); // Turn on the Interior
  InteriorActivatedTime = millis(); // Record the time the Interior was activated
  InteriorActive = true; // Set the Interior active flag
}

void sendMessageToHost(const char* host) {
  bool messageSent = false;
  while (!messageSent) {
    WiFiClient client;
    if (client.connect(host, 80)) {
      // Send the message
      client.print(" Calling...\r\n");
      client.print("Host: ");
      client.print(host);
      client.print("\r\n");
      client.print("Connection: close\r\n\r\n");
      delay(10);

      Serial.println("Message sent to host: " + String(host));

      // Update the receivedMessage and lastMessageTime
      receivedMessage = "     Calling..."; // Update received message
      lastMessageTime = millis(); // Update last message time

      // Set background to green
      tft.fillRect(0, tft.height() / 2, tft.width(), tft.height() / 2, TFT_GREEN);

      client.stop();
      messageSent = true;
    } else {
      Serial.println("Connection to host " + String(host) + " failed, retrying...");
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