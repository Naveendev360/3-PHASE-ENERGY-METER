#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EmonLib.h>
#include <EEPROM.h>

// Phase A pins & calibration
#define V_PIN_A    34
#define I_PIN_A    33
#define V_CAL_A    83.3
#define I_CAL_A    216.4 // Optimized

// Phase B pins & calibration
#define V_PIN_B    35
#define I_PIN_B    25     // Changed for ADC1 stability
#define V_CAL_B    80.2
#define I_CAL_B    171.9// Optimized

// Phase C pins & calibration
#define V_PIN_C    32
#define I_PIN_C    26
#define V_CAL_C    85.8
#define I_CAL_C    171.9// Optimized

#define LCD_ADDR   0x3F

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);
EnergyMonitor emonA, emonB, emonC;

float kWhA = 0, kWhB = 0, kWhC = 0;
static float prevTotalKwh = 0;

unsigned long lastCalcMillis  = 0;
unsigned long lastEEPROMWrite = 0;
unsigned long lastLCDSwitch   = 0;
int lcdPage = 0;

void printSerialHeader() {
  Serial.println();
  Serial.println("Phase |    V (V) |    I (A) |    P (W) |   kWh");
  Serial.println("------------------------------------------------");
}

void printSerialLine(char phase, float V, float I, float P, float e) {
  Serial.printf("  %c   | %7.1f   | %7.2f  | %7.1f  | %7.3f\n",
                phase, V, I, P, e);
}

void setup() {
  Serial.begin(9600);
  Wire.begin(23, 22);

  lcd.begin();
  lcd.backlight();
  lcd.setCursor(3, 1); lcd.print("3-Phase Meter");
  lcd.setCursor(4, 2); lcd.print("Booting...");
  delay(2000);
  lcd.clear();

  emonA.voltage(V_PIN_A, V_CAL_A, 1.7);
  emonA.current(I_PIN_A, I_CAL_A);
  emonB.voltage(V_PIN_B, V_CAL_B, 1.7);
  emonB.current(I_PIN_B, I_CAL_B);
  emonC.voltage(V_PIN_C, V_CAL_C, 1.7);
  emonC.current(I_PIN_C, I_CAL_C);

  EEPROM.begin(512);
  EEPROM.get(0,   kWhA);
  EEPROM.get(4,   kWhB);
  EEPROM.get(8,   kWhC);
  if (isnan(kWhA) || kWhA < 0) kWhA = 0;
  if (isnan(kWhB) || kWhB < 0) kWhB = 0;
  if (isnan(kWhC) || kWhC < 0) kWhC = 0;

  lastCalcMillis  = millis();
  lastEEPROMWrite = millis();
  lastLCDSwitch   = millis();

  printSerialHeader();
}

void loop() {
  unsigned long now = millis();

  // Improved stability with more samples and timeout
  emonA.calcVI(40, 300);
  float Va = emonA.Vrms, Ia = emonA.Irms, Pa = emonA.apparentPower;

  emonB.calcVI(40, 300);
  float Vb = emonB.Vrms, Ib = emonB.Irms, Pb = emonB.apparentPower;

  emonC.calcVI(40, 300);
  float Vc = emonC.Vrms, Ic = emonC.Irms, Pc = emonC.apparentPower;

  // Enhanced filtering to reject noise or faulty data
  if (Va < 10 || Ia < 0.05 || Ia > 100 || Pa < 1) Ia = 0;
  if (Vb < 10 || Ib < 0.05 || Ib > 100 || Pb < 1) Ib = 0;
  if (Vc < 10 || Ic < 0.05 || Ic > 100 || Pc < 1) Ic = 0;

  // Calculate elapsed time in hours for energy calculation
  float dtHours = (now - lastCalcMillis) / 3600000.0;
  kWhA += Pa * dtHours / 1000.0;
  kWhB += Pb * dtHours / 1000.0;
  kWhC += Pc * dtHours / 1000.0;
  lastCalcMillis = now;

  float totalKwh = kWhA + kWhB + kWhC;

  // Write energy to EEPROM every 5 minutes if needed
  if (now - lastEEPROMWrite >= 300000 && (totalKwh - prevTotalKwh) >= 0.005) {
    EEPROM.put(0,   kWhA);
    EEPROM.put(4,   kWhB);
    EEPROM.put(8,   kWhC);
    EEPROM.commit();
    prevTotalKwh    = totalKwh;
    lastEEPROMWrite = now;
  }

  // Print to Serial
  printSerialLine('A', Va, Ia, Pa, kWhA);
  printSerialLine('B', Vb, Ib, Pb, kWhB);
  printSerialLine('C', Vc, Ic, Pc, kWhC);
  Serial.println("------------------------------------------------");
  Serial.printf("Total kWh: %7.3f\n", totalKwh);

  // Display update—unchanged UI
  if (now - lastLCDSwitch >= 3000) {
    lastLCDSwitch = now;
    lcdPage = (lcdPage + 1) % 3;
    lcd.clear();

    switch (lcdPage) {
      case 0:
        lcd.setCursor(0, 0); lcd.print("Phase: A");
        lcd.setCursor(0, 1);
          lcd.print("V:"); lcd.print(Va,1);
          lcd.print(" I:"); lcd.print(Ia,1);
          lcd.print(" P:"); lcd.print((int)Pa);
        lcd.setCursor(0, 2);
          lcd.print("Energy:"); lcd.print(kWhA,3);
        lcd.setCursor(0, 3);
          lcd.print("Total kWh:"); lcd.print(totalKwh,3);
        break;

      case 1:
        lcd.setCursor(0, 0); lcd.print("Phase: B");
        lcd.setCursor(0, 1);
          lcd.print("V:"); lcd.print(Vb,1);
          lcd.print(" I:"); lcd.print(Ib,1);
          lcd.print(" P:"); lcd.print((int)Pb);
        lcd.setCursor(0, 2);
          lcd.print("Energy:"); lcd.print(kWhB,3);
        lcd.setCursor(0, 3);
          lcd.print("Total kWh:"); lcd.print(totalKwh,3);
        break;

      case 2:
        lcd.setCursor(0, 0); lcd.print("Phase: C");
        lcd.setCursor(0, 1);
          lcd.print("V:"); lcd.print(Vc,1);
          lcd.print(" I:"); lcd.print(Ic,1);
          lcd.print(" P:"); lcd.print((int)Pc);
        lcd.setCursor(0, 2);
          lcd.print("Energy:"); lcd.print(kWhC,3);
        lcd.setCursor(0, 3);
          lcd.print("Total kWh:"); lcd.print(totalKwh,3);
        break;
    }
  }
}
