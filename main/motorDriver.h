/*
  motorDriver.h
  Mootori liides: klassid, mis kirjeldavad mootori(te) parameetreid ja ühendust,
  ning meetodid mootori(te) juhtimiseks.

  MI.2028 Elektriajamite juhtimine
  Autor: Villem Haug                                                                                                                          
  2026/2027 sügissemester   
*/

#ifndef MOTOR_H
#define MOTOR_H

#include <Arduino.h>
#include <Servo.h>

/*
  Single-header teek (stb-stiilis, https://github.com/nothings/stb): teostuse saamiseks defineeri täpselt ühes failis enne selle faili kaasamist
    #define MOTORDRIVER_IMPLEMENTATION
*/

// Maksimaalne sammude arv sekundis, mis vastab 'speed' väärtusele 255 (samm-mootori puhul)
#define STEPPER_MAX_STEPS_PER_SEC 1000

// Poolsamme täispöörde kohta (28BYJ-48: 2 * 2037.9 ≈ 4076). Muuda, kui mootor on teistsugune.
#define STEPPER_STEPS_PER_REV 4076

// Servo suurim lubatud nurk kraadides ning nurk, kuhu servo esimesel käivitamisel liigub
#define SERVO_MAX_ANGLE 180
#define SERVO_DEFAULT_ANGLE 90

class StepperMotor;
class ServoMotor;

/*
  Motor on abstraktne baasklass: kõik mootoritüübid (DC, samm-mootor, servo) pärivad sellest.
  Alamklassid realiseerivad apply() ja update(), nii et start/stop/setSpeed ei pea tüüpi kontrollima.
  Samm-mootori ja servo lisavõimalustele pääseb ligi asStepper() ja asServo() kaudu (RTTI-t Arduinos pole).
*/
class Motor {
public:
  virtual ~Motor() {}

  // Kutsutakse setup()-is ja paneb mootori peatatud algseisu. Alamklass seadistab enne oma viigud ja kutsub Motor::begin().
  virtual void begin() {
    running = false;
    speed = 0;
    apply();
  }

  // Tehakse loop()-is võimalikult tihti (nt samm-mootori sammud), ei blokeeri. Vaikimisi ei tee midagi.
  virtual void update() {}

  // Samm-mootori puhul 'this', muidu nullptr.
  virtual StepperMotor *asStepper() { return nullptr; }

  // Servo puhul 'this', muidu nullptr.
  virtual ServoMotor *asServo() { return nullptr; }

  /*
    'start' käivitab mootori alati kiirusel 0, et vältida olukorda, kus mootor
    hüppab käivitamisel järsult varasemalt seatud (nt maksimum) kiirusele.
    Kasutaja peab kiiruse peale mootori käivitamist ise 'speed' käsuga määrama.
    Samm-mootori puhul tähendab 'speed' (0-255) lineaarselt 0..STEPPER_MAX_STEPS_PER_SEC sammu sekundis.
  */
  void start();
  void stop();
  void setSpeed(uint8_t speed);

  bool isRunning() const { return running; }
  uint8_t getSpeed() const { return speed; }

protected:
  // Kirjutab mootori oleku väljunditesse
  virtual void apply() = 0;

  bool running = false;
  uint8_t speed = 0;
};

// Alalisvoolumootor, juhitud PWM-iga
class DCMotor : public Motor {
public:
  explicit DCMotor(uint8_t pwmPin) : pin(pwmPin) {}

  void begin() override;

protected:
  void apply() override;

private:
  uint8_t pin;
};

/*
  Samm-mootor (ULN2003 Darlingtoni massiiv, nt 28BYJ-48), poolsammurežiim (8 faasi).
  setDirection määrab suuna, moveSteps teeb kindla arvu samme (ja peatub ise),
  update() tuleb kutsuda loop()-is võimalikult tihti - see teeb vajalikud sammud ilma blokeerimata.
*/
class StepperMotor : public Motor {
public:
  // ULN2003 sisendid IN1..IN4
  StepperMotor(uint8_t in1, uint8_t in2, uint8_t in3, uint8_t in4)
    : coilPins{in1, in2, in3, in4} {}

  void begin() override;
  void update() override;
  StepperMotor *asStepper() override { return this; }

  void setDirection(bool cw) { clockwise = cw; }
  void moveSteps(uint32_t steps);
  void moveAngle(int32_t degrees);  // positiivne = päripäeva, negatiivne = vastupäeva

  bool isClockwise() const { return clockwise; }
  int32_t getPosition() const { return position; }  // poolsammudes (+1 päripäeva, -1 vastupäeva)

protected:
  void apply() override;

private:
  void writeCoils(uint8_t pattern);

  uint8_t coilPins[4];
  uint8_t phase = 0;                // Praegune faas poolsammu tabelis (0-7)
  bool clockwise = true;            // Pöörlemissuund
  uint32_t remainingSteps = 0;      // Mitu sammu on veel teha (0 = pöörleb pidevalt, kuni 'stop')
  unsigned long lastStepUs = 0;     // Viimase sammu aeg (micros), mitteblokeerivaks ajastamiseks
  int32_t position = 0;
};

/*
  Servo (nt SG90), juhitud Arduino Servo teegiga. Servo ei pöörle, vaid liigub kindlale nurgale (0-SERVO_MAX_ANGLE),
  seega 'speed' siin tähendust ei oma. setAngle salvestab nurga; servo liigub sinna, kui see on käivitatud.
  stop() lõpetab servole impulsside saatmise (servo ei hoia enam asendit), start() liigub salvestatud nurgale tagasi.
*/
class ServoMotor : public Motor {
public:
  explicit ServoMotor(uint8_t signalPin) : pin(signalPin) {}

  void begin() override;
  ServoMotor *asServo() override { return this; }

  void setAngle(uint8_t degrees);  // Üle SERVO_MAX_ANGLE väärtused piiratakse
  uint8_t getAngle() const { return angle; }

protected:
  void apply() override;

private:
  uint8_t pin;
  uint8_t angle = SERVO_DEFAULT_ANGLE;
  Servo servo;
};

#ifdef MOTORDRIVER_IMPLEMENTATION

void Motor::start() {
  if (!running) {
    speed = 0;
  }
  running = true;
  apply();
}

void Motor::stop() {
  running = false;
  apply();
}

void Motor::setSpeed(uint8_t newSpeed) {
  speed = newSpeed;
  apply();
}

/*
  DCMotor::begin seadistab PWM viigu ja paneb mootori peatatud algseisu.
*/

void DCMotor::begin() {
  pinMode(pin, OUTPUT);
  Motor::begin();
}

void DCMotor::apply() {
  analogWrite(pin, running ? speed : 0);
}

/*
  Poolsammu tabel ULN2003 jaoks: iga rida on ühe faasi poolide olek (bitt 0 = IN1 ... bitt 3 = IN4).
  Poolsammurežiim annab sujuvama käigu ja kaks korda suurema lahutuse kui täissamm.
*/

static const uint8_t HALF_STEP_SEQUENCE[8] = {
  0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001
};

void StepperMotor::writeCoils(uint8_t pattern) {
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(coilPins[i], (pattern >> i) & 1);
  }
}

/*
  begin seadistab ULN2003 sisendviigud ning paneb samm-mootori peatatud algseisu (poolid välja lülitatud).
*/

void StepperMotor::begin() {
  phase = 0;
  clockwise = true;
  lastStepUs = 0;
  position = 0;

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(coilPins[i], OUTPUT);
  }
  Motor::begin();  // apply() lülitab poolid välja ja tühistab pooleli olnud liikumise
}

/*
  apply kutsutakse start/stop/setSpeed peale. Peatumisel lülitatakse poolid välja,
  et ULN2003 ja mootor ei kuumeneks asjata, ning pooleli olnud 'steps' liikumine tühistatakse.
  Käimas olles nullitakse ajatempel, et kiiruse muutmisel ei tehtaks mahajäänud samme korraga ära.
*/

void StepperMotor::apply() {
  if (running) {
    lastStepUs = micros();
  } else {
    remainingSteps = 0;
    writeCoils(0);
  }
}

/*
  update teeb vajadusel ühe sammu. Ajastus põhineb micros()-il, mitte delay()-l,
  et CommandInterpreter saaks samal ajal käske vastu võtta.
*/

void StepperMotor::update() {
  if (!running || speed == 0) return;

  unsigned long stepsPerSec = (unsigned long)speed * STEPPER_MAX_STEPS_PER_SEC / 255;
  if (stepsPerSec == 0) stepsPerSec = 1;
  unsigned long intervalUs = 1000000UL / stepsPerSec;

  unsigned long now = micros();
  if (now - lastStepUs < intervalUs) return;
  lastStepUs += intervalUs;

  // Liigume tabelis ühe faasi edasi (või tagasi) ja anname poolidele uue oleku
  phase = (phase + (clockwise ? 1 : 7)) & 7;
  writeCoils(HALF_STEP_SEQUENCE[phase]);
  position += clockwise ? 1 : -1;

  // remainingSteps == 0 tähendab pidevat pöörlemist; lõplik liikumine lõpeb viimase sammuga
  if (remainingSteps > 0) {
    remainingSteps--;
    if (remainingSteps == 0) {
      stop();
    }
  }
}

/*
  moveSteps käivitab mootori ja teeb täpselt 'steps' sammu praegusel kiirusel, seejärel peatub ise.
  Kiirus 0 korral mootor ei liigu, enne kui kasutaja kiiruse määrab.
*/

void StepperMotor::moveSteps(uint32_t steps) {
  if (steps == 0) return;
  remainingSteps = steps;
  lastStepUs = micros();
  running = true;
}

/*
  moveAngle teeb sammud antud nurga jagu (kraadides): märk määrab suuna, ümardatakse lähima sammuni.
*/

void StepperMotor::moveAngle(int32_t degrees) {
  if (degrees == 0) return;
  uint32_t absDeg = degrees < 0 ? -degrees : degrees;
  setDirection(degrees > 0);
  moveSteps(((uint64_t)absDeg * STEPPER_STEPS_PER_REV + 180) / 360);
}

/*
  ServoMotor::begin paneb servo peatatud algseisu. Servot ei seota (attach) enne 'start'i,
  et see ei hüppaks käivitamisel ootamatult mingile asendile.
*/

void ServoMotor::begin() {
  angle = SERVO_DEFAULT_ANGLE;
  Motor::begin();
}

/*
  apply seob servo viiguga (ja liigutab salvestatud nurgale) või vabastab selle, olenevalt sellest, kas servo on käivitatud.
*/

void ServoMotor::apply() {
  if (running) {
    if (!servo.attached()) servo.attach(pin);
    servo.write(angle);
  } else if (servo.attached()) {
    servo.detach();
  }
}

void ServoMotor::setAngle(uint8_t degrees) {
  angle = degrees > SERVO_MAX_ANGLE ? SERVO_MAX_ANGLE : degrees;
  apply();
}

#endif // MOTORDRIVER_IMPLEMENTATION

#endif // MOTOR_H
