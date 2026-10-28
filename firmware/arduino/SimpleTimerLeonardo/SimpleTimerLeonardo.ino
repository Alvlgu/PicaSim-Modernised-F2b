// 1. Подаете питание
// 2. Светодиод RX начинает быстро моргать сразу
// 3. Светодиод TX отмигивает сохраненное число раз
// 4. Пауза одна секунда
// 5. ESC пищит принимая максимум три секунды
// 6. ESC пищит принимая минимум три секунды
// 7. Мотор стартует на выбранной мощности
// 8. Светодиод TX горит непрерывно во время работы
// 9. Через 2 мин — 20 сек "дерганья"
// 10. Мотор глохнет, TX гаснет, RX продолжает моргать

#include <Servo.h>
#include <EEPROM.h>

Servo motor;
const int motorPin = 9;
const int buttonPin = 7;

// НАСТРОЙКИ МОЩНОСТИ
const int stepsN = 10;
const int minThrottle = 1000;
const int maxThrottle = 2000;

// НАСТРОЙКИ ТАЙМЕРА (мс)
const long runDuration = 120000;    // Основной полет (2 мин)
const long warnDuration = 20000;    // Время предупреждения (20 сек)
const int offTime = 500;            // Пауза мотора (0.5 сек)
const int onTime = 2500;            // Работа мотора (2.5 сек)

int currentStep = 1;
unsigned long startTime = 0;
bool motorStarted = false;
const int eepromAddr = 1;

// --- антидребезг кнопки ---
const unsigned long debounceMs = 50;
bool btnStable = HIGH;
bool btnLastRead = HIGH;
unsigned long btnLastChange = 0;

void smartDelay(unsigned long ms) {
  unsigned long startWait = millis();
  while (millis() - startWait < ms) {
    static unsigned long lastFlash = 0;
    if (millis() - lastFlash >= 100) {
      //digitalWrite(LED_BUILTIN_RX, !digitalRead(LED_BUILTIN_RX));
      lastFlash = millis();
    }
  }
}

void setup() {
  //pinMode(LED_BUILTIN_RX, OUTPUT);
  pinMode(LED_BUILTIN_TX, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  //digitalWrite(LED_BUILTIN_RX, HIGH);
  digitalWrite(LED_BUILTIN_TX, HIGH);

  motor.attach(motorPin);

  currentStep = EEPROM.read(eepromAddr);
  if (currentStep < 1 || currentStep > stepsN) currentStep = 1;

  // Пауза 2 сек, чтобы мигания мощности были чёткими
  smartDelay(2000);

  // Индикация выбранного шага
  for (int i = 0; i < currentStep; i++) {
    digitalWrite(LED_BUILTIN_TX, LOW);
    smartDelay(250);
    digitalWrite(LED_BUILTIN_TX, HIGH);
    smartDelay(250);
  }
  smartDelay(1000);

  // Калибровка ESC
  motor.writeMicroseconds(2000);
  smartDelay(3000);
  motor.writeMicroseconds(1000);
  smartDelay(3000);

  startTime = millis();
  motorStarted = true;
}

void loop() {
  unsigned long currentMillis = millis();
  int throttle = map(currentStep, 1, stepsN, minThrottle, maxThrottle);

  // RX моргает всегда (индикатор работы контроллера)
  static unsigned long lastRX = 0;
  if (currentMillis - lastRX >= 200) {
    //digitalWrite(LED_BUILTIN_RX, !digitalRead(LED_BUILTIN_RX));
    lastRX = currentMillis;
  }

  // --- Кнопка с антидребезгом ---
  bool reading = digitalRead(buttonPin);
  if (reading != btnLastRead) {
    btnLastChange = currentMillis;
    btnLastRead = reading;
  }
  if (currentMillis - btnLastChange >= debounceMs) {
    if (reading != btnStable) {
      btnStable = reading;
      if (btnStable == LOW) {          // фронт нажатия
        currentStep++;
        if (currentStep > stepsN) currentStep = 1;
        EEPROM.update(eepromAddr, currentStep);
        // НЕ трогаем startTime и motorStarted —
        // кнопка только регулирует мощность,
        // мотор не перезапускается и не запускается после стопа
      }
    }
  }

  if (motorStarted) {
    unsigned long elapsed = currentMillis - startTime;

    if (elapsed < runDuration) {
      // ПОЛЕТ
      motor.writeMicroseconds(throttle);
      digitalWrite(LED_BUILTIN_TX, LOW);
    }
    else if (elapsed < (runDuration + warnDuration)) {
      // ПРЕДУПРЕЖДЕНИЕ (заикание мотора)
      if ((elapsed % (offTime + onTime)) < offTime) {
        motor.writeMicroseconds(1000);
        digitalWrite(LED_BUILTIN_TX, HIGH);
      } else {
        motor.writeMicroseconds(throttle);
        digitalWrite(LED_BUILTIN_TX, LOW);
      }
    }
    else {
      // ПОСАДКА (выключение)
      motor.writeMicroseconds(1000);
      digitalWrite(LED_BUILTIN_TX, HIGH);
      motorStarted = false;
    }
  }
}
