/*
  motorDriver.cpp
  Mootori liidese teostus.


  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester                                                                                                                                                                                                                                                                  |
*/

#include "motorDriver.h"

/*
  Poolsammu tabel ULN2003 jaoks: iga rida on ühe faasi poolide olek (bitt 0 = IN1 ... bitt 3 = IN4).
  Poolsammurežiim annab sujuvama käigu ja kaks korda suurema lahutuse kui täissamm.
*/

static const uint8_t HALF_STEP_SEQUENCE[8] = {
  0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001
};

static void writeCoils(struct motorFactory &motor, uint8_t pattern) {
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(motor.stepper.coilPins[i], (pattern >> i) & 1);
  }
}

/*
  applyOutput on abifunktsioon, mis paneb paika kuidas peaks(id) töötama mootor(id).
*/

static void applyOutput(struct motorFactory &motor) {
  // Samm-mootori sammud tehakse updateMotor'is; siin lülitatakse peatumisel poolid välja,
  // et ULN2003 ja mootor ei kuumeneks asjata.
  if (motor.type == MOTOR_STEPPER) {
    if (!motor.motionSequence.isRunning) writeCoils(motor, 0);
    return;
  }

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
  if (motor.type == MOTOR_STEPPER) {
    motor.stepper.continuous = true;
    motor.stepper.lastStepUs = micros();
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
  // Ajatempli nullimine, et kiiruse muutmisel ei tehtaks mahajäänud samme korraga ära
  motor.stepper.lastStepUs = micros();
  applyOutput(motor);
}

/*
  initDC seadistab alalisvoolumootori PWM viigu ja paneb mootori peatatud algseisu.
*/

void motorFactory::initDC(uint8_t pwmPin) {
  struct motorFactory &motor = *this;
  motor.type = MOTOR_DC;
  motor.motorPin = pwmPin;
  motor.motionSequence = {false, 0};
  pinMode(pwmPin, OUTPUT);
  applyOutput(motor);
}

/*
  initStepper seadistab ULN2003 sisendviigud ning paneb samm-mootori peatatud algseisu (poolid välja lülitatud).
*/

void motorFactory::initStepper(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4) {
  struct motorFactory &motor = *this;
  motor.type = MOTOR_STEPPER;
  motor.motorPin = in1;
  motor.motionSequence = {false, 0};
  motor.stepper = {{in1, in2, in3, in4}, 0, true, 0, false, 0, 0};

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(motor.stepper.coilPins[i], OUTPUT);
  }
  writeCoils(motor, 0);
}

void setDirection(struct motorFactory &motor, bool clockwise) {
  if (motor.type != MOTOR_STEPPER) return;
  motor.stepper.clockwise = clockwise;
}

/*
  moveSteps käivitab mootori ja teeb täpselt 'steps' sammu praegusel kiirusel, seejärel peatub ise.
  Kiirus 0 korral mootor ei liigu, enne kui kasutaja kiiruse määrab.
*/

void moveSteps(struct motorFactory &motor, uint32_t steps) {
  if (motor.type != MOTOR_STEPPER || steps == 0) return;
  motor.stepper.continuous = false;
  motor.stepper.remainingSteps = steps;
  motor.stepper.lastStepUs = micros();
  motor.motionSequence.isRunning = true;
}

/*
  moveAngle teeb sammud antud nurga jagu (kraadides): märk määrab suuna, ümardatakse lähima sammuni.
*/

void moveAngle(struct motorFactory &motor, int32_t degrees) {
  if (motor.type != MOTOR_STEPPER || degrees == 0) return;
  uint32_t absDeg = degrees < 0 ? -degrees : degrees;
  setDirection(motor, degrees > 0);
  moveSteps(motor, ((uint64_t)absDeg * STEPPER_STEPS_PER_REV + 180) / 360);
}

/*
  updateMotor teeb vajadusel ühe sammu. Ajastus põhineb micros()-il, mitte delay()-l,
  et pollSerial saaks samal ajal käske vastu võtta.
*/

void updateMotor(struct motorFactory &motor) {
  if (motor.type != MOTOR_STEPPER) return;
  if (!motor.motionSequence.isRunning || motor.motionSequence.speed == 0) return;
  if (!motor.stepper.continuous && motor.stepper.remainingSteps == 0) {
    stop(motor);
    return;
  }

  unsigned long stepsPerSec = (unsigned long)motor.motionSequence.speed * STEPPER_MAX_STEPS_PER_SEC / 255;
  if (stepsPerSec == 0) stepsPerSec = 1;
  unsigned long intervalUs = 1000000UL / stepsPerSec;

  unsigned long now = micros();
  if (now - motor.stepper.lastStepUs < intervalUs) return;
  motor.stepper.lastStepUs += intervalUs;

  // Liigume tabelis ühe faasi edasi (või tagasi) ja anname poolidele uue oleku
  motor.stepper.phase = (motor.stepper.phase + (motor.stepper.clockwise ? 1 : 7)) & 7;
  writeCoils(motor, HALF_STEP_SEQUENCE[motor.stepper.phase]);
  motor.stepper.position += motor.stepper.clockwise ? 1 : -1;

  if (!motor.stepper.continuous) {
    motor.stepper.remainingSteps--;
    if (motor.stepper.remainingSteps == 0) {
      stop(motor);
    }
  }
}
