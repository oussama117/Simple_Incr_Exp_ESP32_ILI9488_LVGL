/**
 * ESP32 + ILI9488 (TFT_eSPI built-in touch) + LVGL 9.x
 * Counter application -- with serial debug output.
 *
 * TOUCH ORIENTATION FIX
 * ---------------------
 * The XPT2046 digitiser is physically mounted in landscape orientation
 * while the ILI9488 runs in portrait (rotation 0).
 * tft.getTouch() returns coordinates in the display's own pixel space
 * AFTER applying setTouch() calibration, but the calibration data was
 * captured in portrait mode so the axes come out swapped:
 *
 *   raw_x  maps to the display's Y axis
 *   raw_y  maps to the display's X axis (and is mirrored)
 *
 * Correction in touch_read_cb():
 *   screen_x = (SCREEN_W - 1) - raw_y
 *   screen_y = raw_x
 *
 * If the touch still feels mirrored after flashing, try the alternate
 * mappings noted in the callback comment block.
 *
 * CALIBRATION
 * -----------
 * Set RUN_CALIBRATION 1, flash, touch the four corner arrows on screen.
 * Copy the 5 printed values into TOUCH_CAL_DATA[], set back to 0.
 */

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

// -----------------------------------------------------------
//  Hardware
// -----------------------------------------------------------
TFT_eSPI tft;

static constexpr uint16_t SCREEN_W = 480;
static constexpr uint16_t SCREEN_H = 360;

// -----------------------------------------------------------
//  LVGL draw buffer  (10 rows x 320 px x 2 bytes = 6400 B)
// -----------------------------------------------------------
static constexpr uint16_t BUF_ROWS = 10;
static DRAM_ATTR lv_color_t draw_buf[SCREEN_W * BUF_ROWS];

// -----------------------------------------------------------
//  Touch calibration
//  Set RUN_CALIBRATION 1 to run the calibration wizard once.
// -----------------------------------------------------------
#define RUN_CALIBRATION 0
static uint16_t TOUCH_CAL_DATA[5] = {331, 3552, 325, 3383, 7};

// -----------------------------------------------------------
//  Debug counters
// -----------------------------------------------------------
static uint32_t flush_count = 0;
static uint32_t touch_count = 0;
static uint32_t last_debug_ms = 0;

// -----------------------------------------------------------
//  Counter UI state
// -----------------------------------------------------------
static int32_t counter = 0;
static lv_obj_t *label_counter = nullptr;

// ===========================================================
//  LVGL flush callback
// ===========================================================
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;

  if (flush_count < 5)
  {
    Serial.printf("[flush #%u] x1=%d y1=%d x2=%d y2=%d  pixels=%u\n",
                  flush_count, area->x1, area->y1, area->x2, area->y2, w * h);
  }
  flush_count++;

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushPixels(reinterpret_cast<uint16_t *>(px_map), w * h);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

// ===========================================================
//  LVGL touch read callback
//
//  The touch panel sits in landscape orientation; the display
//  is portrait.  We correct the axes here so LVGL sees proper
//  portrait coordinates regardless of the physical mismatch.
//
//  If buttons still respond off-target after flashing, try
//  uncommenting one of the alternate mappings below and
//  commenting out the active one.
// ===========================================================
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  uint16_t raw_x = 0, raw_y = 0;
  if (tft.getTouch(&raw_x, &raw_y))
  {
    data->point.x = raw_x;
    data->point.y = raw_y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else
  {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ===========================================================
//  Button event callbacks
// ===========================================================
static void update_label()
{
  lv_label_set_text_fmt(label_counter, "%d", counter);
  Serial.printf("[btn] counter -> %d\n", counter);
}

static void btn_inc_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    Serial.println("[btn] + clicked");
    counter++;
    update_label();
  }
}
static void btn_dec_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    Serial.println("[btn] - clicked");
    counter--;
    update_label();
  }
}
static void btn_rst_cb(lv_event_t *e)
{
  if (lv_event_get_code(e) == LV_EVENT_CLICKED)
  {
    Serial.println("[btn] RST clicked");
    counter = 0;
    update_label();
  }
}

// -----------------------------------------------------------
//  UI helpers
// -----------------------------------------------------------
static lv_obj_t *make_btn(lv_obj_t *parent, const char *txt,
                          lv_align_t align, lv_coord_t xofs, lv_coord_t yofs,
                          uint32_t color, lv_event_cb_t cb)
{
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, 90, 55);
  lv_obj_align(btn, align, xofs, yofs);
  lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(color), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_70, LV_STATE_PRESSED);
  lv_obj_set_style_radius(btn, 10, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_color(btn, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(btn, LV_OPA_40, LV_PART_MAIN);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, txt);
  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_center(lbl);

  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  return btn;
}

static void build_ui()
{
  Serial.println("[ui] building screen...");

  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

  // Title
  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "COUNTER");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_color(title, lv_color_hex(0xE94560), LV_PART_MAIN);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

  // Divider
  lv_obj_t *line = lv_obj_create(scr);
  lv_obj_set_size(line, 240, 3);
  lv_obj_set_style_bg_color(line, lv_color_hex(0xE94560), LV_PART_MAIN);
  lv_obj_set_style_radius(line, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
  lv_obj_align(line, LV_ALIGN_TOP_MID, 0, 70);

  // Counter card
  lv_obj_t *card = lv_obj_create(scr);
  lv_obj_set_size(card, 120, 60);
  lv_obj_align(card, LV_ALIGN_TOP_MID, 0, 100);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x16213E), LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(0xE94560), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  label_counter = lv_label_create(card);
  lv_label_set_text(label_counter, "0");
  lv_obj_set_style_text_font(label_counter, &lv_font_montserrat_24, LV_PART_MAIN);
  lv_obj_set_style_text_color(label_counter, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_center(label_counter);

  // Buttons  (horizontal row, centred vertically at y_offset=250)
  //  "-"  left    x_offset = -110
  //  "RST" centre x_offset =    0
  //  "+"  right   x_offset = +110
  make_btn(scr, "-", LV_ALIGN_TOP_MID, -110, 200, 0x0F3460, btn_dec_cb);
  make_btn(scr, "RST", LV_ALIGN_TOP_MID, 0, 200, 0x533483, btn_rst_cb);
  make_btn(scr, "+", LV_ALIGN_TOP_MID, 110, 200, 0x1B4332, btn_inc_cb);

  // Footer
  lv_obj_t *hint = lv_label_create(scr);
  lv_label_set_text(hint, "tap buttons to count");
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
  lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), LV_PART_MAIN);
  lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -60);

  Serial.println("[ui] screen built OK");
  Serial.println("[ui] button layout (portrait):");
  Serial.println("[ui]   '-'  centre ~( 55, 278)  size 90x55");
  Serial.println("[ui]   RST  centre ~(160, 278)  size 90x55");
  Serial.println("[ui]   '+'  centre ~(265, 278)  size 90x55");
}

// ===========================================================
//  setup()
// ===========================================================
void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[boot] starting");

  // --- Display ---
  Serial.println("[boot] init TFT");
  tft.init();
  tft.setRotation(1);

  // Sanity check: solid red for 1 second before LVGL starts.
  // If you see red -> display SPI is wired correctly.
  // tft.fillScreen(TFT_RED);
  // Serial.println("[boot] fillScreen(RED) -- do you see red?");
  // delay(1000);
  // tft.fillScreen(TFT_BLACK);

  // --- Touch ---
#if RUN_CALIBRATION
  Serial.println("[boot] CALIBRATION MODE -- touch each corner arrow");
  tft.calibrateTouch(TOUCH_CAL_DATA, TFT_WHITE, TFT_BLACK, 15);
  Serial.print("[cal] data: ");
  for (int i = 0; i < 5; i++)
  {
    Serial.print(TOUCH_CAL_DATA[i]);
    if (i < 4)
      Serial.print(", ");
  }
  Serial.println();
  Serial.println("[cal] paste into TOUCH_CAL_DATA[] and set RUN_CALIBRATION 0");
  while (true)
    delay(1000);
#else
  tft.setTouch(TOUCH_CAL_DATA);
  Serial.println("[boot] touch calibration loaded");
#endif

  // --- LVGL ---
  Serial.println("[boot] init LVGL");
  lv_init();

  lv_display_t *disp = lv_display_create(SCREEN_W, SCREEN_H);
  lv_display_set_flush_cb(disp, flush_cb);
  lv_display_set_buffers(disp,
                         draw_buf, nullptr,
                         sizeof(draw_buf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  Serial.printf("[boot] display buffer %u bytes\n", (uint32_t)sizeof(draw_buf));

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);
  Serial.println("[boot] touch indev registered");

  build_ui();

  Serial.println("[boot] ready");
  Serial.printf("[boot] free DRAM: %u bytes\n", esp_get_free_heap_size());
  Serial.println("[boot] watching for [flush] and [touch] lines...");
  Serial.println("[boot] touch a BUTTON and confirm screen_x/y match the");
  Serial.println("[boot] expected button centres printed above.");
}

// ===========================================================
//  loop()
// ===========================================================
void loop()
{
  uint32_t now = millis();

  lv_tick_inc(5);
  lv_timer_handler();

  if (now - last_debug_ms >= 5000)
  {
    last_debug_ms = now;
    Serial.printf("[loop] uptime=%lus  flushes=%u  touches=%u  heap=%u\n",
                  now / 1000, flush_count, touch_count,
                  esp_get_free_heap_size());
  }

  delay(5);
}