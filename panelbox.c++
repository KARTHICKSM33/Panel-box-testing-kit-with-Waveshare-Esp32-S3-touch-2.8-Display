

#include <Wire.h>
#include <Adafruit_INA260.h>
#include <ModbusMaster.h>

#include "Display_ST7789.h"
#include "LVGL_Driver.h"
#include "protovolt_logo.c"
//=====================================================
// POWER SENSOR (INA260) CONFIG
//=====================================================
#define SDA_PIN         11
#define SCL_PIN         10

#define VOLT_MIN        11.90
#define VOLT_MAX        12.90

Adafruit_INA260 ina260;
bool  ina260Found      = false;
float lastValidVoltage = 0.0;

//=====================================================
// PLC / RS485 CONFIG
//=====================================================
#define RXD2            44
#define TXD2            43
#define EN485           18

#define PLC_SLAVE_ID    1
#define PLC_BAUDRATE    115200

HardwareSerial RS485Serial(1);
ModbusMaster   node;

//=====================================================
// RELAY STATE (R0 - R7)
//=====================================================
bool relayState[8] =
{
    false, false, false, false,
    false, false, false, false
};

//=====================================================
// UI COLORS
//=====================================================
#define COLOR_BG        lv_color_hex(0xF5F6F8)
#define COLOR_CARD      lv_color_hex(0xFFFFFF)
#define COLOR_BORDER    lv_color_hex(0xE3E3E3)
#define COLOR_TEXT      lv_color_hex(0x2E2E2E)
#define COLOR_SUBTEXT   lv_color_hex(0x8A8A8A)
#define COLOR_PASS      lv_color_hex(0x43A047)
#define COLOR_FAIL      lv_color_hex(0xE53935)
#define COLOR_EMPTY     lv_color_hex(0xCFCFCF)
#define COLOR_ACCENT    lv_color_hex(0x5C6BC0)
#define COLOR_RELAY_ON  lv_color_hex(0x43A047)
#define COLOR_RELAY_OFF lv_color_hex(0xB0B4BB)

// Slot tint backgrounds (relay control page only)
#define COLOR_SLOT1_BG  lv_color_hex(0xEEF1FC)
#define COLOR_SLOT2_BG  lv_color_hex(0xEDF9F0)

//=====================================================
// STATUS ICON STATE
//=====================================================
enum StatusState
{
    STATE_EMPTY = 0,
    STATE_PASS,
    STATE_FAIL
};

//=====================================================
// LVGL SCREEN OBJECTS
//=====================================================

lv_obj_t *logoScreen;
lv_obj_t *mainScreen;

lv_obj_t *testScreen;
lv_obj_t *relayScreen;

lv_obj_t *testIconCircle[4];
lv_obj_t *testIconLabel[4];
lv_obj_t *interlockLabel;

lv_obj_t *relayBtn[8];
lv_obj_t *relayLabel[8];
lv_obj_t *relayStatusLabel;

//=====================================================
// RS485 DIRECTION CONTROL
//=====================================================
void preTransmission()
{
    digitalWrite(EN485, HIGH);
    delayMicroseconds(100);
}

void postTransmission()
{
    delayMicroseconds(100);
    digitalWrite(EN485, LOW);
}

//=====================================================
// SET A TEST STATUS ICON (grey / green tick / red X)
//=====================================================
void setStatusIcon(int index, StatusState state)
{
    switch (state)
    {
        case STATE_PASS:
            lv_obj_set_style_bg_color(testIconCircle[index], COLOR_PASS, 0);
            lv_label_set_text(testIconLabel[index], LV_SYMBOL_OK);
            break;

        case STATE_FAIL:
            lv_obj_set_style_bg_color(testIconCircle[index], COLOR_FAIL, 0);
            lv_label_set_text(testIconLabel[index], LV_SYMBOL_CLOSE);
            break;

        default: // STATE_EMPTY
            lv_obj_set_style_bg_color(testIconCircle[index], COLOR_EMPTY, 0);
            lv_label_set_text(testIconLabel[index], "");
            break;
    }
}

//=====================================================
// RESET ALL BOXES BEFORE A NEW TEST RUN
//=====================================================
void resetTestUI()
{
    for (int i = 0; i < 4; i++)
    {
        setStatusIcon(i, STATE_EMPTY);
    }
    lv_obj_add_flag(interlockLabel, LV_OBJ_FLAG_HIDDEN);
}

//=====================================================
// RUN ALL 4 TESTS
//=====================================================
void runAllTests()
{
     uint8_t result;
    
    resetTestUI();

    //-------------------------------------------------
    // TEST 1 : POWER CHECK (INA260)
    //-------------------------------------------------
    bool  test1Pass = false;
    bool test2Pass = false;
bool test3Pass = false;
    float voltage   = lastValidVoltage;

    if (ina260Found)
    {
        float reading = ina260.readBusVoltage() / 1000.0; // mV -> V

        if (!isnan(reading) && reading >= 0.0 && reading <= 36.0)
        {
            lastValidVoltage = reading;
            voltage = reading;
        }
        else
        {
            voltage = lastValidVoltage; // fallback to last good reading
        }
    }

    if (voltage >= VOLT_MIN && voltage <= VOLT_MAX)
    {
        test1Pass = true;
    }
    setStatusIcon(0, test1Pass ? STATE_PASS : STATE_FAIL);

    Serial.print("Test1 Power Voltage: ");
    Serial.print(voltage, 3);
    Serial.println(test1Pass ? " V  -> PASS" : " V  -> FAIL");

    //-------------------------------------------------
    // TEST 2 : SENSOR 1 CHECK (DI0)
    //-------------------------------------------------
   //-------------------------------------------------
// Turn ON Relay 2
//-------------------------------------------------
node.writeSingleCoil(2, true);

delay(300);

//-------------------------------------------------
// Read DI0
//-------------------------------------------------
bool di0 = false;

result = node.readDiscreteInputs(0, 1);

if(result == node.ku8MBSuccess)
{
    di0 = bitRead(node.getResponseBuffer(0), 0);
}

//-------------------------------------------------
// Update UI
//-------------------------------------------------
setStatusIcon(1, di0 ? STATE_PASS : STATE_FAIL);


Serial.println(test1Pass ? "Sensor 1 -> PASS" : "Sensor 1 -> FAIL");
//-------------------------------------------------
// Turn OFF Relay 2
//-------------------------------------------------
node.writeSingleCoil(2, false);

    //-------------------------------------------------
//-------------------------------------------------
// Turn ON Relay 3
//-------------------------------------------------
node.writeSingleCoil(0, true);

delay(300);

//-------------------------------------------------
// Read DI1
//-------------------------------------------------
bool di1 = false;

result = node.readDiscreteInputs(1, 1);

if(result == node.ku8MBSuccess)
{
    di1 = bitRead(node.getResponseBuffer(0), 0);
}

//-------------------------------------------------
// Update UI
//-------------------------------------------------
test2Pass = di1;

setStatusIcon(2, di1 ? STATE_PASS : STATE_FAIL);

Serial.println(test2Pass ? "Sensor 2 -> PASS" : "Sensor 2 -> FAIL");

//-------------------------------------------------
// Turn OFF Relay 3
//-------------------------------------------------
node.writeSingleCoil(0, false);
   //-------------------------------------------------
// TEST 4 : SENSOR 3 CHECK (DI2)
//-------------------------------------------------

// Turn ON Relay 4
node.writeSingleCoil(4, true);

delay(300);

// Read DI2
bool di2 = false;

result = node.readDiscreteInputs(2, 1);

if(result == node.ku8MBSuccess)
{
    di2 = bitRead(node.getResponseBuffer(0), 0);
}

// Update UI
test3Pass = di2;

setStatusIcon(3, di2 ? STATE_PASS : STATE_FAIL);

Serial.println(test3Pass ? "Sensor 3 -> PASS"
                         : "Sensor 3 -> FAIL");

// Show/Hide Interlock Message
if(test3Pass)
{
    lv_obj_add_flag(interlockLabel, LV_OBJ_FLAG_HIDDEN);
}
else
{
    lv_obj_clear_flag(interlockLabel, LV_OBJ_FLAG_HIDDEN);
}

// Turn OFF Relay 4
node.writeSingleCoil(4, false);
}

//=====================================================
// EVENT : START TEST BUTTON
//=====================================================
static void startTestEventCB(lv_event_t *e)
{
    runAllTests();
}

//=====================================================
// EVENT : RESET BUTTON
//=====================================================
static void resetTestEventCB(lv_event_t *e)
{
    resetTestUI();

    Serial.println("Tests Reset");
}

//=====================================================
// EVENT : OPEN RELAY SCREEN
//=====================================================
static void openRelayScreenCB(lv_event_t *e)
{
    updateRelayStatus();

    lv_scr_load(relayScreen);
}

//=====================================================
// EVENT : BACK TO TEST SCREEN
//=====================================================
static void backToTestScreenCB(lv_event_t *e)
{
    lv_scr_load(testScreen);
}

//=====================================================
// EVENT : RELAY BUTTON TOGGLE
//=====================================================
static void relayEventCB(lv_event_t *e)
{
    uint8_t relay = (uint8_t)(intptr_t)lv_event_get_user_data(e);

    relayState[relay] = !relayState[relay];

    uint8_t result = node.writeSingleCoil(relay, relayState[relay]);

    if(result == node.ku8MBSuccess)
{
    relayState[relay] = !relayState[relay];

    lv_obj_set_style_bg_color(
        relayBtn[relay],
        relayState[relay] ?
        COLOR_PASS :
        COLOR_RELAY_OFF,
        0);

    lv_obj_set_style_text_color(
        relayLabel[relay],
        relayState[relay] ?
        lv_color_white() :
        COLOR_TEXT,
        0);

    String status = "Relay ";
    status += relay;
    status += relayState[relay] ?
              " turned OFF" :
              " turned ON";

    lv_label_set_text(
        relayStatusLabel,
        status.c_str());

    Serial.println(status);
}
    else
    {
        relayState[relay] = !relayState[relay]; // revert on failure
        lv_label_set_text(relayStatusLabel, "Status: Write Failed");

        Serial.print("Modbus Error : ");
        Serial.println(result);
    }
}

//=====================================================
// READ ALL RELAY STATUS FROM PLC
//=====================================================
void updateRelayStatus()
{
    uint8_t result = node.readCoils(0, 8);

    if(result != node.ku8MBSuccess)
    {
        return;
    }

    uint8_t coils = node.getResponseBuffer(0);

    for(int i = 0; i < 8; i++)
    {
        relayState[i] = bitRead(coils, i);

        //----------------------------
        // Relay Button Color
        //----------------------------
        lv_obj_set_style_bg_color(
            relayBtn[i],
            relayState[i] ?
            COLOR_PASS :
            COLOR_RELAY_OFF,
            0);

        //----------------------------
        // Relay Text
        //----------------------------
        lv_obj_set_style_text_color(
            relayLabel[i],
            relayState[i] ?
            lv_color_white() :
            COLOR_TEXT,
            0);
    }
}

//=====================================================
// BUILD TEST SCREEN (SCREEN 1)
//=====================================================
void createTestScreen()
{
    testScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(testScreen, COLOR_BG, 0);
    lv_obj_clear_flag(testScreen, LV_OBJ_FLAG_SCROLLABLE);

    //-------------------------------------------------
    // HEADER : TITLE + SUBTITLE
    //-------------------------------------------------
    lv_obj_t *title = lv_label_create(testScreen);
    lv_label_set_text(title, "PROTOVOLT");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 6);

    lv_obj_t *subtitle = lv_label_create(testScreen);
    lv_label_set_text(subtitle, "Panel Box Testing Kit");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subtitle, COLOR_SUBTEXT, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 10, 30);

    //-------------------------------------------------
    // RELAY CONTROL BUTTON (TOP RIGHT)
    //-------------------------------------------------
    lv_obj_t *relayTopBtn = lv_btn_create(testScreen);
    lv_obj_set_size(relayTopBtn, 66, 40);
    lv_obj_align(relayTopBtn, LV_ALIGN_TOP_RIGHT, -8, 6);
    lv_obj_set_style_bg_color(relayTopBtn, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(relayTopBtn, 8, 0);
    lv_obj_set_style_pad_all(relayTopBtn, 2, 0);
    lv_obj_add_event_cb(relayTopBtn, openRelayScreenCB, LV_EVENT_CLICKED, NULL);

    lv_obj_t *relayTopLbl = lv_label_create(relayTopBtn);
    lv_label_set_text(relayTopLbl, "Relay\nControl");
    lv_obj_set_style_text_font(relayTopLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(relayTopLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(relayTopLbl);

    //-------------------------------------------------
    // SEPARATOR LINE
    //-------------------------------------------------
    lv_obj_t *line = lv_obj_create(testScreen);
    lv_obj_set_size(line, 220, 2);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_bg_color(line, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    //-------------------------------------------------
    // 4 TEST CARDS
    //-------------------------------------------------
    const char *testNames[4] =
    {
        "POWER TEST",
        "TRIGGER CHECK",
        "TB15 CHECK",
        "INTERLOCK CHECK"
    };

    int rowY      = 66;
    int rowHeight = 38;
    int rowGap    = 6;

    for (int i = 0; i < 4; i++)
    {
        lv_obj_t *card = lv_obj_create(testScreen);
        lv_obj_set_size(card, 220, rowHeight);
        lv_obj_set_pos(card, 10, rowY + i * (rowHeight + rowGap));
        lv_obj_set_style_bg_color(card, COLOR_CARD, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, COLOR_BORDER, 0);
        lv_obj_set_style_shadow_width(card, 4, 0);
        lv_obj_set_style_shadow_color(card, lv_color_hex(0xDDDDDD), 0);
        lv_obj_set_style_shadow_ofs_y(card, 1, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, testNames[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl, COLOR_TEXT, 0);
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 10, 0);

        lv_obj_t *circle = lv_obj_create(card);
        lv_obj_set_size(circle, 26, 26);
        lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(circle, 0, 0);
        lv_obj_set_style_bg_color(circle, COLOR_EMPTY, 0);
        lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(circle, LV_ALIGN_RIGHT_MID, -10, 0);
        testIconCircle[i] = circle;

        lv_obj_t *iconLbl = lv_label_create(circle);
        lv_label_set_text(iconLbl, "");
        lv_obj_set_style_text_color(iconLbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(iconLbl);
        testIconLabel[i] = iconLbl;
    }

    //-------------------------------------------------
    // INTERLOCK NOT AVAILABLE LABEL (hidden by default)
    //-------------------------------------------------
    interlockLabel = lv_label_create(testScreen);
    lv_label_set_text(interlockLabel, "Interlock is not Available");
    lv_obj_set_style_text_font(interlockLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(interlockLabel, COLOR_FAIL, 0);
    lv_obj_align(interlockLabel, LV_ALIGN_TOP_MID, 0,
                 rowY + 4 * (rowHeight + rowGap) + 6);
    lv_obj_add_flag(interlockLabel, LV_OBJ_FLAG_HIDDEN);

    //-------------------------------------------------
    // START TEST BUTTON
    //-------------------------------------------------
    lv_obj_t *startBtn = lv_btn_create(testScreen);
    lv_obj_set_size(startBtn, 100, 38);
lv_obj_align(startBtn, LV_ALIGN_BOTTOM_LEFT, 15, -10);
    lv_obj_set_style_bg_color(startBtn, COLOR_PASS, 0);
    lv_obj_set_style_radius(startBtn, 19, 0);
    lv_obj_add_event_cb(startBtn, startTestEventCB, LV_EVENT_CLICKED, NULL);

    lv_obj_t *startLbl = lv_label_create(startBtn);
    lv_label_set_text(startLbl, "START TEST");
    lv_obj_set_style_text_font(startLbl, &lv_font_montserrat_16, 0);
    lv_obj_center(startLbl);
    //-------------------------------------------------
// RESET BUTTON
//-------------------------------------------------
lv_obj_t *resetBtn = lv_btn_create(testScreen);

lv_obj_set_size(resetBtn, 100, 38);

lv_obj_align(resetBtn,
             LV_ALIGN_BOTTOM_RIGHT,
             -15,
             -10);

lv_obj_set_style_bg_color(
    resetBtn,
    COLOR_FAIL,
    0);

lv_obj_set_style_radius(
    resetBtn,
    19,
    0);

lv_obj_add_event_cb(
    resetBtn,
    resetTestEventCB,
    LV_EVENT_CLICKED,
    NULL);

lv_obj_t *resetLbl = lv_label_create(resetBtn);

lv_label_set_text(
    resetLbl,
    "RESET");

lv_obj_set_style_text_font(
    resetLbl,
    &lv_font_montserrat_16,
    0);

lv_obj_center(resetLbl);
}

//=====================================================
// BUILD RELAY CONTROL SCREEN (SCREEN 2)
// Layout (240 x 320 confirmed display resolution):
//
//   [< Back]      RELAY CONTROL
//   -----------------------------------
//   |            SLOT - 1              |
//   |   [ R0 ]          [ R1 ]         |
//   |  Coil Add:0       Coil Add:1     |
//   |                                   |
//   |   [ R2 ]          [ R3 ]         |
//   |  Coil Add:2       Coil Add:3     |
//   -----------------------------------
//   |            SLOT - 2              |
//   |   [ R0 ]          [ R1 ]         |
//   |  Coil Add:4       Coil Add:5     |
//   |                                   |
//   |   [ R2 ]          [ R3 ]         |
//   |  Coil Add:6       Coil Add:7     |
//   -----------------------------------
//   Touch any relay to control
//
// Every row now has a clear 18-19 px gap between the
// "Coil Add" text and the button below it - no overlap.
//=====================================================
void createRelayScreen()
{
    relayScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(relayScreen, COLOR_BG, 0);
    lv_obj_clear_flag(relayScreen, LV_OBJ_FLAG_SCROLLABLE);

    //-------------------------------------------------
    // HEADER ROW : BACK BUTTON + TITLE (clear gap, no overlap)
    //-------------------------------------------------
    lv_obj_t *backBtn = lv_btn_create(relayScreen);
    lv_obj_set_size(backBtn, 52, 28);
    lv_obj_align(backBtn, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_obj_set_style_bg_color(backBtn, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(backBtn, 8, 0);
    lv_obj_set_style_pad_all(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, backToTestScreenCB, LV_EVENT_CLICKED, NULL);

    lv_obj_t *backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(backLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(backLbl);

    lv_obj_t *title = lv_label_create(relayScreen);
    lv_label_set_text(title, "RELAY CONTROL");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 68, 14);

    //-------------------------------------------------
    // TOP DIVIDER
    //-------------------------------------------------
    lv_obj_t *line = lv_obj_create(relayScreen);
    lv_obj_set_size(line, 220, 2);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(line, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    //-------------------------------------------------
    // SLOT 1 CARD (tinted blue, relays 0-3, coils 0-3)
    //-------------------------------------------------
    lv_obj_t *slot1 = lv_obj_create(relayScreen);
    lv_obj_set_size(slot1, 220, 110);
    lv_obj_align(slot1, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_set_style_radius(slot1, 12, 0);
    lv_obj_set_style_bg_color(slot1, COLOR_SLOT1_BG, 0);
    lv_obj_set_style_border_width(slot1, 2, 0);
    lv_obj_set_style_border_color(slot1, COLOR_ACCENT, 0);
    lv_obj_set_style_pad_all(slot1, 0, 0);
    lv_obj_clear_flag(slot1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *slot1Title = lv_label_create(slot1);
    lv_label_set_text(slot1Title, "SLOT - 1");
    lv_obj_set_style_text_font(slot1Title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(slot1Title, COLOR_ACCENT, 0);
    lv_obj_align(slot1Title, LV_ALIGN_TOP_MID, 0, 4);

    //-------------------------------------------------
    // SLOT 2 CARD (tinted green, relays 4-7, coils 4-7)
    //-------------------------------------------------
    lv_obj_t *slot2 = lv_obj_create(relayScreen);
    lv_obj_set_size(slot2, 220, 110);
    lv_obj_align(slot2, LV_ALIGN_TOP_MID, 0, 172);
    lv_obj_set_style_radius(slot2, 12, 0);
    lv_obj_set_style_bg_color(slot2, COLOR_SLOT2_BG, 0);
    lv_obj_set_style_border_width(slot2, 2, 0);
    lv_obj_set_style_border_color(slot2, COLOR_PASS, 0);
    lv_obj_set_style_pad_all(slot2, 0, 0);
    lv_obj_clear_flag(slot2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *slot2Title = lv_label_create(slot2);
    lv_label_set_text(slot2Title, "SLOT - 2");
    lv_obj_set_style_text_font(slot2Title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(slot2Title, COLOR_PASS, 0);
    lv_obj_align(slot2Title, LV_ALIGN_TOP_MID, 0, 4);

    //-------------------------------------------------
    // ROW / COLUMN CONSTANTS (shared by both slots)
    // Row0 button : y = 24  (24 - 48)
    // Row0 coil   : y = 50  (50 - 61)
    // Row1 button : y = 68  (68 - 92)   <- 7px clear of coil0
    // Row1 coil   : y = 94  (94 - 105)  <- fits inside 110px card
    //-------------------------------------------------
    const int colX[2]   = { 10, 120 };   // two columns, wide gap
    const int rowBtnY[2] = { 24, 68 };
    const int rowCoilY[2] = { 51, 95 };
    const int btnW = 90;
    const int btnH = 24;

    //-------------------------------------------------
    // SLOT 1 RELAYS (index 0 - 3, coil address 0 - 3)
    //-------------------------------------------------
    for (int i = 0; i < 4; i++)
    {
        int col = i % 2;
        int row = i / 2;

        relayBtn[i] = lv_btn_create(slot1);
        lv_obj_set_size(relayBtn[i], btnW, btnH);
        lv_obj_set_pos(relayBtn[i], colX[col], rowBtnY[row]);
        lv_obj_set_style_radius(relayBtn[i], 8, 0);
        lv_obj_set_style_bg_color(relayBtn[i], COLOR_RELAY_OFF, 0);
        lv_obj_add_event_cb(relayBtn[i], relayEventCB, LV_EVENT_CLICKED,
                             (void *)(intptr_t)i);

        relayLabel[i] = lv_label_create(relayBtn[i]);
        String txt = "R";
        txt += i;
        lv_label_set_text(relayLabel[i], txt.c_str());
        lv_obj_set_style_text_font(relayLabel[i], &lv_font_montserrat_14, 0);
        lv_obj_center(relayLabel[i]);

        //--------------------------
        // Coil Address label - centered under its own button
        //--------------------------
        lv_obj_t *coil = lv_label_create(slot1);
        String addr = "Coil Add:";
        addr += i;
        lv_label_set_text(coil, addr.c_str());
        lv_obj_set_style_text_font(coil, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(coil, COLOR_SUBTEXT, 0);
        lv_obj_set_width(coil, btnW);
        lv_obj_set_style_text_align(coil, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(coil, colX[col], rowCoilY[row]);
    }

    //-------------------------------------------------
    // SLOT 2 RELAYS (index 4 - 7, coil address 4 - 7)
    //-------------------------------------------------
    for (int i = 4; i < 8; i++)
    {
        int index = i - 4;
        int col = index % 2;
        int row = index / 2;

        relayBtn[i] = lv_btn_create(slot2);
        lv_obj_set_size(relayBtn[i], btnW, btnH);
        lv_obj_set_pos(relayBtn[i], colX[col], rowBtnY[row]);
        lv_obj_set_style_radius(relayBtn[i], 8, 0);
        lv_obj_set_style_bg_color(relayBtn[i], COLOR_RELAY_OFF, 0);
        lv_obj_add_event_cb(relayBtn[i], relayEventCB, LV_EVENT_CLICKED,
                             (void *)(intptr_t)i);

        relayLabel[i] = lv_label_create(relayBtn[i]);
        String txt = "R";
        txt += index;
        lv_label_set_text(relayLabel[i], txt.c_str());
        lv_obj_set_style_text_font(relayLabel[i], &lv_font_montserrat_14, 0);
        lv_obj_center(relayLabel[i]);

        //--------------------------
        // Coil Address label
        //--------------------------
        lv_obj_t *coil = lv_label_create(slot2);
        String addr = "Coil Add:";
        addr += i;
        lv_label_set_text(coil, addr.c_str());
        lv_obj_set_style_text_font(coil, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_color(coil, COLOR_SUBTEXT, 0);
        lv_obj_set_width(coil, btnW);
        lv_obj_set_style_text_align(coil, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(coil, colX[col], rowCoilY[row]);
    }

    //-------------------------------------------------
    // STATUS LABEL
    //-------------------------------------------------
    relayStatusLabel = lv_label_create(relayScreen);
    lv_label_set_text(relayStatusLabel, "Touch any relay to control");
    lv_obj_set_style_text_font(relayStatusLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(relayStatusLabel, COLOR_SUBTEXT, 0);
    lv_obj_align(relayStatusLabel, LV_ALIGN_BOTTOM_MID, 0, -6);
}

/*==================================================================
   ================  NEW SECTION : PLC CHECK SCREEN  ================
   Added between the logo screen and the test screen.
   Nothing above or below this block was modified, other than the
   two lines marked "NEW" inside setup() that build/load this screen.
==================================================================*/

//=====================================================
// PLC CHECK - STATE MACHINE
//=====================================================
enum PLCCheckState
{
    PLC_STATE_HOME = 0,
    PLC_STATE_CHECKING,
    PLC_STATE_SUCCESS,
    PLC_STATE_FAILED,
    PLC_STATE_SCANNING,
    PLC_STATE_BAUD_FOUND
};

//=====================================================
// PLC CHECK - BAUD RATE CANDIDATES FOR SCAN
//=====================================================
const uint32_t plcBaudCandidates[]      = { 9600, 19200, 38400, 57600, 115200, 230400 };
const int      PLC_BAUD_CANDIDATES_COUNT = 6;

uint32_t detectedBaudRate = 0;

//=====================================================
// PLC CHECK - SCREEN / CONTAINER OBJECTS
//=====================================================
lv_obj_t *plcCheckScreen;

lv_obj_t *plcHomeCont;
lv_obj_t *plcCheckingCont;
lv_obj_t *plcSuccessCont;
lv_obj_t *plcFailedCont;
lv_obj_t *plcScanningCont;
lv_obj_t *plcBaudFoundCont;

lv_obj_t *plcScanRowLabels[PLC_BAUD_CANDIDATES_COUNT];
lv_obj_t *plcScanResultLabel;

lv_obj_t *plcFoundBaudValueLabel;
lv_obj_t *plcFlashBtnLabel;

//=====================================================
// PLC CHECK - FORWARD DECLARATIONS
//=====================================================
void setPLCCheckState(PLCCheckState state);
static void checkPLCBtnCB(lv_event_t *e);
static void scanBaudrateCB(lv_event_t *e);

//=====================================================
// PLC CHECK - SET / SHOW STATE
//=====================================================
void setPLCCheckState(PLCCheckState state)
{
    lv_obj_add_flag(plcHomeCont,      LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(plcCheckingCont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(plcSuccessCont,   LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(plcFailedCont,    LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(plcScanningCont,  LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(plcBaudFoundCont, LV_OBJ_FLAG_HIDDEN);

    switch (state)
    {
        case PLC_STATE_HOME:
            lv_obj_clear_flag(plcHomeCont, LV_OBJ_FLAG_HIDDEN);
            break;

        case PLC_STATE_CHECKING:
            lv_obj_clear_flag(plcCheckingCont, LV_OBJ_FLAG_HIDDEN);
            break;

        case PLC_STATE_SUCCESS:
            lv_obj_clear_flag(plcSuccessCont, LV_OBJ_FLAG_HIDDEN);
            break;

        case PLC_STATE_FAILED:
            lv_obj_clear_flag(plcFailedCont, LV_OBJ_FLAG_HIDDEN);
            break;

        case PLC_STATE_SCANNING:
            for (int i = 0; i < PLC_BAUD_CANDIDATES_COUNT; i++)
            {
                lv_label_set_text_fmt(plcScanRowLabels[i], "%lu   -   Waiting...",
                                       (unsigned long)plcBaudCandidates[i]);
            }
            lv_obj_add_flag(plcScanResultLabel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(plcScanningCont, LV_OBJ_FLAG_HIDDEN);
            break;

        case PLC_STATE_BAUD_FOUND:
            lv_label_set_text_fmt(plcFoundBaudValueLabel, "%lu",
                                   (unsigned long)detectedBaudRate);
            lv_label_set_text_fmt(plcFlashBtnLabel, "FLASH ESP32-S3\nBAUDRATE %lu",
                                   (unsigned long)detectedBaudRate);
            lv_obj_clear_flag(plcBaudFoundCont, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

//=====================================================
// EVENT : CHECK PLC BUTTON (default configuration)
//=====================================================
static void checkPLCBtnCB(lv_event_t *e)
{
    setPLCCheckState(PLC_STATE_CHECKING);
    Lvgl_Loop();
    delay(500); // let the "checking" view render before the blocking read

    uint8_t result = node.readCoils(0, 1);

    if (result == node.ku8MBSuccess)
    {
        Serial.println("PLC Check: default config PASS (Slave 1 / 115200 / N / 1)");
        setPLCCheckState(PLC_STATE_SUCCESS);
    }
    else
    {
        Serial.println("PLC Check: default config FAILED");
        setPLCCheckState(PLC_STATE_FAILED);
    }
}

//=====================================================
// EVENT : SCAN BAUDRATE BUTTON
//=====================================================
static void scanBaudrateCB(lv_event_t *e)
{
    setPLCCheckState(PLC_STATE_SCANNING);
    Lvgl_Loop();

    bool     found     = false;
    uint32_t foundBaud  = 0;

    for (int i = 0; i < PLC_BAUD_CANDIDATES_COUNT; i++)
    {
        uint32_t baud = plcBaudCandidates[i];

        lv_label_set_text_fmt(plcScanRowLabels[i], "%lu   -   Testing...",
                               (unsigned long)baud);
        Lvgl_Loop();

        //-------------------------------------------------
        // Re-open RS485 at the candidate baud rate
        //-------------------------------------------------
        RS485Serial.end();
        delay(20);
        RS485Serial.begin(baud, SERIAL_8N1, RXD2, TXD2);

        node.begin(PLC_SLAVE_ID, RS485Serial);
        node.preTransmission(preTransmission);
        node.postTransmission(postTransmission);
        delay(50);

        uint8_t result = node.readCoils(0, 1);

        if (result == node.ku8MBSuccess)
        {
            lv_label_set_text_fmt(plcScanRowLabels[i], "%lu   -   Found!",
                                   (unsigned long)baud);
            Lvgl_Loop();

            found     = true;
            foundBaud = baud;
            break;
        }
        else
        {
            lv_label_set_text_fmt(plcScanRowLabels[i], "%lu   -   No Response",
                                   (unsigned long)baud);
            Lvgl_Loop();
        }
    }

    if (found)
    {
        detectedBaudRate = foundBaud;

        Serial.print("PLC Check: baudrate detected -> ");
        Serial.println(detectedBaudRate);

        delay(400);
        setPLCCheckState(PLC_STATE_BAUD_FOUND);
    }
    else
    {
        Serial.println("PLC Check: no working baudrate found");

        lv_label_set_text(plcScanResultLabel,
                           "No working baudrate found.\nCheck PLC wiring & power.");
        lv_obj_clear_flag(plcScanResultLabel, LV_OBJ_FLAG_HIDDEN);

        //-------------------------------------------------
        // Restore RS485 back to the original default baud
        //-------------------------------------------------
        RS485Serial.end();
        delay(20);
        RS485Serial.begin(PLC_BAUDRATE, SERIAL_8N1, RXD2, TXD2);
        node.begin(PLC_SLAVE_ID, RS485Serial);
        node.preTransmission(preTransmission);
        node.postTransmission(postTransmission);
    }
}

//=====================================================
// BUILD PLC CHECK SCREEN (SCREEN 0)
//=====================================================
void createPLCCheckScreen()
{
    plcCheckScreen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(plcCheckScreen, COLOR_BG, 0);
    lv_obj_clear_flag(plcCheckScreen, LV_OBJ_FLAG_SCROLLABLE);

    //-------------------------------------------------
    // HEADER : TITLE + SUBTITLE (same style as other screens)
    //-------------------------------------------------
    lv_obj_t *title = lv_label_create(plcCheckScreen);
    lv_label_set_text(title, "PROTOVOLT");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 6);

    lv_obj_t *subtitle = lv_label_create(plcCheckScreen);
    lv_label_set_text(subtitle, "PLC Check");
    lv_obj_set_style_text_font(subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(subtitle, COLOR_SUBTEXT, 0);
    lv_obj_align(subtitle, LV_ALIGN_TOP_LEFT, 10, 30);

    lv_obj_t *line = lv_obj_create(plcCheckScreen);
    lv_obj_set_size(line, 220, 2);
    lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_set_style_bg_color(line, COLOR_BORDER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);

    // Shared content area position/size for every state container
    const int contX = 10;
    const int contY = 64;
    const int contW = 220;
    const int contH = 246;

    //=================================================
    // STATE 1 : HOME  (Check PLC button)
    //=================================================
    plcHomeCont = lv_obj_create(plcCheckScreen);
    lv_obj_set_pos(plcHomeCont, contX, contY);
    lv_obj_set_size(plcHomeCont, contW, contH);
    lv_obj_set_style_bg_opa(plcHomeCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(plcHomeCont, 0, 0);
    lv_obj_set_style_pad_all(plcHomeCont, 0, 0);
    lv_obj_clear_flag(plcHomeCont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *plcIconCircle = lv_obj_create(plcHomeCont);
    lv_obj_set_size(plcIconCircle, 70, 70);
    lv_obj_align(plcIconCircle, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_set_style_radius(plcIconCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(plcIconCircle, COLOR_SLOT1_BG, 0);
    lv_obj_set_style_border_width(plcIconCircle, 2, 0);
    lv_obj_set_style_border_color(plcIconCircle, COLOR_ACCENT, 0);
    lv_obj_clear_flag(plcIconCircle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *plcIconLbl = lv_label_create(plcIconCircle);
    lv_label_set_text(plcIconLbl, "PLC");
    lv_obj_set_style_text_font(plcIconLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(plcIconLbl, COLOR_ACCENT, 0);
    lv_obj_center(plcIconLbl);

    lv_obj_t *plcInfoLbl = lv_label_create(plcHomeCont);
    lv_label_set_text(plcInfoLbl, "This will verify the PLC with\ndefault industrial settings.");
    lv_obj_set_style_text_font(plcInfoLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(plcInfoLbl, COLOR_SUBTEXT, 0);
    lv_obj_set_style_text_align(plcInfoLbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(plcInfoLbl, contW);
    lv_obj_align(plcInfoLbl, LV_ALIGN_TOP_MID, 0, 84);

    lv_obj_t *plcDefaultBox = lv_obj_create(plcHomeCont);
    lv_obj_set_size(plcDefaultBox, contW, 90);
    lv_obj_align(plcDefaultBox, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_set_style_bg_color(plcDefaultBox, COLOR_CARD, 0);
    lv_obj_set_style_radius(plcDefaultBox, 10, 0);
    lv_obj_set_style_border_width(plcDefaultBox, 1, 0);
    lv_obj_set_style_border_color(plcDefaultBox, COLOR_BORDER, 0);
    lv_obj_clear_flag(plcDefaultBox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *plcDefaultTitle = lv_label_create(plcDefaultBox);
    lv_label_set_text(plcDefaultTitle, "DEFAULT PLC CONFIGURATION");
    lv_obj_set_style_text_font(plcDefaultTitle, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(plcDefaultTitle, COLOR_SUBTEXT, 0);
    lv_obj_align(plcDefaultTitle, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t *plcDefaultVals = lv_label_create(plcDefaultBox);
    lv_label_set_text_fmt(plcDefaultVals,
                           "Slave ID   : %d\nBaudrate  : %d\nParity     : N (None)\nStop Bits  : 1",
                           PLC_SLAVE_ID, PLC_BAUDRATE);
    lv_obj_set_style_text_font(plcDefaultVals, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(plcDefaultVals, COLOR_TEXT, 0);
    lv_obj_align(plcDefaultVals, LV_ALIGN_TOP_LEFT, 8, 22);

    lv_obj_t *checkPlcBtn = lv_btn_create(plcHomeCont);
    lv_obj_set_size(checkPlcBtn, 160, 40);
    lv_obj_align(checkPlcBtn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(checkPlcBtn, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(checkPlcBtn, 10, 0);
    lv_obj_add_event_cb(checkPlcBtn, checkPLCBtnCB, LV_EVENT_CLICKED, NULL);

    lv_obj_t *checkPlcLbl = lv_label_create(checkPlcBtn);
    lv_label_set_text(checkPlcLbl, LV_SYMBOL_REFRESH "  CHECK PLC");
    lv_obj_set_style_text_font(checkPlcLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(checkPlcLbl);

    //=================================================
    // STATE 2 : CHECKING
    //=================================================
    plcCheckingCont = lv_obj_create(plcCheckScreen);
    lv_obj_set_pos(plcCheckingCont, contX, contY);
    lv_obj_set_size(plcCheckingCont, contW, contH);
    lv_obj_set_style_bg_opa(plcCheckingCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(plcCheckingCont, 0, 0);
    lv_obj_clear_flag(plcCheckingCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(plcCheckingCont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *checkingTitle = lv_label_create(plcCheckingCont);
    lv_label_set_text(checkingTitle, "Checking PLC with\ndefault configuration...");
    lv_obj_set_style_text_font(checkingTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(checkingTitle, COLOR_TEXT, 0);
    lv_obj_set_style_text_align(checkingTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(checkingTitle, contW);
    lv_obj_align(checkingTitle, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *checkingWait = lv_label_create(plcCheckingCont);
    lv_label_set_text(checkingWait, "Please wait...");
    lv_obj_set_style_text_font(checkingWait, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(checkingWait, COLOR_ACCENT, 0);
    lv_obj_align(checkingWait, LV_ALIGN_TOP_MID, 0, 100);

    //=================================================
    // STATE 3 : SUCCESS (default config worked)
    //=================================================
    plcSuccessCont = lv_obj_create(plcCheckScreen);
    lv_obj_set_pos(plcSuccessCont, contX, contY);
    lv_obj_set_size(plcSuccessCont, contW, contH);
    lv_obj_set_style_bg_opa(plcSuccessCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(plcSuccessCont, 0, 0);
    lv_obj_clear_flag(plcSuccessCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(plcSuccessCont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *successCircle = lv_obj_create(plcSuccessCont);
    lv_obj_set_size(successCircle, 50, 50);
    lv_obj_align(successCircle, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_radius(successCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(successCircle, COLOR_PASS, 0);
    lv_obj_set_style_border_width(successCircle, 0, 0);
    lv_obj_clear_flag(successCircle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *successIcon = lv_label_create(successCircle);
    lv_label_set_text(successIcon, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(successIcon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(successIcon, lv_color_white(), 0);
    lv_obj_center(successIcon);

    lv_obj_t *successTitle = lv_label_create(plcSuccessCont);
    lv_label_set_text(successTitle, "PLC FOUND!");
    lv_obj_set_style_text_font(successTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(successTitle, COLOR_PASS, 0);
    lv_obj_set_width(successTitle, contW);
    lv_obj_set_style_text_align(successTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(successTitle, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *successSub = lv_label_create(plcSuccessCont);
    lv_label_set_text(successSub, "Default configuration is correct.");
    lv_obj_set_style_text_font(successSub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(successSub, COLOR_SUBTEXT, 0);
    lv_obj_set_width(successSub, contW);
    lv_obj_set_style_text_align(successSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(successSub, LV_ALIGN_TOP_MID, 0, 82);

    lv_obj_t *successBox = lv_obj_create(plcSuccessCont);
    lv_obj_set_size(successBox, contW, 82);
    lv_obj_align(successBox, LV_ALIGN_TOP_MID, 0, 106);
    lv_obj_set_style_bg_color(successBox, COLOR_CARD, 0);
    lv_obj_set_style_radius(successBox, 10, 0);
    lv_obj_set_style_border_width(successBox, 1, 0);
    lv_obj_set_style_border_color(successBox, COLOR_BORDER, 0);
    lv_obj_clear_flag(successBox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *successVals = lv_label_create(successBox);
    lv_label_set_text_fmt(successVals,
                           "Slave ID   : %d\nBaudrate  : %d\nParity     : N (None)\nStop Bits  : 1",
                           PLC_SLAVE_ID, PLC_BAUDRATE);
    lv_obj_set_style_text_font(successVals, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(successVals, COLOR_TEXT, 0);
    lv_obj_align(successVals, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t *goTestBtn = lv_btn_create(plcSuccessCont);
    lv_obj_set_size(goTestBtn, 180, 40);
    lv_obj_align(goTestBtn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(goTestBtn, COLOR_PASS, 0);
    lv_obj_set_style_radius(goTestBtn, 10, 0);
    lv_obj_add_event_cb(goTestBtn, backToTestScreenCB, LV_EVENT_CLICKED, NULL);

    lv_obj_t *goTestLbl = lv_label_create(goTestBtn);
    lv_label_set_text(goTestLbl, LV_SYMBOL_HOME "  GO TO TEST PAGE");
    lv_obj_set_style_text_font(goTestLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(goTestLbl);

    //=================================================
    // STATE 4 : FAILED (default config did not work)
    //=================================================
    plcFailedCont = lv_obj_create(plcCheckScreen);
    lv_obj_set_pos(plcFailedCont, contX, contY);
    lv_obj_set_size(plcFailedCont, contW, contH);
    lv_obj_set_style_bg_opa(plcFailedCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(plcFailedCont, 0, 0);
    lv_obj_clear_flag(plcFailedCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(plcFailedCont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *failCircle = lv_obj_create(plcFailedCont);
    lv_obj_set_size(failCircle, 50, 50);
    lv_obj_align(failCircle, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_radius(failCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(failCircle, COLOR_FAIL, 0);
    lv_obj_set_style_border_width(failCircle, 0, 0);
    lv_obj_clear_flag(failCircle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *failIcon = lv_label_create(failCircle);
    lv_label_set_text(failIcon, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(failIcon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(failIcon, lv_color_white(), 0);
    lv_obj_center(failIcon);

    lv_obj_t *failTitle = lv_label_create(plcFailedCont);
    lv_label_set_text(failTitle, "PLC NOT FOUND!");
    lv_obj_set_style_text_font(failTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(failTitle, COLOR_FAIL, 0);
    lv_obj_set_width(failTitle, contW);
    lv_obj_set_style_text_align(failTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(failTitle, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *failSub = lv_label_create(plcFailedCont);
    lv_label_set_text(failSub, "Default configuration failed.");
    lv_obj_set_style_text_font(failSub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(failSub, COLOR_SUBTEXT, 0);
    lv_obj_set_width(failSub, contW);
    lv_obj_set_style_text_align(failSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(failSub, LV_ALIGN_TOP_MID, 0, 82);

    lv_obj_t *failBox = lv_obj_create(plcFailedCont);
    lv_obj_set_size(failBox, contW, 82);
    lv_obj_align(failBox, LV_ALIGN_TOP_MID, 0, 106);
    lv_obj_set_style_bg_color(failBox, COLOR_CARD, 0);
    lv_obj_set_style_radius(failBox, 10, 0);
    lv_obj_set_style_border_width(failBox, 1, 0);
    lv_obj_set_style_border_color(failBox, COLOR_FAIL, 0);
    lv_obj_clear_flag(failBox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *failVals = lv_label_create(failBox);
    lv_label_set_text_fmt(failVals,
                           "Slave ID   : %d\nBaudrate  : %d\nParity     : N (None)\nStop Bits  : 1",
                           PLC_SLAVE_ID, PLC_BAUDRATE);
    lv_obj_set_style_text_font(failVals, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(failVals, COLOR_TEXT, 0);
    lv_obj_align(failVals, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t *scanBtn = lv_btn_create(plcFailedCont);
    lv_obj_set_size(scanBtn, 180, 40);
    lv_obj_align(scanBtn, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_bg_color(scanBtn, COLOR_ACCENT, 0);
    lv_obj_set_style_radius(scanBtn, 10, 0);
    lv_obj_add_event_cb(scanBtn, scanBaudrateCB, LV_EVENT_CLICKED, NULL);

    lv_obj_t *scanBtnLbl = lv_label_create(scanBtn);
    lv_label_set_text(scanBtnLbl, LV_SYMBOL_LOOP "  SCAN BAUDRATE");
    lv_obj_set_style_text_font(scanBtnLbl, &lv_font_montserrat_14, 0);
    lv_obj_center(scanBtnLbl);

    //=================================================
    // STATE 5 : SCANNING BAUDRATE
    //=================================================
    plcScanningCont = lv_obj_create(plcCheckScreen);
    lv_obj_set_pos(plcScanningCont, contX, contY);
    lv_obj_set_size(plcScanningCont, contW, contH);
    lv_obj_set_style_bg_opa(plcScanningCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(plcScanningCont, 0, 0);
    lv_obj_clear_flag(plcScanningCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(plcScanningCont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *scanTitle = lv_label_create(plcScanningCont);
    lv_label_set_text(scanTitle, "Scanning Baudrate...");
    lv_obj_set_style_text_font(scanTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(scanTitle, COLOR_TEXT, 0);
    lv_obj_set_width(scanTitle, contW);
    lv_obj_set_style_text_align(scanTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(scanTitle, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *scanSub = lv_label_create(plcScanningCont);
    lv_label_set_text(scanSub, "This may take a few seconds.");
    lv_obj_set_style_text_font(scanSub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(scanSub, COLOR_SUBTEXT, 0);
    lv_obj_set_width(scanSub, contW);
    lv_obj_set_style_text_align(scanSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(scanSub, LV_ALIGN_TOP_MID, 0, 22);

    lv_obj_t *scanListBox = lv_obj_create(plcScanningCont);
    lv_obj_set_size(scanListBox, contW, 150);
    lv_obj_align(scanListBox, LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_style_bg_color(scanListBox, COLOR_CARD, 0);
    lv_obj_set_style_radius(scanListBox, 10, 0);
    lv_obj_set_style_border_width(scanListBox, 1, 0);
    lv_obj_set_style_border_color(scanListBox, COLOR_BORDER, 0);
    lv_obj_clear_flag(scanListBox, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < PLC_BAUD_CANDIDATES_COUNT; i++)
    {
        lv_obj_t *rowLbl = lv_label_create(scanListBox);
        lv_label_set_text_fmt(rowLbl, "%lu   -   Waiting...",
                               (unsigned long)plcBaudCandidates[i]);
        lv_obj_set_style_text_font(rowLbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(rowLbl, COLOR_TEXT, 0);
        lv_obj_set_pos(rowLbl, 8, 6 + i * 22);
        plcScanRowLabels[i] = rowLbl;
    }

    plcScanResultLabel = lv_label_create(plcScanningCont);
    lv_label_set_text(plcScanResultLabel, "");
    lv_obj_set_style_text_font(plcScanResultLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(plcScanResultLabel, COLOR_FAIL, 0);
    lv_obj_set_width(plcScanResultLabel, contW);
    lv_obj_set_style_text_align(plcScanResultLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(plcScanResultLabel, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_add_flag(plcScanResultLabel, LV_OBJ_FLAG_HIDDEN);

    //=================================================
    // STATE 6 : BAUDRATE FOUND
    //=================================================
    plcBaudFoundCont = lv_obj_create(plcCheckScreen);
    lv_obj_set_pos(plcBaudFoundCont, contX, contY);
    lv_obj_set_size(plcBaudFoundCont, contW, contH);
    lv_obj_set_style_bg_opa(plcBaudFoundCont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(plcBaudFoundCont, 0, 0);
    lv_obj_clear_flag(plcBaudFoundCont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(plcBaudFoundCont, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *bfCircle = lv_obj_create(plcBaudFoundCont);
    lv_obj_set_size(bfCircle, 50, 50);
    lv_obj_align(bfCircle, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_radius(bfCircle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bfCircle, COLOR_PASS, 0);
    lv_obj_set_style_border_width(bfCircle, 0, 0);
    lv_obj_clear_flag(bfCircle, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bfIcon = lv_label_create(bfCircle);
    lv_label_set_text(bfIcon, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(bfIcon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(bfIcon, lv_color_white(), 0);
    lv_obj_center(bfIcon);

    lv_obj_t *bfTitle = lv_label_create(plcBaudFoundCont);
    lv_label_set_text(bfTitle, "PLC FOUND!");
    lv_obj_set_style_text_font(bfTitle, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(bfTitle, COLOR_PASS, 0);
    lv_obj_set_width(bfTitle, contW);
    lv_obj_set_style_text_align(bfTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(bfTitle, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t *bfSub = lv_label_create(plcBaudFoundCont);
    lv_label_set_text(bfSub, "Baudrate detected successfully.");
    lv_obj_set_style_text_font(bfSub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bfSub, COLOR_SUBTEXT, 0);
    lv_obj_set_width(bfSub, contW);
    lv_obj_set_style_text_align(bfSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(bfSub, LV_ALIGN_TOP_MID, 0, 78);

    lv_obj_t *bfBox = lv_obj_create(plcBaudFoundCont);
    lv_obj_set_size(bfBox, contW, 68);
    lv_obj_align(bfBox, LV_ALIGN_TOP_MID, 0, 102);
    lv_obj_set_style_bg_color(bfBox, COLOR_CARD, 0);
    lv_obj_set_style_radius(bfBox, 10, 0);
    lv_obj_set_style_border_width(bfBox, 1, 0);
    lv_obj_set_style_border_color(bfBox, COLOR_PASS, 0);
    lv_obj_clear_flag(bfBox, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bfSlaveLbl = lv_label_create(bfBox);
    lv_label_set_text_fmt(bfSlaveLbl, "Slave ID  : %d      Parity : N", PLC_SLAVE_ID);
    lv_obj_set_style_text_font(bfSlaveLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bfSlaveLbl, COLOR_TEXT, 0);
    lv_obj_align(bfSlaveLbl, LV_ALIGN_TOP_LEFT, 8, 6);

    lv_obj_t *bfBaudLbl = lv_label_create(bfBox);
    lv_label_set_text(bfBaudLbl, "Baudrate :");
    lv_obj_set_style_text_font(bfBaudLbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(bfBaudLbl, COLOR_TEXT, 0);
    lv_obj_align(bfBaudLbl, LV_ALIGN_TOP_LEFT, 8, 26);

    plcFoundBaudValueLabel = lv_label_create(bfBox);
    lv_label_set_text(plcFoundBaudValueLabel, "");
    lv_obj_set_style_text_font(plcFoundBaudValueLabel, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(plcFoundBaudValueLabel, COLOR_ACCENT, 0);
    lv_obj_align(plcFoundBaudValueLabel, LV_ALIGN_TOP_LEFT, 68, 24);

    lv_obj_t *bfNote = lv_label_create(bfBox);
    lv_label_set_text(bfNote, "Flash your ESP32-S3 with this\nbaudrate to communicate with the PLC.");
    lv_obj_set_style_text_font(bfNote, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(bfNote, COLOR_SUBTEXT, 0);
    lv_obj_align(bfNote, LV_ALIGN_TOP_LEFT, 8, 44);

    lv_obj_t *flashBtn = lv_btn_create(plcBaudFoundCont);
    lv_obj_set_size(flashBtn, 200, 46);
    lv_obj_align(flashBtn, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(flashBtn, COLOR_PASS, 0);
    lv_obj_set_style_radius(flashBtn, 10, 0);
    lv_obj_add_event_cb(flashBtn, backToTestScreenCB, LV_EVENT_CLICKED, NULL);

    plcFlashBtnLabel = lv_label_create(flashBtn);
    lv_label_set_text(plcFlashBtnLabel, "FLASH ESP32-S3\nBAUDRATE");
    lv_obj_set_style_text_font(plcFlashBtnLabel, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_align(plcFlashBtnLabel, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(plcFlashBtnLabel);

    //-------------------------------------------------
    // START ON THE HOME STATE
    //-------------------------------------------------
    setPLCCheckState(PLC_STATE_HOME);
}

/*==================================================================
   ================  END OF NEW PLC CHECK SECTION  ================
==================================================================*/

void showLogo()
{
    logoScreen = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(
        logoScreen,
        lv_color_white(),
        LV_PART_MAIN);

    lv_obj_t *img = lv_img_create(logoScreen);

    lv_img_set_src(img, &protovolt_logo);

    lv_obj_center(img);

    lv_scr_load(logoScreen);
}



//=====================================================
// SETUP
//=====================================================
void setup()
{
    Serial.begin(115200);

    //-------------------------------------------------
    // DISPLAY INITIALIZATION
    //-------------------------------------------------
    Backlight_Init();
    LCD_Init();
    Lvgl_Init();
    showLogo();
uint32_t startTime = millis();
while (millis() - startTime < 2000)
{
    Lvgl_Loop();
}
    //-------------------------------------------------
    // POWER SENSOR (INA260) INITIALIZATION
    //-------------------------------------------------
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    Wire.setTimeout(100);

    if (ina260.begin())
    {
        ina260Found = true;
        Serial.println("INA260 Found!");
    }
    else
    {
        ina260Found = false;
        Serial.println("INA260 not found! Power test will report FAIL.");
    }

    //-------------------------------------------------
    // RS485 / MODBUS INITIALIZATION
    //-------------------------------------------------
    pinMode(EN485, OUTPUT);
    digitalWrite(EN485, LOW);

    RS485Serial.begin(PLC_BAUDRATE, SERIAL_8N1, RXD2, TXD2);

    node.begin(PLC_SLAVE_ID, RS485Serial);
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);

    //-------------------------------------------------
    // BUILD UI SCREENS
    //-------------------------------------------------
    createPLCCheckScreen();   // NEW: build the PLC check screen
    createTestScreen();
    createRelayScreen();

    lv_scr_load(plcCheckScreen); // NEW: show PLC check first, before the test page

    Serial.println("=================================");
    Serial.println("PROTOVOLT PANEL TESTING KIT READY");
    Serial.println("=================================");
}

//=====================================================
// LOOP
//=====================================================
void loop()
{
    static uint32_t lastRelayRead = 0;

    if(lv_scr_act() == relayScreen)
    {
        if(millis() - lastRelayRead >= 500)
        {
            lastRelayRead = millis();

            updateRelayStatus();
        }
    }

    Lvgl_Loop();

    delay(5);
}
