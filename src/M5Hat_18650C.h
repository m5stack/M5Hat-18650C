/*!
 * @file   M5Hat_18650C.h
 * @brief  Arduino library for M5Stack 18650C Battery Hat (V1.33).
 *
 * Hardware:
 *   - INA226AIDGSR @ 0x41 — 16‑bit voltage / current / power monitor (10 mΩ shunt)
 *   - AW32257FCR   @ 0x6A — Li‑ion switch‑mode charger with I²C interface
 *
 * The INA226 provides precision voltage / current / power readings.
 * The AW32257 provides charge status (Ready / Charging / Done / Fault),
 * boost‑mode detection, and charge‑parameter configuration.
 *
 * @copyright 2026 M5Stack Technology CO LTD
 * @license   MIT
 */
#ifndef M5HAT_18650C_H_
#define M5HAT_18650C_H_

#include <Arduino.h>
#include <Wire.h>

/* ========================================================================== */
/*  Class M5Hat_18650C                                                        */
/* ========================================================================== */

class M5Hat_18650C {
public:
    /* ---- AW32257 charge state enumeration ------------------------------- */

    enum charge_state_t : uint8_t {
        AW_CHG_READY    = 0,  ///< Ready (no VBUS or charge complete)
        AW_CHG_PROGRESS = 1,  ///< Charge in progress
        AW_CHG_DONE     = 2,  ///< Charge done
        AW_CHG_FAULT    = 3   ///< Fault (see fault detail)
    };

    enum fault_detail_t : uint8_t {
        AW_FAULT_NONE        = 0,  ///< Normal operation
        AW_FAULT_VBUS_OVP    = 1,  ///< VBUS over‑voltage
        AW_FAULT_SLEEP       = 2,  ///< Sleep mode
        AW_FAULT_BAD_ADAPTOR = 3,  ///< Bad adaptor or VBUS < UVLO
        AW_FAULT_OUTPUT_OVP  = 4,  ///< Output OVP
        AW_FAULT_THERMAL     = 5,  ///< Thermal shutdown
        AW_FAULT_NO_BATTERY  = 7   ///< No battery detected
    };

    /* ---- construction --------------------------------------------------- */

    M5Hat_18650C();
    ~M5Hat_18650C();

    /* ---- lifecycle ------------------------------------------------------ */

    /**
     * @brief  Initialise I²C and probe both chips.
     * @param  wire  TwoWire instance (default &Wire).
     * @param  sda   SDA GPIO pin (default -1: use Wire's current pins).
     * @param  scl   SCL GPIO pin (default -1: use Wire's current pins).
     * @return true when INA226 (0x41) is detected.
     *
     * @note   M5Hat-18650C V1.33 uses GPIO 0 (SDA) / GPIO 26 (SCL) on the HAT
     *         connector.  On M5StickS3 this is a separate I²C bus from the
     *         onboard one (GPIO 47/48).  Pass these pins explicitly:
     *         @code hat.begin(&Wire, 0, 26); @endcode
     */
    bool begin(TwoWire* wire = &Wire, int8_t sda = -1, int8_t scl = -1);

    /** Poll both chips for fresh readings (auto‑called by getters). */
    void update();

    /* ---- INA226: voltage / current / power ----------------------------- */

    /** Battery bus voltage [V] (INA226 bus voltage register). */
    float getBusVoltage();

    /** Shunt voltage drop [mV] across the 10 mΩ sense resistor. */
    float getShuntVoltage();

    /**
     * @brief  Battery current [A].
     * @note   Positive = into battery (charging), negative = out (discharging).
     */
    float getCurrent();

    /** Power [W] delivered to / drawn from the battery (INA226 power register). */
    float getPower();

    /* ---- state‑of‑charge ------------------------------------------------- */

    /** SOC [%] — linear estimate from bus voltage (3.0–4.2 V by default). */
    float getBatteryPercentage();

    /** Customise the voltage → SOC mapping. */
    void setBatteryVoltageRange(float vEmpty, float vFull);

    /* ---- AW32257: charge state ----------------------------------------- */

    /** AW32257 charge state (Ready / Charging / Done / Fault). */
    charge_state_t getChargeState();

    /** AW32257 fault detail (only meaningful when state == AW_CHG_FAULT). */
    fault_detail_t getFaultDetail();

    /** True when the AW32257 is actively charging the battery. */
    bool isCharging();

    /** True when the AW32257 is in boost (OTG) mode. */
    bool isBoostMode();

    /** True when USB VBUS is present (INA226 bus voltage > 4.5 V). */
    bool isUSBPresent();

    /* ---- detection ------------------------------------------------------ */

    bool isIna226Found() const;
    bool isChargerFound() const;

    /* ---- AW32257 charge parameter helpers (optional) -------------------- */

    /** Enable or disable the charger (CEN bit, reg 0x01). */
    void setChargerEnabled(bool en);

    /** Set fast‑charge current [mA] (reg 0x04).  Values above 2480 are clamped. */
    void setChargeCurrent(uint16_t mA);

    /** Set charge regulation voltage [V] (reg 0x02, 3.50–4.50 V). */
    void setChargeVoltage(float v);

private:
    /* ---- I²C primitives ------------------------------------------------- */
    bool _i2cProbe(uint8_t addr);
    uint16_t _readReg16(uint8_t addr, uint8_t reg);
    void _writeReg16(uint8_t addr, uint8_t reg, uint16_t val);
    uint8_t _readReg8(uint8_t addr, uint8_t reg);

    /* ---- chip init ------------------------------------------------------ */
    bool _initIna226();
    bool _initAw32257();

    /* ---- read routines -------------------------------------------------- */
    void _readIna226();
    void _readAw32257();

    /* ---- AW32257 helpers ------------------------------------------------ */
    uint16_t _mAtoIchg(uint16_t mA);
    uint8_t _vToVoreg(float v);

    /* ---- state ---------------------------------------------------------- */
    TwoWire* _wire = nullptr;

    bool _ina226Found  = false;
    bool _chargerFound = false;

    // INA226 data
    float _busVoltage   = NAN;  // [V]
    float _shuntVoltage = NAN;  // [mV]
    float _current      = NAN;  // [A]
    float _power        = NAN;  // [W]

    // AW32257 data
    charge_state_t _chargeState = AW_CHG_READY;
    fault_detail_t _faultDetail = AW_FAULT_NONE;
    bool _charging              = false;
    bool _boostMode             = false;

    float _vEmpty = 3.0f;
    float _vFull  = 4.2f;

    unsigned long _lastUpdateMs = 0;

    // ── INA226 constants ─────────────────────────────────────────────────
    static constexpr uint8_t INA226_ADDR    = 0x41;
    static constexpr float INA226_SHUNT_OHM = 0.01f;   // 原理图有写 R12电阻 10mΩ/1% 。 --> 0.01Ω
    static constexpr uint16_t INA226_CAL    = 2048;
    // CAL = 0.00512 / (Current_LSB × Rshunt)
    //   Current_LSB = 8.192 / 32768 = 250 µA
    //   CAL = 0.00512 / (0.00025 × 0.01) = 2048        // 此处0.01表示原理图的 10mΩ

    // ── AW32257 constants (from datasheet V1.5) ──────────────────────────
    static constexpr uint8_t AW32257_ADDR = 0x6A;

    // Register addresses
    static constexpr uint8_t AW_REG_STATUS      = 0x00;
    static constexpr uint8_t AW_REG_CONTROL     = 0x01;
    static constexpr uint8_t AW_REG_VOREG       = 0x02;
    static constexpr uint8_t AW_REG_VENDOR      = 0x03;
    static constexpr uint8_t AW_REG_ICHG        = 0x04;
    static constexpr uint8_t AW_REG_SPECIAL     = 0x05;
    static constexpr uint8_t AW_REG_SAFETY      = 0x06;
    static constexpr uint8_t AW_REG_TERM_CFG    = 0x07;
    static constexpr uint8_t AW_REG_VENDOR_ID   = 0x08;
    static constexpr uint8_t AW_REG_BOOST_FAULT = 0x09;
    static constexpr uint8_t AW_REG_BOOST_CFG   = 0x0A;

    // Reg 0x00 bit masks
    static constexpr uint8_t AW_OTG_MASK     = 0x80;
    static constexpr uint8_t AW_EN_STAT_MASK = 0x40;
    static constexpr uint8_t AW_STAT_MASK    = 0x0C;  // bits [3:2]
    static constexpr uint8_t AW_STAT_SHIFT   = 2;

    // Reg 0x01 bit masks
    static constexpr uint8_t AW_TE_MASK       = 0x08;
    static constexpr uint8_t AW_CEN_MASK      = 0x04;
    static constexpr uint8_t AW_HZ_MODE_MASK  = 0x02;
    static constexpr uint8_t AW_OPA_MODE_MASK = 0x01;

    // Reg 0x05 bit masks
    static constexpr uint8_t AW_DPM_STATUS_MASK = 0x10;
    static constexpr uint8_t AW_CD_STATUS_MASK  = 0x08;
    static constexpr uint8_t AW_VSP_MASK        = 0x07;

    // Known vendor/part/revision for AW32257 (reg 0x03 default = 0x53)
    static constexpr uint8_t AW_VENDOR_PN_REV = 0x53;
};

#endif  // M5HAT_18650C_H_
