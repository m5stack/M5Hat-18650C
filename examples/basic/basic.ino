/*!
 * @file   basic.ino
 * @brief  Minimal example for M5Hat-18650C (V1.33).
 *
 * Prints INA226 voltage / current / power and AW32257 charge state to Serial.
 *
 * Hardware:
 *   - INA226AIDGSR @ 0x41, 10 mΩ shunt
 *   - AW32257FCR   @ 0x6A, Li‑ion charger
 *
 * @copyright 2026 M5Stack Technology CO LTD
 * @license   MIT
 */
#include <M5Unified.h>
#include <M5Hat_18650C.h>

M5Hat_18650C hat;

static const char* chargeStateName(M5Hat_18650C::charge_state_t s)
{
    switch (s) {
        case M5Hat_18650C::AW_CHG_READY:
            return "Ready";
        case M5Hat_18650C::AW_CHG_PROGRESS:
            return "Charging";
        case M5Hat_18650C::AW_CHG_DONE:
            return "Done";
        case M5Hat_18650C::AW_CHG_FAULT:
            return "FAULT";
        default:
            return "?";
    }
}

static const char* faultName(M5Hat_18650C::fault_detail_t f)
{
    switch (f) {
        case M5Hat_18650C::AW_FAULT_NONE:
            return "None";
        case M5Hat_18650C::AW_FAULT_VBUS_OVP:
            return "VBUS OVP";
        case M5Hat_18650C::AW_FAULT_SLEEP:
            return "Sleep";
        case M5Hat_18650C::AW_FAULT_BAD_ADAPTOR:
            return "Bad adaptor";
        case M5Hat_18650C::AW_FAULT_OUTPUT_OVP:
            return "Output OVP";
        case M5Hat_18650C::AW_FAULT_THERMAL:
            return "Thermal";
        case M5Hat_18650C::AW_FAULT_NO_BATTERY:
            return "No battery";
        default:
            return "?";
    }
}
uint8_t error = 0;
#define USE_WIRE
bool I2C_Scan()
{
    Serial.println("======== I2C 扫描开始 ========");

    bool foundINA = false;
    bool foundAW  = false;
    uint8_t count = 0;

    for (uint8_t addr = 1; addr < 127; addr++) {
#ifdef USE_WIRE
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

#elif defined(USE_WIRE1)
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();

#elif defined(USE_SOFT_I2C)
        softI2C.start();
        softI2C.writeByte(addr << 1);
        softI2C.stop();
        uint8_t error = 0;  // 软件I2C简单认为成功
#endif

        if (error == 0) {
            Serial.printf("发现设备: 0x%02X\n", addr);
            count++;

            if (addr == 0x41) foundINA = true;
            if (addr == 0x6A) foundAW = true;
        }
    }

    Serial.println("--------------------------------");

    if (!count)
        Serial.println("未发现任何 I2C 设备");
    else
        Serial.printf("共发现 %d 个设备\n", count);

    if (!foundINA) Serial.println("❌ 未找到 INA226 (0x41)");

    if (!foundAW) Serial.println("❌ 未找到 AW32257 (0x6A)");

    Serial.println("======== I2C 扫描结束 ========\n");

    return (foundINA && foundAW);
}

void setup()
{
    M5.begin();
    Serial.begin(115200);

    Serial.println("\n========================================");
    Serial.println("  M5Hat-18650C — Basic Example");
    M5.Display.begin();
    Serial.println("========================================\n");

    M5.Display.setRotation(0);
    M5.Display.setBrightness(100);
    M5.Display.setTextFont(&fonts::efontCN_16);
    Serial.println("I2C 测试文本！");

    if (!hat.begin(&Wire, 0, 26)) {  // HAT I²C: SDA=G0, SCL=G26
        Serial.println("[FATAL] INA226 (0x41) not found on I2C bus!");
        Serial.println("        Check hat connection / I2C wiring.\n");
    }

    I2C_Scan();
    Serial.print("INA226  (0x41): ");
    Serial.println(hat.isIna226Found() ? "OK" : "MISSING");
    Serial.print("AW32257 (0x6A): ");
    Serial.println(hat.isChargerFound() ? "OK" : "MISSING");

    // Optional: customise SOC voltage range
    hat.setChargerEnabled(1);
    hat.setBatteryVoltageRange(3.0f, 4.2f);

    // Optional: configure AW32257 charge parameters
    hat.setChargeCurrent(900);   // 1.5 A fast‑charge
    hat.setChargeVoltage(4.20f);  // 4.20 V regulation

    Serial.println("----------------------------------------\n");
}

void loop()
{
    hat.update();
    static int pos_y = 20;
    static int gap_y = 20;
    pos_y            = 20;

    M5.Display.clear();
    // ── INA226 readings ──────────────────────────────────────────────
    Serial.printf("=== INA226 ===\n");
    M5.Display.setTextColor(WHITE);
    // M5.Display.print("I2C 测试文本2");
    M5.Display.setCursor(5, pos_y);
    Serial.printf("  Bus Voltage  : %7.4f V\n", hat.getBusVoltage());
    M5.Display.printf("vBus: %2.4f V\n", hat.getBusVoltage());

    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    Serial.printf("  Shunt Voltage: %8.3f mV\n", hat.getShuntVoltage());
    M5.Display.printf("vShunt: %2.3f mV\n", hat.getShuntVoltage());

    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    Serial.printf("  Current      : %+8.1f mA\n", hat.getCurrent() * 1000.0f);
    M5.Display.printf("Cur: %+4.1f mA\n", hat.getCurrent() * 1000.0f);

    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    Serial.printf("  Power        : %+8.1f mW\n", hat.getPower() * 1000.0f);
    M5.Display.printf("Pwr: %+6.1f mW\n", hat.getPower() * 1000.0f);
    Serial.printf("  SOC (est.): %7.1f %%\n", hat.getBatteryPercentage());

    // ── AW32257 status ───────────────────────────────────────────────
    Serial.printf("=== AW32257 ===\n");
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    Serial.printf("  Charge state : %s\n", chargeStateName(hat.getChargeState()));
    M5.Display.printf("Crg state: %s\n", chargeStateName(hat.getChargeState()));

    Serial.printf("  Fault detail : %s\n", faultName(hat.getFaultDetail()));
    Serial.printf("  Boost mode   : %s\n", hat.isBoostMode() ? "YES" : "NO");

    // ── Combined ─────────────────────────────────────────────────────
    Serial.printf("=== Summary ===\n");
    Serial.printf("  USB present  : %s\n", hat.isUSBPresent() ? "YES" : "NO");

    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    Serial.printf("Crg ?: %s\n", hat.isCharging() ? "YES" : "NO");
    M5.Display.printf("Charge: %s\n", hat.isCharging() ? "YES" : "NO");
    Serial.println("----------------------------------------\n");

    delay(2000);
}
