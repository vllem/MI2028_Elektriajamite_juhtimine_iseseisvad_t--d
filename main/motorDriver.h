/*
  motorDriver.h
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
  Single-header teek (stb-stiilis, https://github.com/nothings/stb): teostuse saamiseks defineeri täpselt ühes failis enne selle faili kaasamist
    #define MOTORDRIVER_IMPLEMENTATION
*/

/*
  Structid motionSequencer ja motorFactory panevad paika mootori(te) parameetrid kui ka selle,
  kuhu füüsiliselt mootor(id) on ühendatud
*/

struct motionSequencer {
  bool isRunning;
  uint8_t speed;
};

// Mootori tüüp: alalisvoolumootor (PWM) või samm-mootor (ULN2003 Darlingtoni massiiv, nt 28BYJ-48)
enum motorType {
  MOTOR_DC,
  MOTOR_STEPPER
};

// Maksimaalne sammude arv sekundis, mis vastab 'speed' väärtusele 255 (samm-mootori puhul)
#define STEPPER_MAX_STEPS_PER_SEC 1000

// Poolsamme täispöörde kohta (28BYJ-48: 2 * 2037.9 ≈ 4076). Muuda, kui mootor on teistsugune.
#define STEPPER_STEPS_PER_REV 4076

// Samm-mootori lisaparameetrid. ULN2003 sisendid IN1..IN4 on coilPins[0..3] (coilPins[0] == motorPin).
struct stepperConfig {
  uint8_t coilPins[4];         // ULN2003 sisendviigud IN1..IN4
  uint8_t phase;               // Praegune faas poolsammu tabelis (0-7)
  bool clockwise;              // Pöörlemissuund
  uint32_t remainingSteps;     // Mitu sammu on veel teha (0 = pöörleb pidevalt, kuni 'stop')
  unsigned long lastStepUs;    // Viimase sammu aeg (micros), mitteblokeerivaks ajastamiseks
  int32_t position;            // Positsioon sammudes (+1 päripäeva, -1 vastupäeva, poolsammudes)
};

struct motorFactory {
  uint8_t motorPin;            // DC: PWM viik; samm-mootor: IN1 viik
  enum motorType type;
  struct motionSequencer motionSequence;
  struct stepperConfig stepper; // Kasutusel ainult type == MOTOR_STEPPER korral

  // Mootoritüübile omane käitumine; init-funktsioon paneb paika õiged funktsioonid,
  // nii et start/stop/setSpeed/updateMotor ei pea tüüpi kontrollima.
  void (*apply)(struct motorFactory &motor);   // Kirjutab mootori oleku väljunditesse
  void (*update)(struct motorFactory &motor);  // Tehakse loop()-is (nt samm-mootori sammud)

  // Seadistamine: motor.initDC(pin) või motor.initStepper(in1, in2, in3, in4)
  void initDC(uint8_t pwmPin);
  void initStepper(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4);
};

/*
  Funktsioonid start, stop ja setSpeed käsitlevad mootori(te) juhtimist.
  Samm-mootori puhul tähendab 'speed' (0-255) lineaarselt 0..STEPPER_MAX_STEPS_PER_SEC sammu sekundis.
*/

void start(struct motorFactory &motor);
void stop(struct motorFactory &motor);
void setSpeed(struct motorFactory &motor, uint8_t speed);

/*
  Samm-mootori funktsioonid.
  initStepper (motorFactory liige) seadistab ULN2003 sisendviigud (poolsammurežiim, 8 faasi) ja algseisu; peab olema välja kutsutud setup()-is.
  setDirection määrab suuna, moveSteps teeb kindla arvu samme (ja peatub ise),
  updateMotor tuleb kutsuda loop()-is võimalikult tihti - see teeb vajalikud sammud ilma blokeerimata.
  DC-mootori peal on updateMotor ohutu (ei tee midagi).
*/
void setDirection(struct motorFactory &motor, bool clockwise);
void moveSteps(struct motorFactory &motor, uint32_t steps);
void moveAngle(struct motorFactory &motor, int32_t degrees);  // positiivne = päripäeva, negatiivne = vastupäeva
void updateMotor(struct motorFactory &motor);

/*
  MOTOR_APP genereerib setup()- ja loop()-funktsioonid, nii et kasutaja .ino failis neid kirjutama ei pea.
  Kasutus: MOTOR_APP(motor, initStepper(8, 9, 10, 11)) või MOTOR_APP(motor, initDC(3))
  Vajab, et userInterface.h oleks enne makro kasutamist kaasatud (pollSerial).
*/
#define MOTOR_APP(m, initCall)                                          \
  struct motorFactory m;                                                \
  void setup() {                                                        \
    Serial.begin(9600);                                                 \
    m.initCall;                                                         \
    stop(m);                                                            \
    Serial.println("Ready. Type 'help' to see available commands.");    \
  }                                                                     \
  void loop() {                                                         \
    pollSerial(m);                                                      \
    updateMotor(m);                                                     \
  }

#ifdef MOTORDRIVER_IMPLEMENTATION


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
  dcApply paneb paika, kuidas peaks töötama alalisvoolumootor. DC-mootor ei vaja update'i.
*/

static void dcApply(struct motorFactory &motor) {
  if (motor.motionSequence.isRunning) {
    analogWrite(motor.motorPin, motor.motionSequence.speed);
  } else {
    analogWrite(motor.motorPin, 0);
  }
}

static void dcUpdate(struct motorFactory &motor) {}

/*
  stepperApply kutsutakse start/stop/setSpeed peale. Peatumisel lülitatakse poolid välja,
  et ULN2003 ja mootor ei kuumeneks asjata, ning pooleli olnud 'steps' liikumine tühistatakse.
  Käimas olles nullitakse ajatempel, et kiiruse muutmisel ei tehtaks mahajäänud samme korraga ära.
*/

static void stepperApply(struct motorFactory &motor) {
  if (motor.motionSequence.isRunning) {
    motor.stepper.lastStepUs = micros();
  } else {
    motor.stepper.remainingSteps = 0;
    writeCoils(motor, 0);
  }
}

/*
  stepperUpdate teeb vajadusel ühe sammu. Ajastus põhineb micros()-il, mitte delay()-l,
  et pollSerial saaks samal ajal käske vastu võtta.
*/

static void stepperUpdate(struct motorFactory &motor) {
  if (!motor.motionSequence.isRunning || motor.motionSequence.speed == 0) return;

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

  // remainingSteps == 0 tähendab pidevat pöörlemist; lõplik liikumine lõpeb viimase sammuga
  if (motor.stepper.remainingSteps > 0) {
    motor.stepper.remainingSteps--;
    if (motor.stepper.remainingSteps == 0) {
      stop(motor);
    }
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
  motor.apply(motor);
}

void stop(struct motorFactory &motor) {
  motor.motionSequence.isRunning = false;
  motor.apply(motor);
}

void setSpeed(struct motorFactory &motor, uint8_t speed) {
  motor.motionSequence.speed = speed;
  motor.apply(motor);
}

/*
  initDC seadistab alalisvoolumootori PWM viigu ja paneb mootori peatatud algseisu.
*/

void motorFactory::initDC(uint8_t pwmPin) {
  struct motorFactory &motor = *this;
  motor.type = MOTOR_DC;
  motor.motorPin = pwmPin;
  motor.motionSequence = {false, 0};
  motor.apply = dcApply;
  motor.update = dcUpdate;
  pinMode(pwmPin, OUTPUT);
  motor.apply(motor);
}

/*
  initStepper seadistab ULN2003 sisendviigud ning paneb samm-mootori peatatud algseisu (poolid välja lülitatud).
*/

void motorFactory::initStepper(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4) {
  struct motorFactory &motor = *this;
  motor.type = MOTOR_STEPPER;
  motor.motorPin = in1;
  motor.motionSequence = {false, 0};
  motor.stepper = {{in1, in2, in3, in4}, 0, true, 0, 0, 0};
  motor.apply = stepperApply;
  motor.update = stepperUpdate;

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

void updateMotor(struct motorFactory &motor) {
  motor.update(motor);
}

#endif // MOTORDRIVER_IMPLEMENTATION

#endif // MOTOR_H
