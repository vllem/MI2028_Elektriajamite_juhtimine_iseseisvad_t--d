/*
  motor.h
  Mootori liides: structid, mis kirjeldavad mootori(te) parameetreid ja ühendust,
  ning funktsioonid mootori(te) juhtimiseks.

  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester   
*/

#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>

/*
  Structid motionSequencer ja motorFactory panevad paika mootori(te) parameetrid kui ka selle,
  kuhu füüsiliselt mootor(id) on ühendatud
*/

struct motionSequencer {
  bool isRunning;
  uint8_t speed;
};

struct motorFactory {
  uint8_t motorPin;
  struct motionSequencer motionSequence;
};

/*
  Funktsioonid start, stop ja setSpeed käsitlevad mootori(te) juhtimist
*/

void start(struct motorFactory &motor);
void stop(struct motorFactory &motor);
void setSpeed(struct motorFactory &motor, uint8_t speed);

#endif // MOTOR_H
