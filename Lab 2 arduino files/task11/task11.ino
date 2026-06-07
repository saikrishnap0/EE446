#include <Arduino_HS300x.h>
#include <Arduino_APDS9960.h>
#include <Arduino_BMI270_BMM150.h>

#define HUMID_THRESHOLD 5.0
#define TEMP_THRESHOLD 2.0
#define MAG_THRESHOLD 100.0
#define LIGHT_THRESHOLD 2.0

float baseRh, baseTemp;
float baseMx, baseMy, baseMz;
int baseClear;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!HS300x.begin()) {
    Serial.println("Failed to initialize HS3003 sensor.");
    while (1);
  }

  if (!APDS.begin()) {
    Serial.println("Failed to initialize APDS9960 sensor.");
    while (1);
  }

  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU.");
    while (1);
  }

  Serial.println("Calibrating... keep board normal");

  baseRh = HS300x.readHumidity();
  baseTemp = HS300x.readTemperature();

  while (!IMU.magneticFieldAvailable());
  IMU.readMagneticField(baseMx, baseMy, baseMz);

  int r, g, b, c;
  while (!APDS.colorAvailable());
  APDS.readColor(r, g, b, c);
  baseClear = c;

  Serial.println("Task 11 started");
}

void loop() {
  float rh = HS300x.readHumidity();
  float temp = HS300x.readTemperature();

  float mx, my, mz;
  IMU.readMagneticField(mx, my, mz);

  int r, g, b, clearVal;
  APDS.readColor(r, g, b, clearVal);

  float mag = sqrt((mx - baseMx) * (mx - baseMx) +
                   (my - baseMy) * (my - baseMy) +
                   (mz - baseMz) * (mz - baseMz));

  int humid_jump = (rh - baseRh > HUMID_THRESHOLD);
  int temp_rise = (temp - baseTemp > TEMP_THRESHOLD);
  int mag_shift = (mag > MAG_THRESHOLD);
  int light_or_color_change = (abs(clearVal - baseClear) > LIGHT_THRESHOLD);

  String label = "BASELINE_NORMAL";

  if (mag_shift) {
    label = "MAGNETIC_DISTURBANCE_EVENT";
  } else if (light_or_color_change) {
    label = "LIGHT_OR_COLOR_CHANGE_EVENT";
  } else if (humid_jump || temp_rise) {
    label = "BREATH_OR_WARM_AIR_EVENT";
  } else {
    label = "BASELINE_NORMAL";
  }

  Serial.print("raw,rh=");
  Serial.print(rh);
  Serial.print(",temp=");
  Serial.print(temp);
  Serial.print(",mag=");
  Serial.print(mag);
  Serial.print(",r=");
  Serial.print(r);
  Serial.print(",g=");
  Serial.print(g);
  Serial.print(",b=");
  Serial.print(b);
  Serial.print(",clear=");
  Serial.println(clearVal);

  Serial.print("flags,humid_jump=");
  Serial.print(humid_jump);
  Serial.print(",temp_rise=");
  Serial.print(temp_rise);
  Serial.print(",mag_shift=");
  Serial.print(mag_shift);
  Serial.print(",light_or_color_change=");
  Serial.println(light_or_color_change);

  Serial.print("event,");
  Serial.println(label);

  delay(500);
}