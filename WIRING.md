# Wiring Diagram


WS2813 LED strip (3 LEDs)   XIAO ESP32-C6
-----------------------------------------
5V                      →  5V (shared)
GND                     →  GND (shared)
DI (data in)            →  GPIO0 (D0)


## Battery voltage divider

```
BAT+ ──[ 330k ]──┬──[ 330k ]── GND
                 │
                 ├──[ 100 nF ]── GND
                 │
              GPIO1 (D1)
```

- Two equal 330 kΩ resistors halve the battery voltage (firmware assumes
  ratio 2.0, `VOLTAGE_DIVIDER` in sensor.c); 4.2 V full charge → 2.1 V at
  the ADC, within the 11 dB attenuation range.
- Standby drain through the divider: ~6.4 µA at 4.2 V.
- The 100 nF cap across the bottom resistor is required: the divider's
  ~165 kΩ source impedance is too high for the ADC's sample-and-hold and
  would read low without it.
- After changing the divider, verify `ADC_CALIBRATION_FACTOR` in sensor.c
  against a multimeter reading — it compensates the sampling error of the
  previous divider and will likely move toward 1.0 with the cap in place.
