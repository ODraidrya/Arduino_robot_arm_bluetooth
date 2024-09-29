#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <SoftwareSerial.h>

// Constants and definitions
const int numServos = 6; // Number of servos
const int maxConfigurations = 10; // Maximum number of storable poses
const int stepDelay = 10; // Delay between each step to slow down the servo movement
const int stepSize = 1; // The number of degrees to move per step

// Servo channels on the PCA9685
const int servoChannels[numServos] = {0, 4, 8, 9, 12, 13};

// Storage structures
int savedConfigurations[maxConfigurations][numServos];
int currentServoPositions[numServos] = {375, 375, 375, 375, 375, 375}; // Set default position to middle (around 90 degrees)
int configCount = 0; // Counter for stored poses
bool isPlaying = false; // Status indicating if poses are being played
bool loopPlayback = false; // Status indicating if poses should be played in a loop
bool stopPlaying = false; // Status indicating if playback should be stopped
int currentPoseIndex = 0; // Index of the current pose during playback
bool initialized = false; // Flag to check if initialization happened

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
SoftwareSerial BTSerial(10, 11); // RX, TX for Bluetooth communication

void setup() {
  Serial.begin(9600);         // Start serial communication for the monitor
  BTSerial.begin(9600);       // Start Bluetooth serial communication
  pwm.begin();
  pwm.setPWMFreq(60);         // Set frequency to 60 Hz for the servos

  // Move servos to initial positions (middle position) to avoid any jumps
  for (int i = 0; i < numServos; i++) {
    pwm.setPWM(servoChannels[i], 0, currentServoPositions[i]);
  }

  initialized = true; // Set initialized to true once the servos are moved to the middle position

  Serial.println("Bluetooth Servo Controller started. Waiting for commands...");
}

void loop() {
  if (BTSerial.available()) {
    String input = BTSerial.readStringUntil('\n'); // Read the entire line until a newline character
    input.trim(); // Remove unwanted spaces and newline characters
    
    // Output the received data to the serial monitor
    Serial.println("Received data: " + input);

    if (input.equalsIgnoreCase("S")) {
      saveCurrentPose();
    } else if (input.equalsIgnoreCase("P")) {
      startPlayingPoses();
    } else if (input.equalsIgnoreCase("R")) {
      resetPoses();
    } else if (input.equalsIgnoreCase("St")) {
      stopPlayingPoses();
    } else if (input.equalsIgnoreCase("LoopON")) { // When the switch is turned ON
      loopPlayback = true;
      Serial.println("Loop playback enabled.");
    } else if (input.equalsIgnoreCase("LoopOFF")) { // When the switch is turned OFF
      loopPlayback = false;
      Serial.println("Loop playback disabled.");
    } else if (input.indexOf(',') > 0) {
      processLastValue(input);
    } else {
      Serial.println("Unknown command: " + input);
    }
  }

  if (isPlaying && !stopPlaying) {
    if (loopPlayback) {
      playPosesInLoop();
    } else {
      playNextPose();
    }
  }
}

// Function to process the last value in the line
void processLastValue(String command) {
    int lastCommaIndex = command.lastIndexOf(','); // Find the last comma in the line
    if (lastCommaIndex > 0 && lastCommaIndex < command.length() - 1) {
        int servoIndex = command.substring(0, lastCommaIndex).toInt() - 1; // First value: servo index
        float lastValue = command.substring(lastCommaIndex + 1).toFloat(); // Last value in the line

        // Directly use the last value
        int targetPos = (int)lastValue;

        // Map the position from 0-180 to PWM values from 150-600 (may vary depending on the servo)
        int targetPwmValue = map(targetPos, 0, 180, 150, 600);

        // Smooth movement to target position
        moveToPositionSmoothly(servoIndex, targetPwmValue);
    } else {
        Serial.println("Invalid command for servo control: " + command);
    }
}

// Function to move servo smoothly to the target position
void moveToPositionSmoothly(int servoIndex, int targetPwmValue) {
    int currentPwmValue = currentServoPositions[servoIndex];
    
    if (targetPwmValue > currentPwmValue) {
        for (int pos = currentPwmValue; pos <= targetPwmValue; pos += stepSize) {
            pwm.setPWM(servoChannels[servoIndex], 0, pos);
            delay(stepDelay);
        }
    } else {
        for (int pos = currentPwmValue; pos >= targetPwmValue; pos -= stepSize) {
            pwm.setPWM(servoChannels[servoIndex], 0, pos);
            delay(stepDelay);
        }
    }

    currentServoPositions[servoIndex] = targetPwmValue; // Update current position
    Serial.println("Servo " + String(servoIndex + 1) + " set to PWM value: " + String(targetPwmValue));
}

// Function to save the current servo positions as a pose
void saveCurrentPose() {
    if (configCount < maxConfigurations) {
        for (int i = 0; i < numServos; i++) {
            savedConfigurations[configCount][i] = currentServoPositions[i];
            Serial.print("Servo "); Serial.print(i + 1);
            Serial.print(" saved at PWM value: ");
            Serial.println(savedConfigurations[configCount][i]);
        }
        configCount++;
        Serial.println("Pose saved. Total saved poses: " + String(configCount));
    } else {
        Serial.println("Memory full, cannot save more poses.");
    }
}

// Function to start playing the saved poses
void startPlayingPoses() {
    if (configCount == 0) {
        Serial.println("No poses saved, nothing to play.");
        return;
    }

    Serial.println("Playback of saved poses started...");
    isPlaying = true;
    stopPlaying = false;
    currentPoseIndex = 0;
}

// Function to play the next pose
void playNextPose() {
    if (currentPoseIndex < configCount) {
        Serial.println("Playing pose " + String(currentPoseIndex + 1));
        for (int i = 0; i < numServos; i++) {
            moveToPositionSmoothly(i, savedConfigurations[currentPoseIndex][i]);
        }
        delay(1000); // Standard delay between poses
        currentPoseIndex++;
    } else {
        isPlaying = false;
        Serial.println("Playback of poses finished.");
    }
}

// Function to play the poses in a loop
void playPosesInLoop() {
    for (int j = 0; j < configCount; j++) {
        Serial.println("Playing pose " + String(j + 1));
        for (int i = 0; i < numServos; i++) {
            moveToPositionSmoothly(i, savedConfigurations[j][i]);
        }
        delay(1000); // Standard delay between poses
        if (stopPlaying) break;
    }
}

// Function to stop playback
void stopPlayingPoses() {
    stopPlaying = true;
    isPlaying = false;
    Serial.println("Playback of poses stopped.");
}

// Function to reset all saved poses
void resetPoses() {
    configCount = 0;
    isPlaying = false;
    loopPlayback = false;
    currentPoseIndex = 0;
    Serial.println("All saved poses have been deleted.");
}
