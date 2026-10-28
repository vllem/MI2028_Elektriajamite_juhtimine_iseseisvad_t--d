/*
  userInterface.h
  Käsukäsitlus: Serial monitorist käskude tõlgendamine ja täitmine.

  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester   
*/

#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include "motorDriver.h"

/*
  Single-header teek (stb-stiilis, https://github.com/nothings/stb): teostuse saamiseks defineeri täpselt ühes failis enne selle faili kaasamist
    #define USERINTERFACE_IMPLEMENTATION
*/

/*
  CommandInterpreter juhib ühte või mitut mootorit Serial-i käskudega.
  Mootorid registreeritakse nimega (add), nii et nende arv pole piiratud makroga ega ühe globaalse objektiga.

  Kasutus:
    start                  - käsk läheb valitud mootorile (esimesena lisatud mootor on vaikimisi valitud)
    use <mootor>           - vali, millist mootorit käsud edaspidi juhivad
    <mootor> <käsk> [arg]  - käsk ühele kindlale mootorile ilma valikut muutmata, nt 'dc speed 200'
    list                   - näita registreeritud mootoreid
*/
class CommandInterpreter {
public:
  static const uint8_t MAX_MOTORS = 4;

  // Registreerib mootori nimega; false, kui nimi on kasutusel või ruum on täis.
  // Nime string peab elama programmi lõpuni (nt sõnaliteraal).
  bool add(const char *name, Motor &motor);

  // handle võtab sisendrea (nt "speed 150" või "dc speed 150") ja käivitab vastava käsu.
  void handle(String input);

  // poll loeb Serial-ist saadaolevad baidid ilma blokeerimata ning
  // käivitab handle'i, kui terve rida (lõpuga '\n') on kokku kogutud.
  void poll();

private:
  typedef void (CommandInterpreter::*Handler)(Motor &motor, const String &arg);

  //  Struct 'Command' paneb paika käskude struktuuri.
  struct Command {
    const char *name;         // Käsunimetus
    Handler handler;          // Meetod, mis täidab käsu
    const char *usage;        // Kuidas kasutada käsku
    const char *description;  // Käsu enda kirjeldus
  };

  struct Entry {
    const char *name;
    Motor *motor;
  };

  static const Command commands[];
  static const size_t COMMAND_COUNT;

  void cmdStart(Motor &motor, const String &arg);
  void cmdStop(Motor &motor, const String &arg);
  void cmdSpeed(Motor &motor, const String &arg);
  void cmdDir(Motor &motor, const String &arg);
  void cmdSteps(Motor &motor, const String &arg);
  void cmdAngle(Motor &motor, const String &arg);
  void cmdUse(Motor &motor, const String &arg);
  void cmdList(Motor &motor, const String &arg);
  void cmdHelp(Motor &motor, const String &arg);

  static bool isNumeric(const String &s, unsigned int maxLen = 3);
  Entry *find(const String &name);
  void execute(const String &name, const String &arg, Motor *target);

  Entry motors[MAX_MOTORS];
  uint8_t motorCount = 0;
  uint8_t selected = 0;
  String inputLine;
};

#ifdef USERINTERFACE_IMPLEMENTATION

// Kõik olemasolevad käsud massiivis (ehk arrays)
const CommandInterpreter::Command CommandInterpreter::commands[] = {
  { "start", &CommandInterpreter::cmdStart, "start",         "Start the motor" },
  { "stop",  &CommandInterpreter::cmdStop,  "stop",          "Stop the motor" },
  { "speed", &CommandInterpreter::cmdSpeed, "speed <0-255>", "Set the speed (or just enter a number)" },
  { "dir",   &CommandInterpreter::cmdDir,   "dir <cw|ccw>",  "Set stepper direction (stepper only)" },
  { "steps", &CommandInterpreter::cmdSteps, "steps <1-65535>", "Move a number of steps, then stop (stepper only)" },
  { "angle", &CommandInterpreter::cmdAngle, "angle <degrees>", "Turn by an angle, negative = counter-clockwise (stepper only)" },
  { "use",   &CommandInterpreter::cmdUse,   "use <motor>",   "Select the motor that the commands control" },
  { "list",  &CommandInterpreter::cmdList,  "list",          "Show all motors" },
  { "help",  &CommandInterpreter::cmdHelp,  "help",          "Show all commands" },
};

const size_t CommandInterpreter::COMMAND_COUNT = sizeof(commands) / sizeof(commands[0]);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CommandInterpreter::cmdStart(Motor &motor, const String &arg) {
  motor.start();
  Serial.print("Motor started at speed: ");
  Serial.println(motor.getSpeed());
}

void CommandInterpreter::cmdStop(Motor &motor, const String &arg) {
  motor.stop();
  Serial.println("Motor stopped.");
}

/*
  isNumeric on abifunktsioon, millega saab määrata kas sõne on tegelikult number.
  See funktsioon piirab ka kõik numbrid vaikimisi kolme kohani (maxLen), et hoida ära situatsiooni, kus sisestatakse number nagu
  "99999999999", mis lõppeks täisarvu ületäituvusega (integer overflow).
*/

bool CommandInterpreter::isNumeric(const String &s, unsigned int maxLen) {
  if (s.length() == 0 || s.length() > maxLen) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    if (!isDigit(s[i])) return false;
  }
  return true;
}

void CommandInterpreter::cmdSpeed(Motor &motor, const String &arg) {
  if (!isNumeric(arg)) {
    Serial.println("Error: Use 'speed <0-255>', e.g. 'speed 150'.");
    return;
  }
  int val = arg.toInt();
  if (val > 255) {
    Serial.println("Error: Speed must be between 0 and 255!");
    return;
  }
  motor.setSpeed((uint8_t)val);
  if (motor.isRunning()) {
    Serial.print("Speed changed: ");
  } else {
    Serial.print("Speed saved (motor is stopped, enter 'start'): ");
  }
  Serial.println(motor.getSpeed());
}

void CommandInterpreter::cmdDir(Motor &motor, const String &arg) {
  StepperMotor *stepper = motor.asStepper();
  if (!stepper) {
    Serial.println("Error: 'dir' works only with a stepper motor.");
    return;
  }
  if (arg.equalsIgnoreCase("cw")) {
    stepper->setDirection(true);
    Serial.println("Direction: clockwise.");
  } else if (arg.equalsIgnoreCase("ccw")) {
    stepper->setDirection(false);
    Serial.println("Direction: counter-clockwise.");
  } else {
    Serial.println("Error: Use 'dir cw' or 'dir ccw'.");
  }
}

void CommandInterpreter::cmdSteps(Motor &motor, const String &arg) {
  StepperMotor *stepper = motor.asStepper();
  if (!stepper) {
    Serial.println("Error: 'steps' works only with a stepper motor.");
    return;
  }
  if (!isNumeric(arg, 5) || arg.toInt() < 1 || arg.toInt() > 65535L) {
    Serial.println("Error: Use 'steps <1-65535>', e.g. 'steps 200'.");
    return;
  }
  if (motor.getSpeed() == 0) {
    Serial.println("Warning: speed is 0, set it with 'speed' for the motor to move.");
  }
  stepper->moveSteps((uint32_t)arg.toInt());
  Serial.print("Moving steps: ");
  Serial.println(arg);
}

void CommandInterpreter::cmdAngle(Motor &motor, const String &arg) {
  StepperMotor *stepper = motor.asStepper();
  if (!stepper) {
    Serial.println("Error: 'angle' works only with a stepper motor.");
    return;
  }
  bool negative = arg.length() > 0 && arg[0] == '-';
  String digits = negative ? arg.substring(1) : arg;
  if (!isNumeric(digits, 4) || digits.toInt() == 0) {
    Serial.println("Error: Use 'angle <degrees>', e.g. 'angle 180' or 'angle -90'.");
    return;
  }
  if (motor.getSpeed() == 0) {
    Serial.println("Warning: speed is 0, set it with 'speed' for the motor to move.");
  }
  long deg = digits.toInt();
  stepper->moveAngle(negative ? -deg : deg);
  Serial.print("Turning degrees: ");
  Serial.println(negative ? -deg : deg);
}

void CommandInterpreter::cmdHelp(Motor &motor, const String &arg) {
  Serial.println("Commands:");
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    Serial.print("  ");
    Serial.print(commands[i].usage);
    Serial.print(" - ");
    Serial.println(commands[i].description);
  }
}

void CommandInterpreter::cmdUse(Motor &motor, const String &arg) {
  Entry *entry = find(arg);
  if (!entry) {
    Serial.println("Error: Unknown motor. Type 'list' to see motors.");
    return;
  }
  selected = entry - motors;
  Serial.print("Selected motor: ");
  Serial.println(entry->name);
}

void CommandInterpreter::cmdList(Motor &motor, const String &arg) {
  Serial.println("Motors:");
  for (uint8_t i = 0; i < motorCount; i++) {
    Serial.print(i == selected ? "* " : "  ");
    Serial.print(motors[i].name);
    Serial.print(motors[i].motor->asStepper() ? " (stepper)" : " (DC)");
    Serial.print(motors[i].motor->isRunning() ? ", running, speed " : ", stopped, speed ");
    Serial.println(motors[i].motor->getSpeed());
  }
}

bool CommandInterpreter::add(const char *name, Motor &motor) {
  if (motorCount >= MAX_MOTORS || find(name)) return false;
  motors[motorCount++] = { name, &motor };
  return true;
}

CommandInterpreter::Entry *CommandInterpreter::find(const String &name) {
  for (uint8_t i = 0; i < motorCount; i++) {
    if (name.equalsIgnoreCase(motors[i].name)) return &motors[i];
  }
  return nullptr;
}

// Otsib käsu nime järgi ja täidab selle 'target' mootoril (nullptr = valitud mootor).
void CommandInterpreter::execute(const String &name, const String &arg, Motor *target) {
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    if (name.equalsIgnoreCase(commands[i].name)) {
      if (!target && motorCount == 0) {
        // Käsufunktsioonid vajavad mootori viidet, seega ilma mootorita käske ei täideta
        Serial.println("Error: No motors registered.");
        return;
      }
      (this->*commands[i].handler)(target ? *target : *motors[selected].motor, arg);
      return;
    }
  }
  Serial.println("Unknown command! Type 'help' to see available commands.");
}

void CommandInterpreter::handle(String input) {
  input.trim();
  if (input.length() == 0) return;

  if (isDigit(input[0])) {
    execute("speed", input, nullptr);
    return;
  }

  // Sisendrida jagatakse sõnadeks: [mootor] käsk [argument]
  String first, rest;
  int space = input.indexOf(' ');
  if (space < 0) {
    first = input;
  } else {
    first = input.substring(0, space);
    rest = input.substring(space + 1);
    rest.trim();
  }

  Entry *entry = find(first);
  if (!entry) {
    execute(first, rest, nullptr);
    return;
  }

  // Esimene sõna oli mootori nimi: järgmine sõna on käsk
  String name, arg;
  space = rest.indexOf(' ');
  if (space < 0) {
    name = rest;
  } else {
    name = rest.substring(0, space);
    arg = rest.substring(space + 1);
    arg.trim();
  }
  if (name.length() == 0) {
    Serial.println("Error: Use '<motor> <command>', e.g. 'dc speed 200'.");
    return;
  }
  execute(name, arg, entry->motor);
}

void CommandInterpreter::poll() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      handle(inputLine);
      inputLine = "";
    } else if (c != '\r') {
      inputLine += c;
    }
  }
}

#endif // USERINTERFACE_IMPLEMENTATION

#endif // COMMANDS_H
