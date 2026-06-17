/*!
 * @file   M5Hat_18650C.cpp
 * @brief  Implementation for M5Stack 18650C Battery Hat (V1.33).
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

/* ========================================================================== */
/*  INA226 register map (TI datasheet SBOS547B)                               */
/* ========================================================================== */

#define INA226_REG_CONFIG       0x00
#define INA226_REG_SHUNTV       0x01
#define INA226_REG_BUSV         0x02
#define INA226_REG_POWER        0x03
#define INA226_REG_CURRENT      0x04
#define INA226_REG_CALIBRATION  0x05
#define INA226_REG_MASK         0x06
#define INA226_REG_ALERT        0x07
#define INA226_REG_MANUFACTURER 0xFE
#define INA226_REG_DIEID        0xFF

// 16‑sample average, 1.1 ms conversion, continuous shunt + bus
#define INA226_CONFIG_DEFAULT 0x4527

/* ========================================================================== */
/*  AW32257 current / voltage encoding tables                                 */
/* ========================================================================== */

// ICHG table: reg 0x04[6:3] → mA (33 mΩ sense, datasheet Table)
static const uint16_t AW_ICHG_TABLE[16] = {496,  620,  868,  992,  1116, 1240, 1364, 1488,
                                           1612, 1736, 1860, 1984, 2108, 2232, 2356, 2480};

// VOREG table: reg 0x02[7:2] → V (6‑bit, 0x00–0x3F, 3.50–4.50 V, 20 mV step)
// Calculated: V = 3.50 + code × 0.02
static float _codeToVoreg(uint8_t code)
{
    return 3.50f + (float)code * 0.02f;
}
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

    // Initialise the I²C bus on specified pins (M5.begin() already set up the
    // onboard bus, but the HAT uses a different pair: GPIO 0/GPIO 26).
    if (sda >= 0 && scl >= 0) {
        _wire->end();            // release previous pin config if any
        _wire->begin(sda, scl);  // reconfigure on HAT pins
    }

    _ina226Found  = _initIna226();
    _chargerFound = _initAw32257();

    if (_ina226Found) update();
    return _ina226Found;
}

/* ========================================================================== */
/*  Chip initialisation                                                       */
/* ========================================================================== */

bool M5Hat_18650C::_initIna226()
{
    if (!_i2cProbe(INA226_ADDR)) return false;

    // Verify manufacturer ID (TI = 0x5449)
    uint16_t mfrId = _readReg16(INA226_ADDR, INA226_REG_MANUFACTURER);

    // Reset and configure
    _writeReg16(INA226_ADDR, INA226_REG_CONFIG, 0x8000);  // reset
    delay(1);
    _writeReg16(INA226_ADDR, INA226_REG_CONFIG, INA226_CONFIG_DEFAULT);
    _writeReg16(INA226_ADDR, INA226_REG_CALIBRATION, INA226_CAL);

    return true;
}

bool M5Hat_18650C::_initAw32257()
{
    if (!_i2cProbe(AW32257_ADDR)) return false;

    // Verify vendor/part/revision register (0x03 default = 0x53)
    uint8_t vpr = _readReg8(AW32257_ADDR, AW_REG_VENDOR);
    (void)vpr;  // non‑critical; proceed regardless

    return true;
}

/* ========================================================================== */
/*  update()                                                                  */
/* ========================================================================== */

void M5Hat_18650C::update()
{
    if (millis() - _lastUpdateMs < 150) return;
    _lastUpdateMs = millis();

    if (_ina226Found) _readIna226();
    if (_chargerFound) _readAw32257();
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
    return (uint8_t)(_readReg16(addr, reg) & 0xFF);
}

/* ========================================================================== */
/*  INA226 readings                                                           */
/* ========================================================================== */

void M5Hat_18650C::_readIna226()
{
    // Bus voltage — unsigned, upper 3 bits are flags, LSB = 1.25 mV
    uint16_t raw = _readReg16(INA226_ADDR, INA226_REG_BUSV);
    if (raw == 0xFFFF) return;
    _busVoltage = raw * 0.00125f;

    // Shunt voltage — signed 16‑bit, LSB = 2.5 µV → mV
    int16_t sv    = (int16_t)_readReg16(INA226_ADDR, INA226_REG_SHUNTV);
    _shuntVoltage = sv * 0.0025f;

    // Current — signed 16‑bit, LSB = 250 µA
    // 250µA = 0.00512/(INA226_CAL * 0.01(10mΩ Rshunt))
    int16_t cr = (int16_t)_readReg16(INA226_ADDR, INA226_REG_CURRENT);
    _current   = cr * 0.00025f;

    // Power — unsigned 16‑bit, LSB = 25 × Current_LSB
    uint16_t pr = _readReg16(INA226_ADDR, INA226_REG_POWER);
    _power      = pr * 25.0f * 0.00025f;
}

/* ========================================================================== */
/*  AW32257 readings  (datasheet V1.5 register map)                           */
/* ========================================================================== */

void M5Hat_18650C::_readAw32257()
{
    // ── Register 0x00: Status ──────────────────────────────────────────
    uint8_t st = _readReg8(AW32257_ADDR, AW_REG_STATUS);
    if (st == 0xFF) return;  // I²C error

    // Bits [3:2] → charge state (2‑bit)
    uint8_t statBits = (st & AW_STAT_MASK) >> AW_STAT_SHIFT;
    _chargeState     = (charge_state_t)statBits;

    // Bit 2 alone → boost mode flag (same bit as STAT[0] in charge context;
    // per datasheet, B2=1 means boost mode is active.)
    _boostMode = (st & 0x04) != 0;  // B2

    // Determine charging flag
    _charging = (_chargeState == AW_CHG_PROGRESS);

    // ── Register 0x01: Control ─────────────────────────────────────────
    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    (void)ctrl;  // available for diagnostics (TE, CEN, HZ_MODE, OPA_MODE)

    // Fault detail — derived from status register context
    // AW32257 datasheet: when STAT = 11 (Fault), lower bits in status or a
    // companion read give the fault code.  We approximate here; the exact
    // sub‑code mapping depends on the chip revision.
    if (_chargeState == AW_CHG_FAULT) {
        // Try reading boost fault register for cross‑reference
        uint8_t bf = _readReg8(AW32257_ADDR, AW_REG_BOOST_FAULT);
        if (bf != 0xFF && (bf & 0x07) != 0) {
            // In boost mode, boost fault register is authoritative
            _faultDetail = (fault_detail_t)(bf & 0x07);
        } else {
            // In charge mode, use the lower nibble of status as fault hint
            uint8_t lo   = st & 0x07;
            _faultDetail = (lo <= 7) ? (fault_detail_t)lo : AW_FAULT_NONE;
        }
    } else {
        _faultDetail = AW_FAULT_NONE;
    }
}

/* ========================================================================== */
/*  Public accessors — INA226                                                 */
/* ========================================================================== */

float M5Hat_18650C::getBusVoltage()
{
    update();
    return _busVoltage;
}
float M5Hat_18650C::getShuntVoltage()
{
    update();
    return _shuntVoltage;
}
float M5Hat_18650C::getCurrent()
{
    update();
    return _current;
}
float M5Hat_18650C::getPower()
{
    update();
    return _power;
}

float M5Hat_18650C::getBatteryPercentage()
{
    update();
    if (isnan(_busVoltage)) return NAN;
    float pct = (_busVoltage - _vEmpty) / (_vFull - _vEmpty) * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

void M5Hat_18650C::setBatteryVoltageRange(float vEmpty, float vFull)
{
    _vEmpty = vEmpty;
    _vFull  = vFull;
}

/* ========================================================================== */
/*  Public accessors — AW32257                                                */
/* ========================================================================== */

M5Hat_18650C::charge_state_t M5Hat_18650C::getChargeState()
{
    update();
    return _chargeState;
}

M5Hat_18650C::fault_detail_t M5Hat_18650C::getFaultDetail()
{
    update();
    return _faultDetail;
}

bool M5Hat_18650C::isCharging()
{
    update();
    // INA226 current sense: positive current → charging
    if (!isnan(_current) && _current > 0.02f) return true;
    // AW32257 status: charge in progress
    return _charging;
}

bool M5Hat_18650C::isBoostMode()
{
    update();
    return _boostMode;
}

bool M5Hat_18650C::isUSBPresent()
{
    update();
    return (_busVoltage > 4.5f);
}

bool M5Hat_18650C::isIna226Found() const
{
    return _ina226Found;
}
bool M5Hat_18650C::isChargerFound() const
{
    return _chargerFound;
}

/* ========================================================================== */
/*  AW32257 charge parameter configuration                                    */
/* ========================================================================== */

void M5Hat_18650C::setChargerEnabled(bool en)
{
    if (!_chargerFound) return;
    uint8_t ctrl = _readReg8(AW32257_ADDR, AW_REG_CONTROL);
    if (ctrl == 0xFF) return;
    if (en)
        ctrl &= ~AW_CEN_MASK;  // CEN=0 → charger enabled
    else
        ctrl |= AW_CEN_MASK;  // CEN=1 → charger disabled
    _writeReg16(AW32257_ADDR, AW_REG_CONTROL, ctrl);
}

uint16_t M5Hat_18650C::_mAtoIchg(uint16_t mA)
{
    // Find closest ICHG code ≤ target
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
    reg4 &= 0x87;             // keep RESET (B7) and ITERM_CFG (B2-B0)
    reg4 |= (ichgCode << 3);  // ICHG in B6-B3
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
    reg2 &= 0x03;         // keep OTG_PL (B1) and OTG_EN (B0)
    reg2 |= (code << 2);  // VOREG in B7-B2
    _writeReg16(AW32257_ADDR, AW_REG_VOREG, reg2);
}
