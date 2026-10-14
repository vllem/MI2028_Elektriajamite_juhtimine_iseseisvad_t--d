/*
  userInterface.cpp
  Käsukäsitluse teostus.

  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester   
*/

#include "userInterface.h"

// Kõik olemasolevad käsud massiivis (ehk arrays)
static const struct command commands[] = {
  { "start", cmdStart, "start",         "Start the motor" },
  { "stop",  cmdStop,  "stop",          "Stop the motor" },
  { "speed", cmdSpeed, "speed <0-255>", "Set the speed (or just enter a number)" },
  { "dir",   cmdDir,   "dir <cw|ccw>",  "Set stepper direction (stepper only)" },
  { "steps", cmdSteps, "steps <1-65535>", "Move a number of steps, then stop (stepper only)" },
  { "angle", cmdAngle, "angle <degrees>", "Turn by an angle, negative = counter-clockwise (stepper only)" },
  { "help",  cmdHelp,  "help",          "Show all commands" },
};

static const size_t COMMAND_COUNT = sizeof(commands) / sizeof(commands[0]);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void cmdStart(struct motorFactory &motor, const String &arg) {
  start(motor);
  Serial.print("Motor started at speed: ");
  Serial.println(motor.motionSequence.speed);
}

static void cmdStop(struct motorFactory &motor, const String &arg) {
  stop(motor);
  Serial.println("Motor stopped.");
}

/*
  isNumeric on abifunktsioon, millega saab määrata kas sõne on tegelikult number.
  See funktsioon piirab ka kõik numbrid vaikimisi kolme kohani (maxLen), et hoida ära situatsiooni, kus sisestatakse number nagu
  "99999999999", mis lõppeks täisarvu ületäituvusega (integer overflow).
*/

static bool isNumeric(const String &s, unsigned int maxLen = 3) {
  if (s.length() == 0 || s.length() > maxLen) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i])) return false;
  }
  return true;
}

static void cmdSpeed(struct motorFactory &motor, const String &arg) {
  if (!isNumeric(arg)) {
    Serial.println("Error: Use 'speed <0-255>', e.g. 'speed 150'.");
    return;
  }
  int val = arg.toInt();
  if (val > 255) {
    Serial.println("Error: Speed must be between 0 and 255!");
    return;
  }
  setSpeed(motor, (uint8_t)val);
  if (motor.motionSequence.isRunning) {
    Serial.print("Speed changed: ");
  } else {
    Serial.print("Speed saved (motor is stopped, enter 'start'): ");
  }
  Serial.println(motor.motionSequence.speed);
}

static void cmdDir(struct motorFactory &motor, const String &arg) {
  if (motor.type != MOTOR_STEPPER) {
    Serial.println("Error: 'dir' works only with a stepper motor.");
    return;
  }
  if (arg.equalsIgnoreCase("cw")) {
    setDirection(motor, true);
    Serial.println("Direction: clockwise.");
  } else if (arg.equalsIgnoreCase("ccw")) {
    setDirection(motor, false);
    Serial.println("Direction: counter-clockwise.");
  } else {
    Serial.println("Error: Use 'dir cw' or 'dir ccw'.");
  }
}

static void cmdSteps(struct motorFactory &motor, const String &arg) {
  if (motor.type != MOTOR_STEPPER) {
    Serial.println("Error: 'steps' works only with a stepper motor.");
    return;
  }
  if (!isNumeric(arg, 5) || arg.toInt() < 1 || arg.toInt() > 65535L) {
    Serial.println("Error: Use 'steps <1-65535>', e.g. 'steps 200'.");
    return;
  }
  if (motor.motionSequence.speed == 0) {
    Serial.println("Warning: speed is 0, set it with 'speed' for the motor to move.");
  }
  moveSteps(motor, (uint32_t)arg.toInt());
  Serial.print("Moving steps: ");
  Serial.println(arg);
}

static void cmdAngle(struct motorFactory &motor, const String &arg) {
  if (motor.type != MOTOR_STEPPER) {
    Serial.println("Error: 'angle' works only with a stepper motor.");
    return;
  }
  bool negative = arg.length() > 0 && arg[0] == '-';
  String digits = negative ? arg.substring(1) : arg;
  if (!isNumeric(digits, 4) || digits.toInt() == 0) {
    Serial.println("Error: Use 'angle <degrees>', e.g. 'angle 180' or 'angle -90'.");
    return;
  }
  if (motor.motionSequence.speed == 0) {
    Serial.println("Warning: speed is 0, set it with 'speed' for the motor to move.");
  }
  long deg = digits.toInt();
  moveAngle(motor, negative ? -deg : deg);
  Serial.print("Turning degrees: ");
  Serial.println(negative ? -deg : deg);
}

static void cmdHelp(struct motorFactory &motor, const String &arg) {
  Serial.println("Commands:");
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    Serial.print("  ");
    Serial.print(commands[i].usage);
    Serial.print(" - ");
    Serial.println(commands[i].description);
  }
}

void handleCommand(struct motorFactory &motor, String input) {
  input.trim();
  if (input.length() == 0) return;

  if (isDigit(input[0])) {
    cmdSpeed(motor, input);
    return;
  }

  String name, arg;
  int space = input.indexOf(' ');
  if (space < 0) {
    name = input;
  } else {
    name = input.substring(0, space);
    arg = input.substring(space + 1);
    arg.trim();
  }

  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    if (name.equalsIgnoreCase(commands[i].name)) {
      commands[i].handler(motor, arg);
      return;
    }
  }

  Serial.println("Unknown command! Type 'help' to see available commands.");
}

void pollSerial(struct motorFactory &motor) {
  static String inputLine;

  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      handleCommand(motor, inputLine);
      inputLine = "";
    } else if (c != '\r') {
      inputLine += c;
    }
  }
}
