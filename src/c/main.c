#include <pebble.h>

// ── Layout (emery 200x228) ────────────────────────────────────────────────────
//
//  Y=  0 ┌──────────────────────────────────────┐  outer border
//        │                                      │
//  Y=  8 │  [5/27  WED]    [████░░ 88%]        │  s_top_layer    (184x52)
//  Y= 60 │  [         7:17             ]        │  s_time_layer   (184x84)
//  Y=144 │  [● Garmin Connected        ]        │  s_status_layer (184x16)
//  Y=160 │  [BB  87│STR  42│HR   68   ]        │  s_data_layer   (184x60)
//        │  [STP  8K│SLP  78│HRV  Bal ]        │
//  Y=228 └──────────────────────────────────────┘

#define SETTINGS_KEY    1
#define DATA_KEY        2
#define FACE_PADDING    8
#define MAX_SLOTS       6
#define SLOT_STR_LEN    8
#define DEFAULT_INTERVAL 30

typedef struct {
  char    slot[MAX_SLOTS][SLOT_STR_LEN];
  uint8_t interval;
} WatchSettings;

typedef struct {
  int8_t  bb;
  int8_t  stress;
  char    hrv[8];
  int16_t recovery;
  int8_t  vo2max;
  int16_t ftp;
  int16_t intmin;
  int16_t load;
  int8_t  hr;
  int16_t steps;
  int8_t  sleep_score;
  char    coach[8];
  int8_t  spo2;
  int8_t  status;
  int8_t  respiration;
  int8_t  heat_acclim;
  int8_t  alt_acclim;
  char    train_status[8];
  int8_t  train_ready;
} GarminData;

static Window       *s_window;
static Layer        *s_top_layer;
static Layer        *s_time_layer;
static Layer        *s_status_layer;
static Layer        *s_data_layer;
static Layer        *s_border_layer;

static GFont         s_font_time;
static GFont         s_font_label;

static int           s_battery_level = 100;
static bool          s_is_24h        = true;
static WatchSettings s_settings;
static GarminData    s_garmin;

static char s_time_buf[8];
static char s_ampm_buf[3];
static char s_date_buf[8];
static char s_day_buf[4];
static char s_bat_buf[5];

// ── Helpers ───────────────────────────────────────────────────────────────────

static void settings_init_defaults(void) {
  strncpy(s_settings.slot[0], "BB",  SLOT_STR_LEN);
  strncpy(s_settings.slot[1], "STR", SLOT_STR_LEN);
  strncpy(s_settings.slot[2], "HR",  SLOT_STR_LEN);
  strncpy(s_settings.slot[3], "STP", SLOT_STR_LEN);
  strncpy(s_settings.slot[4], "SLP", SLOT_STR_LEN);
  strncpy(s_settings.slot[5], "HRV", SLOT_STR_LEN);
  s_settings.interval = DEFAULT_INTERVAL;
}

static void garmin_init_defaults(void) {
  s_garmin.bb          = -1;
  s_garmin.stress      = -1;
  strncpy(s_garmin.hrv, "", sizeof(s_garmin.hrv));
  s_garmin.recovery    = -1;
  s_garmin.vo2max      = -1;
  s_garmin.ftp         = -1;
  s_garmin.intmin      = -1;
  s_garmin.load        = -1;
  s_garmin.hr          = -1;
  s_garmin.steps       = -1;
  s_garmin.sleep_score = -1;
  strncpy(s_garmin.coach, "", sizeof(s_garmin.coach));
  s_garmin.spo2        = -1;
  s_garmin.status      = 0;
  s_garmin.respiration = -1;
  s_garmin.heat_acclim = -1;
  s_garmin.alt_acclim  = -1;
  strncpy(s_garmin.train_status, "", sizeof(s_garmin.train_status));
  s_garmin.train_ready = -1;
}

static void slot_get_display(const char *key,
                              char *label, size_t label_sz,
                              char *value, size_t value_sz) {
  if (strcmp(key, "BB") == 0) {
    snprintf(label, label_sz, "Body");
    if (s_garmin.bb >= 0) snprintf(value, value_sz, "%d", (int)s_garmin.bb);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "STR") == 0) {
    snprintf(label, label_sz, "Stress");
    if (s_garmin.stress >= 0) snprintf(value, value_sz, "%d", (int)s_garmin.stress);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "HRV") == 0) {
    snprintf(label, label_sz, "HRV");
    snprintf(value, value_sz, "%s", s_garmin.hrv[0] ? s_garmin.hrv : "--");
  } else if (strcmp(key, "REC") == 0) {
    snprintf(label, label_sz, "Recov");
    if (s_garmin.recovery >= 0) snprintf(value, value_sz, "%dh", (int)s_garmin.recovery);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "VO2") == 0) {
    snprintf(label, label_sz, "VO2Max");
    if (s_garmin.vo2max > 0) snprintf(value, value_sz, "%d", (int)s_garmin.vo2max);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "FTP") == 0) {
    snprintf(label, label_sz, "FTP");
    if (s_garmin.ftp > 0) snprintf(value, value_sz, "%dW", (int)s_garmin.ftp);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "MIN") == 0) {
    snprintf(label, label_sz, "IntMin");
    if (s_garmin.intmin >= 0) snprintf(value, value_sz, "%d", (int)s_garmin.intmin);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "TLD") == 0) {
    snprintf(label, label_sz, "Load");
    if (s_garmin.load > 0) snprintf(value, value_sz, "%d", (int)s_garmin.load);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "HR") == 0) {
    snprintf(label, label_sz, "HR");
    if (s_garmin.hr > 0) snprintf(value, value_sz, "%d", (int)s_garmin.hr);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "STP") == 0) {
    snprintf(label, label_sz, "Steps");
    if (s_garmin.steps >= 0) snprintf(value, value_sz, "%d", (int)s_garmin.steps);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "SLP") == 0) {
    snprintf(label, label_sz, "Score");
    if (s_garmin.sleep_score >= 0) snprintf(value, value_sz, "%d", (int)s_garmin.sleep_score);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "SCH") == 0) {
    snprintf(label, label_sz, "Coach");
    snprintf(value, value_sz, "%s", s_garmin.coach[0] ? s_garmin.coach : "--");
  } else if (strcmp(key, "O2") == 0) {
    snprintf(label, label_sz, "PulOx");
    if (s_garmin.spo2 > 0) snprintf(value, value_sz, "%d%%", (int)s_garmin.spo2);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "RSP") == 0) {
    snprintf(label, label_sz, "Resp");
    if (s_garmin.respiration > 0) snprintf(value, value_sz, "%d", (int)s_garmin.respiration);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "HEA") == 0) {
    snprintf(label, label_sz, "HeatAc");
    if (s_garmin.heat_acclim >= 0) snprintf(value, value_sz, "%d%%", (int)s_garmin.heat_acclim);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "ALT") == 0) {
    snprintf(label, label_sz, "AltAc");
    if (s_garmin.alt_acclim >= 0) snprintf(value, value_sz, "%d%%", (int)s_garmin.alt_acclim);
    else snprintf(value, value_sz, "--");
  } else if (strcmp(key, "TST") == 0) {
    snprintf(label, label_sz, "TrStat");
    snprintf(value, value_sz, "%s", s_garmin.train_status[0] ? s_garmin.train_status : "--");
  } else if (strcmp(key, "TRD") == 0) {
    snprintf(label, label_sz, "TrRedy");
    if (s_garmin.train_ready >= 0) snprintf(value, value_sz, "%d", (int)s_garmin.train_ready);
    else snprintf(value, value_sz, "--");
  } else {
    snprintf(label, label_sz, "---");
    snprintf(value, value_sz, "--");
  }
}

static uint32_t slot_get_resource_id(const char *key) {
  if (strcmp(key, "BB")  == 0) return RESOURCE_ID_METRIC_BB;
  if (strcmp(key, "STR") == 0) return RESOURCE_ID_METRIC_STR;
  if (strcmp(key, "HRV") == 0) return RESOURCE_ID_METRIC_HRV;
  if (strcmp(key, "REC") == 0) return RESOURCE_ID_METRIC_REC;
  if (strcmp(key, "VO2") == 0) return RESOURCE_ID_METRIC_VO2;
  if (strcmp(key, "FTP") == 0) return RESOURCE_ID_METRIC_FTP;
  if (strcmp(key, "MIN") == 0) return RESOURCE_ID_METRIC_MIN;
  if (strcmp(key, "TLD") == 0) return RESOURCE_ID_METRIC_TLD;
  if (strcmp(key, "HR")  == 0) return RESOURCE_ID_METRIC_HR;
  if (strcmp(key, "STP") == 0) return RESOURCE_ID_METRIC_STP;
  if (strcmp(key, "SLP") == 0) return RESOURCE_ID_METRIC_SLP;
  if (strcmp(key, "SCH") == 0) return RESOURCE_ID_METRIC_SCH;
  if (strcmp(key, "O2")  == 0) return RESOURCE_ID_METRIC_O2;
  if (strcmp(key, "RSP") == 0) return RESOURCE_ID_METRIC_RSP;
  if (strcmp(key, "HEA") == 0) return RESOURCE_ID_METRIC_HEA;
  if (strcmp(key, "ALT") == 0) return RESOURCE_ID_METRIC_ALT;
  if (strcmp(key, "TST") == 0) return RESOURCE_ID_METRIC_TST;
  if (strcmp(key, "TRD") == 0) return RESOURCE_ID_METRIC_TRD;
  return 0;
}

// ── Draw procs ────────────────────────────────────────────────────────────────

static void top_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int half_h = b.size.h / 2;

  graphics_context_set_text_color(ctx, GColorWhite);

  // Left: date and day of week
  GFont f20 = s_font_label;
  graphics_draw_text(ctx, s_date_buf, f20,
      GRect(4, 2, 88, half_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  graphics_draw_text(ctx, s_day_buf, f20,
      GRect(4, half_h, 88, half_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

  // Right: Pebble battery bar + percentage
  int bar_x = 94, bar_y = 8, bar_w = 72, bar_h = 20;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  graphics_draw_round_rect(ctx, GRect(bar_x, bar_y, bar_w, bar_h), 2);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(bar_x + bar_w, bar_y + bar_h / 2 - 3, 4, 6), 1, GCornersRight);
  GColor fill = (s_battery_level <= 20) ? GColorRed :
                (s_battery_level <= 40) ? GColorChromeYellow : GColorGreen;
  int fw = (s_battery_level * (bar_w - 8)) / 100;
  if (fw < 0) fw = 0;
  graphics_context_set_fill_color(ctx, fill);
  graphics_fill_rect(ctx, GRect(bar_x + 4, bar_y + 4, fw, bar_h - 8), 0, GCornerNone);

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_bat_buf, f20,
      GRect(94, half_h + 2, 80, half_h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
}

static void time_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GFont f = s_font_time ? s_font_time : fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, s_time_buf, f,
      GRect(-8, 0, b.size.w + 16, b.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
  if (!s_is_24h && s_ampm_buf[0]) {
    graphics_draw_text(ctx, s_ampm_buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(4, b.size.h - 22, b.size.w, 20),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  }
}

static void status_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  GColor dot_color = (s_garmin.status == 1) ? GColorGreen :
                     (s_garmin.status == 2) ? GColorChromeYellow : GColorRed;
  graphics_context_set_fill_color(ctx, dot_color);
  graphics_fill_circle(ctx, GPoint(7, b.size.h / 2), 4);

  const char *status_text = (s_garmin.status == 1) ? "Garmin Connected" :
                            (s_garmin.status == 2) ? "Updating..." : "Garmin Offline";
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, status_text, fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(16, 0, b.size.w - 16, b.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void data_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int slot_w = b.size.w / 3;
  int slot_h = b.size.h / 2;
  GFont f_value = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);

  // Dividers
  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(slot_w,     0), GPoint(slot_w,     b.size.h));
  graphics_draw_line(ctx, GPoint(slot_w * 2, 0), GPoint(slot_w * 2, b.size.h));
  graphics_draw_line(ctx, GPoint(0, slot_h),     GPoint(b.size.w,   slot_h));

  char lbl[8], val[10];
  for (int i = 0; i < MAX_SLOTS; i++) {
    int col = i % 3;
    int row = i / 3;
    int x = col * slot_w;
    int y = row * slot_h;

    const char *key = s_settings.slot[i];
    if (strcmp(key, "NONE") == 0 || key[0] == '\0') continue;

    slot_get_display(key, lbl, sizeof(lbl), val, sizeof(val));

    uint32_t res_id = slot_get_resource_id(key);
    if (res_id) {
      GBitmap *icon = gbitmap_create_with_resource(res_id);
      if (icon) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        int icon_y = y + (slot_h - 18) / 2;
        graphics_draw_bitmap_in_rect(ctx, icon, GRect(x + 2, icon_y, 18, 18));
        gbitmap_destroy(icon);
      }
    }

    int icon_y = y + (slot_h - 18) / 2;
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(ctx, val, f_value,
        GRect(x + 22, icon_y - 3, slot_w - 24, 18),
        GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  }
}

static void border_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 4);
  graphics_draw_round_rect(ctx, GRect(1, 1, b.size.w - 2, b.size.h - 2), 16);
}

// ── Event handlers ────────────────────────────────────────────────────────────

static void battery_callback(BatteryChargeState state) {
  s_battery_level = state.charge_percent;
  snprintf(s_bat_buf, sizeof(s_bat_buf), "%d%%", state.charge_percent);
  if (s_top_layer) layer_mark_dirty(s_top_layer);
}

static void request_garmin_data(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_GARMIN, 1);
    app_message_outbox_send();
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *t;
  bool garmin_updated  = false;
  bool settings_updated = false;

  // Garmin status
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_STATUS);
  if (t) { s_garmin.status = (int8_t)t->value->int32; garmin_updated = true; }

  // Garmin data
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_BB);
  if (t) { s_garmin.bb = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_STRESS);
  if (t) { s_garmin.stress = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_HRV);
  if (t) { strncpy(s_garmin.hrv, t->value->cstring, sizeof(s_garmin.hrv) - 1); garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_RECOVERY);
  if (t) { s_garmin.recovery = (int16_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_VO2MAX);
  if (t) { s_garmin.vo2max = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_FTP);
  if (t) { s_garmin.ftp = (int16_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_INTMIN);
  if (t) { s_garmin.intmin = (int16_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_LOAD);
  if (t) { s_garmin.load = (int16_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_HR);
  if (t) { s_garmin.hr = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_STEPS);
  if (t) { s_garmin.steps = (int16_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_SLEEP);
  if (t) { s_garmin.sleep_score = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_COACH);
  if (t) { strncpy(s_garmin.coach, t->value->cstring, sizeof(s_garmin.coach) - 1); garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_SPO2);
  if (t) { s_garmin.spo2 = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_RSP);
  if (t) { s_garmin.respiration = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_HEAT);
  if (t) { s_garmin.heat_acclim = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_ALTACL);
  if (t) { s_garmin.alt_acclim = (int8_t)t->value->int32; garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_TSTATUS);
  if (t) { strncpy(s_garmin.train_status, t->value->cstring, sizeof(s_garmin.train_status) - 1); garmin_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_GARMIN_TREADY);
  if (t) { s_garmin.train_ready = (int8_t)t->value->int32; garmin_updated = true; }

  // Clay settings
  t = dict_find(iterator, MESSAGE_KEY_Slot0);
  if (t) { strncpy(s_settings.slot[0], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot1);
  if (t) { strncpy(s_settings.slot[1], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot2);
  if (t) { strncpy(s_settings.slot[2], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot3);
  if (t) { strncpy(s_settings.slot[3], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot4);
  if (t) { strncpy(s_settings.slot[4], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot5);
  if (t) { strncpy(s_settings.slot[5], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_UpdateInterval);
  if (t) {
    int v = atoi(t->value->cstring);
    if (v > 0) s_settings.interval = (uint8_t)v;
    settings_updated = true;
  }

  // Trigger immediate fetch when credentials are saved
  t = dict_find(iterator, MESSAGE_KEY_GarminUser);
  bool creds_updated = (t != NULL);
  if (!creds_updated) {
    t = dict_find(iterator, MESSAGE_KEY_GarminPass);
    creds_updated = (t != NULL);
  }

  if (settings_updated) {
    persist_write_data(SETTINGS_KEY, &s_settings, sizeof(WatchSettings));
  }
  if (creds_updated) {
    request_garmin_data();
  }
  if (garmin_updated) {
    persist_write_data(DATA_KEY, &s_garmin, sizeof(GarminData));
    if (s_status_layer) layer_mark_dirty(s_status_layer);
    if (s_data_layer)   layer_mark_dirty(s_data_layer);
  }
}

static void update_time(struct tm *t) {
  s_is_24h = clock_is_24h_style();
  if (s_is_24h) {
    strftime(s_time_buf, sizeof(s_time_buf), "%H:%M", t);
    if (s_time_buf[0] == '0') memmove(s_time_buf, s_time_buf + 1, sizeof(s_time_buf) - 1);
  } else {
    strftime(s_time_buf, sizeof(s_time_buf), "%I:%M", t);
    if (s_time_buf[0] == '0') memmove(s_time_buf, s_time_buf + 1, sizeof(s_time_buf) - 1);
    strftime(s_ampm_buf, sizeof(s_ampm_buf), "%p", t);
    s_ampm_buf[1] = '\0';
  }
  if (s_time_layer) layer_mark_dirty(s_time_layer);
}

static void update_date(struct tm *t) {
  strftime(s_date_buf, sizeof(s_date_buf), "%m/%d", t);
  if (s_date_buf[0] == '0') memmove(s_date_buf, s_date_buf + 1, strlen(s_date_buf));
  strftime(s_day_buf, sizeof(s_day_buf), "%a", t);
  for (int i = 0; s_day_buf[i]; i++) {
    if (s_day_buf[i] >= 'a' && s_day_buf[i] <= 'z') s_day_buf[i] -= 32;
  }
  if (s_top_layer) layer_mark_dirty(s_top_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
  update_date(tick_time);
  int iv = (s_settings.interval > 0) ? s_settings.interval : DEFAULT_INTERVAL;
  if (tick_time->tm_min % iv == 0) {
    request_garmin_data();
  }
}

// ── Window ────────────────────────────────────────────────────────────────────

static void main_window_load(Window *window) {
  Layer *wl = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(wl);
  window_set_background_color(window, GColorBlack);

  s_font_time  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_RUSSO_ONE_62));
  s_font_label = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_RUSSO_ONE_20));

  int p       = FACE_PADDING;
  int cw      = bounds.size.w - 2 * p;
  int top_h   = 52;
  int stat_h  = 16;
  int data_h  = 52;  // 26px per row
  int time_h  = bounds.size.h - 2 * p - top_h - stat_h - data_h;  // 92
  int time_y  = p + top_h;
  int stat_y  = time_y + time_h;
  int data_y  = stat_y + stat_h;

  s_top_layer    = layer_create(GRect(p, p,      cw, top_h));
  s_time_layer   = layer_create(GRect(p, time_y, cw, time_h));
  s_status_layer = layer_create(GRect(p, stat_y, cw, stat_h));
  s_data_layer   = layer_create(GRect(p, data_y, cw, data_h));
  s_border_layer = layer_create(bounds);

  layer_set_update_proc(s_top_layer,    top_update_proc);
  layer_set_update_proc(s_time_layer,   time_update_proc);
  layer_set_update_proc(s_status_layer, status_update_proc);
  layer_set_update_proc(s_data_layer,   data_update_proc);
  layer_set_update_proc(s_border_layer, border_update_proc);

  layer_add_child(wl, s_top_layer);
  layer_add_child(wl, s_time_layer);
  layer_add_child(wl, s_status_layer);
  layer_add_child(wl, s_data_layer);
  layer_add_child(wl, s_border_layer);

  if (persist_exists(SETTINGS_KEY)) {
    persist_read_data(SETTINGS_KEY, &s_settings, sizeof(WatchSettings));
  }
  if (persist_exists(DATA_KEY)) {
    persist_read_data(DATA_KEY, &s_garmin, sizeof(GarminData));
  }

  snprintf(s_bat_buf, sizeof(s_bat_buf), "%d%%", s_battery_level);
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_time(t);
  update_date(t);
}

static void main_window_unload(Window *window) {
  fonts_unload_custom_font(s_font_time);
  fonts_unload_custom_font(s_font_label);
  layer_destroy(s_top_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_status_layer);
  layer_destroy(s_data_layer);
  layer_destroy(s_border_layer);
}

// ── App lifecycle ─────────────────────────────────────────────────────────────

static void init(void) {
  settings_init_defaults();
  garmin_init_defaults();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());

  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(256, 64);

  request_garmin_data();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
