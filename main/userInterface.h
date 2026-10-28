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
  // Käsumeetod tagastab false, kui argument on vigane: siis teatab execute käsu 'usage' teksti.
  typedef bool (CommandInterpreter::*Handler)(Motor &motor, const String &arg);

  // Mootoritüübid on bitid, et üks käsk saaks töötada mitmel tüübil (nt DC | STEPPER).
  enum Kind : uint8_t { DC = 1, STEPPER = 2, SERVO = 4, ANY = DC | STEPPER | SERVO };

  //  Struct 'Command' paneb paika käskude struktuuri.
  struct Command {
    const char *name;         // Käsunimetus
    Handler handler;          // Meetod, mis täidab käsu
    uint8_t kinds;            // Millistel mootoritüüpidel käsk töötab (Kind bitid); teistel keeldub execute
    const char *usage;        // Kuidas kasutada käsku
    const char *description;  // Käsu enda kirjeldus
  };

  struct Entry {
    const char *name;
    Motor *motor;
  };

  static const Command commands[];
  static const size_t COMMAND_COUNT;

  bool cmdStart(Motor &motor, const String &arg);
  bool cmdStop(Motor &motor, const String &arg);
  bool cmdSpeed(Motor &motor, const String &arg);
  bool cmdDir(Motor &motor, const String &arg);
  bool cmdSteps(Motor &motor, const String &arg);
  bool cmdAngle(Motor &motor, const String &arg);
  bool cmdPos(Motor &motor, const String &arg);
  bool cmdUse(Motor &motor, const String &arg);
  bool cmdList(Motor &motor, const String &arg);
  bool cmdHelp(Motor &motor, const String &arg);

  static bool parseInt(const String &s, long lo, long hi, long &out);
  static void splitWord(const String &line, String &word, String &rest);
  static Kind kindOf(Motor &motor);
  static void printValue(Motor &motor);
  static void reportSet(Motor &motor);
  static void warnIfNoSpeed(Motor &motor);
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
  { "start", &CommandInterpreter::cmdStart, ANY,            "start",           "Start the motor" },
  { "stop",  &CommandInterpreter::cmdStop,  ANY,            "stop",            "Stop the motor" },
  { "speed", &CommandInterpreter::cmdSpeed, DC | STEPPER,   "speed <0-255>",   "Set the speed, or just enter a number (not for servo)" },
  { "dir",   &CommandInterpreter::cmdDir,   STEPPER,        "dir <cw|ccw>",    "Set stepper direction (stepper only)" },
  { "steps", &CommandInterpreter::cmdSteps, STEPPER,        "steps <1-65535>", "Move a number of steps, then stop (stepper only)" },
  { "angle", &CommandInterpreter::cmdAngle, STEPPER,        "angle <degrees>", "Turn by an angle, negative = counter-clockwise (stepper only)" },
  { "pos",   &CommandInterpreter::cmdPos,   SERVO,          "pos <0-180>",     "Move to an angle in degrees (servo only)" },
  { "use",   &CommandInterpreter::cmdUse,   ANY,            "use <motor>",     "Select the motor that the commands control" },
  { "list",  &CommandInterpreter::cmdList,  ANY,            "list",            "Show all motors" },
  { "help",  &CommandInterpreter::cmdHelp,  ANY,            "help",            "Show all commands" },
};

const size_t CommandInterpreter::COMMAND_COUNT = sizeof(commands) / sizeof(commands[0]);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CommandInterpreter::cmdStart(Motor &motor, const String &arg) {
  motor.start();
  Serial.print(F("Started: "));
  printValue(motor);
  Serial.println();
  return true;
}

bool CommandInterpreter::cmdStop(Motor &motor, const String &arg) {
  motor.stop();
  Serial.println(F("Motor stopped."));
  return true;
}

/*
  parseInt kontrollib, et sõne on täisarv vahemikus [lo, hi], ja annab väärtuse tagasi 'out' kaudu.
  See funktsioon piirab ka kõik numbrid viie kohani, et hoida ära situatsiooni, kus sisestatakse number nagu
  "99999999999", mis lõppeks täisarvu ületäituvusega (integer overflow). Miinusmärk on lubatud, kuid vahemik lükkab negatiivsed tagasi, kui lo >= 0.
*/

bool CommandInterpreter::parseInt(const String &s, long lo, long hi, long &out) {
  unsigned int first = (s.length() > 0 && s[0] == '-') ? 1 : 0;
  if (s.length() == first || s.length() - first > 5) return false;
  for (unsigned int i = first; i < s.length(); i++) {
    if (!isDigit(s[i])) return false;
  }
  long val = s.toInt();
  if (val < lo || val > hi) return false;
  out = val;
  return true;
}

// Jagab rea esimeseks sõnaks ja ülejäänud osaks (kärbitud)
void CommandInterpreter::splitWord(const String &line, String &word, String &rest) {
  int space = line.indexOf(' ');
  if (space < 0) {
    word = line;
    rest = "";
  } else {
    word = line.substring(0, space);
    rest = line.substring(space + 1);
    rest.trim();
  }
}

CommandInterpreter::Kind CommandInterpreter::kindOf(Motor &motor) {
  return motor.asStepper() ? STEPPER : motor.asServo() ? SERVO : DC;
}

// Servol on nurk, teistel mootoritel kiirus
void CommandInterpreter::printValue(Motor &motor) {
  if (ServoMotor *servo = motor.asServo()) {
    Serial.print(F("angle "));
    Serial.print(servo->getAngle());
  } else {
    Serial.print(F("speed "));
    Serial.print(motor.getSpeed());
  }
}

// Teatab äsja seatud väärtusest; peatatud mootor võtab selle kasutusele alles 'start' peale
void CommandInterpreter::reportSet(Motor &motor) {
  Serial.print(F("Set: "));
  printValue(motor);
  if (!motor.isRunning()) Serial.print(F(" (motor is stopped, enter 'start')"));
  Serial.println();
}

void CommandInterpreter::warnIfNoSpeed(Motor &motor) {
  if (motor.getSpeed() == 0) {
    Serial.println(F("Warning: speed is 0, set it with 'speed' for the motor to move."));
  }
}

bool CommandInterpreter::cmdSpeed(Motor &motor, const String &arg) {
  long val;
  if (!parseInt(arg, 0, 255, val)) return false;
  motor.setSpeed((uint8_t)val);
  reportSet(motor);
  return true;
}

// Käsud 'dir', 'steps', 'angle' ja 'pos' töötavad ainult oma tüübiga (vt käsutabel), seega asStepper()/asServo() pole siin nullptr.

bool CommandInterpreter::cmdDir(Motor &motor, const String &arg) {
  bool cw = arg.equalsIgnoreCase("cw");
  if (!cw && !arg.equalsIgnoreCase("ccw")) return false;
  motor.asStepper()->setDirection(cw);
  Serial.println(cw ? F("Direction: clockwise.") : F("Direction: counter-clockwise."));
  return true;
}

bool CommandInterpreter::cmdSteps(Motor &motor, const String &arg) {
  long steps;
  if (!parseInt(arg, 1, 65535, steps)) return false;
  warnIfNoSpeed(motor);
  motor.asStepper()->moveSteps(steps);
  Serial.print(F("Moving steps: "));
  Serial.println(steps);
  return true;
}

bool CommandInterpreter::cmdAngle(Motor &motor, const String &arg) {
  long deg;
  if (!parseInt(arg, -9999, 9999, deg) || deg == 0) return false;
  warnIfNoSpeed(motor);
  motor.asStepper()->moveAngle(deg);
  Serial.print(F("Turning degrees: "));
  Serial.println(deg);
  return true;
}

bool CommandInterpreter::cmdPos(Motor &motor, const String &arg) {
  long angle;
  if (!parseInt(arg, 0, SERVO_MAX_ANGLE, angle)) return false;
  motor.asServo()->setAngle((uint8_t)angle);
  reportSet(motor);
  return true;
}

bool CommandInterpreter::cmdHelp(Motor &motor, const String &arg) {
  Serial.println(F("Commands:"));
  for (size_t i = 0; i < COMMAND_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(commands[i].usage);
    Serial.print(F(" - "));
    Serial.println(commands[i].description);
  }
  return true;
}

bool CommandInterpreter::cmdUse(Motor &motor, const String &arg) {
  Entry *entry = find(arg);
  if (!entry) {
    Serial.println(F("Error: Unknown motor. Type 'list' to see motors."));
    return true;
  }
  selected = entry - motors;
  Serial.print(F("Selected motor: "));
  Serial.println(entry->name);
  return true;
}

bool CommandInterpreter::cmdList(Motor &motor, const String &arg) {
  Serial.println(F("Motors:"));
  for (uint8_t i = 0; i < motorCount; i++) {
    Motor &m = *motors[i].motor;
    Kind kind = kindOf(m);
    Serial.print(i == selected ? F("* ") : F("  "));
    Serial.print(motors[i].name);
    Serial.print(kind == STEPPER ? F(" (stepper)") : kind == SERVO ? F(" (servo)") : F(" (DC)"));
    Serial.print(m.isRunning() ? F(", running, ") : F(", stopped, "));
    printValue(m);
    Serial.println();
  }
  return true;
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
    const Command &cmd = commands[i];
    if (name.equalsIgnoreCase(cmd.name)) {
      if (!target && motorCount == 0) {
        // Käsufunktsioonid vajavad mootori viidet, seega ilma mootorita käske ei täideta
        Serial.println(F("Error: No motors registered."));
        return;
      }
      Motor &motor = target ? *target : *motors[selected].motor;
      if (!(cmd.kinds & kindOf(motor))) {
        Serial.print(F("Error: '"));
        Serial.print(cmd.name);
        Serial.println(F("' does not work with this type of motor."));
      } else if (!(this->*cmd.handler)(motor, arg)) {
        Serial.print(F("Error: Use '"));
        Serial.print(cmd.usage);
        Serial.println(F("'."));
      }
      return;
    }
  }
  Serial.println(F("Unknown command! Type 'help' to see available commands."));
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
  splitWord(input, first, rest);

  Entry *entry = find(first);
  if (!entry) {
    execute(first, rest, nullptr);
    return;
  }

  // Esimene sõna oli mootori nimi: järgmine sõna on käsk
  String name, arg;
  splitWord(rest, name, arg);
  if (name.length() == 0) {
    Serial.println(F("Error: Use '<motor> <command>', e.g. 'dc speed 200'."));
    return;
  }
  execute(name, arg, entry->motor);
}
//Tuvastab \n ja \r reavahetus lõppe

void CommandInterpreter::poll() {
  static unsigned long lastCharTime = 0;
  static bool waitingForLineEnding = false;

  const unsigned long INPUT_TIMEOUT = 1000; // 1 second

  while (Serial.available() > 0) {
    char c = Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputLine.length() > 0) {
        handle(inputLine);
        inputLine = "";
      }

      waitingForLineEnding = false;
    } else {
      inputLine += c;

      // Start or reset the timer after every character
      lastCharTime = millis();
      waitingForLineEnding = true;
    }
  }

  // Fallback for terminals that send no line ending
  if (waitingForLineEnding &&
      millis() - lastCharTime >= INPUT_TIMEOUT) {

    Serial.println(
      "No line ending detected. Executing command after 1 second of inactivity."
    );

    handle(inputLine);
    inputLine = "";
    waitingForLineEnding = false;
  }
}


#endif // USERINTERFACE_IMPLEMENTATION

#endif // COMMANDS_H
