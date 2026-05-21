#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <time.h>
#include <queue>

#define FIREBASE_HOST   "medichine-29fff-default-rtdb.asia-southeast1.firebasedatabase.app"
#define DATABASE_SECRET "x5ksx5aMAjc7BoIHULEgSJz12oghKopVQBUeyJJy"

#define IR1_PIN 32
#define IR2_PIN 33
#define IR3_PIN 26
#define IR4_PIN 19
#define US_TRIG 18
#define US_ECHO 5
#define RED_LED_PIN    15
#define YELLOW_LED_PIN 2
#define GREEN_LED_PIN  4

#define DFPLAYER_RX_PIN 16
#define DFPLAYER_TX_PIN 17

#define CONTINUOUS_SERVO_1_PIN 13
#define CONTINUOUS_SERVO_2_PIN 12
#define CONTINUOUS_SERVO_3_PIN 14
#define CONTINUOUS_SERVO_4_PIN 27

#define POSITIONAL_SERVO_1_PIN 23
#define POSITIONAL_SERVO_2_PIN 22
#define POSITIONAL_SERVO_3_PIN 21
#define POSITIONAL_SERVO_4_PIN 25

#define SERVO_STOP 1500
#define SERVO_SLOW 1400
#define SERVO_FAST 1600

#define AUDIO_READY 1
#define AUDIO_DISPENSING 2
#define AUDIO_DISPENSED 3
#define AUDIO_TAKEMED 4
#define AUDIO_COMPLETED 5

#define DFPLAYER_START_BYTE 0x7E
#define DFPLAYER_VERSION 0xFF
#define DFPLAYER_LENGTH 0x06
#define DFPLAYER_END_BYTE 0xEF

#define DFPLAYER_CMD_PLAY 0x03
#define DFPLAYER_CMD_SET_VOLUME 0x06
#define DFPLAYER_CMD_STOP 0x16

// FAST & FLEXIBLE ULTRASONIC SYSTEM
float emptyDrawerHeight = 0;     
float withPillsHeight = 0;       
float drawerTakenHeight = 0;     
bool isCalibrated = false;       
bool pillsDispensed = false;     
bool drawerTakenCalibrated = false; 
bool waitingForPillSettle = false;  
unsigned long pillDispenseTime = 0; 
int drawerChangeCount = 0;       

// FAST CONFIGURATION - Reduced samples for speed
float PILL_SETTLE_TIME = 2.0;  // Reduced from 3.0
int CALIBRATION_SAMPLES = 5;   // Reduced from 15
int READING_SAMPLES = 3;       // Reduced from 7
float DRAWER_TAKEN_THRESHOLD = 3.0;
float DRAWER_RETURN_THRESHOLD = 1.5;  // More sensitive
int MAX_DRAWER_CHANGES = 10;

Servo continuousServos[4];
Servo positionalServos[4];

bool audioEnabled = false;
bool isPlaying = false;
bool drawerTaken = false;
bool isDispensing = false;
bool drawerMonitoring = true; 
unsigned long lastDrawerCheck = 0;
unsigned long lastReminderTime = 0;
unsigned long yellowBlinkTime = 0;
bool yellowBlinkState = false;

struct Command {
  int containerId;
  int quantity;
  String medicineName;
  String commandKey;
};

std::queue<Command> commandQueue;
bool processingCommands = false;

struct DispensedMedicine {
  int containerId;
  String medicineName;
  int quantity;
  String scheduledTime;
  String scheduledDays;
};

DispensedMedicine drawerContents[50]; 
int drawerMedicineCount = 0;

void sendDFPlayerCommand(byte command, int parameter);

// FAST ULTRASONIC READING - Only 3 samples, quick median
float readDistanceCM() {
  float readings[READING_SAMPLES];
  int validCount = 0;
  
  for (int i = 0; i < READING_SAMPLES; i++) {
    digitalWrite(US_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(US_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(US_TRIG, LOW);
    
    float duration = pulseIn(US_ECHO, HIGH, 30000);  // Reduced timeout
    if (duration > 0) {
      readings[validCount] = duration * 0.034 / 2;
      validCount++;
    }
    delay(10);  // Reduced from 15
  }
  
  if (validCount == 0) {
    Serial.println("SENSOR ERROR: No valid readings");
    return -1;
  }
  
  // Quick sort for median
  for (int i = 0; i < validCount - 1; i++) {
    for (int j = 0; j < validCount - i - 1; j++) {
      if (readings[j] > readings[j + 1]) {
        float temp = readings[j];
        readings[j] = readings[j + 1];
        readings[j + 1] = temp;
      }
    }
  }
  
  return readings[validCount / 2];  // Return median
}

void calibrateDrawerHeight() {
    Serial.println("=== FAST CALIBRATION ===");
    float totalDistance = 0;
    int validReadings = 0;
    
    Serial.println("Taking " + String(CALIBRATION_SAMPLES) + " quick readings...");
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        float distance = readDistanceCM();
        if (distance > 0) {
            totalDistance += distance;
            validReadings++;
            Serial.println("Reading " + String(i+1) + ": " + String(distance, 2) + "cm");
        }
        delay(50);  // Reduced delay
    }
    
    if (validReadings >= CALIBRATION_SAMPLES * 0.6) {  // More lenient
        emptyDrawerHeight = totalDistance / validReadings;
        isCalibrated = true;
        
        Serial.println("CALIBRATION SUCCESS:");
        Serial.println("- Empty drawer: " + String(emptyDrawerHeight, 2) + "cm");
    } else {
        Serial.println("CALIBRATION FAILED: Only " + String(validReadings) + "/" + String(CALIBRATION_SAMPLES) + " valid readings");
        isCalibrated = false;
    }
    Serial.println("=======================");
}

void setupDFPlayer() {
  Serial.println("=== DFPlayer Hardware Debug ===");
  Serial.println("Expected connections:");
  Serial.println("ESP32 Pin 17 -> DFPlayer RX");
  Serial.println("ESP32 Pin 16 -> DFPlayer TX"); 
  Serial.println("ESP32 5V -> DFPlayer VCC");
  Serial.println("ESP32 GND -> DFPlayer GND");
  Serial.println("Speaker -> DFPlayer SPK_1 and SPK_2");
  Serial.println("");
  
  Serial2.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  delay(3000);
  
  Serial.println("Sending DFPlayer commands...");
  
  byte testCmd[] = {0x7E, 0xFF, 0x06, 0x06, 0x00, 0x00, 0x1E, 0xFF, 0xE1, 0xEF};
  
  for(int i = 0; i < 10; i++) {
    Serial2.write(testCmd[i]);
    Serial.print("0x");
    Serial.print(testCmd[i], HEX);
    Serial.print(" ");
  }
  Serial.println("");
  
  delay(1000);
  
  Serial.println("Checking for DFPlayer response...");
  int responseCount = 0;
  unsigned long startTime = millis();
  while(millis() - startTime < 2000 && responseCount < 10) {
    if(Serial2.available()) {
      byte response = Serial2.read();
      Serial.print("Response byte: 0x");
      Serial.println(response, HEX);
      responseCount++;
    }
    delay(10);
  }
  
  if(responseCount == 0) {
    Serial.println("ERROR: No response from DFPlayer!");
    Serial.println("Check wiring and power connections");
    audioEnabled = false;
    return;
  }
  
  Serial.println("DFPlayer responding - trying play command...");
  
  byte playCmd[] = {0x7E, 0xFF, 0x06, 0x03, 0x00, 0x00, 0x01, 0xFF, 0xF7, 0xEF};
  for(int i = 0; i < 10; i++) {
    Serial2.write(playCmd[i]);
  }
  
  Serial.println("Play command sent. You should hear audio now.");
  Serial.println("If no audio, check speaker connections and SD card");
  
  delay(3000);
  
  byte stopCmd[] = {0x7E, 0xFF, 0x06, 0x16, 0x00, 0x00, 0x00, 0xFF, 0xDB, 0xEF};
  for(int i = 0; i < 10; i++) {
    Serial2.write(stopCmd[i]);
  }
  
  audioEnabled = true;
}

void sendDFPlayerCommand(byte command, int parameter) {
  byte buffer[10] = {0};
  buffer[0] = DFPLAYER_START_BYTE;
  buffer[1] = DFPLAYER_VERSION;
  buffer[2] = DFPLAYER_LENGTH;
  buffer[3] = command;
  buffer[4] = 0x00;
  buffer[5] = (byte)(parameter >> 8);
  buffer[6] = (byte)(parameter);
  
  int checksum = -(buffer[1] + buffer[2] + buffer[3] + buffer[4] + buffer[5] + buffer[6]);
  buffer[7] = (byte)(checksum >> 8);
  buffer[8] = (byte)(checksum);
  buffer[9] = DFPLAYER_END_BYTE;
  
  for (int i = 0; i < 10; i++) {
    Serial2.write(buffer[i]);
  }
  delay(100);
}

void stopAudio() {
  if (isPlaying && audioEnabled) {
    sendDFPlayerCommand(DFPLAYER_CMD_STOP, 0);
    isPlaying = false;
    Serial.println("Audio stopped");
  }
}

void playAudio(int trackNumber) {
  if (!audioEnabled) {
    Serial.println("Audio not enabled - check DFPlayer Mini");
    return;
  }

  stopAudio();
  Serial.println("Playing audio track: " + String(trackNumber));
  
  sendDFPlayerCommand(DFPLAYER_CMD_PLAY, trackNumber);
  isPlaying = true;
  
  if (trackNumber == AUDIO_TAKEMED) {
    Serial.println("Track 004 (TAKEMED) - can be interrupted");
  } else {
    Serial.println("Track " + String(trackNumber) + " - playing");
  }
  
  delay(500);
}

void turnAllOff() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void showStartup() {
  Serial.println("Showing startup sequence");
  turnAllOff();
  digitalWrite(RED_LED_PIN, HIGH);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(500);
  turnAllOff();
  digitalWrite(RED_LED_PIN, HIGH);
  delay(300);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(YELLOW_LED_PIN, HIGH);
  delay(300);
  digitalWrite(YELLOW_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(300);
  turnAllOff();
}

void showStandby() {
  Serial.println("Showing standby (RED ON)");
  turnAllOff();
  digitalWrite(RED_LED_PIN, HIGH);
}

void showDispensed() {
  Serial.println("Medicine dispensed (YELLOW STEADY)");
  turnAllOff();
  digitalWrite(YELLOW_LED_PIN, HIGH);
}

void showTaken() {
  Serial.println("Medicine taken (GREEN ON)");
  turnAllOff();
  digitalWrite(GREEN_LED_PIN, HIGH);
}

void getCurrentTimeAndDay(String &schedTime, String &schedDays) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    schedTime = "";
    schedDays = "";
    Serial.println("Failed to get local time");
    return;
  }
  char timeBuf[9];
  strftime(timeBuf, sizeof(timeBuf), "%I:%M %p", &timeinfo);
  schedTime = String(timeBuf);
  char dayBuf[10];
  strftime(dayBuf, sizeof(dayBuf), "%A", &timeinfo);
  schedDays = String(dayBuf);
  schedDays.toLowerCase();
}

void logMedicineTaken(const String& medicineName, int containerId, int quantity,
                      const String& scheduledTime, const String& scheduledDays) {
  struct tm timeinfo;
  String actualTimeTaken = "";
  if (getLocalTime(&timeinfo)) {
      char buf[30];
      strftime(buf, sizeof(buf), "%I:%M %p", &timeinfo);
      actualTimeTaken = String(buf);
  }

  Serial.println("Logging medicine taken: " + medicineName + " from container " + String(containerId));
  HTTPClient http;
  http.begin("https://Werniverse-medichine.hf.space/push_history");
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<512> doc;
  doc["medicine_name"] = medicineName;
  doc["container_id"] = containerId;
  doc["quantity"] = quantity;
  doc["scheduled_time"] = scheduledTime;
  doc["scheduled_days"] = scheduledDays;
  doc["datetime_taken"] = actualTimeTaken;
  doc["time_taken"] = actualTimeTaken;
  String payload;
  serializeJson(doc, payload);
  http.POST(payload);
  http.end();
  Serial.println("Medicine log sent to server");
}

void addToDrawerContents(int containerId, const String& medicineName, int quantity,
                        const String& scheduledTime, const String& scheduledDays) {
    if (drawerMedicineCount < 50) {
        drawerContents[drawerMedicineCount].containerId = containerId;
        drawerContents[drawerMedicineCount].medicineName = medicineName;
        drawerContents[drawerMedicineCount].quantity = quantity;
        drawerContents[drawerMedicineCount].scheduledTime = scheduledTime;
        drawerContents[drawerMedicineCount].scheduledDays = scheduledDays;
        drawerMedicineCount++;
        Serial.println("Added to drawer: " + medicineName + " (Total: " + String(drawerMedicineCount) + ")");
    }
}

void clearDrawerContents() {
    drawerMedicineCount = 0;
    Serial.println("Drawer contents cleared");
}

void activatePositionalServo(int containerId) {
    Serial.println("Activating positional servo for container " + String(containerId));
    
    positionalServos[containerId - 1].attach(containerId == 1 ? POSITIONAL_SERVO_1_PIN :
                                           containerId == 2 ? POSITIONAL_SERVO_2_PIN :
                                           containerId == 3 ? POSITIONAL_SERVO_3_PIN :
                                           POSITIONAL_SERVO_4_PIN);
    delay(50);
    
    positionalServos[containerId - 1].write(180);
    delay(1000); 
    
    Serial.println("Returning positional servo to 90 degrees");
    positionalServos[containerId - 1].write(90);
    delay(500);
    
    positionalServos[containerId - 1].detach();
    Serial.println("Positional servo detached");

    Serial.println("Playing dispensed audio");
    delay(200); 
    playAudio(AUDIO_DISPENSED);
    delay(1500); 
}

bool detectPillFast(int irPin) {
    bool irState1 = digitalRead(irPin);
    delayMicroseconds(100); 
    bool irState2 = digitalRead(irPin);
    delayMicroseconds(100);
    bool irState3 = digitalRead(irPin);

    int lowCount = 0;
    if (irState1 == LOW) lowCount++;
    if (irState2 == LOW) lowCount++;
    if (irState3 == LOW) lowCount++;
    
    return lowCount >= 2;
}

// ADD THESE VARIABLES AT THE TOP WITH OTHER GLOBAL VARIABLES:
float lastDrawerReading = 0;
int stableReadingCount = 0;

// REPLACE YOUR checkDrawerStatus() FUNCTION WITH THIS:
void checkDrawerStatus() {
    if (!drawerMonitoring || isDispensing) return;
    
    if (millis() - lastDrawerCheck < 100) return;
    lastDrawerCheck = millis();
    
    if (!isCalibrated) {
        Serial.println("SYSTEM NOT CALIBRATED");
        return;
    }
    
    float current = readDistanceCM();
    
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint >= 2000) {
        Serial.println("=== DRAWER STATUS ===");
        Serial.println("Current: " + String(current, 2) + "cm");
        Serial.println("Empty: " + String(emptyDrawerHeight, 2) + "cm");
        if (withPillsHeight > 0) {
            Serial.println("With pills: " + String(withPillsHeight, 2) + "cm");
        }
        if (drawerTakenHeight > 0) {
            Serial.println("Drawer taken ref: " + String(drawerTakenHeight, 2) + "cm");
        }
        Serial.println("States: Pills=" + String(pillsDispensed) + " Taken=" + String(drawerTaken));
        Serial.println("====================");
        lastPrint = millis();
    }
    
    if (current <= 0) {
        Serial.println("ERROR: Invalid ultrasonic reading");
        return;
    }
    
    // PILL SETTLING
    if (drawerMedicineCount > 0 && !waitingForPillSettle && !pillsDispensed) {
        waitingForPillSettle = true;
        pillDispenseTime = millis();
        Serial.println("Pills dispensed - waiting " + String(PILL_SETTLE_TIME, 1) + "s to settle...");
        return;
    }
    
    if (waitingForPillSettle) {
        if (millis() - pillDispenseTime >= PILL_SETTLE_TIME * 1000) {
            withPillsHeight = current;
            waitingForPillSettle = false;
            pillsDispensed = true;
            drawerChangeCount = 0;
            
            Serial.println("*** Pills settled at: " + String(withPillsHeight, 2) + "cm ***");
            Serial.println("*** Pill thickness: " + String((emptyDrawerHeight - withPillsHeight), 2) + "cm ***");
        }
        return;
    }
    
    // DRAWER TAKEN DETECTION - Option 3: Rate of change
    if (pillsDispensed && !drawerTaken) {
        float threshold = emptyDrawerHeight + 1.0;
        float changeRate = abs(current - lastDrawerReading);
        
        Serial.println("Checking drawer removal: current=" + String(current, 2) + 
                      " empty=" + String(emptyDrawerHeight, 2) +
                      " threshold=" + String(threshold, 2) + 
                      " changeRate=" + String(changeRate, 2) +
                      " stableCount=" + String(stableReadingCount));
        
        // Reading must be above threshold AND stable (not changing rapidly)
        if (current > threshold && changeRate < 0.5) {
            stableReadingCount++;
            Serial.println("Stable reading " + String(stableReadingCount) + "/5");
            
            if (stableReadingCount >= 5) {  // 5 stable readings = truly removed
                if (isPlaying) {
                    Serial.println(">>> DRAWER REMOVED - STOPPING AUDIO <<<");
                    stopAudio();
                }
                
                drawerTaken = true;
                drawerChangeCount++;
                stableReadingCount = 0;  // Reset for next cycle
                
                if (!drawerTakenCalibrated) {
                    drawerTakenHeight = current;
                    drawerTakenCalibrated = true;
                    Serial.println("*** DRAWER TAKEN - Calibrated: " + String(drawerTakenHeight, 2) + "cm ***");
                } else {
                    Serial.println("*** DRAWER TAKEN: " + String(current, 2) + "cm ***");
                }
                
                yellowBlinkState = true;
                yellowBlinkTime = millis();
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(YELLOW_LED_PIN, HIGH);
                Serial.println("*** YELLOW BLINKING STARTED ***");
            }
        } else {
            // Reset if not stable or below threshold
            if (stableReadingCount > 0) {
                Serial.println("Unstable reading - resetting count (was " + String(stableReadingCount) + ")");
            }
            stableReadingCount = 0;
        }
        
        lastDrawerReading = current;
    }
    
    // CONTINUOUS YELLOW BLINK while drawer is out
    if (drawerTaken && pillsDispensed) {
        float lowerBound = emptyDrawerHeight + 0.7;
        
        if (current > lowerBound) {
            if (millis() - yellowBlinkTime >= 300) {
                yellowBlinkState = !yellowBlinkState;
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(YELLOW_LED_PIN, yellowBlinkState ? HIGH : LOW);
                yellowBlinkTime = millis();
            }
            return;
        }
    }
    
    // DRAWER RETURN DETECTION - SIMPLIFIED LOGIC!
    if (drawerTaken && pillsDispensed) {
        float upperBound = emptyDrawerHeight + 0.5;
        
        Serial.println("Checking drawer return: current=" + String(current, 2) + 
                      " upperBound=" + String(upperBound, 2) +
                      " diff=" + String(current - emptyDrawerHeight, 2));
        
        if (current <= upperBound) {
            drawerTaken = false;
            drawerChangeCount++;
            Serial.println("*** DRAWER RETURNED: " + String(current, 2) + "cm (Change #" + String(drawerChangeCount) + ") ***");
            
            // SAFETY FALLBACK
            if (drawerChangeCount >= MAX_DRAWER_CHANGES) {
                Serial.println(">>> MAX CHANGES - AUTO COMPLETE <<<");
                
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, HIGH);
                digitalWrite(YELLOW_LED_PIN, LOW);
                
                for (int i = 0; i < drawerMedicineCount; i++) {
                    logMedicineTaken(drawerContents[i].medicineName, 
                                   drawerContents[i].containerId, 
                                   drawerContents[i].quantity,
                                   drawerContents[i].scheduledTime, 
                                   drawerContents[i].scheduledDays);
                    delay(50);
                }
                
                clearDrawerContents();
                pillsDispensed = false;
                withPillsHeight = 0;
                drawerTakenHeight = 0;
                drawerTakenCalibrated = false;
                drawerChangeCount = 0;
                
                delay(2000);
                playAudio(AUDIO_COMPLETED);
                calibrateDrawerHeight();
                
                digitalWrite(RED_LED_PIN, HIGH);
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(YELLOW_LED_PIN, LOW);
                return;
            }
            
            // SIMPLE RULE: Is reading close to empty drawer height?
            // If current < empty - 0.02, there's something in drawer (pills present)
            // If current ≈ empty (within 0.02cm), drawer is empty (pills taken)
            
            float tolerance = 0.02;  // 0.02cm tolerance for "at empty height"
            float diffFromEmpty = abs(current - emptyDrawerHeight);
            
            Serial.println(">>> SIMPLE PILL CHECK <<<");
            Serial.println("Current: " + String(current, 2) + "cm");
            Serial.println("Empty: " + String(emptyDrawerHeight, 2) + "cm");
            Serial.println("Difference: " + String(diffFromEmpty, 2) + "cm");
            Serial.println("Below empty? " + String(current < (emptyDrawerHeight - tolerance)));
            
            // SIMPLE CHECK: Is current reading below empty height?
            if (current < (emptyDrawerHeight - tolerance)) {
                // Reading is BELOW empty = something is in drawer = PILLS PRESENT
                Serial.println(">>> READING BELOW EMPTY - PILLS STILL THERE! (YELLOW) <<<");
                
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(YELLOW_LED_PIN, HIGH);
                
                if (lastReminderTime == 0) {
                    lastReminderTime = millis();
                }
                // Audio will continue via reminder system
                
            } else {
                // Reading is AT or ABOVE empty = drawer is empty = PILLS TAKEN!
                Serial.println(">>> AT EMPTY HEIGHT - PILLS TAKEN! (GREEN) <<<");
                
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, HIGH);
                digitalWrite(YELLOW_LED_PIN, LOW);

                stopAudio();
                lastReminderTime = 0;
                
                for (int i = 0; i < drawerMedicineCount; i++) {
                    logMedicineTaken(drawerContents[i].medicineName, 
                                   drawerContents[i].containerId, 
                                   drawerContents[i].quantity,
                                   drawerContents[i].scheduledTime, 
                                   drawerContents[i].scheduledDays);
                    delay(50);
                }
                
                clearDrawerContents();
                pillsDispensed = false;
                withPillsHeight = 0;
                drawerTakenHeight = 0;
                drawerTakenCalibrated = false;
                drawerChangeCount = 0;
                
                delay(2000);
                playAudio(AUDIO_COMPLETED);
                calibrateDrawerHeight();
                
                digitalWrite(RED_LED_PIN, HIGH);
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(YELLOW_LED_PIN, LOW);
            }
        }
    }
    
    // REMINDER SYSTEM - plays audio when pills not taken
    // Should remind as long as pills are dispensed AND drawer is not currently out
    if (pillsDispensed && !isPlaying && !processingCommands) {
        unsigned long now = millis();
        
        // Initialize reminder timer if not set
        if (lastReminderTime == 0) {
            lastReminderTime = now;
            Serial.println("*** REMINDER TIMER STARTED ***");
        }
        
        // Play reminder every 60 seconds (only when drawer is NOT currently out)
        if (!drawerTaken && (now - lastReminderTime >= 60000)) {
            Serial.println("*** REMINDER: Playing TAKEMED (004) - Pills still in drawer ***");
            playAudio(AUDIO_TAKEMED);
            lastReminderTime = now;
        }
        
        // Log status every 10 seconds for debugging
        static unsigned long lastStatusLog = 0;
        if (now - lastStatusLog >= 10000) {
            Serial.println("REMINDER STATUS: Pills=" + String(pillsDispensed) + 
                          " DrawerOut=" + String(drawerTaken) + 
                          " TimeSinceLastReminder=" + String((now - lastReminderTime) / 1000) + "s");
            lastStatusLog = now;
        }
    }
}

void activateServo(int servoIndex, int id, int irPin, int targetQty, const String& medicineName) {
    String scheduledTime, scheduledDays;
    getCurrentTimeAndDay(scheduledTime, scheduledDays);
    int dispensedCount = 0;
    unsigned long timeout = 8000;
    int attemptCount = 0;
    const int maxAttempts = 3;

    Serial.println("Activating servo for container " + String(id) + ", quantity: " + String(targetQty));
    isDispensing = true;
    
    // INTERRUPT AUDIO 004 when new command starts
    if (isPlaying) {
        Serial.println(">>> NEW COMMAND - INTERRUPTING AUDIO 004 <<<");
        stopAudio();
    }
    
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(YELLOW_LED_PIN, HIGH);
    yellowBlinkState = true;
    yellowBlinkTime = millis();

    Serial.println("Playing dispensing audio");
    delay(100); 
    playAudio(AUDIO_DISPENSING);
    delay(1500); 
    
    while (dispensedCount < targetQty && attemptCount < maxAttempts) {
        Serial.println("Dispensing pill " + String(dispensedCount + 1) + "/" + String(targetQty) + 
                       " (Attempt " + String(attemptCount + 1) + ")");
        
        unsigned long rotationStart = millis();
        bool pillDropped = false;
        
        while (millis() - rotationStart < 8000 && !pillDropped) {
            
            if (millis() - yellowBlinkTime >= 200) { 
                yellowBlinkState = !yellowBlinkState;
                digitalWrite(RED_LED_PIN, LOW);
                digitalWrite(GREEN_LED_PIN, LOW);
                digitalWrite(YELLOW_LED_PIN, yellowBlinkState ? HIGH : LOW);
                yellowBlinkTime = millis();
            }
            
            continuousServos[servoIndex].writeMicroseconds(SERVO_FAST);
            delay(500);
            
            continuousServos[servoIndex].writeMicroseconds(SERVO_STOP);
            delay(50);
            
            if (detectPillFast(irPin)) {
                pillDropped = true;
                dispensedCount++;
                Serial.println("Pill detected! Count: " + String(dispensedCount));
                continuousServos[servoIndex].writeMicroseconds(SERVO_STOP);
                stopAudio(); 
                activatePositionalServo(id);
                attemptCount = 0;
                break;
            }
            
            continuousServos[servoIndex].writeMicroseconds(SERVO_SLOW);
            delay(50);
            
            continuousServos[servoIndex].writeMicroseconds(SERVO_STOP);
            delay(50);
            
            if (detectPillFast(irPin)) {
                pillDropped = true;
                dispensedCount++;
                Serial.println("Pill detected on reverse! Count: " + String(dispensedCount));
                continuousServos[servoIndex].writeMicroseconds(SERVO_STOP);
                stopAudio(); 
                activatePositionalServo(id);
                attemptCount = 0;
                break;
            }
        }

        continuousServos[servoIndex].writeMicroseconds(SERVO_STOP);
        
        if (!pillDropped) {
            attemptCount++;
            Serial.println("No pill detected (Attempt " + String(attemptCount) + ")");
            
            if (attemptCount < maxAttempts) {
                Serial.println("Waiting 2 seconds...");
                unsigned long waitStart = millis();
                bool pillDetectedDuringWait = false;
                
                while (millis() - waitStart < 2000 && !pillDetectedDuringWait) {
                    if (millis() - yellowBlinkTime >= 200) { 
                        yellowBlinkState = !yellowBlinkState;
                        digitalWrite(RED_LED_PIN, LOW);
                        digitalWrite(GREEN_LED_PIN, LOW);
                        digitalWrite(YELLOW_LED_PIN, yellowBlinkState ? HIGH : LOW);
                        yellowBlinkTime = millis();
                    }
                    
                    if (detectPillFast(irPin)) {
                        pillDetectedDuringWait = true;
                        dispensedCount++;
                        Serial.println("Pill detected during wait! Count: " + String(dispensedCount));
                        continuousServos[servoIndex].writeMicroseconds(SERVO_STOP);
                        stopAudio();
                        activatePositionalServo(id);
                        attemptCount = 0;
                        break;
                    }
                    delay(50);
                }
                
                if (pillDetectedDuringWait) {
                    continue;
                }
            } else {
                Serial.println("Max attempts reached - no pill detected");
                stopAudio();
                break;
            }
        } else {
            delay(200);
        }
    }

    if (dispensedCount > 0) {
        Serial.println("Dispensing complete - " + String(dispensedCount) + " pills dispensed");
        addToDrawerContents(id, medicineName, dispensedCount, scheduledTime, scheduledDays);
        
        digitalWrite(RED_LED_PIN, LOW);
        digitalWrite(GREEN_LED_PIN, LOW);
        digitalWrite(YELLOW_LED_PIN, HIGH);
        
        lastReminderTime = millis(); 
        Serial.println("Playing TAKEMED audio");
        delay(200); 
        playAudio(AUDIO_TAKEMED);
        delay(500); 
    } else {
        Serial.println("No pills dispensed for container " + String(id));
    }
    
    isDispensing = false;
}

void processCommandQueue() {
    if (commandQueue.empty() || processingCommands) return;
    
    processingCommands = true;
    Serial.println("Processing command queue. Size: " + String(commandQueue.size()));

    // INTERRUPT AUDIO 004 when processing new commands
    if (isPlaying) {
        Serial.println(">>> NEW COMMAND QUEUE - INTERRUPTING AUDIO 004 <<<");
        stopAudio();
    }
    
    while (!commandQueue.empty()) {
        Command cmd = commandQueue.front();
        commandQueue.pop();
        
        Serial.println("Processing: Container " + String(cmd.containerId) + 
                      ", Qty: " + String(cmd.quantity) + 
                      ", Medicine: " + cmd.medicineName);

        deleteFirebaseCommand(cmd.commandKey);
        
        activateServo(cmd.containerId - 1, cmd.containerId,
            (cmd.containerId == 1 ? IR1_PIN : cmd.containerId == 2 ? IR2_PIN : cmd.containerId == 3 ? IR3_PIN : IR4_PIN),
            cmd.quantity, cmd.medicineName);
        
        delay(500); 
    }
    
    processingCommands = false;
    Serial.println("Command queue complete");
}

void deleteFirebaseCommand(const String& commandKey) {
  HTTPClient del;
  String delUrl = "https://" + String(FIREBASE_HOST) +
                  "/commands/" + commandKey +
                  ".json?auth=" + DATABASE_SECRET;
  del.begin(delUrl);
  int deleteCode = del.sendRequest("DELETE");
  del.end();
  
  if (deleteCode > 0) {
    Serial.println("Command deleted: " + commandKey);
  } else {
    Serial.println("Failed to delete: " + commandKey);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting setup...");

  Serial.println("Initializing DFPlayer Mini...");
  setupDFPlayer();
  
  showStartup();

  continuousServos[0].attach(CONTINUOUS_SERVO_1_PIN);
  continuousServos[1].attach(CONTINUOUS_SERVO_2_PIN);
  continuousServos[2].attach(CONTINUOUS_SERVO_3_PIN);
  continuousServos[3].attach(CONTINUOUS_SERVO_4_PIN);

  for (int i = 0; i < 4; i++) {
    continuousServos[i].writeMicroseconds(SERVO_STOP);
  }

  Serial.println("Servos initialized");
  delay(500);

  WiFiManager wm;
  Serial.println("Starting Wi-Fi autoConnect...");
  wm.autoConnect("MediChine-ESP32");
  Serial.println("Wi-Fi connected");

  configTime(28800, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  int timeAttempts = 0;
  while (!getLocalTime(&timeinfo) && timeAttempts < 10) {
    delay(1000);
    timeAttempts++;
    Serial.println("Waiting for NTP time...");
  }

  pinMode(IR1_PIN, INPUT_PULLUP);
  pinMode(IR2_PIN, INPUT_PULLUP);
  pinMode(IR3_PIN, INPUT_PULLUP);
  pinMode(IR4_PIN, INPUT_PULLUP);
  pinMode(US_TRIG, OUTPUT);
  pinMode(US_ECHO, INPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  drawerMonitoring = true;
  
  Serial.println("Starting FAST calibration...");
  calibrateDrawerHeight();
  
  showStandby();
  
  if (audioEnabled) {
    playAudio(AUDIO_READY);
  }
  
  Serial.println("Setup complete - monitoring started");
}

void loop() {
  checkDrawerStatus();

  processCommandQueue();

  if (!processingCommands) {
    HTTPClient http;
    String url = "https://" + String(FIREBASE_HOST) + "/commands.json?auth=" + DATABASE_SECRET;
    http.begin(url);
    int code = http.GET();

    if (code == HTTP_CODE_OK) {
      String payload = http.getString();
      StaticJsonDocument<4096> doc;
      DeserializationError err = deserializeJson(doc, payload);
      if (!err && doc.is<JsonObject>()) {
        JsonObject obj = doc.as<JsonObject>();
        int newCommands = 0;
        
        for (JsonPair kv : obj) {
          String key = String(kv.key().c_str());
          int container_id = 0;
          int qty = 1;
          String medName = "Unknown";

          if (kv.value().is<JsonObject>()) {
            JsonObject cmdObj = kv.value().as<JsonObject>();
            container_id = cmdObj["container_id"] | 0;
            qty = cmdObj["quantity"] | 1;
            if (cmdObj.containsKey("name")) medName = cmdObj["name"].as<String>();
            else if (cmdObj.containsKey("medicine_name")) medName = cmdObj["medicine_name"].as<String>();
          } else if (kv.value().is<const char*>()) {
            String cmdStr = kv.value().as<const char*>();
            if (cmdStr.startsWith("command")) {
              container_id = cmdStr.substring(7).toInt();
              qty = 1;
              medName = "TestMedicine";
            }
          }

          if (container_id >= 1 && container_id <= 4) {
            Command newCmd;
            newCmd.containerId = container_id;
            newCmd.quantity = qty;
            newCmd.medicineName = medName;
            newCmd.commandKey = key;
            commandQueue.push(newCmd);
            newCommands++;
            Serial.println("Queued: container " + String(container_id) + ", qty " + String(qty));
          }
        }
        
        if (newCommands > 0) {
          Serial.println("Added " + String(newCommands) + " commands to queue");
        }
      }
    }

    http.end();
  }
  
  delay(200);
}