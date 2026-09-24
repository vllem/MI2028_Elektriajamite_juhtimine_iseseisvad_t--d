/*
  commands.cpp
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
  See funktsioon piirab ka kõik numbrid kolme kohani, et hoida ära situatsiooni, kus sisestatakse number nagu
  "99999999999", mis lõppeks täisarvu ületäituvusega (integer overflow).
*/

static bool isNumeric(const String &s) {
  if (s.length() == 0 || s.length() > 3) return false;
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
