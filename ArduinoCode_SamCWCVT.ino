#include <M5StickCPlus2.h> // Defines Hardware
#include <WiFi.h> // Wifi Access point
#include <HardwareSerial.h>
#include <BLEDevice.h> // For Bluetooth
#include <BLEServer.h> // For Bluetooth
#include <BLEUtils.h> // For Bluetooth
#include <BLE2902.h> // For Bluetooth
#include <ModbusRTU.h> // Modbus library
#include <EEPROM.h>    // For storing settings
#include <ESPmDNS.h> //For Mapgwy.local link
#include "esp_system.h"

uint8_t getCPUTemperature() {
    return temperatureRead();  // Returns temperature in Celsius
}


// Define colors for the blue theme
#define BACKGROUND_COLOR 0x102D // Dark blue
#define TITLE_BAR_COLOR 0x42DF // Darker blue for the title bar
#define TEXT_COLOR 0xFFFF      // White
#define HIGHLIGHT_COLOR 0x27EC // Red

// Buzzer pin
#define BUZZER_PIN 26 // Buzzer is connected to GPIO26

// Access Point credentials
const char* ssid = "CWCVT-EB:6B"; // SSID of the Access Point (no password)

// Web server running on port 80
WiFiServer server(80);

// Modbus RTU setup
#define RX_PIN 33 // RX pin for RS485-to-TTL module
#define TX_PIN 32 // TX pin for RS485-to-TTL module
#define DE_PIN 26 // DE pin for RS485-to-TTL module (if needed)
#define RE_PIN 25 // RE pin for RS485-to-TTL module (if needed)

HardwareSerial RS485Serial(1); // Use UART1 for RS485 communication
ModbusRTU mb; // Modbus RTU object


// Modbus data
uint16_t modbusData[10]; // Array to store Modbus register values

// Menu index
int menuIndex = 0; // Current menu index
String menuItems[] = {"- CWCVT:Wi-fi", "- BTCVT:Ble", "- Information"}; // Menu items

// MS/TP Statistics
String mstpStatus = "Active"; // Example: MS/TP status
int mstpNetworkNumber = 1;    // Example: MS/TP network number
int mstpTokenLoopTime = 100;  // Example: Token loop time in milliseconds
int mstpFramesSent = 1234;    // Example: Frames sent
int mstpFramesReceived = 567; // Example: Frames received

// BACnet and MS/TP settings
struct Settings {
  int bacnetIpPort = 47808;
  int bacnetNetworkNumber = 10001;
  int deviceObjectIdentifier = 27485;
  int busStartBaudRate = 38400;

    // Modbus settings
  int modbusSlaveID = 1;          // Default Modbus slave ID
  int modbusRegisterAddress = 0;  // Default Modbus register address
  int modbusBaudRate = 9600;      // Default Modbus baud rate

  uint8_t padding[4096 - sizeof(int) * 7]; // Adjust padding size as needed

};


Settings settings; // Global settings object

// Define the notes and their frequencies (in Hz)
#define NOTE_E7  2637
#define NOTE_C7  2093
#define NOTE_G7  3136
#define NOTE_G6  1568
#define NOTE_A6  1760
#define NOTE_F7  2794
#define NOTE_F6  1397
#define NOTE_E6  1319

// Define the durations (in milliseconds)
const int shortNote = 125;  // Short notes
const int mediumNote = 150; // Medium notes
const int longNote = 300;   // Long notes
const int shortPause = 50;  // Quick pause
const int mediumPause = 100; 
const int longPause = 200;

void playJingle() {
  tone(2, NOTE_E7, shortNote);
  delay(shortNote + shortPause);

  tone(2, NOTE_E7, shortNote);
  delay(shortNote + shortPause);

  tone(2, NOTE_E7, shortNote);
  delay(shortNote + mediumPause);

  tone(2, NOTE_C7, shortNote);
  delay(shortNote + mediumPause);

  tone(2, NOTE_E7, longNote);
  delay(longNote + mediumPause);

  tone(2, NOTE_G7, longNote);
  delay(longNote + longPause);

  delay(mediumPause); // Extra rhythm delay

  tone(2, NOTE_G6, longNote);
  delay(longNote + mediumPause);

  noTone(2); // Stop the tone
}

// BLE variables
BLEServer *pServer = nullptr;
BLEService *pService = nullptr;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// BLE UUIDs
#define BLE_SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// BLE server callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
    }
};


// Function to Stop BLE
void stopBLE() {
    BLEDevice::deinit();  // Clean up BLE properly
    delay(500);
}

// Function to start BLE
void startBLE() {
    BLEDevice::deinit(false);  // Clears bonding info
    M5.Lcd.fillScreen(BACKGROUND_COLOR);
    M5.Lcd.fillRect(0, 0, M5.Lcd.width(), 30, TITLE_BAR_COLOR);
    M5.Lcd.setTextColor(0xAE3F);
    M5.Lcd.println("  Starting BT...");
    M5.Lcd.setTextColor(TEXT_COLOR);
    M5.Lcd.println("");
    M5.Lcd.setTextColor(0xF965);
    M5.Lcd.println("");
    M5.Lcd.println(" Press B to Stop BT");
    M5.Lcd.println("   at any time...");
    M5.Lcd.setTextColor(TEXT_COLOR);
    delay(4000);

    // ✅ Prevent multiple initializations
    if (!BLEDevice::getInitialized()) {
        BLEDevice::init("BTCVT-Ble"); // Initialize BLE with a name
    }

    pServer = BLEDevice::createServer(); 
    pServer->setCallbacks(new MyServerCallbacks());
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
    pService = pServer->createService(BLE_SERVICE_UUID);
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(BLE_SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    // Update screen to show BLE is active
    M5.Lcd.fillScreen(BACKGROUND_COLOR);
    M5.Lcd.fillRect(0, 0, M5.Lcd.width(), 30, TITLE_BAR_COLOR);
    M5.Lcd.setCursor(10, 5);
    M5.Lcd.setTextColor(0xAE3F);
    M5.Lcd.println("  Bluetooth Mode");
    M5.Lcd.println("");
    M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
    M5.Lcd.print(" NODE: ");
    M5.Lcd.setTextColor(0x42DF); // Set text color
    M5.Lcd.println("BTCVT-Ble");
    M5.Lcd.setCursor(10, 60);
    M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
    M5.Lcd.print(" STATUS: ");
    M5.Lcd.setTextColor(0x27E9); // Set text color
    M5.Lcd.println("Live");
    M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
    M5.Lcd.println("");
    M5.Lcd.println(" Use your JCI CWA");
    M5.Lcd.println("  App to connect..");

    // Keep BLE running until B is pressed
     while (true) {
        M5.update();
        if (M5.BtnB.wasPressed()) {
            tone(2, 1000, 200); // Play a 1000 Hz tone for 200 milliseconds
            M5.Lcd.fillScreen(BACKGROUND_COLOR);
            M5.Lcd.setCursor(10, 10);
            M5.Lcd.setTextColor(0xF965); // Set text color
            M5.Lcd.println("");
            M5.Lcd.println("");
            M5.Lcd.println("");
            M5.Lcd.println("   Stopping BT...");
            M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
            BLEDevice::stopAdvertising();
            pService->stop();
            pServer->removeService(pService);
            stopBLE();  // Call cleanup function
            delay(1000);
            break;
        }
    }
}


void setup() {
  M5.begin(); // Initialize M5StickC Plus 2
  pinMode(BUZZER_PIN, OUTPUT); // Set buzzer pin as output
  digitalWrite(BUZZER_PIN, LOW); // Ensure buzzer is off
  M5.Lcd.setRotation(1); // Set screen rotation
  M5.Lcd.fillScreen(BACKGROUND_COLOR); // Set background color
  M5.Lcd.setTextSize(2); // Set text size
  M5.Lcd.setTextColor(0x42DF); // Set text color

  // Initialize EEPROM
  EEPROM.begin(sizeof(Settings));
  loadSettings(); // Load saved settings

  // Initialize RS485 communication
  RS485Serial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN); // Adjust baud rate as needed
  mb.begin(&RS485Serial, DE_PIN, RE_PIN); // Initialize Modbus RTU


  BLEDevice::deinit(true);  // Clear BLE storage and restart


  // Display centered boot screen text
  M5.Lcd.setTextSize(3); // Set text size
  playJingle();   // Play the startup jingle

  M5.Lcd.setTextColor(0xAE3F);
  M5.Lcd.setTextDatum(MC_DATUM); // Set text datum to midd                      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjle-center
  M5.Lcd.drawString("JCI", M5.Lcd.width() / 2, M5.Lcd.height() / 3); // Centered at 1/3 height

  M5.Lcd.setTextColor(0x2FED);
  M5.Lcd.drawString("C W C V T", M5.Lcd.width() / 2, (M5.Lcd.height() / 3) * 2); // Centered at 2/3 height
  M5.Lcd.setTextDatum(TL_DATUM); // Reset text alignment to Top-Left
  delay(5000); // Simulate boot time
  M5.Lcd.fillScreen(BACKGROUND_COLOR); // Clear the screen after boot
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color back to normal
  M5.Lcd.setTextSize(2); // Set text size

  // Display initial menu
  displayMenu();
}

void loop() {
   M5.update(); // Update button states
  M5.Lcd.setTextFont(3); // Set text Font

  // Read the battery voltage (in millivolts)
  int batteryVoltage = M5.Power.getBatteryVoltage();

  // Convert voltage to percentage (assuming 3.3V min and 4.2V max)
  float batteryPercentage = (batteryVoltage - 3300) / (4200.0 - 3300.0) * 100;

  // Constrain the percentage to 0-100%
  if (batteryPercentage < 0) {
    batteryPercentage = 0;
  } else if (batteryPercentage > 100) {
    batteryPercentage = 100;
  }

  // Clear the previous percentage display
  M5.Lcd.fillRect(M5.Lcd.width() - 60, M5.Lcd.height() - 20, 60, 20, 0x3145); // Clear the area

  // Display "Battery:" in the bottom left corner
  M5.Lcd.setCursor(10, M5.Lcd.height() - 20); // Bottom left corner
  M5.Lcd.setTextColor(TEXT_COLOR);
  M5.Lcd.print("");

  // Determine the color based on the battery percentage
  uint16_t batteryColor;
  if (batteryPercentage <= 25) {
    batteryColor = TFT_RED; // Red for <= 25%
  } else if (batteryPercentage <= 75) {
    batteryColor = TFT_YELLOW; // Yellow for <= 75%
  } else {
    batteryColor = TFT_GREEN; // Green for > 75%
  }

  // Display the battery percentage in the bottom right corner
  M5.Lcd.setCursor(M5.Lcd.width() - 50, M5.Lcd.height() - 20); // Adjust position as needed
  M5.Lcd.setTextColor(batteryColor);
  M5.Lcd.printf("%.0f%%", batteryPercentage);

  // Update MS/TP statistics
  updateMstpStats();

  // Handle button presses
  if (M5.BtnB.wasPressed()) { // Button B cycles down through the menu
    menuIndex = (menuIndex + 1) % 3; // Cycle through menu items
    tone(2, 1000, 200); // Play a 1000 Hz tone for 200 milliseconds
    chirpBuzzer(); // Chirp the buzzer
    displayMenu();
  }

  if (M5.BtnPWR.wasPressed()) { // Button C cycles up through the menu
    menuIndex = (menuIndex - 1 + 3) % 3; // Cycle through menu items
    tone(2, 1000, 200); // Play a 1000 Hz tone for 200 milliseconds
    chirpBuzzer(); // Chirp the buzzer
    displayMenu();
  }

  if (M5.BtnA.wasPressed()) { // Button A selects the menu option
    chirpBuzzer(); // Chirp the buzzer
    tone(2, 2000, 100); // Frequency: 2000 Hz, Duration: 100 ms
    selectMenuItem();
  }

  // Handle client requests if the Access Point is enabled
  if (menuIndex == 0 && WiFi.getMode() == WIFI_AP) {
    handleClientRequests();
  }
}

void chirpBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH); // Turn on the buzzer
  delay(50);                      // Keep it on for 50ms
  digitalWrite(BUZZER_PIN, LOW);  // Turn off the buzzer
}

void updateMstpStats() {
  // Example: Update MS/TP statistics from your BACnet stack
  mstpStatus = "Active"; // Replace with actual status
  mstpNetworkNumber = 1; // Replace with actual network number
  mstpTokenLoopTime = 100; // Replace with actual token loop time
  mstpFramesSent = 1234; // Replace with actual frames sent
  mstpFramesReceived = 567; // Replace with actual frames received
}

void displayMenu() {
  M5.Lcd.fillScreen(BACKGROUND_COLOR); // Clear the screen with blue background

  // Draw a dark blue title bar
  M5.Lcd.fillRect(0, 0, M5.Lcd.width(), 30, TITLE_BAR_COLOR); // Title bar at the top

  // Display "Menu:" text on the title bar
  M5.Lcd.setCursor(10, 5);
  M5.Lcd.setTextColor(0xAE3F);
  M5.Lcd.println("    Home Page");
  M5.Lcd.setTextColor(TEXT_COLOR);

  // Display menu items
  for (int i = 0; i < 3; i++) {
    if (i == menuIndex) {
      M5.Lcd.setTextColor(HIGHLIGHT_COLOR); // Highlight the selected item in light blue
    } else {
      M5.Lcd.setTextColor(TEXT_COLOR);
    }
    M5.Lcd.setCursor(20, 40 + i * 20);
    M5.Lcd.println(menuItems[i]);
  }
}

void selectMenuItem() {
  M5.Lcd.fillScreen(BACKGROUND_COLOR); // Clear the screen with blue background
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.setTextColor(TEXT_COLOR);

  // Handle the selected menu item
  switch (menuIndex) {
    case 0: // Start AP
      enableAccessPoint();
      break;
    case 1:
      startBLE(); // Start BLE
      break;
    case 2:
  int batteryVoltage = M5.Power.getBatteryVoltage();

  // Convert voltage to percentage (assuming 3.3V min and 4.2V max)
  float batteryPercentage = (batteryVoltage - 3300) / (4200.0 - 3300.0) * 100;

  // Constrain the percentage to 0-100% (directly in the loop)
  if (batteryPercentage < 0) {
    batteryPercentage = 0;
  } else if (batteryPercentage > 100) {
    batteryPercentage = 100;
  }
      M5.Lcd.fillScreen(BACKGROUND_COLOR); // Clear the screen
      M5.Lcd.fillRect(0, 0, M5.Lcd.width(), 30, TITLE_BAR_COLOR); // Title bar at the top
      M5.Lcd.setTextColor(0xAE3F);
      M5.Lcd.println("   Device info");
      M5.Lcd.setTextColor(TEXT_COLOR);
      M5.Lcd.println(" ");
      M5.Lcd.print(" SW:");
      M5.Lcd.setTextColor(0x42DF);
      M5.Lcd.println("  Ver2.4");
      M5.Lcd.setTextColor(TEXT_COLOR);
      M5.Lcd.print(" HW:");
      M5.Lcd.setTextColor(0x42DF);
      M5.Lcd.println("  M5StickCplus2");
      M5.Lcd.setTextColor(TEXT_COLOR);
      M5.Lcd.print(" CPU TEMP(C): ");
      M5.Lcd.setTextColor(0x42DF);
      M5.Lcd.println(getCPUTemperature()); // Call the function
      M5.Lcd.setTextColor(TEXT_COLOR);

      // Display battery information
      M5.Lcd.setTextColor(TEXT_COLOR);
      M5.Lcd.print(" Battery: ");
      M5.Lcd.setTextColor(0x42DF);
      M5.Lcd.print(batteryVoltage);
      M5.Lcd.println(" mV");
      M5.Lcd.setTextColor(TEXT_COLOR);

            // Display system uptime
      unsigned long uptime = millis() / 1000; // Convert to seconds
      int hours = uptime / 3600;
      int minutes = (uptime % 3600) / 60;
      int seconds = uptime % 60;
      M5.Lcd.setTextColor(TEXT_COLOR);
      M5.Lcd.print(" Uptime: ");
      M5.Lcd.setTextColor(0x42DF);
      M5.Lcd.printf("%02d:%02d:%02d", hours, minutes, seconds);
      M5.Lcd.setTextColor(TEXT_COLOR);
      M5.Lcd.println("         Hrs, min, sec");


      break;
  }

  delay(5000); // Display the selected item for 5 seconds
  displayMenu(); // Return to the menu
}

void displayCenteredText(const char* text) {
  M5.Lcd.setTextSize(2); // Set text size
  int textWidth = M5.Lcd.textWidth(text); // Get the width of the text
  int textHeight = M5.Lcd.fontHeight(); // Get the height of the text

  // Calculate the X and Y positions to center the text
  int screenWidth = M5.Lcd.width();
  int screenHeight = M5.Lcd.height();
  int x = (screenWidth - textWidth) / 2;
  int y = (screenHeight - textHeight) / 2;

  // Display the centered text
  M5.Lcd.setCursor(x, y);
  M5.Lcd.println(text);
}

void drawWiFiSignalIndicator() {
    if (WiFi.status() == WL_CONNECTED) {
        long rssi = WiFi.RSSI(); // Get signal strength
        
        // Define square position and size (adjust as needed)
      int squareX = M5.Lcd.width() - 20;  // Near the right edge
      int squareY = M5.Lcd.height() - 20; // Near the bottom edge
      int squareSize = 17;   // Small square size

        // Determine color based on signal strength
        uint16_t signalColor;
        if (rssi > -50) {
            signalColor = TFT_GREEN;  // Strong signal
        } else if (rssi > -70) {
            signalColor = TFT_YELLOW;  // Medium signal
        } else {
            signalColor = TFT_RED;  // Weak signal
        }

        // Draw the WiFi signal square indicator
        M5.Lcd.fillRect(squareX, squareY, squareSize, squareSize, signalColor);
    }
}

void enableAccessPoint() {
  M5.Lcd.fillScreen(BACKGROUND_COLOR); // Clear the screen
  M5.Lcd.fillRect(0, 0, M5.Lcd.width(), 30, TITLE_BAR_COLOR); // Title bar at the top
  M5.Lcd.setTextColor(0xAE3F);
  M5.Lcd.println("   Starting AP...");
  M5.Lcd.setTextColor(TEXT_COLOR);
  M5.Lcd.println("");
  M5.Lcd.println("");
  M5.Lcd.setTextColor(0xF965); // Set text color
  M5.Lcd.println(" Press B to Stop AP");
  M5.Lcd.println("   at any time...");
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
  delay(4000); // Display the selected item for 2 seconds
  M5.Lcd.fillScreen(BACKGROUND_COLOR); // Clear the screen
  M5.Lcd.setCursor(10, 10);
  M5.Lcd.fillRect(0, 0, M5.Lcd.width(), 30, TITLE_BAR_COLOR); // Title bar at the top
  M5.Lcd.setTextColor(0xAE3F);
  M5.Lcd.println("  Wi-fi AP Router");
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color

  // Start the Access Point (no password)
  WiFi.softAP(ssid);
  MDNS.begin("Mapgwy");
  IPAddress IP = WiFi.softAPIP();
  server.begin();

  // Display AP status on the M5StickC Plus 2 screen

  M5.Lcd.setCursor(10, 40);
  M5.Lcd.print("SSID: ");
  M5.Lcd.setTextColor(0xF19F); // Set text color
  M5.Lcd.println(ssid);
  M5.Lcd.setCursor(10, 60);
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
  M5.Lcd.print("HTTP: ");
  M5.Lcd.setTextColor(0x419F); // Set text color
  M5.Lcd.println("Mapgwy.local");
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
  M5.Lcd.print(" IP: ");
  M5.Lcd.setTextColor(0xF7E3); // Set text color
  M5.Lcd.println(IP);
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
  M5.Lcd.print(" PORT: ");
  M5.Lcd.setTextColor(0xFCA3); // Set text color
  M5.Lcd.println("80");
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
  M5.Lcd.print(" STATUS: ");
  M5.Lcd.setTextColor(0x27E9); // Set text color
  M5.Lcd.println("Live");
  M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
  drawWiFiSignalIndicator();

  // Keep the status screen active
  while (true) {
    M5.update();
    if (M5.BtnB.wasPressed()) { // Exit the status screen
      tone(2, 1000, 200); // Play a 1000 Hz tone for 200 milliseconds
      M5.Lcd.fillScreen(BACKGROUND_COLOR);
      M5.Lcd.setTextColor(0xF965); // Set text color
      M5.Lcd.setCursor(10, 10);
      M5.Lcd.println("");
      M5.Lcd.println("");
      M5.Lcd.println("");
      M5.Lcd.println("   Stopping AP...");
      M5.Lcd.setTextColor(TEXT_COLOR); // Set text color
      WiFi.softAPdisconnect(true); // Disable the Access Point
      break;
    }
    handleClientRequests(); // Handle web requests
  }
}

void handleClientRequests() {
  WiFiClient client = server.available(); // Check for client connections

  if (client) {
    String request = client.readStringUntil('\r'); // Read the HTTP request
    client.flush();

    // Serve the BACnet and MS/TP settings page
    if (request.indexOf("GET /settings") >= 0) {
      serveSettingsPage(client);
    }
    // Handle form submission
    else if (request.indexOf("POST /update-settings") >= 0) {
      handleSettingsUpdate(client, request);
    }
    // Serve the default page with Modbus data
    else {
      serveDefaultPage(client);
    }

    delay(10);
    client.stop();
  }
}

void serveDefaultPage(WiFiClient &client) {
  // Read Modbus data
  if (mb.slave()) { // Check if Modbus communication is active
    mb.readHreg(1, 0, modbusData, 10); // Read 10 holding registers from slave ID 1
  }

  // Serve a web page with Modbus data and MS/TP statistics
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<div style='text-align: center;'>");
  client.println("<img src='data:image/png;base64,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' alt='Logo' style='width: 150px;'>");
  client.println("</div>");
  client.println("<title>HVAC Controller Data</title>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"); // Mobile-friendly
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 0; padding: 0; }");
  client.println(".container { max-width: 800px; margin: 20px auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }");
  client.println("h1 { color: #007BFF; text-align: center; margin-bottom: 20px; }");
  client.println("h2 { color: #0056b3; margin-top: 20px; }");
  client.println(".card { background: #ffffff; border: 1px solid #ddd; border-radius: 8px; padding: 15px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1); }");
  client.println("button { background-color: #007BFF; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-size: 16px; width: 100%; margin-top: 10px; }");
  client.println("button:hover { background-color: #0056b3; }");
  client.println("ul { list-style-type: none; padding: 0; }");
  client.println("li { background: #e9f2ff; margin: 5px 0; padding: 10px; border-radius: 5px; }");
  client.println("a { color: #007BFF; text-decoration: none; font-weight: bold; display: block; margin-top: 15px; }");
  client.println("a:hover { text-decoration: underline; }");
  client.println("</style>");
  client.println("<script>");
  client.println("function refreshPage() { location.reload(); }");
  client.println("</script>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");
  client.println("<h1>HVAC Controller Data</h1>");

  // Refresh Button
  client.println("<button onclick='refreshPage()'>Refresh Data</button>");

  // Modbus Data Section
  client.println("<div class='card'>");
  client.println("<h2>Modbus Register Values</h2>");
  client.println("<ul>");
  for (int i = 0; i < 10; i++) {
    client.print("<li><strong>Register ");
    client.print(i);
    client.print(":</strong> ");
    client.print(modbusData[i]);
    client.println("</li>");
  }
  client.println("</ul>");
  client.println("</div>"); // End Modbus Data Card

  // MS/TP Statistics Section
  client.println("<div class='card'>");
  client.println("<h2>MS/TP Statistics</h2>");
  client.println("<ul>");
  client.print("<li><strong>Status:</strong> ");
  client.print(mstpStatus);
  client.println("</li>");
  client.print("<li><strong>Network Number:</strong> ");
  client.print(mstpNetworkNumber);
  client.println("</li>");
  client.print("<li><strong>Token Loop Time:</strong> ");
  client.print(mstpTokenLoopTime);
  client.println(" ms</li>");
  client.print("<li><strong>Frames Sent:</strong> ");
  client.print(mstpFramesSent);
  client.println("</li>");
  client.print("<li><strong>Frames Received:</strong> ");
  client.print(mstpFramesReceived);
  client.println("</li>");
  client.println("</ul>");
  client.println("</div>"); // End MS/TP Statistics Card

  // Links to Settings Pages
  client.println("<div class='card'>");
  client.println("<h2>Configuration</h2>");
  client.println("<p><a href='/modbus-settings'>Configure Modbus Settings</a></p>");
  client.println("<p><a href='/settings'>Configure BACnet and MS/TP Settings</a></p>");
  client.println("</div>"); // End Configuration Card

  client.println("</div>"); // End Container
  client.println("</body>");
  client.println("</html>");
}

void serveModbusSettingsPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>Modbus Settings</title>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"); // Mobile-friendly
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 0; padding: 0; }");
  client.println(".container { max-width: 500px; margin: 20px auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }");
  client.println("h1 { color: #007BFF; text-align: center; margin-bottom: 20px; }");
  client.println("form { display: flex; flex-direction: column; align-items: center; }");
  client.println("label { font-weight: bold; margin-top: 10px; }");
  client.println("input { width: 90%; padding: 8px; margin: 5px 0; border: 1px solid #ccc; border-radius: 5px; font-size: 16px; }");
  client.println("input[type='submit'] { background-color: #007BFF; color: white; border: none; padding: 10px 15px; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 15px; }");
  client.println("input[type='submit']:hover { background-color: #0056b3; }");
  client.println("a { color: #007BFF; text-decoration: none; font-weight: bold; display: block; margin-top: 15px; }");
  client.println("a:hover { text-decoration: underline; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");
  client.println("<h1>Modbus Settings</h1>");
  client.println("<form method='POST' action='/update-modbus-settings'>");
  
  // Modbus Slave ID
  client.println("<label for='modbusSlaveID'>Modbus Slave ID:</label>");
  client.println("<input type='number' id='modbusSlaveID' name='modbusSlaveID' value='" + String(settings.modbusSlaveID) + "'>");
  
  // Modbus Register Address
  client.println("<label for='modbusRegisterAddress'>Modbus Register Address:</label>");
  client.println("<input type='number' id='modbusRegisterAddress' name='modbusRegisterAddress' value='" + String(settings.modbusRegisterAddress) + "'>");
  
  // Modbus Baud Rate
  client.println("<label for='modbusBaudRate'>Modbus Baud Rate:</label>");
  client.println("<input type='number' id='modbusBaudRate' name='modbusBaudRate' value='" + String(settings.modbusBaudRate) + "'>");
  
  // Save Button
  client.println("<input type='submit' value='Save'>");
  
  client.println("</form>");
  client.println("<a href='/'>Back to Home</a>");
  client.println("</div>");
  client.println("</body>");
  client.println("</html>");
}

void handleModbusSettingsUpdate(WiFiClient &client, String request) {
  // Parse form data
  int modbusSlaveID = getFormValue(request, "modbusSlaveID").toInt();
  int modbusRegisterAddress = getFormValue(request, "modbusRegisterAddress").toInt();
  int modbusBaudRate = getFormValue(request, "modbusBaudRate").toInt();

  // Update Modbus settings
  settings.modbusSlaveID = modbusSlaveID;
  settings.modbusRegisterAddress = modbusRegisterAddress;
  settings.modbusBaudRate = modbusBaudRate;

  // Save settings to EEPROM
  saveSettings();

  // Reinitialize Modbus with the new settings
  RS485Serial.begin(settings.modbusBaudRate, SERIAL_8N1, RX_PIN, TX_PIN);
  mb.begin(&RS485Serial, DE_PIN, RE_PIN);

  // Redirect to the Modbus settings page
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /modbus-settings");
  client.println();
}

void serveSettingsPage(WiFiClient &client) {
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  client.println("<head>");
  client.println("<title>BACnet and MS/TP Settings</title>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"); // Mobile-friendly
  client.println("<style>");
  client.println("body { font-family: Arial, sans-serif; background-color: #f4f4f4; color: #333; margin: 0; padding: 0; }");
  client.println(".container { max-width: 500px; margin: 20px auto; padding: 20px; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1); }");
  client.println("h1 { color: #007BFF; text-align: center; margin-bottom: 20px; }");
  client.println("form { display: flex; flex-direction: column; align-items: center; }");
  client.println("label { font-weight: bold; margin-top: 10px; }");
  client.println("input { width: 90%; padding: 8px; margin: 5px 0; border: 1px solid #ccc; border-radius: 5px; font-size: 16px; }");
  client.println("input[type='submit'] { background-color: #007BFF; color: white; border: none; padding: 10px 15px; border-radius: 5px; cursor: pointer; font-size: 16px; margin-top: 15px; }");
  client.println("input[type='submit']:hover { background-color: #0056b3; }");
  client.println("a { color: #007BFF; text-decoration: none; font-weight: bold; display: block; margin-top: 15px; }");
  client.println("a:hover { text-decoration: underline; }");
  client.println("</style>");
  client.println("</head>");
  client.println("<body>");
  client.println("<div class='container'>");
  client.println("<h1>BACnet and MS/TP Settings</h1>");
  client.println("<form method='POST' action='/update-settings'>");
  
  // BACnet IP Port
  client.println("<label for='bacnetIpPort'>BACnet IP Port:</label>");
  client.println("<input type='number' id='bacnetIpPort' name='bacnetIpPort' value='" + String(settings.bacnetIpPort) + "'>");
  
  // BACnet Network Number
  client.println("<label for='bacnetNetworkNumber'>BACnet Network Number:</label>");
  client.println("<input type='number' id='bacnetNetworkNumber' name='bacnetNetworkNumber' value='" + String(settings.bacnetNetworkNumber) + "'>");
  
  // Device Object Identifier
  client.println("<label for='deviceObjectIdentifier'>Device Object Identifier:</label>");
  client.println("<input type='number' id='deviceObjectIdentifier' name='deviceObjectIdentifier' value='" + String(settings.deviceObjectIdentifier) + "'>");
  
  // Bus Start Baud Rate
  client.println("<label for='busStartBaudRate'>Bus Start Baud Rate:</label>");
  client.println("<input type='number' id='busStartBaudRate' name='busStartBaudRate' value='" + String(settings.busStartBaudRate) + "'>");
  
  // Save Button
  client.println("<input type='submit' value='Save'>");
  
  client.println("</form>");
  client.println("<a href='/'>Back to Home</a>");
  client.println("</div>");
  client.println("</body>");
  client.println("</html>");
}

void handleSettingsUpdate(WiFiClient &client, String request) {
  // Parse form data
  int bacnetIpPort = getFormValue(request, "bacnetIpPort").toInt();
  int bacnetNetworkNumber = getFormValue(request, "bacnetNetworkNumber").toInt();
  int deviceObjectIdentifier = getFormValue(request, "deviceObjectIdentifier").toInt();
  int busStartBaudRate = getFormValue(request, "busStartBaudRate").toInt();

  // Update settings
  settings.bacnetIpPort = bacnetIpPort;
  settings.bacnetNetworkNumber = bacnetNetworkNumber;
  settings.deviceObjectIdentifier = deviceObjectIdentifier;
  settings.busStartBaudRate = busStartBaudRate;

  // Save settings to EEPROM
  saveSettings();

  // Redirect to the settings page
  client.println("HTTP/1.1 303 See Other");
  client.println("Location: /settings");
  client.println();
}

String getFormValue(String request, String fieldName) {
  int startIndex = request.indexOf(fieldName + "=");
  if (startIndex < 0) return "";
  startIndex += fieldName.length() + 1;
  int endIndex = request.indexOf("&", startIndex);
  if (endIndex < 0) endIndex = request.length();
  return request.substring(startIndex, endIndex);
}

void loadSettings() {
  EEPROM.get(0, settings);
}

void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}
