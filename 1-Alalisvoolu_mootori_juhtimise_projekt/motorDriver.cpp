/*
  motorDriver.cpp
  Mootori liidese teostus.


  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester                                                                                                                                                                                                                                                                  |
*/

#include "motorDriver.h"

/*
  applyOutput on abifunktsioon, mis paneb paika kuidas peaks(id) töötama mootor(id).
*/

static void applyOutput(struct motorFactory &motor) {
  if (motor.motionSequence.isRunning) {
    analogWrite(motor.motorPin, motor.motionSequence.speed);
  } else {
    analogWrite(motor.motorPin, 0);
  }
}

/*
  'start' käivitab mootori alati kiirusel 0, et vältida olukorda, kus mootor
  hüppab käivitamisel järsult varasemalt seatud (nt maksimum) kiirusele.
  Kasutaja peab kiiruse peale mootori käivitamist ise 'speed' käsuga määrama.
*/

void start(struct motorFactory &motor) {
  if (!motor.motionSequence.isRunning) {
    motor.motionSequence.speed = 0;
  }
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
