/* 

  Firmware with three intensity, blinking, dazzle, and night mode.
  Inspired from: Factory firmware for HexBright FLEX - v2.4  Dec 6, 2012
  
*/

#include <math.h>
#include <Wire.h>

// Settings
#define OVERTEMP                340
#define ACC_SENSITIVITY         4
#define NIGHT_MODE_TIMEOUT      20000
// Constants
#define ACC_ADDRESS             0x4C
#define ACC_REG_XOUT            0
#define ACC_REG_YOUT            1
#define ACC_REG_ZOUT            2
#define ACC_REG_TILT            3
#define ACC_REG_INTS            6
#define ACC_REG_MODE            7
// Pin assignments
#define DPIN_RLED_SW            2
#define DPIN_GLED               5
#define DPIN_PWR                8
#define DPIN_DRV_MODE           9
#define DPIN_DRV_EN             10
#define DPIN_ACC_INT            3
#define APIN_TEMP               0
#define APIN_CHARGE             3
// Modes
#define MODE_OFF                0
#define MODE_LOW                1
#define MODE_MED                2
#define MODE_HIGH               3
#define MODE_BLINKING           4
#define MODE_BLINKING_PREVIEW   5
#define MODE_DAZZLING           6
#define MODE_NIGHT_OFF          7
#define MODE_NIGHT_ON           8
#define MODE_NIGHT_PREVIEW      9

// State
byte mode = 0;
unsigned long btnTime = 0;
boolean btnDown = false;

void setup()
{
  // We just powered on!  That means either we got plugged 
  // into USB, or the user is pressing the power button.
  pinMode(DPIN_PWR,      INPUT);
  digitalWrite(DPIN_PWR, LOW);

  // Initialize GPIO
  pinMode(DPIN_RLED_SW,  INPUT);
  pinMode(DPIN_GLED,     OUTPUT);
  pinMode(DPIN_DRV_MODE, OUTPUT);
  pinMode(DPIN_DRV_EN,   OUTPUT);
  pinMode(DPIN_ACC_INT,  INPUT);
  digitalWrite(DPIN_DRV_MODE, LOW);
  digitalWrite(DPIN_DRV_EN,   LOW);
  digitalWrite(DPIN_ACC_INT,  HIGH);
  
  // Initialize serial busses
  Serial.begin(9600);
  Wire.begin();
  
  // Configure accelerometer
  byte config[] = {
    ACC_REG_INTS,  // First register (see next line)
    0xE4,  // Interrupts: shakes, taps
    0x00,  // Mode: not enabled yet
    0x00,  // Sample rate: 120 Hz
    0x0F,  // Tap threshold
    0x10   // Tap debounce samples
  };
  Wire.beginTransmission(ACC_ADDRESS);
  Wire.write(config, sizeof(config));
  Wire.endTransmission();

  // Enable accelerometer
  byte enable[] = {ACC_REG_MODE, 0x01};  // Mode: active!
  Wire.beginTransmission(ACC_ADDRESS);
  Wire.write(enable, sizeof(enable));
  Wire.endTransmission();
  
  btnTime = millis();
  btnDown = digitalRead(DPIN_RLED_SW);
  mode = MODE_OFF;

  Serial.println("Powered up!");
}

void loop()
{
  static unsigned long lastTempTime, lastAccTime, lastDazzleTime, lastAccCheckTime, lastMovedTime;
  unsigned long time = millis();
  
  // Check the state of the charge controller
  int chargeState = analogRead(APIN_CHARGE);
  if (chargeState < 128)  // Low - charging
  {
    digitalWrite(DPIN_GLED, (time&0x1000)?LOW:HIGH);
  }
  else if (chargeState > 768) // High - charged
  {
    digitalWrite(DPIN_GLED, HIGH);
  }
  else // Hi-Z - shutdown
  {
    digitalWrite(DPIN_GLED, LOW);    
  }
  
  // Check the temperature sensor
  if (time-lastTempTime > 1000)
  {
    lastTempTime = time;

    int temperature = analogRead(APIN_TEMP);
    Serial.print("Temp: ");
    Serial.println(temperature);
    if (temperature > OVERTEMP && mode != MODE_OFF)
    {
      Serial.println("Overheating!");

      for (int i = 0; i < 6; i++)
      {
        digitalWrite(DPIN_DRV_MODE, LOW);
        delay(100);
        digitalWrite(DPIN_DRV_MODE, HIGH);
        delay(100);
      }
      digitalWrite(DPIN_DRV_MODE, LOW);
      mode = MODE_LOW;
    }
  }

  // Do whatever this mode does
  switch (mode)
  {
  case MODE_BLINKING:
  case MODE_BLINKING_PREVIEW:
    digitalWrite(DPIN_DRV_EN, (time%300)<75);
    break;
  case MODE_DAZZLING:
    if (time-lastDazzleTime < 10) break;
    lastDazzleTime = time;
    
    digitalWrite(DPIN_DRV_EN, random(4)<1);
    break;
  case MODE_NIGHT_OFF:
    digitalWrite(DPIN_RLED_SW, (time%3000)<500);
  case MODE_NIGHT_PREVIEW:
  case MODE_NIGHT_ON:
    // Check the accelerometer to see if we moved
    if (time-lastAccCheckTime > 500)
    {
      lastAccCheckTime = time;
      if(moved())
      {
        Serial.println("Moved!");
        lastMovedTime = time;
      }
    }
  }
    
  // Check for mode changes
  byte newMode = mode;
  byte newBtnDown = digitalRead(DPIN_RLED_SW);
  switch (mode)
  {
  case MODE_OFF:
    if (btnDown && !newBtnDown && (time-btnTime)>20)
      newMode = MODE_LOW;
    if (btnDown && newBtnDown && (time-btnTime)>500)
      newMode = MODE_BLINKING_PREVIEW;
    break;
  case MODE_LOW:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_MED;
    if (btnDown && newBtnDown && (time-btnTime)>500)
      newMode = MODE_NIGHT_PREVIEW;
    break;
  case MODE_MED:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_HIGH;
    if (btnDown && newBtnDown && (time-btnTime)>500)
      newMode = MODE_NIGHT_PREVIEW;
    break;
  case MODE_HIGH:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_OFF;
    if (btnDown && newBtnDown && (time-btnTime)>500)
      newMode = MODE_NIGHT_PREVIEW;
    break;
  case MODE_BLINKING_PREVIEW:
    // This mode exists just to ignore this button release.
    if (btnDown && !newBtnDown)
      newMode = MODE_BLINKING;
    break;
  case MODE_BLINKING:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_DAZZLING;
    break;
  case MODE_DAZZLING:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_OFF;
    break;
  case MODE_NIGHT_OFF:
    if (time-lastMovedTime<1000)
      newMode = MODE_NIGHT_ON;
    break;
  case MODE_NIGHT_ON:
    if (btnDown && !newBtnDown && (time-btnTime)>50)
      newMode = MODE_OFF;
    else
      if (time-lastMovedTime>NIGHT_MODE_TIMEOUT)
        newMode = MODE_NIGHT_OFF;
    break;
  case MODE_NIGHT_PREVIEW:
    // This mode exists just to ignore this button release.
    if (btnDown && !newBtnDown)
      newMode = MODE_NIGHT_ON;
    break;
  }

  // Do the mode transitions
  if (newMode != mode)
  {
    switch (newMode)
    {
    case MODE_OFF:
      Serial.println("Mode = off");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, LOW);
      digitalWrite(DPIN_DRV_MODE, LOW);
      digitalWrite(DPIN_DRV_EN, LOW);
      break;
    case MODE_LOW:
      Serial.println("Mode = low");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, 64);
      break;
    case MODE_MED:
      Serial.println("Mode = medium");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    case MODE_HIGH:
      Serial.println("Mode = high");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    case MODE_BLINKING:
    case MODE_BLINKING_PREVIEW:
      Serial.println("Mode = blinking");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      break;
    case MODE_DAZZLING:
      Serial.println("Mode = dazzling");
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      break;
    case MODE_NIGHT_OFF:
      Serial.println("Mode = night (off)");
      pinMode(DPIN_RLED_SW, OUTPUT);
      digitalWrite(DPIN_RLED_SW, HIGH);
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, LOW);
      digitalWrite(DPIN_DRV_EN, LOW);
      break;
    case MODE_NIGHT_PREVIEW: 
      Serial.println("Mode = night (preview)");
      notification();
      break; 
    case MODE_NIGHT_ON:
      Serial.println("Mode = night (on)");
      pinMode(DPIN_RLED_SW,  INPUT);
      digitalWrite(DPIN_RLED_SW, LOW);
      newBtnDown = LOW;
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      analogWrite(DPIN_DRV_EN, 255);
      break;
    }
    
    mode = newMode;
  }

  // Remember button state so we can detect transitions
  if (newBtnDown != btnDown)
  {
    btnTime = time;
    btnDown = newBtnDown;
    delay(50);
  }
}

void notification() {
  for(int i = 0; i<5;i++ ){
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, HIGH);
      digitalWrite(DPIN_DRV_MODE, HIGH);
      analogWrite(DPIN_DRV_EN, 255);
      delay(20);
      pinMode(DPIN_PWR, OUTPUT);
      digitalWrite(DPIN_PWR, LOW);
      digitalWrite(DPIN_DRV_MODE, LOW);
      digitalWrite(DPIN_DRV_EN, LOW);
      delay(20);
   }
}

void readAccel(char *acc)
{
  while (1)
  {
    Wire.beginTransmission(ACC_ADDRESS);
    Wire.write(ACC_REG_XOUT);
    Wire.endTransmission(false);       // End, but do not stop!
    Wire.requestFrom(ACC_ADDRESS, 3);  // This one stops.

    for (int i = 0; i < 3; i++)
    {
      if (!Wire.available())
        continue;
      acc[i] = Wire.read();
      if (acc[i] & 0x40)  // Indicates failed read; redo!
        continue;
      if (acc[i] & 0x20)  // Sign-extend
        acc[i] |= 0xC0;
    }
    break;
  }
}

int distance(int a, int b)
{
  if(b>a)
    return abs((b-a+128)%256-128);
  return abs((a-b+128)%256-128);
}

boolean moved()
{
  static byte lastPositions[3];
  int distances[3];
  char newPositions[3];
  readAccel(newPositions);
  int count = 0;
  boolean result = false;
  for(int i=0; i<3; i++) {
    distances[i] = distance(lastPositions[i],newPositions[i]);
    if(distances[i]> ACC_SENSITIVITY and distances[i]<50) 
    {
      result = true;
    }
    count += distances[i] > ACC_SENSITIVITY ? 1 : 0;
  }
  
  for(int i=0; i<3; i++) {
    lastPositions[i] = newPositions[i];
  }

  return result;
}

