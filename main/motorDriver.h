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

#endif // MOTOR_H
