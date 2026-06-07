#include <PDM.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

#define AUDIO_THRESHOLD 50
#define MOTION_THRESHOLD 0.80

#define SOUND_HOLD_TIME 1500

#define LIGHT_LOW 100
#define LIGHT_HIGH 140

#define PROX_NEAR_ON 10
#define PROX_NEAR_OFF 3

short sampleBuffer[256];
volatile int samplesRead = 0;

float baseAx = 0, baseAy = 0, baseAz = 0;

int clearVal = 0;
int proxVal = 0;
int proxBase = 0;
int micLevel = 0;

int dark = 0;
int near = 0;

unsigned long lastSoundTime = 0;

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void setup() {
  Serial.begin(115200);

  while (!Serial) {
    ;
  }

  delay(1000);
  Serial.println("Program started");

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  Serial.println("Calibrating IMU... keep board still");

  float sx = 0, sy = 0, sz = 0;
  int count = 0;

  while (count < 20) {
    if (IMU.accelerationAvailable()) {
      float x, y, z;
      IMU.readAcceleration(x, y, z);

      sx += x;
      sy += y;
      sz += z;

      count++;
      delay(20);
    }
  }

  baseAx = sx / 20;
  baseAy = sy / 20;
  baseAz = sz / 20;

  Serial.println("Calibrating proximity... keep area CLEAR");

  int sum = 0;
  int samples = 0;

  while (samples < 20) {
    if (APDS.proximityAvailable()) {
      sum += APDS.readProximity();
      samples++;
      delay(20);
    }
  }

  proxBase = sum / 20;

  PDM.onReceive(onPDMdata);

  if (!PDM.begin(1, 16000)) {
    Serial.println("Failed to start PDM microphone.");
    while (1);
  }

  Serial.println("Workspace classifier started");
}

void loop() {
  if (samplesRead > 0) {
    long sum = 0;

    for (int i = 0; i < samplesRead; i++) {
      sum += abs(sampleBuffer[i]);
    }

    micLevel = sum / samplesRead;
    samplesRead = 0;

    if (micLevel > AUDIO_THRESHOLD) {
      lastSoundTime = millis();
    }
  }

  if (APDS.colorAvailable()) {
    int r, g, b, c;
    APDS.readColor(r, g, b, c);
    clearVal = (clearVal + c) / 2;
  }

  // Proximity
  if (APDS.proximityAvailable()) {
    int p = APDS.readProximity();
    proxVal = (proxVal + p) / 2;
  }

  int adjustedProx = proxVal - proxBase;

  float motionVal = 0;

  if (IMU.accelerationAvailable()) {
    float x, y, z;
    IMU.readAcceleration(x, y, z);

    float dx = x - baseAx;
    float dy = y - baseAy;
    float dz = z - baseAz;

    motionVal = sqrt(dx * dx + dy * dy + dz * dz);
  }

  if (clearVal < LIGHT_LOW) {
    dark = 1;
  } else if (clearVal > LIGHT_HIGH) {
    dark = 0;
  }

  if (adjustedProx > PROX_NEAR_ON) {
    near = 1;
  } else if (adjustedProx < PROX_NEAR_OFF) {
    near = 0;
  }

  int sound = (millis() - lastSoundTime < SOUND_HOLD_TIME) ? 1 : 0;
  int moving = motionVal > MOTION_THRESHOLD;

  String label;

  if (sound && !dark && moving && near) {
    label = "NOISY_BRIGHT_MOVING_NEAR";
  } 
  else if (!sound && dark && !moving && near) {
    label = "QUIET_DARK_STEADY_NEAR";
  } 
  else if (sound && !dark && !moving && !near) {
    label = "NOISY_BRIGHT_STEADY_FAR";
  } 
  else {
    label = "QUIET_BRIGHT_STEADY_FAR";
  }

  Serial.print("raw,mic=");
  Serial.print(micLevel);
  Serial.print(",clear=");
  Serial.print(clearVal);
  Serial.print(",motion=");
  Serial.print(motionVal, 4);
  Serial.print(",prox=");
  Serial.println(proxVal);

  Serial.print("flags,sound=");
  Serial.print(sound);
  Serial.print(",dark=");
  Serial.print(dark);
  Serial.print(",moving=");
  Serial.print(moving);
  Serial.print(",near=");
  Serial.println(near);

  Serial.print("state,");
  Serial.println(label);

  delay(200);
}