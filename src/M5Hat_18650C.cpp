/*!
 * @file M5Hat_18650C.cpp
 * @copyright 2026 M5Stack Technology CO LTD
 * @license MIT
 */
#include "M5Hat_18650C.h"

static const uint16_t AW_ICHG_TABLE[16] = {496,  620,  868,  992,  1116, 1240, 1364, 1488,
                                           1612, 1736, 1860, 1984, 2108, 2232, 2356, 2480};

bool M5Hat_18650C::begin(TwoWire* wire, int8_t sda, int8_t scl)
{
    _wire = wire;

    if (sda >= 0 && scl >= 0) {
        _wire->end();
        _wire->begin(sda, scl, 400000);
    }

    _ina226Found  = _initIna226();
    _chargerFound = _initAw32257();
    return (_ina226Found && _chargerFound);
}

bool M5Hat_18650C::_initIna226()
{
    if (!_i2cProbe(INA226_ADDR)) return false;

    uint16_t cal = (uint16_t)(0.00512f / (INA226_CURRENT_LSB * INA226_SHUNT_OHM));
    _writeReg16(INA226_ADDR, INA226_REG_CALIBRATION, cal);
    return true;
}

bool M5Hat_18650C::_initAw32257()
{
    if (!_i2cProbe(AW32257_ADDR)) return false;

    _writeReg8(AW32257_ADDR, AW_REG_VOREG, AW_VOREG_4V20);
    _writeReg8(AW32257_ADDR, AW_REG_ICHG, (0x02 << 3) | AW_ITERM_DEFAULT);

    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    if (ctrl == 0xFF) return false;
    ctrl &= ~AW_OPA_MODE_MASK;
    _writeReg8(AW32257_ADDR, AW_REG_CONTROL, ctrl);
    return true;
}

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

void M5Hat_18650C::_writeReg8(uint8_t addr, uint8_t reg, uint8_t val)
{
    _wire->beginTransmission(addr);
    _wire->write(reg);
    _wire->write(val);
    _wire->endTransmission(true);
}

float M5Hat_18650C::getBusVoltage()
{
    uint16_t raw = _readReg16(INA226_ADDR, INA226_REG_BUSV);
    if (raw == 0xFFFF) return NAN;
    return raw * 1.25e-3f;
}

float M5Hat_18650C::getShuntVoltage()
{
    int16_t raw = (int16_t)_readReg16(INA226_ADDR, INA226_REG_SHUNTV);
    if (raw == (int16_t)0xFFFF) return NAN;
    return raw * 2.5e-3f;
}

float M5Hat_18650C::getCurrent()
{
    int16_t raw = (int16_t)_readReg16(INA226_ADDR, INA226_REG_CURRENT);
    if (raw == (int16_t)0xFFFF) return NAN;
    return raw * INA226_CURRENT_LSB;
}

float M5Hat_18650C::getPower()
{
    uint16_t raw = _readReg16(INA226_ADDR, INA226_REG_POWER);
    if (raw == 0xFFFF) return NAN;
    return raw * (25.0f * INA226_CURRENT_LSB);
}

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
    if (isnan(vEmpty) || isnan(vFull) || vEmpty >= vFull) return false;
    _vEmpty = vEmpty;
    _vFull  = vFull;
    return true;
}

aw32257_status_t M5Hat_18650C::getStatus()
{
    aw32257_status_t st = {};
    uint8_t reg         = _readReg8(AW32257_ADDR, AW_REG_STATUS);
    if (reg == 0xFF) return st;

    st.otg       = (reg & AW_OTG_MASK) != 0;
    st.en_stat   = (reg & AW_EN_STAT_MASK) != 0;
    st.stat      = (aw_chg_stat_t)((reg >> 4) & 0x03);
    st.boost     = (reg & AW_BOOST_MASK) != 0;
    st.chg_fault = (aw_chg_fault_t)(reg & AW_CHG_FAULT_MASK);
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
    return getChargeState() == AW_CHG_PROGRESS;
}

bool M5Hat_18650C::isBoostMode()
{
    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    if (ctrl == 0xFF) return false;
    return (ctrl & AW_OPA_MODE_MASK) != 0;
}

bool M5Hat_18650C::isIna226Found() const
{
    return _ina226Found;
}

bool M5Hat_18650C::isChargerFound() const
{
    return _chargerFound;
}

void M5Hat_18650C::setChargerEnabled(bool en)
{
    if (!_chargerFound) return;
    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    if (ctrl == 0xFF) return;
    if (en)
        ctrl &= ~AW_CEN_MASK;
    else
        ctrl |= AW_CEN_MASK;
    _writeReg8(AW32257_ADDR, AW_REG_CONTROL, ctrl);
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
    _writeReg8(AW32257_ADDR, AW_REG_ICHG, (ichgCode << 3) | AW_ITERM_DEFAULT);
}

uint8_t M5Hat_18650C::_voregToCode(float v)
{
    if (v < 3.50f) v = 3.50f;
    if (v > 4.50f) v = 4.50f;
    return (uint8_t)((v - 3.50f) / 0.02f + 0.5f);
}

void M5Hat_18650C::setChargeVoltage(float v)
{
    if (!_chargerFound) return;
    if (v >= 4.19f && v <= 4.21f) {
        _writeReg8(AW32257_ADDR, AW_REG_VOREG, AW_VOREG_4V20);
        return;
    }
    uint8_t code = _voregToCode(v);
    uint8_t reg2 = _readReg8(AW32257_ADDR, AW_REG_VOREG);
    if (reg2 == 0xFF) return;
    reg2 &= 0x03;
    reg2 |= (code << 2);
    _writeReg8(AW32257_ADDR, AW_REG_VOREG, reg2);
}

void M5Hat_18650C::setBoostMode(bool boost)
{
    if (!_chargerFound) return;
    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    if (ctrl == 0xFF) return;
    if (boost)
        ctrl |= AW_OPA_MODE_MASK;
    else
        ctrl &= ~AW_OPA_MODE_MASK;
    _writeReg8(AW32257_ADDR, AW_REG_CONTROL, ctrl);
}
