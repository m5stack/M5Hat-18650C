/*!
 * @file   M5Hat_18650C.h
 * @brief  Arduino library for M5Stack 18650C Battery Hat (V1.33).
 *
 * Hardware:
 *   - INA226AIDGSR @ 0x41 — 16‑bit voltage / current / power monitor (10 mΩ shunt)
 *   - AW32257FCR   @ 0x6A — Li‑ion switch‑mode charger with I²C interface
 *
 * @copyright 2026 M5Stack Technology CO LTD
 * @license   MIT
 */
#ifndef M5HAT_18650C_H_
#define M5HAT_18650C_H_

#include <Arduino.h>
#include <Wire.h>

#define USE_WIRE

#ifdef ARDUINO_M5STACK_STICKC_PLUS
#define I2C_PIN_SCL 26
#define I2C_PIN_SDA 0
#elifdef ARDUINO_M5STACK_STICKS3
#define I2C_PIN_SCL 0
#define I2C_PIN_SDA 8
#else
#define I2C_PIN_SCL -1
#define I2C_PIN_SDA -1
#endif

/* ========================================================================== */
/*  INA226 config                                                             */
/* ========================================================================== */

/* ========================================================================== */
/*  AW32257 charge-state / fault types                                        */
/* ========================================================================== */

typedef enum : uint8_t {
    AW_CHG_READY    = 0,
    AW_CHG_PROGRESS = 1,
    AW_CHG_DONE     = 2,
    AW_CHG_FAULT    = 3,
} aw_chg_stat_t;

typedef enum : uint8_t {
    AW_FAULT_NORMAL           = 0,
    AW_FAULT_VBUS_OVP         = 1,
    AW_FAULT_SLEEP_MODE       = 2,
    AW_FAULT_BAD_ADAPTER      = 3,
    AW_FAULT_OUTPUT_OVP       = 4,
    AW_FAULT_THERMAL_SHUTDOWN = 5,
    AW_FAULT_NA               = 6,
    AW_FAULT_NO_BATTERY       = 7,
} aw_chg_fault_t;

typedef struct {
    bool otg;
    bool en_stat;
    bool boost;
    aw_chg_stat_t stat;
    aw_chg_fault_t chg_fault;
} aw32257_status_t;

/* ========================================================================== */
/*  Class M5Hat_18650C                                                        */
/* ========================================================================== */

class M5Hat_18650C {
public:
    M5Hat_18650C();
    ~M5Hat_18650C();

    bool begin(TwoWire* wire = &Wire, int8_t sda = -1, int8_t scl = -1);

    void INA_RegisterCheck(float voltage, float current, float power);

    float getBusVoltage();

    float getShuntVoltage();

    float getCurrent();

    float getPower();

    float getBatteryPercentage();

    bool setBatteryVoltageRange(float vEmpty, float vFull);

    /* ---- AW32257 status (reads register 0x00 once, parses all fields) --- */
    aw32257_status_t getStatus();

    aw_chg_stat_t getChargeState();

    aw_chg_fault_t getFaultDetail();

    bool isCharging();

    bool isBoostMode();

    /** True when USB VBUS is present (bus voltage > 4.5 V). */
    bool isUSBPresent();

    /* ---- detection ------------------------------------------------------ */
    bool isIna226Found() const;
    bool isChargerFound() const;

    /* ---- AW32257 charge-parameter config (optional) --------------------- */
    void setChargerEnabled(bool en);
    void setChargeCurrent(uint16_t mA);
    void setChargeVoltage(float v);

private:
    bool _i2cProbe(uint8_t addr);

    uint16_t _readReg16(uint8_t addr, uint8_t reg);

    void _writeReg16(uint8_t addr, uint8_t reg, uint16_t val);

    uint8_t _readReg8(uint8_t addr, uint8_t reg);

    /* ---- chip init ------------------------------------------------------ */
    bool _initIna226();

    bool _initAw32257();

    /* ---- helpers -------------------------------------------------------- */
    uint16_t _mAtoIchg(uint16_t mA);
    uint8_t _vToVoreg(float v);

    /* ---- state ---------------------------------------------------------- */
    TwoWire* _wire = nullptr;

    bool _ina226Found  = false;
    bool _chargerFound = false;

    float _vEmpty = 3.0f;
    float _vFull  = 4.2f;

    // ── INA226 constants ─────────────────────────────────────────────────
    static constexpr uint8_t INA226_ADDR    = 0x41;
    static constexpr float INA226_SHUNT_OHM = 0.01f;
    static constexpr uint16_t INA226_CAL    = 2048;
    // 16‑sample average, 1.1 ms conversion, continuous shunt + bus
    static constexpr uint16_t INA226_CONFIG_DEFAULT = 0x4527;

    enum : uint8_t {
        INA226_REG_CONFIG       = 0x00,
        INA226_REG_SHUNTV       = 0x01,
        INA226_REG_BUSV         = 0x02,
        INA226_REG_POWER        = 0x03,
        INA226_REG_CURRENT      = 0x04,
        INA226_REG_CALIBRATION  = 0x05,
        INA226_REG_MASK         = 0x06,
        INA226_REG_ALERT        = 0x07,
        INA226_REG_MANUFACTURER = 0xFE,
        INA226_REG_DIEID        = 0xFF,
    };

    // ── AW32257 constants ────────────────────────────────────────────────
    static constexpr uint8_t AW32257_ADDR = 0x6A;

    enum : uint8_t {
        AW_REG_STATUS      = 0x00,
        AW_REG_CONTROL     = 0x01,
        AW_REG_VOREG       = 0x02,
        AW_REG_VENDOR      = 0x03,
        AW_REG_ICHG        = 0x04,
        AW_REG_SPECIAL     = 0x05,
        AW_REG_SAFETY      = 0x06,
        AW_REG_TERM_CFG    = 0x07,
        AW_REG_VENDOR_ID   = 0x08,
        AW_REG_BOOST_FAULT = 0x09,
        AW_REG_BOOST_CFG   = 0x0A,
    };

    // Reg 0x00 bit masks
    static constexpr uint8_t AW_OTG_MASK       = 0x80;
    static constexpr uint8_t AW_EN_STAT_MASK   = 0x40;
    static constexpr uint8_t AW_STAT_MASK      = 0x30;  // bits [5:4]
    static constexpr uint8_t AW_BOOST_MASK     = 0x08;  // bit  [3]
    static constexpr uint8_t AW_CHG_FAULT_MASK = 0x07;  // bits [2:0]

    // Reg 0x01 bit masks
    static constexpr uint8_t AW_TE_MASK       = 0x08;
    static constexpr uint8_t AW_CEN_MASK      = 0x04;
    static constexpr uint8_t AW_HZ_MODE_MASK  = 0x02;
    static constexpr uint8_t AW_OPA_MODE_MASK = 0x01;
};

#endif  // M5HAT_18650C_H_
