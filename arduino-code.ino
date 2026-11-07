#include <Arduino.h>
#include <EEPROM.h>

const byte DATA_PIN = 4;
const byte LATCH_PIN = 7;
const byte CLOCK_PIN = 8;
const byte BUTTON_PIN = 2;
const byte RELAY_PIN = 9;
const byte POT_PIN = A0;
const byte LED_COUNT = 5;

volatile unsigned long timerTicks = 0;
volatile bool buttonFlag = false;

bool reverseDirection = false;
bool relayState = false;
bool previousRelayState = false;
byte currentLED = 0;

unsigned long previousLEDTime = 0;
unsigned long previousPotTime = 0;
unsigned long lastButtonTime = 0;
unsigned long ledInterval = 250;

// Timer1 pertraukimas kas 1 ms:
ISR(TIMER1_COMPA_vect) {
  timerTicks++;
}

// Mygtuko pertraukimas:
void buttonISR() {
  buttonFlag = true;
}

unsigned long getTimeMs() {
  noInterrupts();
  unsigned long t = timerTicks;
  interrupts();
  return t;
}

// Tiesioginis 74HC595 valdymas:
void writeRegister(byte value) {
  PORTD &= ~(1 << PD7);

  for (int i = 7; i >= 0; i--) {
    if (value & (1 << i)){
      PORTD |= (1 << PD4);
    } else {
      PORTD &= ~(1 << PD4);
    }

    PORTB |= (1 << PB0);
    PORTB &= ~(1 << PB0);
  }

  PORTD |= (1 << PD7);
  PORTD &= ~(1 << PD7);
}

void setupTimer1() {
  noInterrupts();
  
  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;
  OCR1A = 249;
  TCCR1B |= (1 << WGM12);
  TCCR1B |= (1 << CS11) | (1 << CS10);
  TIMSK1 |= (1 << OCIE1A);
  
  interrupts();
}

void setup() {
  pinMode(DATA_PIN, OUTPUT);
  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  writeRegister(0);

  reverseDirection = EEPROM.read(0) == 1;

  setupTimer1();
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, FALLING);

  Serial.begin(9600);
  Serial.println("Arduino testas paleistas");
}

void loop() {
  unsigned long now = getTimeMs();
  
  // Mygtuko nuskaitymas:
  bool pressed;
  noInterrupts();
  pressed = buttonFlag;
  buttonFlag = false;
  interrupts();

  if (pressed && now - lastButtonTime >= 200) {
    lastButtonTime = now;
    reverseDirection = !reverseDirection;
    EEPROM.update(0, reverseDirection ? 1 : 0);
    Serial.println(reverseDirection ? "Kryptis: atgal" : "Kryptis: pirmyn");
  }

  // Potenciometro nuskaitymas:
  if (now - previousPotTime >= 100) {
    previousPotTime = now;
    int potValue = analogRead(POT_PIN);
    ledInterval = map(potValue, 0, 1023, 50, 1000);
  }

  // 5s ON / 5s OFF:
  relayState = ((now / 5000UL) % 2UL) == 0UL;
  digitalWrite(RELAY_PIN, relayState ? HIGH : LOW);

  if (relayState != previousRelayState) {
    previousRelayState = relayState;
    previousLEDTime = now;

    if (relayState) {
      writeRegister(1 << currentLED);
    } else {
      writeRegister(0);
    }
  }

  // LED animacija:
  if (relayState && now - previousLEDTime >= ledInterval) {
    previousLEDTime = now;

    if (reverseDirection) {
      currentLED = (currentLED + 4) % LED_COUNT;
    } else {
      currentLED = (currentLED + 1) % LED_COUNT;
    }

    writeRegister(1 << currentLED);
  }
}
