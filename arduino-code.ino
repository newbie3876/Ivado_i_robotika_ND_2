#include <EEPROM.h>

// PIN CONFIGURATION:
const byte TEMP_PIN   = A0;
const byte BUTTON_PIN = 2;   // INT0 external interrupt
const byte MOTOR_PIN  = 3;   // PWM
const byte RED_LED    = 8;
const byte GREEN_LED  = 9;
const byte BUZZER_PIN = 10;


// SYSTEM STATES:
enum SystemState {
  NORMAL,
  COOLING_LOW,
  COOLING_HIGH,
  CRITICAL
};

SystemState currentState = NORMAL;

// EEPROM SETTINGS:
const uint16_t EEPROM_MAGIC = 0xCAFE;
const byte EEPROM_VERSION = 1;

struct Settings {
  uint16_t magic;
  byte version;
  byte coolingThreshold;
  bool alarmEnabled;
};

Settings settings;

// Default values used when EEPROM is invalid:
const byte DEFAULT_COOLING_THRESHOLD = 30;
const bool DEFAULT_ALARM_ENABLED = true;

// INTERRUPT FLAGS:
// These variables are changed inside ISRs, therefore they must be volatile.
volatile bool sampleFlag = false;
volatile bool buttonFlag = false;
volatile bool outputTickFlag = false;

volatile byte timerTicks = 0; // Timer counter, Timer1 interrupt occurs every 100 ms.

// SYSTEM VARIABLES:
float temperature = 0.0;
// Used for asynchronous blinking / alarm:
bool redLedState = false;
bool buzzerState = false;

// EXTERNAL INTERRUPT:
void buttonISR() {
  buttonFlag = true; // ISR must be short. We only notify the main loop that an event occurred
}

// TIMER1 INTERRUPT:
ISR(TIMER1_COMPA_vect) {
  timerTicks++;
  // Update asynchronous outputs every 100 ms:
  outputTickFlag = true;

  // Measure temperature every 500 ms:
  if (timerTicks >= 5) {
    timerTicks = 0;
    sampleFlag = true;
  }
}

// TIMER1 CONFIGURATION:
void setupTimer1() {
  // Disable interrupts while configuring timer:
  noInterrupts();

  TCCR1A = 0;
  TCCR1B = 0;
  TCNT1 = 0;

  /*
     Arduino Uno clock = 16 MHz
     Prescaler = 64

     Timer frequency:
     16,000,000 / 64 = 250,000 Hz

     We want interrupt every 100 ms:
     250,000 * 0.1 = 25,000 counts
     OCR1A = 24,999
  */

  OCR1A = 24999;
  
  // CTC mode:
  TCCR1B |= (1 << WGM12);
  // Prescaler = 64:
  TCCR1B |= (1 << CS11);
  TCCR1B |= (1 << CS10);
  // Enable Timer1 Compare Match A interrupt:
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

// EEPROM FUNCTIONS:
void loadSettings() {
  EEPROM.get(0, settings);
  // Validate EEPROM data:
  if (settings.magic != EEPROM_MAGIC ||
      settings.version != EEPROM_VERSION ||
      settings.coolingThreshold < 25 ||
      settings.coolingThreshold > 35) {

    Serial.println("EEPROM invalid - loading defaults.");
    settings.magic = EEPROM_MAGIC;
    settings.version = EEPROM_VERSION;
    settings.coolingThreshold = DEFAULT_COOLING_THRESHOLD;
    settings.alarmEnabled = DEFAULT_ALARM_ENABLED;
    EEPROM.put(0, settings);
  } else {
    Serial.println("Settings loaded from EEPROM.");
  }

  Serial.print("Cooling threshold: ");
  Serial.print(settings.coolingThreshold);
  Serial.println(" C");
  Serial.print("Alarm: ");

  if (settings.alarmEnabled) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }
}

void saveSettingsIfChanged() {
  Settings storedSettings;
  EEPROM.get(0, storedSettings);

  // Write-on-change:
  // EEPROM is written only if a setting really changed:
  if (storedSettings.magic != settings.magic ||
      storedSettings.version != settings.version ||
      storedSettings.coolingThreshold != settings.coolingThreshold ||
      storedSettings.alarmEnabled != settings.alarmEnabled) {

    EEPROM.put(0, settings);
    Serial.println("Settings saved to EEPROM.");
  }
}

// TEMPERATURE SENSOR:
void readTemperature() {
  int sensorValue = analogRead(TEMP_PIN);
  float voltage = sensorValue * (5.0 / 1023.0);
  temperature = (voltage - 0.5) * 100.0;
}

void printStatus() {
  Serial.print("Temperature: ");
  Serial.print(temperature, 1);
  Serial.print(" C | Threshold: ");
  Serial.print(settings.coolingThreshold);
  Serial.print(" C | State: ");

  switch (currentState) {
    case NORMAL:
      Serial.println("NORMAL");
      break;
    
    case COOLING_LOW:
      Serial.println("COOLING_LOW");
      break;

    case COOLING_HIGH:
      Serial.println("COOLING_HIGH");
      break;

    case CRITICAL:
      Serial.println("CRITICAL");
      break;
  }
}


void updateState() {
  /*
     Cooling threshold can be: 30 C, 32 C, 34 C
     HIGH cooling starts 7 degrees above threshold.
     Critical temperature is ALWAYS 45 C.
  */

  float lowThreshold = settings.coolingThreshold;
  float highThreshold = settings.coolingThreshold + 7;

  // Safety limit:
  if (highThreshold > 42) {
    highThreshold = 42;
  }

  // CRITICAL:
  if (temperature >= 45.0) {
    currentState = CRITICAL;
  }

  else if (temperature >= highThreshold) {
    currentState = COOLING_HIGH; // COOLING HIGH
  } else if (temperature >= lowThreshold) {
    currentState = COOLING_LOW; // COOLING LOW
  } else {
    currentState = NORMAL; // NORMAL
  }
}

// MOTOR CONTROL:
void updateMotor() {
  switch (currentState) {
    case NORMAL:
      analogWrite(MOTOR_PIN, 0); // Fan OFF
      break;

    case COOLING_LOW:
      analogWrite(MOTOR_PIN, 102); // About 40 %
      break;

    case COOLING_HIGH:
      analogWrite(MOTOR_PIN, 191); // About 75 %
      break;

    case CRITICAL:
      analogWrite(MOTOR_PIN, 255); // 100 %
      break;
  }
}

// LED + BUZZER OUTPUTS:
void updateOutputs() {
  switch (currentState) {
    case NORMAL:
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      noTone(BUZZER_PIN);
      
      redLedState = false;
      buzzerState = false;
      break;

    case COOLING_LOW:
      digitalWrite(GREEN_LED, HIGH);
      digitalWrite(RED_LED, LOW);
      noTone(BUZZER_PIN);
      
      redLedState = false;
      buzzerState = false;
      break;

    case COOLING_HIGH:
      digitalWrite(GREEN_LED, LOW);
      // Slow blinking:
      redLedState = !redLedState;
      
      digitalWrite(RED_LED, redLedState ? HIGH : LOW);
      noTone(BUZZER_PIN);
      
      buzzerState = false;
      break;


    case CRITICAL:
      digitalWrite(GREEN_LED, LOW);
      // Fast blinking red LED:
      redLedState = !redLedState;
      digitalWrite(RED_LED, redLedState ? HIGH : LOW);

      // Periodic buzzer alarm:
      if (settings.alarmEnabled) {
        buzzerState = !buzzerState;

        if (buzzerState) {
          tone(BUZZER_PIN, 1000);
        } else {
          noTone(BUZZER_PIN);
        }
      } else {
        noTone(BUZZER_PIN);
      }
    
      break;
  }
}


void handleButton() {
  static unsigned long lastButtonTime = 0;
  unsigned long now = millis();

  if (now - lastButtonTime < 250) return;

  lastButtonTime = now;

  // Cycle cooling threshold: 30 -> 32 -> 34 -> 30
  if (settings.coolingThreshold == 30) {
    settings.coolingThreshold = 32;
  } else if (settings.coolingThreshold == 32) {
    settings.coolingThreshold = 34;
  } else {
    settings.coolingThreshold = 30;
  }

  Serial.println();
  Serial.println("*** BUTTON EVENT ***");
  Serial.print("New cooling threshold: ");
  Serial.print(settings.coolingThreshold);
  Serial.println(" C");

  // Save only if the value changed:
  saveSettingsIfChanged();
  // Immediately re-evaluate state:
  updateState();
}


void setup() {
  Serial.begin(9600);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Safe initial outputs:
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  analogWrite(MOTOR_PIN, 0);
  noTone(BUZZER_PIN);

  // EEPROM:
  loadSettings();

  // External interrupt:
  attachInterrupt(
    digitalPinToInterrupt(BUTTON_PIN),
    buttonISR,
    FALLING
  );

  setupTimer1(); // Hardware Timer1
  
  Serial.println();
  Serial.println("==============================");
  Serial.println(" Temperature Control System");
  Serial.println("==============================");
  Serial.println();
  Serial.println("System started.");
}

void loop() {
  // External button event:
  if (buttonFlag) {
    // Read/clear the shared flag atomically:
    noInterrupts();
    buttonFlag = false;
    interrupts();
    handleButton();
  }

  // Timer sensor event:
  if (sampleFlag) {
    noInterrupts();
    sampleFlag = false;
    interrupts();

    readTemperature();
    updateState();
    updateMotor();
    printStatus();
}

  // Periodic output event:
  if (outputTickFlag) {
    noInterrupts();
    outputTickFlag = false;
    interrupts();
    updateOutputs();
  }
}
