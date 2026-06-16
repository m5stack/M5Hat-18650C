# M5Hat-18650C

## Overview

### SKU: HAT-18650C

M5Stack **18650C Battery Hat** (V1.33) is a stackable module that provides 18650 Li-ion battery power to your M5Stack device. It includes two on‑board I²C chips for precision battery monitoring and charging:

| Chip | Address | Function |
|---|---|---|
| **INA226AIDGSR** (TI) | 0x41 | 16‑bit voltage / current / power monitor, 10 mΩ shunt |
| **AW32257FCR** (AWINIC) | 0x6A | Li‑ion switch‑mode charger with I²C interface |

**Features:**

- Bus voltage measurement (±1.25 mV resolution)
- Charge / discharge current (±250 µA/LSB, ±8.2 A range)
- Power calculation (INA226 hardware register)
- Charge state from AW32257: Ready / Charging / Done / Fault
- Fault diagnostics: VBUS OVP, thermal shutdown, no battery, etc.
- Boost (OTG) mode detection
- Configurable charge current & voltage via AW32257 registers
- I²C auto‑detection of both chips

## Related Link

- [Document & Datasheet](https://docs.m5stack.com/en/products)

## Required Libraries

- [M5Hat-18650C](https://github.com/m5stack/M5Hat-18650C) — this library
- [M5Unified](https://github.com/m5stack/M5Unified) — M5Stack hardware abstraction layer

## Usage

```cpp
#include <M5Unified.h>
#include <M5Hat_18650C.h>

M5Hat_18650C hat;

void setup() {
    M5.begin();
    hat.begin();

    Serial.printf("INA226: %s, AW32257: %s\n",
        hat.isIna226Found()  ? "OK" : "NO",
        hat.isChargerFound() ? "OK" : "NO");
}

void loop() {
    hat.update();

    // INA226 readings
    Serial.printf("Voltage : %.4f V\n", hat.getBusVoltage());
    Serial.printf("Current : %+.1f mA\n", hat.getCurrent() * 1000.0f);
    Serial.printf("Power   : %+.1f mW\n", hat.getPower() * 1000.0f);

    // AW32257 charge state
    Serial.printf("ChgState: %d\n", (int)hat.getChargeState());
    // 0=Ready, 1=Charging, 2=Done, 3=Fault
    Serial.printf("Charging: %s\n", hat.isCharging() ? "YES" : "NO");
    Serial.printf("Boost   : %s\n", hat.isBoostMode() ? "YES" : "NO");

    delay(2000);
}
```

## API Reference

### INA226 (voltage / current / power)

| Method | Returns | Description |
|---|---|---|
| `getBusVoltage()` | `float` | Battery bus voltage [V] |
| `getShuntVoltage()` | `float` | Shunt voltage drop [mV] |
| `getCurrent()` | `float` | Charge (+) / discharge (−) current [A] |
| `getPower()` | `float` | Power [W] |
| `getBatteryPercentage()` | `float` | SOC estimate [%] (linear from voltage) |
| `setBatteryVoltageRange(vEmpty, vFull)` | `void` | Custom SOC mapping (default 3.0 / 4.2 V) |

### AW32257 (charge control & status)

| Method | Returns | Description |
|---|---|---|
| `getChargeState()` | `charge_state_t` | 0=Ready, 1=Charging, 2=Done, 3=Fault |
| `getFaultDetail()` | `fault_detail_t` | Fault code when state == AW_CHG_FAULT |
| `isCharging()` | `bool` | Combined: INA226 current > 0 OR AW32257 charging |
| `isBoostMode()` | `bool` | True when AW32257 is in boost (OTG) mode |
| `isUSBPresent()` | `bool` | True when bus voltage > 4.5 V |
| `setChargerEnabled(bool)` | `void` | Enable / disable the charger (CEN bit) |
| `setChargeCurrent(mA)` | `void` | Set fast‑charge current (496–2480 mA) |
| `setChargeVoltage(V)` | `void` | Set charge regulation voltage (3.50–4.50 V) |

### Charge state enum

| Value | Meaning |
|---|---|
| `AW_CHG_READY` | Ready (no VBUS, or charge complete standby) |
| `AW_CHG_PROGRESS` | Charge in progress |
| `AW_CHG_DONE` | Charge done |
| `AW_CHG_FAULT` | Fault — check `getFaultDetail()` |

### Fault detail enum

| Value | Meaning |
|---|---|
| `AW_FAULT_NONE` | Normal |
| `AW_FAULT_VBUS_OVP` | VBUS over‑voltage |
| `AW_FAULT_SLEEP` | Sleep mode |
| `AW_FAULT_BAD_ADAPTOR` | Bad adaptor / VBUS < UVLO |
| `AW_FAULT_OUTPUT_OVP` | Output over‑voltage |
| `AW_FAULT_THERMAL` | Thermal shutdown |
| `AW_FAULT_NO_BATTERY` | No battery detected |

### Current sign convention

```
Current > 0  →  battery is charging  (current flows into battery)
Current ≈ 0  →  idle
Current < 0  →  battery is discharging
```

## AW32257 Register Map (datasheet V1.5)

| Reg | Name | Key bits |
|---|---|---|
| 0x00 | Status | B7=OTG, B6=EN_STAT, B3‑B2=Charge state, B2=Boost |
| 0x01 | Control | B3=TE, B2=CEN, B1=HZ_MODE, B0=OPA_MODE |
| 0x02 | VOREG | B7‑B2=Battery regulation voltage (3.50–4.50 V), B1=OTG_PL, B0=OTG_EN |
| 0x03 | Vendor | B7‑B5=Vendor code, B4‑B3=PN, B2‑B0=Revision (default 0x53) |
| 0x04 | ICHG | B7=RESET, B6‑B3=Fast‑charge current, B2‑B0=Termination threshold |
| 0x05 | Special | B4=DPM status, B3=CD pin, B2‑B0=VSP (DPM voltage) |
| 0x06 | Safety | B7‑B4=Max charge current, B3‑B0=Max charge voltage |
| 0x07 | Term CFG | Charge termination algorithm settings |
| 0x08 | Vendor ID | AWINIC vendor number (0xFF) |
| 0x09 | Boost Fault | Boost‑mode fault codes |
| 0x0A | Boost CFG | Boost output voltage / driver settings |

## License

- [M5Hat-18650C - MIT](LICENSE)
