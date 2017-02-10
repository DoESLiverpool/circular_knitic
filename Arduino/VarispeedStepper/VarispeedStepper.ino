// Varispeed Stepper
// (c) Copyright 2017 MCQN Ltd.
//
// Code to drive a stepper motor at a constant speed, adjustable
// via a potentiometer
//
// Hardware:
// Big easy driver
// Speed potentiometer
// [optional] Acceleration potentiometer

// Pin assignations
// Pin wired to the step pin on the Big Easy Driver
const int kStepPin = 3;
const int kDirectionPin = 2;
const int kSpeedPotPin = A0;
const int kAccelerationPotPin = A1;
const int kActivatePin = 12;

// Design notes
// The "Baseline" values are the centre-marker for the speed or
// acceleration.  The potentiometers can vary either of those by
// 

// Steps per revolution
const long kStepsPerRevolution = 200 * 8; // 8-step microstepping
// Baseline speed, in rpm
const long kBaselineRpm = 80;
const float kBaselineStepsPerSecond = kStepsPerRevolution * kBaselineRpm / 60;
const long kMicrosecondsInOneSecond = 1000L * 1000;
// Step intervals are all in microseconds
const long kBaselineStepInterval = kMicrosecondsInOneSecond / kBaselineStepsPerSecond;
const long kInitialStepInterval = kMicrosecondsInOneSecond / 2;
const long kBaselineAcceleration = 1500;
const float kVariableRangeRpm = 1;
const long kVariableStepsPerSecond = kStepsPerRevolution * kVariableRangeRpm / 60;
const long kVariableStepRange = kMicrosecondsInOneSecond / kVariableStepsPerSecond;
const long kLowerStepLimit = 0; //kBaselineStepInterval - (kVariableStepRange/2);
const long kVariableAccelerationRange = 1000;
const long kLowerAccelerationLimit = kBaselineAcceleration - (kVariableAccelerationRange/2);
// How many microseconds we should keep the step pin high
const long kStepDelay = 50;

// The desired step interval to use when running, as set
// by the speed potentiometer
long gConfiguredStepInterval = kBaselineStepInterval;
// The desired acceleration speed, as set by the acceleration
// potentiometer
long gConfiguredAcceleration = kBaselineAcceleration;

// Speed variables
// The speed we want to be running at
long gTargetStepInterval = kInitialStepInterval;
// The speed we're currently running at
long gCurrentStepInterval = kInitialStepInterval;

// Keep some stats on our performance
int gStepCount = 0;
unsigned long gActiveStart = 0L;
unsigned long gActiveStop = 0L;

void setup() {
  Serial.begin(9600);
  Serial.println("VarispeedStepper");
  Serial.println("================");
  Serial.println("Configuration:");
  Serial.print("kBaselineStepInterval = ");
  Serial.println(kBaselineStepInterval);
  Serial.print("kInitialStepInterval = ");
  Serial.println(kInitialStepInterval);
  Serial.print("kMicrosecondsInOneSecond = ");
  Serial.println(kMicrosecondsInOneSecond);
  Serial.print("kBaselineStepsPerSecond = ");
  Serial.println(kBaselineStepsPerSecond);
  Serial.print("kVariableStepRange = ");
  Serial.println(kVariableStepRange);
  Serial.print("kLowerStepLimit = ");
  Serial.println(kLowerStepLimit);
  Serial.println();

  // Configure IO pins
  pinMode(kStepPin, OUTPUT);
  pinMode(kDirectionPin, OUTPUT);
  pinMode(kSpeedPotPin, INPUT);
  pinMode(kAccelerationPotPin, INPUT);
  pinMode(kActivatePin, INPUT);

  Serial.println("Let's go!");
}

void loop() {
  // Adjust our settings
  gConfiguredStepInterval = kLowerStepLimit + map(analogRead(kSpeedPotPin), 0, 1023, 0, kVariableStepRange);
  gConfiguredAcceleration = kLowerAccelerationLimit + map(analogRead(kAccelerationPotPin), 0, 1023, 0, kVariableAccelerationRange);

  // Check for activation
  if (digitalRead(kActivatePin) == HIGH)
  {
    // We're aiming for running
    gTargetStepInterval = gConfiguredStepInterval;
  }
  else
  {
    // We're trying to stop
    gTargetStepInterval = kInitialStepInterval;
    if (gActiveStop == 0)
    {
      gActiveStop = micros();
    }
  }

  if (gTargetStepInterval == kInitialStepInterval)
  {
    // We're stopped
    if ((millis() % 1000) == 0)
    {
      // Output some stats every second while we're stopped
      Serial.print("Speed: ");
      Serial.println(gConfiguredStepInterval);
      Serial.print("Accel: ");
      Serial.println(gConfiguredAcceleration);
      if (gActiveStart)
      {
        // Work out what speed we achieved
        Serial.println();
        Serial.print("Running time: ");
        Serial.print((gActiveStop-gActiveStart)/(1000.0*1000));
        Serial.println(" seconds");
        Serial.print("Steps: ");
        Serial.println(gStepCount);
        Serial.print("Actual speed: ");
        Serial.print((1.0*gStepCount/kStepsPerRevolution)/((gActiveStop-gActiveStart)/(1000.0*1000*60)));
        Serial.println(" rpm");
        gStepCount = 0;        
        gActiveStart = 0;
        gActiveStop = 0;
      }
    }
  }
  else
  {
    // We're running!
    if (gCurrentStepInterval < gTargetStepInterval)
    {
      gCurrentStepInterval += gConfiguredAcceleration;
    }
    else if (gCurrentStepInterval > gTargetStepInterval)
    {
      gCurrentStepInterval -= gConfiguredAcceleration;
    }
    else
    {
      // We're reached our target
      if (gActiveStart == 0)
      {
        // We've just come up to speed on starting
        gActiveStart = micros();
      }
    }
    gCurrentStepInterval = constrain(gCurrentStepInterval, kLowerStepLimit, kInitialStepInterval);
    int wait = micros() % gCurrentStepInterval;
    delayMicroseconds(wait);
    digitalWrite(kStepPin, HIGH);
    delayMicroseconds(kStepDelay);
    digitalWrite(kStepPin, LOW);
    gStepCount++;
  }
}

