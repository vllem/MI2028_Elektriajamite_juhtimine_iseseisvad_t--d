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

// Genereerib setup() ja loop() (vt motorDriver.h)
MOTOR_APP(motor, initStepper(8, 9, 10, 11))  // ULN2003 IN1..IN4 = pinid 8..11
// MOTOR_APP(motor, initDC(3))               // Alalisvoolumootor: PWM = pin 3
