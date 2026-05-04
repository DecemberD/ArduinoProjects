#include <Servo.h>

Servo myServo;

const int servoPin = 9;
String inputBuffer = "";

void setup() {
  Serial.begin(9600);
  myServo.attach(servoPin);

  Serial.println("Enter servo position (0-360):");
}

void loop() {
  // Read incoming serial data
  while (Serial.available() > 0) {
    char c = Serial.read();

    // If newline or carriage return, process input
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processInput(inputBuffer);
        inputBuffer = ""; // Clear buffer
      }
    } else {
      inputBuffer += c; // Build input string
    }
  }
}

void processInput(String input) {
  input.trim();

  // Check if input is numeric
  bool isValidNumber = true;
  for (unsigned int i = 0; i < input.length(); i++) {
    if (!isDigit(input[i])) {
      isValidNumber = false;
      break;
    }
  }

  if (!isValidNumber) {
    Serial.println("Error: Input must be a number.");
    return;
  }

  int angle = input.toInt();

  // Validate range 0–360
  if (angle < 0 || angle > 360) {
    Serial.println("Error: Value must be between 0 and 360.");
    return;
  }

  // Map 0–360 to servo range (0–180)
  int servoAngle = map(angle, 0, 360, 0, 180);

  myServo.write(servoAngle);

  Serial.print("OK. Input angle: ");
  Serial.print(angle);
  Serial.print(" → Servo angle: ");
  Serial.println(servoAngle);
}