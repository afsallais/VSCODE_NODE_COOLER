/*
 * Project: VSCODE_NODE_COOLER
 * Author: Afsal Lais
 * Version: 1.0.3
 * Description: This project monitors temperature and controls a fan and LEDs based on the temperature readings.
 * Date: March 2025
 */

#include <Arduino.h>  // Include the Arduino core library
#include <Wire.h>   // Include the I2C library
#include <Adafruit_GFX.h>  // Include the Adafruit graphics library
#include <Adafruit_ST7735.h>  // Include the Adafruit ST7735 library
#include <Ticker.h>  // Include the Ticker library
#include <ESP8266WiFi.h>  // Include the ESP8266 WiFi library
#include <ArduinoOTA.h>  // Include the Arduino OTA library
#include <WiFiUdp.h>  
#include <NTPClient.h>
#include <time.h>
#include <Fonts/FreeSansBold12pt7b.h>  // Include the custom font header file
#include <Fonts/TomThumb.h>  // Include the custom font header file

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 19800, 60000); // Offset for India (UTC+5:30)


// WiFi credentials
const char* ssid = "Lais Deco 1";
const char* password = "8089128014";



// Define macro to enable/disable buzzer
#define DISABLE_LOCAL 1  
#define ENABLE_BUZZER 0  // Set to 1 to enable buzzer, 0 to disable
#define LED_ENABLE 0     // Set to 1 to enable LEDs, 0 to disable
#define BUTTON_ENABLE 1  // Set to 1 to enable buttons, 0 to disable
#define PLOT_STYLE_LINE 0  // Set to 1 for line plot, 0 for dot plot

/*
| Component       | Description       | Pin       | GPIO       | PWM       | Multi-Function Capabilities                 | Drive Type       |
|-----------------|-------------------|-----------|------------|-----------|---------------------------------------------|------------------|
| **LEDs**        |                   |           |            |           |                                             |                  |
| RED             |                   | GPIO6     | GPIO6      | No        | Internal Flash Connection                   | Push-Pull        |
| YELLOW          |                   | GPIO10    | GPIO10     | No        | Internal Flash Connection                   | Open-Drain       |
| GREEN           |                   | GPIO11    | GPIO11     | No        | Internal Flash Connection                   | Push-Pull        |
| **Buttons**     |                   |           |            |           |                                             |                  |
| BUTTON1 (back)  |                   | D3        | GPIO0      | No        | Can be used to wake up from deep sleep      | Push-Pull        |
| BUTTON2 (select)|                   | D2        | GPIO4      | Yes       |                                             | Push-Pull        |
| BUTTON3 (up)    |                   | D1        | GPIO5      | Yes       |                                             | Push-Pull        |
| **Thermistor**  |                   | A0        | Analog Pin | No        |                                             |                  |
| **Buzzer**      |                   | GPIO8     | SD1        | No        | Internal Flash Connection                   | Push-Pull        |
| **Display**     |                   |           |            |           |                                             |                  |
| CS              |                   | D8        | GPIO15     | No        | Must be low at boot to enter flash mode     | Push-Pull        |
| SCL             |                   | D5        | GPIO14     | Yes       |                                             | Push-Pull        |
| SDA             |                   | D7        | GPIO13     | Yes       |                                             | Push-Pull        |
| RES             |                   | D4        | GPIO2      | Yes       | Must be high at boot to enter flash mode    | Push-Pull        |
| DC              |                   | D3        | GPIO0      | No        | Can be used to wake up from deep sleep      | Push-Pull        |
*/


#if DISABLE_LOCAL

Ticker Timer1;// Create a Ticker object to manage the timer interrupt

unsigned long currentTime = millis() / 1000; // Convert millis to seconds
int hours = (currentTime / 3600) % 24;  // Extract hours (mod 24 for 24-hour format)
int minutes = (currentTime / 60) % 60;  // Extract minutes
int seconds = currentTime % 60;         // Extract seconds

unsigned long startMillis = millis();  // Store the reference time
bool isTimeManuallySet = false; // Flag to track if time has been manually set
// Function to update time based on elapsed time
void updateTime() {
  // Fetch the current time from the NTP server
  timeClient.update();

  // Get the time in Indian Standard Time (IST)
  hours = timeClient.getHours();
  minutes = timeClient.getMinutes();
  seconds = timeClient.getSeconds();

  // Debug: Print the time
  Serial.print("Indian Time: ");
  Serial.print(hours);
  Serial.print(":");
  Serial.print(minutes);
  Serial.print(":");
  Serial.println(seconds);
}

#if LED_ENABLE
#define LED_PIN_RED  6
#define LED_PIN_YELLOW  10
#define LED_PIN_GREEN  11
#define LED_ZERO_BRIGHTNESS  12

// Define LED struct
struct LED {
    int pin;           // LED pin number
    int brightness;    // LED brightness (0-255)
    bool increasing;   // Fade direction (true = increasing, false = decreasing)
    bool active;       // LED enabled or disabled
};

// Define LED objects with correct pins
LED leds[] = {
    {LED_PIN_RED, LED_ZERO_BRIGHTNESS, true, false},  // LED_PIN_RED
    {LED_PIN_YELLOW, LED_ZERO_BRIGHTNESS, true, false}, // LED_PIN_YELLOW
    {LED_PIN_GREEN, LED_ZERO_BRIGHTNESS, true, false}  // LED_PIN_GREEN
};

// Define LED colors
typedef enum {
  RED, // 0
  YELLOW, // 1
  GREEN // 2
} LEDColor;

// Define number of LEDs
const int numLEDs = sizeof(leds) / sizeof(LED);  

// Timer-based LED brightness update
void updateLEDs() {
  for (int i = 0; i < numLEDs; i++) {
    if (leds[i].active) {  // If LED is active, update brightness
      if (leds[i].increasing) leds[i].brightness += 10;
      else leds[i].brightness -= 10;

      // Reverse direction at limits
      if (leds[i].brightness >= 255) leds[i].increasing = false;
      if (leds[i].brightness <= 0) leds[i].increasing = true;

      analogWrite(leds[i].pin, leds[i].brightness);
      delay(10);  // Delay for LED fade effect
    } else {
      //analogWrite(leds[i].pin, 0); // Turn off inactive LEDs
    }
  }
}

// Function to enable or disable LED fading
void controlLED(LEDColor ledIndex, bool enable) {
  if (ledIndex >= 0 && ledIndex < numLEDs) {
    leds[ledIndex].active = enable;
  }
  updateLEDs();
}
#endif

// Define pin for the buzzer
#if ENABLE_BUZZER 
const int buzzerPin = 9; // Buzzer connected to pin 5
#endif

// Display and character height details
int menuScrollOffset = 0;  // Variable to control the scrolling offset
#define MENU_ITEM_HEIGHT 20  // Height for each menu item
#define VISIBLE_MENU_ITEMS 5 // Number of items visible on menu screen at a time
int start_index = 0;   // Menu start position

// OLED Display Setup
#define SCREEN_WIDTH 128 // OLED display width, in pixels 
#define SCREEN_HEIGHT 160 // OLED display height, in pixels
#undef ST7735_TFTHEIGHT_160  // for 1.8" and mini display
#define ST7735_TFTHEIGHT_160 162 // for 1.8" and mini display
#undef ST7735_TFTWIDTH_128   // for 1.44 and mini
#define ST7735_TFTWIDTH_128 132  // for 1.44 and mini

#define OLED_RESET -1 
#define display_CS         15  // CS pin
#define display_RST        0  // RESET pin
#define display_DC         2  // DC pin
#define display_SDA        D7  // SDA (MOSI) pin
#define display_SCL        D5  // SCL (SCK) pin

#endif

// Create a derived class to access protected members
class MyST7735 : public Adafruit_ST7735 { // Create a derived class to access protected members
public: // Public access
  MyST7735(int8_t cs, int8_t dc, int8_t rst) : Adafruit_ST7735(cs, dc, rst) {}// Constructor

  void setColRowStartOffset(int8_t col, int8_t row) {// Set the column and row start offset
    setColRowStart(col, row); // Call the protected function
  }  
};

MyST7735 display = MyST7735(display_CS, display_DC, display_RST);

#if BUTTON_ENABLE
// Button Pins
//#define BTN_UP D1 // Up button
//#define BTN_SELECT D2 // Select button
//#define BTN_BACK D3 // Back button
#define BTN_UP D1 // Digital pin for joystick down
#define BTN_SELECT D2 // Digital pin for joystick press
#define BTN_BACK D3 // Digital pin for joystick left
#endif
// Fan Status
volatile int FAN_STATUS = 0;
// SET ,RESET and FAN control macros 
#define SET 1 
#define RESET 0
#define FAN_ON SET // Fan ON
#define FAN_OFF RESET // Fan OFF
// Menu States
typedef enum { HOME, SETTINGS, CHART, TRENDS, ALERTS, DIAGNOSTICS, ABOUT, SETTINGS_EDIT } Menu;
Menu currentMenu = HOME;

// Logos
#define FRAME_DELAY (42)  // Delay between frames
#define FRAME_WIDTH (48)  // Width of each frame
#define FRAME_HEIGHT (48) // Height of each frame
#define FRAME_COUNT_HOME (sizeof(frames_home) / sizeof(frames_home[0]))

// Settings Variables
float highTempAlert = 40.0;  // Upper temperature limit
float lowTempAlert = 20.0;   // Lower temperature limit
String mode = "Auto";
int editIndex = 0;           // Index for editing settings

// Temperature Data
float internalTemp = 25.0;   // Simulated temperature
float offsetTemp = 40.0;      // Offset temperature
float tempHistory[20] = { 25.0 };  // Last 20 temperature readings
int peakCrossCount = 0;      // Count of times temperature crossed peak
float peakTemp = 0.0;        // Peak temperature
float lowerTemp = 100.0;     // Lower temperature

 
// Function Prototypes
void drawMenu();  // Draw the main menu
#if BUTTON_ENABLE==1
void drawHome();  // Draw the home screen
void drawSettings();    // Draw the settings screen
void drawSettingsEdit(); // Draw the settings edit screen
void drawChart();  // Draw the chart screen
void drawTrends();  // Draw the trends screen
void drawAlerts();  // Draw the alerts screen
void drawDiagnostics();  // Draw the diagnostics screen
void drawAbout();  // Draw the about screen
void updateChartData(); // Update temperature history and peak/lower temperatures
void readTemperature(); // Read temperature from the thermistor
void handleUp(); // Handle the up button press
void handleSelect(); // Handle the select button press
void handleBack(); // Handle the back button press
#if ENABLE_BUZZER
void buzzer_alarm(); // Buzzer alarm sound
void playerPaddleTone(); // Buzzer sound for player paddle
#endif
#if LED_ENABLE
void controlLED(LEDColor ledIndex, bool enable); // Control LED brightness
#endif
void Tool_bar(Menu choice);   // Tool bar for the menu
void button_debouce_delay();  // Debounce delay for buttons
void checkSerialForOTAUpdate(); // Check for OTA update

void button_debouce_delay() { // Debounce delay for buttons
  delay(1000);
}

#if ENABLE_BUZZER
void buzzer_alarm() { // Buzzer alarm sound
    delay(25);
    noTone(5);
}

void buzzer_sciFiStartup() {  // Sci-Fi startup sound
    int buzzerPin = 5; // Define the buzzer pin

    tone(buzzerPin, 660, 100); delay(150); // E
    tone(buzzerPin, 660, 100); delay(300); // E
    tone(buzzerPin, 660, 100); delay(300); // E
    tone(buzzerPin, 510, 100); delay(150); // C
    tone(buzzerPin, 660, 100); delay(300); // E
    tone(buzzerPin, 770, 100); delay(550); // G
    tone(buzzerPin, 380, 100); delay(575); // Low G
    
    noTone(buzzerPin);
}

void buzzer_buttonClick() { // Button click sound
    int buzzerPin = 5; // Define the buzzer pin

    tone(buzzerPin, 200, 30); // Low "click" tone
    delay(30);
    tone(buzzerPin, 300, 20); // Slightly higher tone for feedback
    delay(20);
    
    noTone(buzzerPin); // Stop the buzzer
}

void buzzer_alert(float temperature) { // Buzzer alert based on temperature
    int buzzerPin = 5; // Define the buzzer pin

    // Map temperature (45°C to 60°C) to rate (1 to 3)
    int rate = 1 + ((temperature - 45) / (60 - 45)) * (3 - 1);
    
    int duration = 200 * rate; // Scale duration
    tone(buzzerPin, 220, 200); // Fixed frequency, variable duration
    delay(50);
    noTone(buzzerPin); // Stop the buzzer
    delay(200);
}

void buzzer_back() {
    int buzzerPin = 5; // Define the buzzer pin

    tone(buzzerPin, 800, 50); // Start with a higher beep
    delay(60);
    tone(buzzerPin, 600, 50); // Drop to a lower beep
    delay(60);

    noTone(buzzerPin); // Stop the buzzer
}

void buzzer_select() {
    int buzzerPin = 5; // Define the buzzer pin

    tone(buzzerPin, 600, 50); // Quick beep
    delay(60);
    tone(buzzerPin, 800, 50); // Slightly higher beep
    delay(60);

    noTone(buzzerPin); // Stop the buzzer
}
#endif

#if BUTTON_ENABLE
void handleUp() {
  Serial.println("Up button pressed");
#if ENABLE_BUZZER
  buzzer_buttonClick();
#endif
  if (currentMenu == SETTINGS_EDIT) {
    // Edit settings (upper/lower temp or time)
    switch (editIndex) {
      case 0:  // Upper temperature limit
        highTempAlert += 1.0;
        if (highTempAlert > 60.0) highTempAlert = 40.0;  // Roll over at 60°C
        break;
      case 1:  // Lower temperature limit
        lowTempAlert -= 1.0;
        if (lowTempAlert < 25.0) lowTempAlert = 35.0;    // Roll over at 25°C
        break;
      case 2:  // Time
        currentTime += 1;
        if (currentTime > 235959) currentTime = 0;       // Roll over at 23:59:59
        break;
    }
    drawSettingsEdit();  // Redraw the settings edit screen
  } else if (currentMenu == HOME || currentMenu == SETTINGS || currentMenu == CHART || 
             currentMenu == TRENDS || currentMenu == ALERTS || currentMenu == DIAGNOSTICS || 
             currentMenu == ABOUT) {
    currentMenu = static_cast<Menu>((currentMenu + 1) % 7);
    currentMenu = static_cast<Menu>(currentMenu % 7);
    drawMenu();  // Redraw the menu after scrolling
  }
}

void handleSelect() {
  Serial.println("Select button pressed");
#if ENABLE_BUZZER
  buzzer_select();
#endif
  button_debouce_delay();
  // Transition to the selected menu item
  switch (currentMenu) {
    case HOME:
      drawHome();
      break;
    case SETTINGS:
      currentMenu = SETTINGS_EDIT;
      drawSettingsEdit();
      break;
    case CHART:
      drawChart();
      break;
    case TRENDS:
      drawTrends();
      break;
    case ALERTS:
      drawAlerts();
      break;
    case DIAGNOSTICS:
      drawDiagnostics();
      break;
    case ABOUT:
      drawAbout();
      break;
    default:
      drawMenu();
      break;
  } 
}

void handleBack() {
  Serial.println("Back button pressed");
#if ENABLE_BUZZER
  buzzer_back();
#endif
  if (currentMenu != HOME) {
    // Return to home menu
    currentMenu = HOME;
    start_index = 0;
    drawMenu();
  }
}
#endif

#if DISABLE_LOCAL
void interrupt_Handler() {
  // Update time
  //updateTime();
  // Read Temperature
  readTemperature();
  // Read Chart buffer
  //updateChartData();
  #if LED_ENABLE
  // Update LED's
  updateLEDs();
  #endif 
}

void readTemperature() {
  const float BETA = 3950; // BETA coefficient of the thermistor
  const float R0 = 10000;  // Resistance of the thermistor at 25°C (10k ohms)
  const float T0 = 298.15; // Reference temperature in Kelvin (25°C)

  int sensorValue = analogRead(A0);  // Read from analog pin A0
  if (sensorValue == 0) {
    Serial.println("Error: Sensor value is 0, check the sensor connection.");
    return; // Avoid division by zero
  }

  float voltage = sensorValue * (3.3 / 1023.0); // Convert ADC value to voltage
  float R = R0 * (3.3 / voltage - 1); // Calculate the resistance of the thermistor

  // Calculate temperature in Kelvin using the Steinhart-Hart equation
  float temperatureK = 1 / (1 / T0 + log(R / R0) / BETA);
  // Convert temperature from Kelvin to Celsius
  internalTemp = temperatureK - 273.15;
  // Temperature offset correction
  internalTemp += offsetTemp;

  Serial.print("Temperature: "); // Print temperature to serial monitor
  Serial.println(internalTemp);  // Print temperature to serial monitor

  if (internalTemp >= peakTemp) {
    peakTemp = internalTemp;
  } 

  if (internalTemp < lowerTemp) {
    lowerTemp = internalTemp; 
  }

  if (internalTemp > highTempAlert) {
    peakCrossCount++;
#if LED_ENABLE
    // Turn on over temperature status led
    digitalWrite(LED_PIN_RED, HIGH);
#endif
    // Update Fan status
    FAN_STATUS = FAN_ON;
    // Calculate rate dynamically based on temperature deviation
    int rate = 1 + (internalTemp - highTempAlert) / 5; // Adjust scaling factor as needed
    rate = constrain(rate, 1, 5); // Keep within a reasonable range
    // Send buzzer alert
#if ENABLE_BUZZER
    buzzer_alert(internalTemp);
#endif
  } else {
#if LED_ENABLE
    // Turn off over temperature status led
    digitalWrite(LED_PIN_RED, LOW);
#endif
    // Update Fan status
    FAN_STATUS = FAN_OFF;
  }

  if (internalTemp < highTempAlert && internalTemp > lowTempAlert) {
#if LED_ENABLE
    // Turn on fan status led
    digitalWrite(LED_PIN_YELLOW, HIGH);
#endif
  } else {
#if LED_ENABLE
    // Turn off fan status led
    digitalWrite(LED_PIN_YELLOW, LOW);
#endif
  }
}

void setTheme() {
  // Set background color (e.g., orange)
  uint16_t backgroundColor = ST77XX_ORANGE; // Orange color
  display.fillScreen(backgroundColor);      // Fill the entire screen with the background color

  // Set text color to white
  display.setTextColor(ST7735_WHITE);

  // Set the custom font
  display.setFont(&TomThumb);

  // Set text size to 10 for the default size
  display.setTextSize(20);
}

// Update temperature history and track peak/lower temperatures
void updateChartData() {
  for (int i = 0; i < 19; i++) {
    tempHistory[i] = tempHistory[i + 1];
  }
  tempHistory[19] = internalTemp;

  // Track peak and lower temperatures
  if (internalTemp > peakTemp) peakTemp = internalTemp;
  if (internalTemp < lowerTemp) lowerTemp = internalTemp;

  // Count how many times temperature crossed the peak
  if (internalTemp > highTempAlert) peakCrossCount++;
}

unsigned long previousMillis = 0; // Store the last time the screen was updated
const long interval = 1000;        // Update interval (1 second)

unsigned long currentMillis; // Variable to store current millis

void drawHome() {
  static float lastTemp = -1000.0; // Store the last temperature to detect changes
  static int lastHours = -1, lastMinutes = -1, lastSeconds = -1; // Store the last time to detect changes

  // Initial screen setup
  setTheme();
  //display.drawRect(0, 0, display.width(), display.height(), ST7735_WHITE);
  display.setTextSize(2);  // Larger text size for the title
  display.setCursor(10, 10);
  display.println(F("Home"));
  display.setTextSize(1);
  display.setCursor(10, 40);  // Move down a bit for temperature info
  display.print(F("Temperature: "));
  display.println(internalTemp);

  while (digitalRead(BTN_BACK) == HIGH) {
    // Update time
    updateTime();
    // Read Temperature
    readTemperature();
    bool updateDisplay = false;

    // Check if temperature has changed
    if (internalTemp != lastTemp) {
      // Set text color to orange
      display.setTextColor(ST7735_ORANGE);
      display.print(F("Temperature: "));
      display.println(lastTemp); // Clear any remaining characters
      lastTemp = internalTemp;
      updateDisplay = true;
    }

    // Check if time has changed
    if (hours != lastHours || minutes != lastMinutes || seconds != lastSeconds) {
      
      // Clear previous time value
      display.setCursor(10, 60);
      // Set text color to orange
      display.setTextColor(ST7735_ORANGE);
      // Write OLD time value to erase time
      display.setCursor(10, 60);  // Adjust the position
      display.print(F("Time: "));
      display.print(lastHours);
      display.print(F(":"));
      if (minutes < 10) display.print(F("0"));
      display.print(lastMinutes);
      display.print(F(":"));
      if (seconds < 10) display.print(F("0"));
      display.print(lastSeconds);

      lastHours = hours;
      lastMinutes = minutes;
      lastSeconds = seconds;
      updateDisplay = true;
    }

    if (updateDisplay) {
      // Write new temperature value
      display.setCursor(10, 40);
      // Set text color to white
      display.setTextColor(ST7735_WHITE);
      display.print(F("Temperature: "));
      display.print(internalTemp);

      // Write new time value
      display.setCursor(10, 60);  // Adjust the position
      // Set text color to white
      display.setTextColor(ST7735_WHITE);
      display.print(F("Time: "));
      display.print(hours);
      display.print(F(":"));
      if (minutes < 10) display.print(F("0"));
      display.print(minutes);
      display.print(F(":"));
      if (seconds < 10) display.print(F("0"));
      display.print(seconds);
    }
    delay(100); // Small delay to reduce CPU usage
  }
  drawMenu();
}

// Draw Settings Screen
void drawSettingsEdit() {
  bool inEditMode = false;  // Flag to track if we're in edit mode
  int editValue = 0;        // Temporary variable to hold the value being edited
  int editIndex = 0;        // Tracks the selected setting (0: High Temp, 1: Low Temp, 2: Time)
  int timeEditIndex = 0;    // 0 = Hours, 1 = Minutes, 2 = Seconds (for time editing)
  // Update time values
  updateTime();
  int local_hours = hours;
  int local_minutes = minutes;
  int local_seconds = seconds;

  while (true) {
    // Set the theme
    setTheme();

    display.setTextSize(1);
    display.setTextColor(ST7735_WHITE);
    display.setCursor(10, 10);
    display.println(F("Edit Settings"));

    // Display the settings options
    int i = 0;
    for (; i < 3; i++) {
      display.setCursor(10, 30 + (i * MENU_ITEM_HEIGHT));

      // Highlight selected option
      if (i == editIndex && !inEditMode) {
        display.print(F("> "));
      } else {
        display.print(F("  "));
      }

      // Display setting name and value
      switch (i) {
        case 0:
          display.print(F("High Temp: "));
          display.print(i == editIndex && inEditMode ? editValue : highTempAlert);
          break;
        case 1:
          display.print(F("Low Temp: "));
          display.print(i == editIndex && inEditMode ? editValue : lowTempAlert);
          break;
        case 2: {
          if (!inEditMode) {
            display.print(F("Time: "));
            display.print(hours);
            display.print(F(":"));
            if (minutes < 10) display.print(F("0"));
            display.print(minutes);
            display.print(F(":"));
            if (seconds < 10) display.print(F("0"));
            display.println(seconds);
          } else {
            display.print(F("Time: "));
            display.print(local_hours);
            display.print(F(":"));
            if (local_minutes < 10) display.print(F("0"));
            display.print(local_minutes);
            display.print(F(":"));
            if (local_seconds < 10) display.print(F("0"));
            display.println(local_seconds);

            // Display `_` indicator under the currently edited time field
            if (inEditMode && editIndex == 2) {
              int xOffset = (timeEditIndex == 0) ? 56 : (timeEditIndex == 1) ? 75 : 95;
              display.setCursor(xOffset, 53);
              display.print(F("_"));
            }
          }
          break;
        }
      }
    }

    display.setCursor(10, 100);  // Position instructions below the menu
    display.print(inEditMode ? F("<DOWN>  <SET>   <UP>") : F("<BACK> <SELECT> <UP>"));

    // Button handling
    if (digitalRead(BTN_UP) == LOW) {
      if (inEditMode) {
        if (editIndex == 2) {
          // Editing time fields
          if (timeEditIndex == 0) local_hours = (local_hours + 1) % 24;  // Hours (0-23)
          else if (timeEditIndex == 1) local_minutes = (local_minutes + 1) % 60;  // Minutes (0-59)
          else local_seconds = (local_seconds + 1) % 60;  // Seconds (0-59)
        } else {
          editValue++;  // Increment other values
        }
      } else {
        editIndex = (editIndex - 1 + 3) % 3;  // Move up in menu
      }
      delay(200);
    }

    if (digitalRead(BTN_BACK) == LOW) {
      if (inEditMode) {
        if (editIndex == 2) {
          // Editing time fields
          if (timeEditIndex == 0) local_hours = (local_hours - 1 + 24) % 24;
          else if (timeEditIndex == 1) local_minutes = (local_minutes - 1 + 60) % 60;
          else local_seconds = (local_seconds - 1 + 60) % 60;
        } else {
          editValue--;  // Decrement other values
        }
      } else {
        break;  // Exit the loop and return to the main menu
      }
      delay(200);
    }

    if (digitalRead(BTN_SELECT) == LOW) {
      if (inEditMode) {
        if (editIndex == 2) {
          // Cycle between HH, MM, and SS when editing time
          timeEditIndex++;
          if (timeEditIndex > 2) { // End editing time after setting seconds
            inEditMode = false;
            hours = local_hours;
            minutes = local_minutes;
            seconds = local_seconds;
            timeEditIndex = 0;

            // Update startMillis to reflect the new manually set time
            unsigned long totalSeconds = (hours * 3600) + (minutes * 60) + (seconds * 1);
            startMillis = millis() - (totalSeconds * 1000); // Adjust startMillis
            isTimeManuallySet = true; // Set the flag to indicate time has been manually set
          }
        } else {
          // Save edited values and exit edit mode
          if (editIndex == 0) highTempAlert = editValue;
          if (editIndex == 1) lowTempAlert = editValue;
          inEditMode = false;
        }
      } else {
        // Enter edit mode for selected setting
        inEditMode = true;
        if (editIndex == 0) editValue = highTempAlert;
        else if (editIndex == 1) editValue = lowTempAlert;
        else {
          // Initialize time editing
          timeEditIndex = 0;
          editValue = currentTime / 10000;
        }
      }
      delay(200);
    }

    // Exit if BACK is pressed and not in edit mode
    if (digitalRead(BTN_BACK) == LOW && !inEditMode) {
      break;
    }

    yield(); // Allow the system to reset the watchdog timer
  }

  currentMenu = HOME;
  drawMenu();  // Return to main menu
}

// Draw Chart Screen
void drawChart() {
  while (digitalRead(BTN_BACK) == HIGH) {
    unsigned long startTime = millis();  // Record the start time for the X-axis
    float timeStamps[20] = {0};         // Array to store timestamps for each temperature reading

    while (digitalRead(BTN_BACK) == HIGH) {  // Exit when BACK button is pressed
      // Update timestamps
      for (int i = 0; i < 19; i++) {
        timeStamps[i] = timeStamps[i + 1];  // Shift timestamps to the left
      }
      timeStamps[19] = (millis() - startTime) / 1000.0;  // Add new timestamp in seconds

      // Calculate dynamic Y-axis limits
      float minTemp = 100.0, maxTemp = -100.0;
      for (int i = 0; i < 20; i++) {
        if (tempHistory[i] < minTemp) minTemp = tempHistory[i];
        if (tempHistory[i] > maxTemp) maxTemp = tempHistory[i];
      }
      // Add some padding to the Y-axis limits
      minTemp = minTemp - 2.0;
      maxTemp = maxTemp + 2.0;

      // Set the theme
      setTheme();

      display.setTextSize(1);
      display.setTextColor(ST7735_WHITE);
      display.setCursor(10, 10);
      display.println(F("Chart"));

      // Draw Y-axis (Temperature) with labels and unit
      display.setCursor(2, 30);
      display.print(maxTemp, 1);  // Maximum temperature (1 decimal place)
      display.setCursor(2, 70);
      display.print((maxTemp + minTemp) / 2, 1);  // Middle temperature
      display.setCursor(2, 110);
      display.print(minTemp, 1);  // Minimum temperature

      // Draw X-axis (Time) with steps of 5 seconds
      for (int i = 0; i <= 20; i += 5) {
        int x = 10 + i * 5;  // X position (time)
        display.setCursor(x, 140);
        display.println(i);  // Time label (0s, 5s, 10s, 15s, 20s)
      }

      // Draw X and Y axis lines
      display.drawLine(27, 27, 27, 118, ST7735_WHITE);  // Y-axis line
      display.drawLine(27, 110, 150, 110, ST7735_WHITE);  // X-axis line

      // Draw the temperature graph
      for (int i = 0; i < 19; i++) {
        int x1 = 27 + i * 5;  // X position (time)
        int y1 = map(tempHistory[i], minTemp, maxTemp, 118, 27);  // Y position (temperature)
        int x2 = 27 + (i + 1) * 5;  // Next X position (time)
        int y2 = map(tempHistory[i + 1], minTemp, maxTemp, 118, 27);  // Next Y position (temperature)

        #if PLOT_STYLE_LINE
        display.drawLine(x1, y1, x2, y2, ST7735_WHITE);  // Draw line between points
        #else
        display.drawPixel(x1, y1, ST7735_RED);  // Draw data point
        #endif
      }

      // Draw the legend "C" at the top right corner
      display.setCursor(160 - 10, 10);  // 10 pixels from the top and right edges
      display.print(F("C"));

      delay(100);  // Small delay to control update rate
      yield(); // Allow the system to reset the watchdog timer
    }
  }
  drawMenu();
}

// Draw Trends Screen
void drawTrends() {
  while (digitalRead(BTN_BACK) == HIGH) {
    // Set the theme
    setTheme();

    display.setTextSize(1);
    display.setTextColor(ST7735_WHITE);
    display.setCursor(10, 10);
    display.println(F("Trends"));
    display.setCursor(10, 30);
    display.print(F("Peak Temp: "));
    display.println(peakTemp);
    display.setCursor(10, 50);
    display.print(F("Lower Temp: "));
    display.println(lowerTemp);
    display.setCursor(10, 70);
    display.print(F("Peak Crosses: "));
    display.println(peakCrossCount);
  }
  drawMenu();
}

// Draw Alerts Screen
void drawAlerts() {
  while (digitalRead(BTN_BACK) == HIGH) {
    // Set the theme
    setTheme();

    display.setTextSize(1);
    display.setCursor(10, 0);
    display.println(F("Alerts"));
    if (internalTemp > highTempAlert) {
      display.println(F("High Temp Alert!"));
    } else {
      display.println(F("No Alerts"));
    }
  }
  drawMenu();
}

void drawDiagnostics() {
  int fanState = 0;  // 0 = \, 1 = /, 2 = |, 3 = -
  while (digitalRead(BTN_BACK) == HIGH) {
    // Set the theme
    setTheme();

    display.setTextSize(1);
    display.setTextColor(ST7735_WHITE);
    display.setCursor(10, 10);
    display.println(F("Diagnostics"));
    display.println(F(" "));
    display.println(F("-> Power: ON"));
    display.print(F("-> Blower:"));
    
    // Fan rotation logic
    if (FAN_STATUS == SET) {
      display.print(F(" ON"));
      switch(fanState) {
        case 0:
          display.print(F(" \\"));
          break;
        case 1:
          display.print(F(" /"));
          break;
        case 2:
          display.print(F(" |"));
          break;
        case 3:
          display.print(F(" -"));
          break;
      }
      fanState = (fanState + 1) % 4;  // Loop through 0, 1, 2, 3
      display.println("");
    } else {
      display.println(F(" OFF"));
    }
    display.print(F("-> Over Temp: "));
    
    // Fan rotation logic
    if (FAN_STATUS == SET) {
      display.print(F(" True"));
    } else {
      display.println(F(" False"));
    }

    delay(1000);  // Delay for rotation effect
  }
  drawMenu();
}

void Tool_bar(Menu choice) {
  // Set Cursor to last line of screen
  display.setCursor(0, 10 + (((VISIBLE_MENU_ITEMS - 1)) * MENU_ITEM_HEIGHT));  
  switch (choice) {
    case 1:
      // Home
      display.print(F("<BACK>"));
      break;
    case 2:
      // settings
      display.print(F("<BACK>"));
      break;
    case 3:
      // Chart
      break;
    case 4:
      // trends
      display.print(F("<BACK>"));
      break;
    case 5:
      // alert
      display.print(F("<BACK>"));
      break;
    case 6:
      // diagnostics    
      display.print(F("<BACK>          <UP>"));
      break;
    case 7:
      // about
      display.print(F("<BACK>"));
      break;
  }
}

// Draw About Screen
void drawAbout() {
  // Set the theme
  setTheme();

  display.setTextSize(1);
  display.setTextColor(ST7735_WHITE);
  display.setCursor(10, 10);
  display.println(F("About"));
  display.setCursor(10, 30);
  display.println(F("Device: Temp Monitor"));
  display.setCursor(10, 50);
  display.println(F("Developer: Afsal Lais"));
  display.setCursor(10, 70);
  display.println(F("Version: 1.0.3"));
  display.setCursor(10, 90);
  while (digitalRead(BTN_BACK) == HIGH) 
  {
    Tool_bar(ABOUT);
    delay(100);
  }    
  drawMenu();
}
#endif
#endif


// Self-test function to check hardware components

void selfTest() {
  Serial.println("Starting self-test...");

  // Test LEDs
#if LED_ENABLE
  Serial.println("Testing LEDs...");
  for (int i = 0; i < numLEDs; i++) {
    Serial.print("Testing LED ");
    Serial.println(i);
    pinMode(leds[i].pin, OUTPUT);
    Serial.print("Configured LED pin: ");
    Serial.println(leds[i].pin);
    digitalWrite(leds[i].pin, HIGH);
    delay(500);
    digitalWrite(leds[i].pin, LOW);
    delay(500);
  }
  Serial.println("LED test completed.");
#endif

  // Test Buttons
#if BUTTON_ENABLE
  Serial.println("Testing buttons...");
  pinMode(BTN_UP, INPUT);
  Serial.println("BTN_UP configured.");
  delay(10);

  pinMode(BTN_SELECT, INPUT);
  Serial.println("BTN_SELECT configured.");
  delay(10);

  pinMode(BTN_BACK, INPUT);
  Serial.println("BTN_BACK configured.");
  delay(10);

  if (digitalRead(BTN_UP) == LOW) {
    Serial.println("BTN_UP is pressed.");
  }
  if (digitalRead(BTN_SELECT) == LOW) {
    Serial.println("BTN_SELECT is pressed.");
  }
  if (digitalRead(BTN_BACK) == LOW) {
    Serial.println("BTN_BACK is pressed.");
  }
  Serial.println("Button test completed.");
#endif

  // Test Buzzer
#if ENABLE_BUZZER
  Serial.println("Testing buzzer...");
  int buzzerPin = 5; // Define the buzzer pin
  tone(buzzerPin, 1000, 500); // 1kHz tone for 500ms
  delay(500);
  noTone(buzzerPin);
  Serial.println("Buzzer test completed.");
  delay(10);
#endif

  // Test Analog Read
  Serial.println("Testing analog read...");
  int sensorValue = analogRead(A0);
  Serial.print("Analog read value: ");
  Serial.println(sensorValue);
  Serial.println("Analog read test completed.");
  delay(10);
  Serial.print("Free Heap: ");  
  Serial.println(ESP.getFreeHeap());
  // Test Display
  Serial.println("Testing display...");
  display.initR(INITR_BLACKTAB);      // Init ST7735S chip, black tab
  // Set the rotation of the display
  display.setRotation(3);
  display.fillScreen(ST77XX_BLACK);
  display.setColRowStartOffset(4, 2); // Use the derived class method
  Serial.println("Display initialization completed.");

  display.fillScreen(0xFD20);
  delay(10);
  display.setTextSize(1);
  display.setTextColor(ST77XX_WHITE);
  display.setCursor(10, 10);
  display.println("Display Test");
  delay(10);
  Serial.println("Display test completed.");

  Serial.println("Self-test completed.");
}

void connectToWiFi() {
  int attempt = 3;
 
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  // Wait for the connection to establish
  while (WiFi.status() != WL_CONNECTED && attempt > 0) {
    delay(500);
    Serial.print(".");
    attempt--;
  }
  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected.");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    // Initialize NTP Client
    timeClient.begin();
  } else {
    Serial.println("Failed to connect to WiFi.");
  }
}


void setup() {
  Serial.setDebugOutput(true);
  Serial.begin(115200);
  Serial.println("System initialization started.");
  SPI.begin();  // Initialize SPI
  SPI.setFrequency(40000000);  // Set SPI speed to 80 MHz

  // Connect to WiFi
  connectToWiFi();

  // Run self-test
  selfTest();

  display.setColRowStartOffset(4, 2); // Use the derived class method
  Serial.println("Display setup completed.");

#if ENABLE_BUZZER
  //pinMode(buzzerPin, OUTPUT); // Set buzzer pin as out
  //Serial.println("Buzzer setup completed.");
#endif
  // Initialize NTP Client
  timeClient.begin();
  if (timeClient.update()) { // Update time if necessary
    // Get time in IST
    timeClient.setTimeOffset(19800); // IST is UTC+5:30 Hrs.
    timeClient.update();
    Serial.print("Current Time: ");
    Serial.print(timeClient.getFormattedTime());
    Serial.println(" (IST)");
  } else {
    Serial.println("Failed to update time");
  }

#if BUTTON_ENABLE
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  Serial.println("Button pins configured.");
#endif

#if LED_ENABLE
  // Set LED pins as outputs
  for (int i = 0; i < numLEDs; i++) {
    pinMode(leds[i].pin, OUTPUT);
  }
  Serial.println("LED pins configured.");
#endif

#if ENABLE_BUZZER
  buzzer_sciFiStartup();
  Serial.println("System startup sound played.");
#endif

  // Attach the timerISR function to run every 1000 milliseconds
  Timer1.attach_ms(1000, interrupt_Handler);
  Serial.println("Timer interrupt attached.");

#if LED_ENABLE
  // Turn on power status led
  controlLED(GREEN, true);  // Start GREEN LED fade (pin 11)

  // Turn on fan status led
  digitalWrite(LED_PIN_YELLOW, HIGH);
#endif

  // Set the theme
  setTheme();

  // Draw menu
  drawMenu();
  Serial.println("System initialization completed.");
}

void drawMenu() {
    int visibleMenuItems = VISIBLE_MENU_ITEMS; // Number of items visible at a time
    const char* menuItems[7] = { "Home", "Settings", "Chart", "Trends", "Alerts", "Diagnostics", "About" };

    // Adjust start_index based on currentMenu
    if ((int)currentMenu >= (start_index + visibleMenuItems)) {
        start_index++;
    } else if ((int)currentMenu < start_index) {
        start_index = 0;
    }

    // Ensure start_index stays within bounds
    if (start_index < 0) {
        start_index = 0;
    } else if (start_index > 7) {
        start_index = 0;
    } else if (start_index + visibleMenuItems > 7) {
        start_index = 7 - visibleMenuItems;
    }

    display.fillScreen(ST77XX_ORANGE); // Fill the entire screen with the background color
    display.setTextSize(1);
    display.setTextColor(ST7735_WHITE);
    display.setCursor(10, 10);
    display.print("MENU:");

    int i = start_index;
    for (int cursor_index = 0; cursor_index < visibleMenuItems; cursor_index++, i++) {
        if (i >= 0 && i < 7) { // Ensure index is within bounds
            display.setCursor(10, 30 + (cursor_index * MENU_ITEM_HEIGHT));

            if (i == (int)currentMenu) {
                display.print("> "); // Highlight the current menu item
            } else {
                display.print("  ");
            }

            display.print(menuItems[i]); // Use char* menu items
        }
    }
}

void loop() {
  delay(10);
  // Update the time from the NTP server
  updateTime();

  
  static unsigned long lastUpdate = 0;
  unsigned long currentMillis = millis();

  // Read temperature every second
  if (currentMillis - lastUpdate > 1000) {
    lastUpdate = currentMillis;
    readTemperature(); // Ensure temperature is read periodically
  }

#if BUTTON_ENABLE
  // Handle button presses
  if (digitalRead(BTN_UP) == LOW) {
     handleUp(); delay(200); 
  }
  if (digitalRead(BTN_SELECT) == LOW) {
     handleSelect(); 
     delay(200);
  }
  if (digitalRead(BTN_BACK) == LOW) {
    handleBack();
    delay(200); 
  }
#endif
  
  checkSerialForOTAUpdate();
}

void enterOTAUpdateMode() {
  unsigned long startMillis = millis();
  unsigned long currentMillis;
  int countdown = 30;

  while (countdown > 0) {
    currentMillis = millis();
    if (currentMillis - startMillis >= 1000) {
      startMillis = currentMillis;
      countdown--;

      // Clear the display
      display.fillScreen(ST7735_BLACK);

      // Display OTA update message
      display.setTextSize(2);
      display.setTextColor(ST7735_WHITE);
      display.setCursor(10, 10);
      display.println(F("OTA Update"));

      // Display countdown timer
      display.setTextSize(1);
      display.setCursor(10, 50);
      display.print(F("Time remaining: "));
      display.print(countdown);
      display.println(F("s"));

      //display.display(); // Update the display
    }

    delay(100); // Reduce delay to 100ms
    yield(); // Allow the system to reset the watchdog timer
  }

  // After countdown, you can add code to start OTA update if needed
  // For example:
  // ArduinoOTA.handle();
}

void checkSerialForOTAUpdate() {
  /*if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Remove any leading/trailing whitespace

    if (command.equalsIgnoreCase("OTA_UPDATE")) {
      Serial.println("Entering OTA update mode...");
      enterOTAUpdateMode();
    }
  }*/
 if(digitalRead(BTN_UP) == LOW && digitalRead(BTN_SELECT) == LOW && digitalRead(BTN_BACK) == LOW) {
    Serial.println("Entering OTA update mode...");
    enterOTAUpdateMode();
  }
}
