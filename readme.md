## Display Format

All instantaneous measurements are shown in **volts (V)**, **amps (A)**, and **watts (W)** with **4 decimal places** (e.g. `3.4423 V`, `0.1234 A`, `0.4567 W`).  
The totals remain in **mAh** and **mWh** with two decimals.

## GitHub Releases

To download the latest firmware:
1. Go to the [Releases page](../../releases).
2. Download `PowerController-vX.X.X.hex`.
3. Flash it to your Arduino Nano using the Arduino IDE or **avrdude**.

**Using avrdude (example for COM3):**
```bash
avrdude -c arduino -p atmega328p -P COM3 -b 115200 -U flash:w:PowerController-v2.0.0.hex:i
```

## Output Sample
```
INA226 Power Monitor v2.0.0
Ready.
-------------------------------
Voltage: 17.1525 V, Max: 17.2662
Current: 0.0000 A, Max: 0.0000
  [Raw Shunt: 0.0000 mV, Raw Cur Reg: 0]
Power:   0.0000 W, Max: 0.0015
Total:   0.00 mAh, 0.00 mWh
Runtime: 00:00:00
-------------------------------
Voltage: 12.2437 V, Max: 17.2750
Current: 0.7459 A, Max: 0.8188
  [Raw Shunt: 74.6250 mV, Raw Cur Reg: 24443]
Power:   10.0838 W, Max: 10.2386
Total:   2.66 mAh, 32.35 mWh
Runtime: 00:00:30
-------------------------------
```