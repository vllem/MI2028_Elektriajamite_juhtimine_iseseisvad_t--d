/*
  motor.cpp
  Mootori liidese teostus.


  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester                                                                                                                                                                                                                                                                  |
*/

#include "motorDriver.h"

/*
  applyOutput on abifunktsioon, mis paneb paika kuidas peaks(id) töötama mootor(id).
  "static" tähendab, et see on nähtav ainult selles failis.
*/

static void applyOutput(struct motorFactory &motor) {
  if (motor.motionSequence.isRunning) {
    analogWrite(motor.motorPin, motor.motionSequence.speed);
  } else {
    analogWrite(motor.motorPin, 0);
  }
}

void start(struct motorFactory &motor) {
  motor.motionSequence.isRunning = true;
  applyOutput(motor);
}

void stop(struct motorFactory &motor) {
  motor.motionSequence.isRunning = false;
  applyOutput(motor);
}

void setSpeed(struct motorFactory &motor, uint8_t speed) {
  motor.motionSequence.speed = speed;
  applyOutput(motor);
}
