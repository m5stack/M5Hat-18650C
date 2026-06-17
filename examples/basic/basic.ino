/*!
 * @file   basic.ino
 * @brief  Minimal example for M5Hat-18650C (V1.33).
 *
 * Prints INA226 voltage / current / power and AW32257 charge state to Serial
 * and the built‑in display.
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

/* ---- helper: AW32257 status strings ------------------------------------ */

static const char* chargeStateName(aw_chg_stat_t s)
{
    switch (s) {
        case AW_CHG_READY:
            return "Ready";
        case AW_CHG_PROGRESS:
            return "Charging";
        case AW_CHG_DONE:
            return "Done";
        case AW_CHG_FAULT:
            return "Fault";
        default:
            return "?";
    }
}

static const char* faultName(aw_chg_fault_t f)
{
    switch (f) {
        case AW_FAULT_NORMAL:
            return "None";
        case AW_FAULT_VBUS_OVP:
            return "VBUS OVP";
        case AW_FAULT_SLEEP_MODE:
            return "Sleep";
        case AW_FAULT_BAD_ADAPTER:
            return "Bad adaptor";
        case AW_FAULT_OUTPUT_OVP:
            return "Output OVP";
        case AW_FAULT_THERMAL_SHUTDOWN:
            return "Thermal";
        case AW_FAULT_NA:
            return "N/A";
        case AW_FAULT_NO_BATTERY:
            return "No battery";
        default:
            return "?";
    }
}

/* ======================================================================== */
/*  setup / loop                                                            */
/* ======================================================================== */

void setup()
{
    int retry = 0;

    M5.begin();
    Serial.begin(115200);

    Serial.println("\n========================================");
    Serial.println("  M5Hat-18650C — Basic Example");
    Serial.println("========================================\n");

    M5.Display.begin();
    M5.Display.setRotation(0);
    M5.Display.setBrightness(100);
    M5.Display.setTextFont(&fonts::efontCN_16);

    while (!hat.begin(&Wire, I2C_PIN_SDA, I2C_PIN_SCL) && retry < 10) {
        retry++;
        M5.Display.clear();
        M5.Display.setTextColor(WHITE);
        M5.Display.setCursor(5, 0);
        M5.Display.print("not found");
        M5.Display.setCursor(5, 20);
        M5.Display.print("INA226 / AW32257");
        M5.Display.setCursor(5, 40);
        M5.Display.printf("retry: %d", retry);
        Serial.println("[FATAL] INA226 (0x41) not found on I2C bus!");
        delay(1000);
    }

    if (retry >= 10 && !hat.isIna226Found()) {
        M5.Display.clear();
        M5.Display.setTextColor(RED);
        M5.Display.setCursor(5, 40);
        M5.Display.print("FATAL: I2C timeout");
        Serial.println("[FATAL] I2C init failed after 10 retries — halting.");
        while (1) delay(1000);
    }

    Serial.print("INA226  (0x41): ");
    Serial.println(hat.isIna226Found() ? "OK" : "MISSING");
    Serial.print("AW32257 (0x6A): ");
    Serial.println(hat.isChargerFound() ? "OK" : "MISSING");

    // Optional: configure charge parameters
    hat.setChargerEnabled(true);
    hat.setBatteryVoltageRange(3.0f, 4.2f);
    hat.setChargeCurrent(900);    // 900 mA fast‑charge
    hat.setChargeVoltage(4.20f);  // 4.20 V regulation

    Serial.println("----------------------------------------\n");
}

void loop()
{
    int pos_y = 20, gap_y = 20;

    // ── INA226 readings (direct I²C, no update() call needed) ──────────
    float v   = hat.getBusVoltage();
    float sv  = hat.getShuntVoltage();
    float cur = hat.getCurrent();
    float pwr = hat.getPower();
    float soc = hat.getBatteryPercentage();

    // ── AW32257 status (one I²C read, parsed into struct) ──────────────
    aw32257_status_t st = hat.getStatus();
    bool boost          = st.boost;
    bool chg            = (st.stat == AW_CHG_PROGRESS);
    bool usb            = hat.isUSBPresent();

    // ── Display ────────────────────────────────────────────────────────
    M5.Display.clear();
    M5.Display.setTextColor(WHITE);

    pos_y = 20;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("vBus: %.4f V\n", v);
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("vShunt: %.3f mV\n", sv);
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("Cur: %+.1f mA\n", cur * 1000.0f);
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("Pwr: %+.1f mW\n", pwr * 1000.0f);
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("State: %s\n", chargeStateName(st.stat));
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("Boost: %s\n", boost ? "YES" : "NO");
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("USB: %s\n", usb ? "YES" : "NO");
    pos_y += gap_y;
    M5.Display.setCursor(5, pos_y);
    M5.Display.printf("Charge: %s\n", chg ? "YES" : "NO");

    // ── Serial ─────────────────────────────────────────────────────────
    Serial.printf("=== INA226 ===\n");
    Serial.printf("  Bus Voltage  : %7.4f V\n", v);
    Serial.printf("  Shunt Voltage: %8.3f mV\n", sv);
    Serial.printf("  Current      : %+8.1f mA\n", cur * 1000.0f);
    Serial.printf("  Power        : %+8.1f mW\n", pwr * 1000.0f);
    Serial.printf("  SOC (est.)   : %7.1f %%\n", soc);
    Serial.printf("=== AW32257 ===\n");
    Serial.printf("  Charge state : %s\n", chargeStateName(st.stat));
    Serial.printf("  Fault detail : %s\n", faultName(st.chg_fault));
    Serial.printf("  Boost mode   : %s\n", boost ? "YES" : "NO");
    Serial.printf("=== Summary ===\n");
    Serial.printf("  USB present  : %s\n", usb ? "YES" : "NO");
    Serial.printf("  Charging     : %s\n", chg ? "YES" : "NO");
    Serial.println("----------------------------------------\n");

    hat.INA_RegisterCheck(v, cur, pwr);

    delay(2000);
}
