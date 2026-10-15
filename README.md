# Temperature-Controlled Cooling and Alarm System

## Overview

This project implements a small embedded temperature-control system using an Arduino Uno R3. The system measures ambient temperature with a TMP36 sensor and reacts using a DC motor, two LEDs, and a piezo buzzer.

The project is designed around an **asynchronous, event-driven architecture**. A hardware timer schedules periodic work, an external interrupt handles the push button, and the main loop processes events through a state machine. Long blocking delays are avoided in the final application.

## Main Features

- TMP36 temperature measurement
- Four operating states: `NORMAL`, `COOLING_LOW`, `COOLING_HIGH`, and `CRITICAL`
- PWM-controlled DC motor used as a cooling fan
- Green and red LED status indication
- Piezo alarm for critical temperature
- Push button handled through an external interrupt
- Timer1 interrupt for periodic scheduling
- EEPROM storage with validation and write-on-change behavior
- Serial Monitor output for testing and debugging
- NPN transistor and flyback diode for safe motor control

## Hardware

| Component | Arduino connection / role |
|---|---|
| TMP36 temperature sensor | `A0` |
| Push button | `D2` (`INT0`) |
| DC motor control | `D3` (PWM, through NPN transistor) |
| Red LED | `D8` |
| Green LED | `D9` |
| Piezo buzzer | `D10` |
| NPN transistor | Motor driver |
| 1 kΩ resistor | Transistor base resistor |
| Flyback diode | Motor inductive-kick protection |
| LED resistors | Current limiting |
| Breadboard | Circuit interconnection |

The motor is **not driven directly from an Arduino GPIO pin**. Arduino pin D3 drives the base of an NPN transistor through a resistor. The transistor switches the motor current, while a flyback diode is connected across the motor to protect the circuit from inductive voltage spikes.

## Pin Configuration

```cpp
const byte TEMP_PIN   = A0;
const byte BUTTON_PIN = 2;
const byte MOTOR_PIN  = 3;
const byte RED_LED    = 8;
const byte GREEN_LED  = 9;
const byte BUZZER_PIN = 10;
```

## System States

The application uses a main-loop state machine.

| State | Temperature condition | Motor | Indicators |
|---|---|---|---|
| `NORMAL` | Below cooling threshold | Off | Green LED on |
| `COOLING_LOW` | From cooling threshold to high-cooling threshold | ~40% PWM | Green LED on |
| `COOLING_HIGH` | Higher temperature, below critical limit | ~75% PWM | Red LED blinks |
| `CRITICAL` | `>= 45 °C` | 100% PWM | Red LED blinks and piezo alarm sounds |

The user-selectable cooling threshold cycles between **30 °C, 32 °C, and 34 °C**. The critical temperature is fixed at **45 °C**.

## Event-Driven Design

The final application does not rely on `delay()` for normal operation.

### Timer1 interrupt

Timer1 is configured in CTC mode to generate an interrupt every **100 ms**. The interrupt service routine performs only short scheduling work by setting flags.

- Output updates: every 100 ms
- Temperature sampling: every 500 ms

Time-consuming operations such as `analogRead()`, serial output, EEPROM access, and state processing are performed in `loop()`, not inside the ISR.

### External interrupt

The push button is connected to Arduino D2 (`INT0`) and configured with `FALLING` mode. Its ISR only sets a `buttonFlag`. The button event is then processed by the main loop, including software debouncing.

## Temperature Measurement

The TMP36 output is read using the Arduino ADC:

```cpp
int sensorValue = analogRead(TEMP_PIN);
float voltage = sensorValue * (5.0 / 1023.0);
temperature = (voltage - 0.5) * 100.0;
```

The TMP36 has a 500 mV offset at 0 °C and changes by approximately 10 mV per °C.

## Motor Control

Motor speed is controlled using PWM on D3:

- `0` — motor off
- `102` — approximately 40%
- `191` — approximately 75%
- `255` — 100%

The motor therefore increases cooling effort as the measured temperature rises.

## EEPROM

The program stores persistent configuration in EEPROM using a structure containing:

- magic value
- data version
- cooling threshold
- alarm enabled/disabled setting

The magic value and version are checked during startup. If the EEPROM contents are invalid, default settings are loaded.

Before writing, the program compares the new settings with the stored settings. EEPROM is therefore updated only when a value has actually changed (**write-on-change**), reducing unnecessary EEPROM wear.

## Building and Running in Tinkercad

1. Create an Arduino Uno circuit in Tinkercad Circuits.
2. Wire the components according to the pin table above.
3. Verify the TMP36 orientation: `Vs -> 5V`, `Vout -> A0`, `GND -> GND`.
4. Verify the motor driver: `D3 -> resistor -> NPN base`, emitter to GND, collector to motor negative.
5. Connect the motor positive terminal to the supply and place the flyback diode across the motor with the cathode toward the positive side.
6. Paste the Arduino source code into the Code editor.
7. Start the simulation.
8. Open Serial Monitor and change the TMP36 temperature to test each state.

## Suggested Tests

- Set temperature below the cooling threshold: motor should stop and the green LED should be on.
- Raise temperature above the cooling threshold: motor should start at low PWM.
- Raise temperature further: motor should increase speed and the red LED should blink.
- Set temperature to 45 °C or higher: motor should run at full speed and the piezo alarm should activate.
- Press the button and verify that the cooling threshold changes.
- Restart/power-cycle the system and verify that EEPROM settings persist.

## Assignment Requirements Covered

- Asynchronous/event-driven design
- No busy-wait based application logic
- External interrupt
- Hardware timer interrupt
- Timer-set event flags
- Main-loop state machine
- PWM output/control
- EEPROM persistence of multiple values
- EEPROM magic/version validation
- EEPROM write-on-change
- LED, motor, piezo, and serial user feedback
- Safe motor switching using a transistor and flyback diode

## Notes

The project was developed and tested using **Tinkercad Circuits with an Arduino Uno R3**. When building the circuit physically, motor current and power-supply limits must be checked for the selected motor and transistor.
