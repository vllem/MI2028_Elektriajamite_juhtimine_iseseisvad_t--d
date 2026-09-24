/*//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
| MI.2028 Elektriajamite juhtimine                                                                                                             |
| Autor: Villem Haug                                                                                                                           | 
| 2026/2027 sügissemester                                                                                                                      | 
|                                                                                                                                              |
| Eesmärk on luua raamistik/tarkvarateek millega saaks juhtida mootoreid lihtsasti ja vähese koodiga.                                          |
| Selleks kasutan ma structe tavapärase OOP asemel kuna mul on rohkem kogemust eheda C programmeerimisega ja vähem kogemust C++ stiilis OOP'ga.|
| motorDriver.cpp/motorDriver.h sisaldab koodi, mis paneb paika kuidas juhtida mootorit ja juhib mootorit                                      |
| userInterface.cpp/userInterface.h sisaldab koodi, mis paneb paika kuidas käsitleda serialis sisestatud käske.                                |
*///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "motorDriver.h"
#include "userInterface.h"

struct motorFactory DC;

void setup() {
  Serial.begin(9600);

  DC.motorPin = LED_BUILTIN;
  DC.motionSequence = {false, 0};

  pinMode(DC.motorPin, OUTPUT);
  stop(DC);

  Serial.println("Ready. Type 'help' to see available commands.");
}

void loop() {
  if (Serial.available() > 0) {
    handleCommand(DC, Serial.readStringUntil('\n'));
  }
}
