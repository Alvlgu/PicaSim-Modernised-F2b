// ====================================================================
// ПЛАТА: Arduino Nano (ATmega328P)
//
// ПОДКЛЮЧЕНИЕ:
// 1. ESC (Контроллер мотора): 
//    - Сигнальный провод (обычно белый или желтый) -> пин D9
//    - Земля (GND, черный или коричневый) -> GND Arduino
// 2. Кнопка выбора мощности:
//    - Один контакт -> пин D7
//    - Второй контакт -> GND Arduino (внутренний подтягивающий резистор включен)
// 3. Индикация:
//    - Используется штатный встроенный светодиод на плате (пин D13)
//
// АЛГОРИТМ РАБОТЫ:
// 1. Подаете питание.
// 2. Пауза две секунды перед индикацией сохраненного шага.
// 3. Светодиод отмигивает сохраненное число раз (выбранный шаг мощности).
// 4. Пауза одна секунда.
// 5. ESC пищит, принимая максимум (3 секунды).
// 6. ESC пищит, принимая минимум (3 секунды).
// 7. Мотор стартует на выбранной мощности.
// 8. Светодиод горит непрерывно во время работы.
// 9. Через 2 мин — 20 сек "дерганья" (мотор прерывается, диод мигает в такт).
// 10. Мотор глохнет, светодиод переходит в режим редкого мигания.
// ====================================================================

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
const long runDuration = 3*60*1000;    // Основной полет (3 мин)
const long warnDuration = 10*1000;    // Время предупреждения (10 сек)
const int offTime = 500;            // Пауза мотора (0.5 сек)
const int onTime = 1000;            // Работа мотора (1.0 сек)

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
    // Быстрое моргание светодиода полностью удалено из пауз
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  digitalWrite(LED_BUILTIN, LOW); // ИЗМЕНЕНО: При старте светодиод ВЫКЛЮЧЕН

  motor.attach(motorPin);

  currentStep = EEPROM.read(eepromAddr);
  if (currentStep < 1 || currentStep > stepsN) currentStep = 1;

  // Пауза 2 сек (светодиод не горит)
  smartDelay(2000);

  // Индикация выбранного шага: теперь он будет четко вспыхивать HIGH->LOW
  for (int i = 0; i < currentStep; i++) {
    digitalWrite(LED_BUILTIN, HIGH); // Вспышка
    delay(250); 
    digitalWrite(LED_BUILTIN, LOW);  // Пауза
    delay(250);
  }
  smartDelay(1000); // Пауза 1 сек перед калибровкой

  // Калибровка ESC (во время калибровки диод не горит)
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
      }
    }
  }

  if (motorStarted) {
    unsigned long elapsed = currentMillis - startTime;

    if (elapsed < runDuration) {
      // ПОЛЕТ (Мотор работает, светодиод горит непрерывно)
      motor.writeMicroseconds(throttle);
      digitalWrite(LED_BUILTIN, HIGH);
    }
    else if (elapsed < (runDuration + warnDuration)) {
      // ПРЕДУПРЕЖДЕНИЕ (Заикание мотора, светодиод мигает синхронно с паузами)
      if ((elapsed % (offTime + onTime)) < offTime) {
        motor.writeMicroseconds(1000);
        digitalWrite(LED_BUILTIN, LOW);
      } else {
        motor.writeMicroseconds(throttle);
        digitalWrite(LED_BUILTIN, HIGH);
      }
    }
    else {
      // ПОСАДКА (Выключение мотора, светодиод гаснет)
      motor.writeMicroseconds(1000);
      digitalWrite(LED_BUILTIN, LOW);
      motorStarted = false;
    }
  } else {
    // Сигнал окончания работы таймера
    static unsigned long lastEndFlash = 0;
    if (currentMillis - lastEndFlash >= 500) {
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      lastEndFlash = currentMillis;
    }
  }
}
