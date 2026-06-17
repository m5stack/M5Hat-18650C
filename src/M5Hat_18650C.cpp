/*!
 * @file   M5Hat_18650C.cpp
 * @brief  Implementation for M5Stack HAT 18650C V1.1.
 *
 * Hardware:
 *   - INA226AIDGSR @ 0x41, 10 mΩ shunt — voltage / current / power
 *   - AW32257FCR   @ 0x6A           — Li‑ion switch‑mode charger
 *
 * Datasheet: AW32257 V1.5 (Nov. 2023), Awinic Technology.
 *
 * @copyright 2026 M5Stack Technology CO LTD
 * @license   MIT
 */
#include "M5Hat_18650C.h"

/* ---- I2C scan (optional debug) ----------------------------------------- */

bool I2C_Scan()
{
    Serial.println("======== I2C scan ========");
    int error     = 0;
    bool foundINA = false, foundAW = false;
    uint8_t count = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
#ifdef USE_WIRE
        Wire.beginTransmission(addr);
        error = Wire.endTransmission();
#endif
        if (error == 0) {
            Serial.printf("Found: 0x%02X\n", addr);
            count++;
            if (addr == 0x41) foundINA = true;
            if (addr == 0x6A) foundAW = true;
        }
    }
    Serial.printf("%d device(s) found\n", count);
    if (!foundINA) Serial.println("INA226 (0x41) MISSING!");
    if (!foundAW) Serial.println("AW32257 (0x6A) MISSING!");
    Serial.println("==========================\n");
    return (foundINA && foundAW);
}

/* ========================================================================== */
/*  AW32257 helpers (datasheet V1.5 tables)                                   */
/* ========================================================================== */

// ICHG table: reg 0x04[6:3] → mA (33 mΩ sense)
static const uint16_t AW_ICHG_TABLE[16] = {496,  620,  868,  992,  1116, 1240, 1364, 1488,
                                           1612, 1736, 1860, 1984, 2108, 2232, 2356, 2480};

static uint8_t _voregToCode(float v)
{
    if (v < 3.50f) v = 3.50f;
    if (v > 4.50f) v = 4.50f;
    return (uint8_t)((v - 3.50f) / 0.02f + 0.5f);
}

/* ========================================================================== */
/*  Construction / destruction                                                */
/* ========================================================================== */

M5Hat_18650C::M5Hat_18650C()
{
}
M5Hat_18650C::~M5Hat_18650C()
{
}

/* ========================================================================== */
/*  begin()                                                                   */
/* ========================================================================== */

bool M5Hat_18650C::begin(TwoWire* wire, int8_t sda, int8_t scl)
{
    _wire = wire;

    if (sda >= 0 && scl >= 0) {
        _wire->end();
        _wire->begin(sda, scl, 400000);
    }

    I2C_Scan();
    _ina226Found  = _initIna226();
    _chargerFound = _initAw32257();
    return (_ina226Found && _chargerFound);
}

/* ========================================================================== */
/*  Chip initialisation                                                       */
/* ========================================================================== */

bool M5Hat_18650C::_initIna226()
{
    if (!_i2cProbe(INA226_ADDR)) return false;

    // Reset and configure
    _writeReg16(INA226_ADDR, INA226_REG_CONFIG, 0x8000);
    delay(1);
    _writeReg16(INA226_ADDR, INA226_REG_CONFIG, INA226_CONFIG_DEFAULT);
    _writeReg16(INA226_ADDR, INA226_REG_CALIBRATION, INA226_CAL);
    return true;
}

bool M5Hat_18650C::_initAw32257()
{
    if (!_i2cProbe(AW32257_ADDR)) return false;
    return true;
}

/* ========================================================================== */
/*  Periodic Register Check                                                   */
/* ========================================================================== */

void M5Hat_18650C::INA_RegisterCheck(float voltage, float current, float power)
{
    static unsigned long lastCheck = 0;

    if (millis() - lastCheck >= 10000) {
        Serial.println("\n--- cyclic check register ---");

        uint16_t calReg    = _readReg16(INA226_ADDR, INA226_REG_CALIBRATION);
        uint16_t shuntReg  = _readReg16(INA226_ADDR, INA226_REG_SHUNTV);
        uint16_t curReg    = _readReg16(INA226_ADDR, INA226_REG_CURRENT);
        uint16_t configReg = _readReg16(INA226_ADDR, INA226_REG_CONFIG);

        Serial.printf("Config=0x%04X Shunt=0x%04X Cal=0x%04X Cur=0x%04X\n", configReg, shuntReg, calReg, curReg);

        Serial.printf("API: V=%.4f I=%.4f P=%.4f\n", voltage, current, power);

        if (calReg == 0) {
            Serial.println("calibration fail, reinit");
            this->begin(&Wire, I2C_PIN_SDA, I2C_PIN_SCL);
        }

        lastCheck = millis();
    }
}

/* ========================================================================== */
/*  I²C primitives                                                            */
/* ========================================================================== */

bool M5Hat_18650C::_i2cProbe(uint8_t addr)
{
    _wire->beginTransmission(addr);
    return (_wire->endTransmission(true) == 0);
}

uint16_t M5Hat_18650C::_readReg16(uint8_t addr, uint8_t reg)
{
    _wire->beginTransmission(addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0xFFFF;
    _wire->requestFrom(addr, (uint8_t)2);
    if (_wire->available() < 2) return 0xFFFF;
    return ((uint16_t)_wire->read() << 8) | _wire->read();
}

void M5Hat_18650C::_writeReg16(uint8_t addr, uint8_t reg, uint16_t val)
{
    _wire->beginTransmission(addr);
    _wire->write(reg);
    _wire->write((uint8_t)(val >> 8));
    _wire->write((uint8_t)(val & 0xFF));
    _wire->endTransmission(true);
}

uint8_t M5Hat_18650C::_readReg8(uint8_t addr, uint8_t reg)
{
    _wire->beginTransmission(addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0xFF;
    _wire->requestFrom(addr, (uint8_t)1);
    if (_wire->available() < 1) return 0xFF;
    return _wire->read();
}

/* ========================================================================== */
/*  INA226 readings  —  cada getter  lee  directamente I²C (estilo main.cpp)  */
/* ========================================================================== */

float M5Hat_18650C::getBusVoltage()
{
    uint16_t raw = _readReg16(INA226_ADDR, INA226_REG_BUSV);
    if (raw == 0xFFFF) return NAN;
    return raw * 0.00125f;
}

float M5Hat_18650C::getShuntVoltage()
{
    int16_t raw = (int16_t)_readReg16(INA226_ADDR, INA226_REG_SHUNTV);
    if (raw == (int16_t)0xFFFF) return NAN;
    return raw * 0.0025f;  // mV
}

float M5Hat_18650C::getCurrent()
{
    int16_t raw = (int16_t)_readReg16(INA226_ADDR, INA226_REG_CURRENT);
    if (raw == (int16_t)0xFFFF) return NAN;
    return raw * 0.00025f;  // A
}

float M5Hat_18650C::getPower()
{
    uint16_t raw = _readReg16(INA226_ADDR, INA226_REG_POWER);
    if (raw == 0xFFFF) return NAN;
    return raw * 25.0f * 0.00025f;  // W
}

/* ========================================================================== */
/*  State‑of‑charge (derived from bus voltage)                                */
/* ========================================================================== */

float M5Hat_18650C::getBatteryPercentage()
{
    float v = getBusVoltage();
    if (isnan(v)) return NAN;
    float pct = (v - _vEmpty) / (_vFull - _vEmpty) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

bool M5Hat_18650C::setBatteryVoltageRange(float vEmpty, float vFull)
{
    _vEmpty = vEmpty;
    _vFull  = vFull;
    if (vEmpty > vFull) {
        Serial.printf("voltage Empty shouldn't bigger than Full!");
        return false;
    }
    return true;
}

/* ========================================================================== */
/*  USB present                                                               */
/* ========================================================================== */

bool M5Hat_18650C::isUSBPresent()
{
    float v = getBusVoltage();
    return (!isnan(v) && v > 4.5f);
}

/* ========================================================================== */
/*  AW32257 status  —  lee reg 0x00 una vez y parsea todo                     */
/* ========================================================================== */

aw32257_status_t M5Hat_18650C::getStatus()
{
    aw32257_status_t st = {};
    uint8_t reg         = _readReg8(AW32257_ADDR, AW_REG_STATUS);
    if (reg == 0xFF) return st;  // I²C error → all-zero struct

    st.otg       = (reg & AW_OTG_MASK) != 0;
    st.en_stat   = (reg & AW_EN_STAT_MASK) != 0;
    st.stat      = (aw_chg_stat_t)((reg & AW_STAT_MASK) >> 4);  // bits [5:4]
    st.boost     = (reg & AW_BOOST_MASK) != 0;                  // bit  [3]
    st.chg_fault = (aw_chg_fault_t)(reg & AW_CHG_FAULT_MASK);   // bits [2:0]
    return st;
}

aw_chg_stat_t M5Hat_18650C::getChargeState()
{
    return getStatus().stat;
}

aw_chg_fault_t M5Hat_18650C::getFaultDetail()
{
    return getStatus().chg_fault;
}

bool M5Hat_18650C::isCharging()
{
    aw32257_status_t st = getStatus();
    // INA226 current > 0 → charging; AW32257 stat == PROGRESS → charging
    if (st.stat == AW_CHG_PROGRESS) return true;
    float cur = getCurrent();
    return (!isnan(cur) && cur > 0.02f);
}

bool M5Hat_18650C::isBoostMode()
{
    return getStatus().boost;
}

/* ========================================================================== */
/*  Detection                                                                 */
/* ========================================================================== */

bool M5Hat_18650C::isIna226Found() const
{
    return _ina226Found;
}
bool M5Hat_18650C::isChargerFound() const
{
    return _chargerFound;
}

/* ========================================================================== */
/*  AW32257 charge parameter config                                           */
/* ========================================================================== */

void M5Hat_18650C::setChargerEnabled(bool en)
{
    if (!_chargerFound) return;
    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    if (ctrl == 0xFF) return;
    if (en)
        ctrl &= ~AW_CEN_MASK;
    else
        ctrl |= AW_CEN_MASK;
    _writeReg16(AW32257_ADDR, AW_REG_CONTROL, ctrl);
}

uint16_t M5Hat_18650C::_mAtoIchg(uint16_t mA)
{
    for (int i = 15; i >= 0; i--) {
        if (mA >= AW_ICHG_TABLE[i]) return (uint16_t)i;
    }
    return 0;
}

void M5Hat_18650C::setChargeCurrent(uint16_t mA)
{
    if (!_chargerFound) return;
    uint8_t ichgCode = (uint8_t)_mAtoIchg(mA);
    uint8_t reg4     = _readReg8(AW32257_ADDR, AW_REG_ICHG);
    if (reg4 == 0xFF) return;
    reg4 &= 0x87;
    reg4 |= (ichgCode << 3);
    _writeReg16(AW32257_ADDR, AW_REG_ICHG, reg4);
}

uint8_t M5Hat_18650C::_vToVoreg(float v)
{
    return _voregToCode(v);
}

void M5Hat_18650C::setChargeVoltage(float v)
{
    if (!_chargerFound) return;
    uint8_t code = _voregToCode(v);
    uint8_t reg2 = _readReg8(AW32257_ADDR, AW_REG_VOREG);
    if (reg2 == 0xFF) return;
    reg2 &= 0x03;
    reg2 |= (code << 2);
    _writeReg16(AW32257_ADDR, AW_REG_VOREG, reg2);
}
