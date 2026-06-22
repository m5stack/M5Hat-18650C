/*!
 * @file M5Hat_18650C.h
 * @brief Arduino library for M5Stack HAT 18650C V1.1.
 *
 * INA226 @ 0x41 (10 mΩ) — voltage / current / power
 * AW32257 @ 0x6A — Li-ion charger
 *
 * @copyright 2026 M5Stack Technology CO LTD
 * @license MIT
 */
#ifndef M5HAT_18650C_H_
#define M5HAT_18650C_H_

#include <Arduino.h>
#include <Wire.h>

/** AW32257 reg 0x00 bits [5:4] */
typedef enum : uint8_t {
    AW_CHG_READY    = 0,
    AW_CHG_PROGRESS = 1,
    AW_CHG_DONE     = 2,
    AW_CHG_FAULT    = 3,
} aw_chg_stat_t;

/** AW32257 reg 0x00 bits [2:0] */
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

/** AW32257 status register snapshot. */
typedef struct {
    bool otg;
    bool en_stat;
    bool boost;
    aw_chg_stat_t stat;
    aw_chg_fault_t chg_fault;
} aw32257_status_t;

/**
 * @class M5Hat_18650C
 * @brief Driver for M5Stack HAT 18650C V1.1.
 *
 * Provides INA226 power measurements and AW32257 charge management over I2C.
 */
class M5Hat_18650C {
public:
    M5Hat_18650C()  = default;
    ~M5Hat_18650C() = default;

    /**
     * @brief  Initialize I2C and both on-board chips (INA226 @ 0x41, AW32257 @ 0x6A).
     * @param  wire I2C bus instance (default Wire).
     * @param  sda  SDA pin; pass -1 to keep the current bus configuration.
     * @param  scl  SCL pin; pass -1 to keep the current bus configuration.
     * @return true when both chips are detected and configured.
     */
    bool begin(TwoWire* wire = &Wire, int8_t sda = -1, int8_t scl = -1);

    /**
     * @brief  Read battery voltage (VBAT) from INA226.
     * @return Voltage in volts; NAN on I2C error.
     */
    float getBatteryVoltage();

    /**
     * @brief  Read shunt voltage from INA226.
     * @return Voltage in millivolts; NAN on I2C error.
     */
    float getShuntVoltage();

    /**
     * @brief  Read battery current from INA226.
     * @return Current in amperes (positive = charging); NAN on I2C error.
     */
    float getBatteryCurrent();

    /**
     * @brief  Read battery power from INA226.
     * @return Power in watts; NAN on I2C error.
     */
    float getBatteryPower();

    /**
     * @brief  Estimate state-of-charge from battery voltage.
     * @return Percentage 0–100; NAN on I2C error. Linear estimate only.
     */
    float getBatteryPercentage();

    /**
     * @brief  Set the voltage range used by getBatteryPercentage().
     * @param  vEmpty Empty-battery voltage (V).
     * @param  vFull  Full-charge voltage (V).
     * @return false if vEmpty >= vFull or either value is NAN.
     */
    bool setBatteryVoltageRange(float vEmpty, float vFull);

    /**
     * @brief  Read and parse AW32257 status register (0x00) in one I2C transaction.
     * @return Status struct; all fields zero on I2C error.
     */
    aw32257_status_t getChargerStatus();

    /**
     * @brief  Get AW32257 charge state (register 0x00, bits [5:4]).
     * @return Charge state enum value.
     */
    aw_chg_stat_t getChargeState();

    /**
     * @brief  Get AW32257 fault detail (register 0x00, bits [2:0]).
     * @return Fault enum value.
     */
    aw_chg_fault_t getChargeFault();

    /**
     * @brief  Check whether the battery is charging.
     * @return true when charge state is AW_CHG_PROGRESS.
     */
    bool isCharging();

    /**
     * @brief  Check whether AW32257 is in boost (OTG) mode.
     * @return true when control register bit 0 (OPA_MODE) is set.
     */
    bool isBoostMode();

    /** @return true if INA226 power monitor was found during begin(). */
    bool isPowerMonitorFound() const;

    /** @return true if AW32257 was found during begin(). */
    bool isChargerFound() const;

    /**
     * @brief  Enable or disable the AW32257 charger.
     * @param  en true to enable charging, false to disable.
     */
    void setChargerEnabled(bool en);

    /**
     * @brief  Set fast-charge current on AW32257.
     * @param  mA Target current in mA (496–2480, snapped to nearest table step).
     */
    void setChargeCurrent(uint16_t mA);

    /**
     * @brief  Set charge regulation voltage on AW32257.
     * @param  v Target voltage in V (clamped to 3.50–4.50 V).
     */
    void setChargeVoltage(float v);

    /**
     * @brief  Switch AW32257 between charge and boost (OTG) mode.
     * @param  boost true for boost mode, false for charge mode.
     */
    void setBoostMode(bool boost);

private:
    bool _i2cProbe(uint8_t addr);
    bool _readReg16(uint8_t addr, uint8_t reg, uint16_t& val);
    void _writeReg16(uint8_t addr, uint8_t reg, uint16_t val);
    bool _readReg8(uint8_t addr, uint8_t reg, uint8_t& val);
    void _writeReg8(uint8_t addr, uint8_t reg, uint8_t val);
    bool _initIna226();
    bool _initAw32257();
    uint16_t _mAtoIchg(uint16_t mA);
    uint8_t _voregToCode(float v);

    TwoWire* _wire     = nullptr;
    bool _ina226Found  = false;
    bool _chargerFound = false;
    float _vEmpty      = 3.0f;
    float _vFull       = 4.2f;

    static constexpr uint8_t INA226_ADDR      = 0x41;
    static constexpr float INA226_SHUNT_OHM   = 0.01f;
    static constexpr float INA226_MAX_CURRENT = 2.0f;
    static constexpr float INA226_CURRENT_LSB = INA226_MAX_CURRENT / 32768.0f;

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

    static constexpr uint8_t AW_OTG_MASK       = 0x80;
    static constexpr uint8_t AW_EN_STAT_MASK   = 0x40;
    static constexpr uint8_t AW_STAT_MASK      = 0x30;
    static constexpr uint8_t AW_BOOST_MASK     = 0x08;
    static constexpr uint8_t AW_CHG_FAULT_MASK = 0x07;
    static constexpr uint8_t AW_TE_MASK        = 0x08;
    static constexpr uint8_t AW_CEN_MASK       = 0x04;
    static constexpr uint8_t AW_HZ_MODE_MASK   = 0x02;
    static constexpr uint8_t AW_OPA_MODE_MASK  = 0x01;
    static constexpr uint8_t AW_ITERM_DEFAULT  = 0x01;
    static constexpr uint8_t AW_VOREG_4V20     = 0x8E;
};

#endif
