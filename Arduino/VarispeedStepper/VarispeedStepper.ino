// Varispeed Stepper
// (c) Copyright 2017 MCQN Ltd.
//
// Code to drive a stepper motor at a constant speed, adjustable
// via a potentiometer
//
// Hardware:
// Big easy driver
// Speed potentiometer
#include "Arduino.h"
#include "DigitalIO.h"

// Pin assignations
// Pin wired to the step pin on the Big Easy Driver
const int kStepPin = 4;
// Pin wired to the direction pin on the Big Easy Driver
const int kDirectionPin = 3;
// Pin wired to mid-point of a pot wired between 5V and GND, to
// measure the fine-tuning setting
const int kSpeedPotPin = A0;
// Pin which, when pulled to ground, switches the rotation on
const int kActivatePin = 2;
// Pin wired to the enable pin on the Big Easy Driver
const int kEnablePin = 8;
// The enable pin is active high
const int kEnabled = HIGH;
const int kDisabled = LOW;

// How many microseconds we should keep the step pin high
const int kStepPulseLength = 50;

// Steps per revolution
const long kStepsPerRevolution = 400 * 8; // 8-step microstepping
// Baseline speed, in rpm
const long kBaselineRpm = 12;
const float kBaselineStepsPerSecond = kStepsPerRevolution * kBaselineRpm / 60;
const long kMicrosecondsInOneSecond = 1000L * 1000;
// Step intervals are all in microseconds
const long kBaselineStepInterval = kMicrosecondsInOneSecond / kBaselineStepsPerSecond;
const long kStoppedStepInterval = 0x42ff/2;
const float kVariableRangeRpm = 1;
const long kVariableStepsPerSecond = kStepsPerRevolution * kVariableRangeRpm / 60;
//const long kVariableStepRange = kMicrosecondsInOneSecond / kVariableStepsPerSecond;
const long kVariableStepRange = 800;
const long kDesiredLowerStepIntervalLimit = kBaselineStepInterval - (kVariableStepRange/2);
const long kMinimumStepIntervalLimit = 40;
const long kLowerStepIntervalLimit = kMinimumStepIntervalLimit < kDesiredLowerStepIntervalLimit ? kDesiredLowerStepIntervalLimit : kMinimumStepIntervalLimit;

// How many microseconds we should shorten the interval time by
// when accelerating up to speed.  Calculated through experimentation
const long kAcceleration = 200;
// How many microseconds we should lengthen the interval time by
// when slowing to a stop.  Calculated through experimentation
const long kDeceleration = 5;

long gConfiguredStepInterval = kBaselineStepInterval;
unsigned long gStartIntervalTime = millis();
long gStartIntervalStepCount = 0;
volatile long gTargetStepInterval = kStoppedStepInterval;
volatile long gCurrentStepInterval = kStoppedStepInterval;
volatile long gStepCount = 0;


ISR(TIMER1_COMPA_vect)
{
static boolean state = false;
  if (state)
  {
    // It's on, turn it off and wait for specified time
    fastDigitalWrite(kStepPin, LOW);
    TCCR1B = 0;                         // stop timer
    TCNT1 = 0;                          // count back to zero
    TCCR1B = bit(WGM12) | bit(CS11);    // CTC, scale to clock / 8
    // time before timer fires (zero relative)
    // multiply by two because we are on a prescaler of 8
    OCR1A = (gCurrentStepInterval * 2) - (1 * 2) - 1;     
  }
  else
  {
    // Start a step...
    fastDigitalWrite(kStepPin, HIGH);
    gStepCount++;
    // ...and ask to be called again quickly to turn it off
    TCCR1B = 0;                         // stop timer
    TCNT1 = 0;                          // count back to zero
    TCCR1B = bit(WGM12) | bit(CS11);    // CTC, scale to clock / 8
    // time before timer fires (zero relative)
    // multiply by two because we are on a prescaler of 8
    OCR1A = (kStepPulseLength * 2) - (1 * 2) - 1;     
  }
  state = !state;  // toggle
}


// the setup function runs once when you press reset or power the board
void setup() {
  Serial.begin(9600);
  Serial.println("VarispeedStepper");

  Serial.println("================");
  Serial.println("Configuration:");
  Serial.print("kBaselineStepInterval = ");
  Serial.println(kBaselineStepInterval);
  Serial.print("kStoppedStepInterval = ");
  Serial.println(kStoppedStepInterval);
  Serial.print("kMicrosecondsInOneSecond = ");
  Serial.println(kMicrosecondsInOneSecond);
  Serial.print("kBaselineStepsPerSecond = ");
  Serial.println(kBaselineStepsPerSecond);
  Serial.print("kVariableStepRange = ");
  Serial.println(kVariableStepRange);
  Serial.print("kLowerStepLimit = ");
  Serial.println(kLowerStepIntervalLimit);
  Serial.println();

  // Configure IO pins
  pinMode(kStepPin, OUTPUT);
  pinMode(kDirectionPin, OUTPUT);
  digitalWrite(kDirectionPin, HIGH);
  pinMode(kSpeedPotPin, INPUT);
  pinMode(kActivatePin, INPUT_PULLUP);
  pinMode(kEnablePin, OUTPUT);
  digitalWrite(kEnablePin, kDisabled);

  Serial.println("Let's go!");
  // set up Timer 1
  TCCR1A = 0;          // normal operation
  TCCR1B = bit(WGM12) | bit(CS10);   // CTC, no pre-scaling
  OCR1A =  999;       // compare A register value (1000 * clock speed)
  TIMSK1 = bit (OCIE1A);             // interrupt on Compare A Match
}

// the loop function runs over and over again forever
void loop() {
  // Adjust our settings
  gConfiguredStepInterval = kLowerStepIntervalLimit + map(analogRead(kSpeedPotPin), 0, 1023, 0, kVariableStepRange);
  //gConfiguredStepInterval = map(analogRead(kSpeedPotPin), 0, 1023, 40, 800);

  // Check for activation
  if (digitalRead(kActivatePin) == LOW)
  {
    // We're aiming for running
    gTargetStepInterval = gConfiguredStepInterval;
  }
  else
  {
    // We're trying to stop
    gTargetStepInterval = kStoppedStepInterval;
  }

/*
  if (millis() % 5000 < 50)
  {
    Serial.print("gConfiguredStepInterval = ");
    Serial.println(gConfiguredStepInterval);
    Serial.print("gCurrentStepInterval = ");
    Serial.println(gCurrentStepInterval);
    Serial.print("gCurrentStepInterval (16-bit) = ");
    Serial.println((int16_t)gCurrentStepInterval);
    Serial.print("gTargetStepInterval = ");
    Serial.println(gTargetStepInterval);
  }
*/
  if (millis() % (300*1000UL) < 80)
  {
    unsigned long endIntervalTime = millis();
    long stepCount = gStepCount - gStartIntervalStepCount;
    float revolutions = stepCount * 1.0 / kStepsPerRevolution;
    float minutesElapsed = (endIntervalTime - gStartIntervalTime) / (1000.0 * 60);
    Serial.print("### Steps: ");
    Serial.print(stepCount);
    Serial.print(" Revolutions: ");
    Serial.print(revolutions);
    Serial.print(" Time: ");
    Serial.print(endIntervalTime - gStartIntervalTime);
    Serial.print(" Minutes elapsed: ");
    Serial.print(minutesElapsed);
    Serial.print(" Speed: ");
    Serial.println(revolutions / minutesElapsed);
    gStartIntervalTime = millis();
    gStartIntervalStepCount = gStepCount;
  }

  // We're running!
  if (gCurrentStepInterval > gTargetStepInterval)
  {
    digitalWrite(kEnablePin, kEnabled);
    long newInterval = gCurrentStepInterval -= kAcceleration;
    newInterval = constrain(newInterval, gTargetStepInterval, kStoppedStepInterval);
    noInterrupts();
    gCurrentStepInterval = newInterval;
    interrupts();
  }
  else if (gCurrentStepInterval < gTargetStepInterval)
  {
    digitalWrite(kEnablePin, kEnabled);
    long newInterval = gCurrentStepInterval + kDeceleration;
    newInterval = constrain(newInterval, kLowerStepIntervalLimit, gTargetStepInterval);
    noInterrupts();
    gCurrentStepInterval = newInterval;
    interrupts();
  }
  else if (gCurrentStepInterval == kStoppedStepInterval)
  {
    digitalWrite(kEnablePin, kDisabled);
  }
  delay(3);
}

