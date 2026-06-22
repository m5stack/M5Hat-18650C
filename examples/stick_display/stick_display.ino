/*!
 * @file stick_display.ino
 * @brief StickS3/StickC/StickCPlus/StickCPlus2/ArduinoNessoN1 display example. BtnA: Ichg  BtnB: Charge/Boost
 * @copyright 2026 M5Stack Technology CO LTD
 * @license MIT
 */
#include <M5Unified.h>
#include <M5Hat_18650C.h>

M5Hat_18650C hat;
M5Canvas canvas(&M5.Display);

static constexpr float LOW_BATTERY_V = 3.30f;
static const uint16_t ICHG_TABLE[]   = {496, 868, 992};
static uint8_t ichgLevel             = 1;

static bool getHatI2CPins(int8_t& sda, int8_t& scl)
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickS3:
            sda = 8;
            scl = 0;
            return true;
        case m5::board_t::board_M5StickC:
        case m5::board_t::board_M5StickCPlus:
        case m5::board_t::board_M5StickCPlus2:
            sda = 0;
            scl = 26;
            return true;
        case m5::board_t::board_ArduinoNessoN1:
            sda = 6;
            scl = 7;
            return true;
        default:
            return false;
    }
}

static const char* stateName(aw_chg_stat_t s)
{
    switch (s) {
        case AW_CHG_READY:
            return "Ready";
        case AW_CHG_PROGRESS:
            return "Charging";
        case AW_CHG_DONE:
            return "Full";
        case AW_CHG_FAULT:
            return "Fault";
        default:
            return "?";
    }
}

static uint16_t stateColor(aw_chg_stat_t s)
{
    switch (s) {
        case AW_CHG_READY:
            return ORANGE;
        case AW_CHG_PROGRESS:
            return YELLOW;
        case AW_CHG_DONE:
            return GREEN;
        case AW_CHG_FAULT:
            return RED;
        default:
            return WHITE;
    }
}

static void onButtons(float vbat, aw_chg_stat_t chg)
{
    if (M5.BtnA.wasPressed()) {
        ichgLevel = (ichgLevel + 1) % 3;
        hat.setChargeCurrent(ICHG_TABLE[ichgLevel]);
    }
    if (M5.BtnB.wasPressed()) {
        hat.setBoostMode(!hat.isBoostMode());
    }
}

static void draw(aw_chg_stat_t chg, float vbat, float cur, float pwr, float soc)
{
    canvas.fillScreen(BLACK);
    canvas.setTextColor(CYAN, BLACK);
    canvas.setTextSize(1.5);
    canvas.setCursor(4, 4);
    canvas.println("HAT 18650C 1.1");

    canvas.setTextColor(WHITE);
    canvas.setCursor(4, 20);
    canvas.printf("VBAT %.2fV\n", vbat);
    canvas.printf("I %+.3fA\n", cur);
    canvas.printf("P %+.3fW\n", pwr);
    canvas.printf("SOC %.0f%%\n", soc);

    canvas.setTextColor(stateColor(chg));
    canvas.printf("State: %s\n", stateName(chg));
    canvas.setTextColor(WHITE);
    canvas.printf("Ichg:%umA\n", ICHG_TABLE[ichgLevel]);
    canvas.printf("Mode: %s\n", hat.isBoostMode() ? "Boost" : "Charge");
    canvas.setTextColor(YELLOW);
    canvas.println("A:Ichg  B:Mode");
}

void setup()
{
    M5.begin();
    canvas.createSprite(M5.Display.width(), M5.Display.height());

    int8_t sda, scl;
    if (!getHatI2CPins(sda, scl)) {
        canvas.setTextSize(2);
        canvas.setCursor(4, 40);
        canvas.println("Unsupported");
        canvas.pushSprite(0, 0);
        while (1) delay(1000);
    }

    while (!hat.begin(&Wire, sda, scl)) {
        canvas.fillScreen(BLACK);
        canvas.setTextSize(2);
        canvas.setCursor(4, 40);
        canvas.println("HAT not found");
        canvas.pushSprite(0, 0);
        delay(500);
    }

    hat.setBatteryVoltageRange(3.0f, 4.2f);
}

void loop()
{
    M5.update();

    float vbat          = hat.getBatteryVoltage();
    float cur           = hat.getBatteryCurrent();
    float pwr           = hat.getBatteryPower();
    float soc           = hat.getBatteryPercentage();
    aw32257_status_t st = hat.getChargerStatus();

    onButtons(vbat, st.stat);
    draw(st.stat, vbat, cur, pwr, soc);
    canvas.pushSprite(0, 0);

    delay(200);
}
