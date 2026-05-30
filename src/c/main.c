#include <pebble.h>

// ── Layout (emery 200x228) ────────────────────────────────────────────────────
//
//  Y=  0 ┌──────────────────────────────────────┐  outer border
//        │                                      │
//  Y=  8 │ [DATE ] [BAT  ] [HR   ]             │  s_top_data_layer (184x36)
//  Y= 44 │ [BT][🔋]  Garmin Connected      [●] │  s_status_layer   (184x16)
//  Y= 60 │                                      │
//        │           22:06                      │  s_time_layer     (184x116)
//  Y=176 │  [icon]   [icon]   [icon]            │  s_data_layer     (184x44)
//        │   val      val      val              │
//  Y=228 └──────────────────────────────────────┘

#define SETTINGS_KEY     1
#define DATA_KEY         2
#define FACE_PADDING     8
#define MAX_TOP_SLOTS    3
#define MAX_BOT_SLOTS    4
#define SLOT_STR_LEN     8
#define DEFAULT_INTERVAL 30

typedef struct {
  char    top_slot[MAX_TOP_SLOTS][SLOT_STR_LEN];
  char    bot_slot[MAX_BOT_SLOTS][SLOT_STR_LEN];
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

typedef struct {
  int16_t  heart_rate;
  int32_t  steps;
  int32_t  sleep_minutes;
  int8_t   weather_temp;
  int8_t   weather_code;
  uint16_t sunrise;
  uint16_t sunset;
} PebbleData;

static Window       *s_window;
static Layer        *s_top_data_layer;
static Layer        *s_status_layer;        // BT + battery (centered)
static Layer        *s_time_layer;
static Layer        *s_garmin_status_layer; // "GARMIN ▲" above data
static Layer        *s_data_layer;
static Layer        *s_border_layer;

static GFont         s_font_time;
static GFont         s_font_num;
static GFont         s_font_num_small;
static GFont         s_font_unit;
static GFont         s_font_label;

static int           s_battery_level = 100;
static bool          s_is_24h        = true;
static WatchSettings s_settings;
static GarminData    s_garmin;
static PebbleData    s_pebble;

static char s_time_buf[8];
static char s_ampm_buf[3];
static char s_date_buf[8];
static char s_day_buf[4];
static char s_bat_buf[5];

// ── Helpers ───────────────────────────────────────────────────────────────────

static void settings_init_defaults(void) {
  strncpy(s_settings.top_slot[0], "DATE", SLOT_STR_LEN);
  strncpy(s_settings.top_slot[1], "BAT",  SLOT_STR_LEN);
  strncpy(s_settings.top_slot[2], "HR",   SLOT_STR_LEN);
  strncpy(s_settings.bot_slot[0], "BB",   SLOT_STR_LEN);
  strncpy(s_settings.bot_slot[1], "STR",  SLOT_STR_LEN);
  strncpy(s_settings.bot_slot[2], "HRV",  SLOT_STR_LEN);
  strncpy(s_settings.bot_slot[3], "REC",  SLOT_STR_LEN);
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

static void pebble_init_defaults(void) {
  s_pebble.heart_rate    = -1;
  s_pebble.steps         = -1;
  s_pebble.sleep_minutes = -1;
  s_pebble.weather_temp  = -99;
  s_pebble.weather_code  = -1;
  s_pebble.sunrise       = 0;
  s_pebble.sunset        = 0;
}

static void format_sun_time(uint16_t minutes, char *buf, size_t sz) {
  if (minutes == 0) { snprintf(buf, sz, "--:--"); return; }
  snprintf(buf, sz, "%d:%02d", (int)(minutes / 60), (int)(minutes % 60));
}

static void format_sleep(int32_t minutes, char *buf, size_t sz) {
  if (minutes <= 0) { snprintf(buf, sz, "--"); return; }
  int h = (int)(minutes / 60);
  int m = (int)(minutes % 60);
  if (h > 0) snprintf(buf, sz, "%dh%02d", h, m);
  else        snprintf(buf, sz, "%dm", m);
}

static const char *weather_condition(int8_t code) {
  switch (code) {
    case 0: return "Sun";
    case 1: return "Pcld";
    case 2: return "Cld";
    case 3: return "Ovrc";
    case 4: return "Fog";
    case 5: return "Drzl";
    case 6: return "Rain";
    case 7: return "Snow";
    case 8: return "Strm";
    default: return "--";
  }
}

static void top_slot_get_lines(const char *key,
                                char *line1, size_t l1sz,
                                char *line2, size_t l2sz) {
  if (strcmp(key, "DATE") == 0) {
    snprintf(line1, l1sz, "%s", s_date_buf);
    snprintf(line2, l2sz, "%s", s_day_buf);
  } else if (strcmp(key, "BAT") == 0) {
    snprintf(line1, l1sz, "Bat");
    snprintf(line2, l2sz, "%s", s_bat_buf);
  } else if (strcmp(key, "HR") == 0) {
    snprintf(line1, l1sz, "HR");
    if (s_pebble.heart_rate > 0) snprintf(line2, l2sz, "%d", (int)s_pebble.heart_rate);
    else snprintf(line2, l2sz, "--");
  } else if (strcmp(key, "STP") == 0) {
    snprintf(line1, l1sz, "Steps");
    if (s_pebble.steps >= 0) {
      int32_t s = s_pebble.steps;
      if (s >= 10000)     snprintf(line2, l2sz, "%dk", (int)(s / 1000));
      else if (s >= 1000) snprintf(line2, l2sz, "%d.%dk", (int)(s / 1000), (int)((s % 1000) / 100));
      else                snprintf(line2, l2sz, "%d", (int)s);
    } else snprintf(line2, l2sz, "--");
  } else if (strcmp(key, "SLP") == 0) {
    snprintf(line1, l1sz, "Sleep");
    format_sleep(s_pebble.sleep_minutes, line2, l2sz);
  } else if (strcmp(key, "SRS") == 0) {
    format_sun_time(s_pebble.sunrise, line1, l1sz);
    format_sun_time(s_pebble.sunset,  line2, l2sz);
  } else if (strcmp(key, "WTH") == 0) {
    snprintf(line1, l1sz, "%s", weather_condition(s_pebble.weather_code));
    if (s_pebble.weather_temp != -99) snprintf(line2, l2sz, "%dC", (int)s_pebble.weather_temp);
    else snprintf(line2, l2sz, "--");
  } else {
    snprintf(line1, l1sz, "---");
    snprintf(line2, l2sz, "--");
  }
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

typedef enum {
  COND_CLEAR_DAY = 0,
  COND_PARTLY_CLOUDY,
  COND_CLOUDY,
  COND_LIGHT_RAIN,
  COND_HEAVY_RAIN,
  COND_RAINING_SNOWING,
  COND_LIGHT_SNOW,
  COND_HEAVY_SNOW,
  COND_THUNDERSTORM,
  COND_GENERIC,
} WeatherCondition;

static uint32_t weather_pdc_id(WeatherCondition cond) {
  switch (cond) {
    case COND_CLEAR_DAY:       return RESOURCE_ID_WEATHER_CLEAR_DAY;
    case COND_PARTLY_CLOUDY:   return RESOURCE_ID_WEATHER_PARTLY_CLOUDY;
    case COND_CLOUDY:          return RESOURCE_ID_WEATHER_CLOUDY;
    case COND_LIGHT_RAIN:      return RESOURCE_ID_WEATHER_LIGHT_RAIN;
    case COND_HEAVY_RAIN:      return RESOURCE_ID_WEATHER_HEAVY_RAIN;
    case COND_RAINING_SNOWING: return RESOURCE_ID_WEATHER_RAINING_SNOWING;
    case COND_LIGHT_SNOW:      return RESOURCE_ID_WEATHER_LIGHT_SNOW;
    case COND_HEAVY_SNOW:      return RESOURCE_ID_WEATHER_HEAVY_SNOW;
    case COND_THUNDERSTORM:    return RESOURCE_ID_WEATHER_THUNDERSTORM;
    default:                   return RESOURCE_ID_WEATHER_GENERIC;
  }
}

static void draw_pdc_top(GContext *ctx, uint32_t res_id, int x, int slot_w, int lh) {
  if (!res_id) return;
  GDrawCommandImage *pdc = gdraw_command_image_create_with_resource(res_id);
  if (!pdc) return;

  // 黒背景で見えるよう黒↔白を反転（透明はそのまま）
  GDrawCommandList *list = gdraw_command_image_get_command_list(pdc);
  int n = gdraw_command_list_get_num_commands(list);
  for (int i = 0; i < n; i++) {
    GDrawCommand *cmd = gdraw_command_list_get_command(list, i);
    GColor fill = gdraw_command_get_fill_color(cmd);
    if (fill.argb == GColorBlack.argb)      gdraw_command_set_fill_color(cmd, GColorWhite);
    else if (fill.argb == GColorWhite.argb) gdraw_command_set_fill_color(cmd, GColorBlack);
    if (gdraw_command_get_stroke_width(cmd) > 0) {
      GColor stroke = gdraw_command_get_stroke_color(cmd);
      if (stroke.argb == GColorBlack.argb)      gdraw_command_set_stroke_color(cmd, GColorWhite);
      else if (stroke.argb == GColorWhite.argb) gdraw_command_set_stroke_color(cmd, GColorBlack);
    }
  }

  GSize sz = gdraw_command_image_get_bounds_size(pdc);
  gdraw_command_image_draw(ctx, pdc,
      GPoint(x + (slot_w - sz.w) / 2, (lh - sz.h) / 2));
  gdraw_command_image_destroy(pdc);
}

static void draw_num_with_unit(GContext *ctx, const char *val,
                               GFont font_num, GFont font_unit,
                               GRect rect, GTextAlignment align) {
  int split = 0;
  while (val[split] && (val[split] == '-' ||
         (val[split] >= '0' && val[split] <= '9') || val[split] == '.')) {
    split++;
  }
  if (val[split] == '\0') {
    graphics_draw_text(ctx, val, font_num, rect,
        GTextOverflowModeTrailingEllipsis, align, NULL);
    return;
  }

  char num_part[10], unit_part[6];
  strncpy(num_part, val, split);
  num_part[split] = '\0';
  strncpy(unit_part, val + split, sizeof(unit_part) - 1);
  unit_part[sizeof(unit_part) - 1] = '\0';

  GRect big = GRect(0, 0, 200, 50);
  GSize nsz = graphics_text_layout_get_content_size(num_part, font_num,
      big, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
  GSize usz = graphics_text_layout_get_content_size(unit_part, font_unit,
      big, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);

  int total_w = nsz.w + usz.w;
  int sx;
  if (align == GTextAlignmentCenter)
    sx = rect.origin.x + (rect.size.w - total_w) / 2;
  else if (align == GTextAlignmentRight)
    sx = rect.origin.x + rect.size.w - total_w;
  else
    sx = rect.origin.x;

  graphics_draw_text(ctx, num_part, font_num,
      GRect(sx, rect.origin.y, nsz.w + 4, rect.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
  graphics_draw_text(ctx, unit_part, font_unit,
      GRect(sx + nsz.w, rect.origin.y + (nsz.h - usz.h), usz.w + 4, rect.size.h),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

// ── Draw procs ────────────────────────────────────────────────────────────────

static void top_data_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int slot_w = b.size.w / 3;
  int lh = b.size.h / 2;

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(slot_w,     0), GPoint(slot_w,     b.size.h));
  graphics_draw_line(ctx, GPoint(slot_w * 2, 0), GPoint(slot_w * 2, b.size.h));

  char line1[12], line2[12];
  for (int i = 0; i < MAX_TOP_SLOTS; i++) {
    int x = i * slot_w;
    const char *key = s_settings.top_slot[i];
    top_slot_get_lines(key, line1, sizeof(line1), line2, sizeof(line2));
    graphics_context_set_text_color(ctx, GColorWhite);

    if (strcmp(key, "BAT") == 0) {
      // 上段: outdoor-typical スタイルのバッテリーバー
      int bw = slot_w - 10, bh = 14;
      int bx = x + 5, by = (lh - bh) / 2;
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, 2);
      graphics_draw_round_rect(ctx, GRect(bx, by, bw, bh), 2);
      graphics_context_set_fill_color(ctx, GColorWhite);
      graphics_fill_rect(ctx, GRect(bx + bw, by + bh/2 - 2, 3, 4), 1, GCornersRight);
      GColor fc = (s_battery_level <= 20) ? GColorRed :
                  (s_battery_level <= 40) ? GColorChromeYellow : GColorGreen;
      int fw = (s_battery_level * (bw - 6)) / 100;
      if (fw < 0) fw = 0;
      graphics_context_set_fill_color(ctx, fc);
      graphics_fill_rect(ctx, GRect(bx + 3, by + 3, fw, bh - 6), 0, GCornerNone);
      // 下段: %
      graphics_context_set_text_color(ctx, GColorWhite);
      graphics_draw_text(ctx, line2, s_font_num,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    } else if (strcmp(key, "HR") == 0) {
      draw_pdc_top(ctx, RESOURCE_ID_ICON_HEART, x, slot_w, lh);
      graphics_draw_text(ctx, line2, s_font_num,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    } else if (strcmp(key, "STP") == 0) {
      draw_pdc_top(ctx, RESOURCE_ID_ICON_STEPS, x, slot_w, lh);
      graphics_draw_text(ctx, line2, s_font_num,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    } else if (strcmp(key, "SLP") == 0) {
      draw_pdc_top(ctx, RESOURCE_ID_ICON_SLEEP, x, slot_w, lh);
      graphics_draw_text(ctx, line2, s_font_num,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    } else if (strcmp(key, "WTH") == 0) {
      draw_pdc_top(ctx, weather_pdc_id((WeatherCondition)s_pebble.weather_code), x, slot_w, lh);
      graphics_draw_text(ctx, line2, s_font_num,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);

    } else if (strcmp(key, "SRS") == 0) {
      graphics_draw_text(ctx, line1, s_font_num,
          GRect(x + 1, 0, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
      graphics_draw_text(ctx, line2, s_font_num,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

    } else {
      // 標準 (DATE 等): 上=値(num) 下=ラベル(label)
      graphics_draw_text(ctx, line1, s_font_num,
          GRect(x + 1, 0, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
      graphics_draw_text(ctx, line2, s_font_label,
          GRect(x + 1, lh, slot_w - 2, lh),
          GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
    }
  }
}

static void status_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int cy = b.size.h / 2;

  // BT(12) + gap(6) + Battery(20) = 38px, centered in layer
  int total_w = 12 + 6 + 20;
  int sx = (b.size.w - total_w) / 2;

  bool bt = bluetooth_connection_service_peek();
  GBitmap *bt_bmp = gbitmap_create_with_resource(
      bt ? RESOURCE_ID_BT_CONNECTED : RESOURCE_ID_BT_DISCONNECTED);
  if (bt_bmp) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, bt_bmp, GRect(sx, cy - 6, 12, 12));
    gbitmap_destroy(bt_bmp);
  }

  static const uint32_t bat_res[8] = {
    RESOURCE_ID_BAT_0, RESOURCE_ID_BAT_1, RESOURCE_ID_BAT_2, RESOURCE_ID_BAT_3,
    RESOURCE_ID_BAT_4, RESOURCE_ID_BAT_5, RESOURCE_ID_BAT_6, RESOURCE_ID_BAT_7
  };
  int bat_idx = s_battery_level * 7 / 100;
  if (bat_idx < 0) bat_idx = 0;
  if (bat_idx > 7) bat_idx = 7;
  GBitmap *bat_bmp = gbitmap_create_with_resource(bat_res[bat_idx]);
  if (bat_bmp) {
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, bat_bmp, GRect(sx + 18, cy - 5, 20, 10));
    gbitmap_destroy(bat_bmp);
  }
}

static void garmin_status_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int cy = b.size.h / 2;

  GColor icon_color = (s_garmin.status == 1) ? GColorGreen :
                      (s_garmin.status == 2) ? GColorChromeYellow : GColorRed;

  // "GARMIN" (~46px) + gap(6) + icon(12) = ~64px, centered
  int text_w = 42, gap = 1, icon_sz = 12;
  int sx = (b.size.w - text_w - gap - icon_sz) / 2;

  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, "GARMIN", fonts_get_system_font(FONT_KEY_GOTHIC_14),
      GRect(sx, -2, text_w, 14),
      GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  int ix = sx + text_w + gap;
  int iy = cy - icon_sz / 2;

  GBitmap *icon = gbitmap_create_with_resource(RESOURCE_ID_GARMIN_STATUS);
  if (icon) {
    // 1-bitパレット形式: palette[0]=背景色, palette[1]=前景色を状態色に変更
    GColor *palette = gbitmap_get_palette(icon);
    if (palette) {
      palette[0] = GColorBlack;
      palette[1] = icon_color;
    }
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(ctx, icon, GRect(ix, iy, icon_sz, icon_sz));
    gbitmap_destroy(icon);
  }
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

static void data_update_proc(Layer *layer, GContext *ctx) {
  GRect b = layer_get_bounds(layer);
  int slot_w = b.size.w / 4;

  graphics_context_set_stroke_color(ctx, GColorDarkGray);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(slot_w,     0), GPoint(slot_w,     b.size.h));
  graphics_draw_line(ctx, GPoint(slot_w * 2, 0), GPoint(slot_w * 2, b.size.h));
  graphics_draw_line(ctx, GPoint(slot_w * 3, 0), GPoint(slot_w * 3, b.size.h));

  char lbl[8], val[10];
  for (int i = 0; i < MAX_BOT_SLOTS; i++) {
    int x = i * slot_w;

    const char *key = s_settings.bot_slot[i];
    if (strcmp(key, "NONE") == 0 || key[0] == '\0') continue;

    slot_get_display(key, lbl, sizeof(lbl), val, sizeof(val));

    uint32_t res_id = slot_get_resource_id(key);
    if (res_id) {
      GBitmap *icon = gbitmap_create_with_resource(res_id);
      if (icon) {
        graphics_context_set_compositing_mode(ctx, GCompOpSet);
        int ix = x + (slot_w - 18) / 2;
        graphics_draw_bitmap_in_rect(ctx, icon, GRect(ix, 6, 18, 18));
        gbitmap_destroy(icon);
      }
    }

    int digit_count = 0;
    for (int j = 0; val[j]; j++) {
      if (val[j] >= '0' && val[j] <= '9') digit_count++;
    }
    bool large = (digit_count > 4) || i == 3;
    GFont num_font  = large ? s_font_num_small : s_font_num;
    GFont unit_font = s_font_unit;
    int val_y       = large ? 23 : 17;

    graphics_context_set_text_color(ctx, GColorWhite);
    draw_num_with_unit(ctx, val, num_font, unit_font,
        GRect(x + 1, val_y, slot_w - 2, 28), GTextAlignmentCenter);
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
  if (s_top_data_layer) layer_mark_dirty(s_top_data_layer);
  if (s_status_layer)   layer_mark_dirty(s_status_layer);
}

static void bluetooth_callback(bool connected) {
  if (s_status_layer) layer_mark_dirty(s_status_layer);
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  bool changed = false;
  if (event == HealthEventHeartRateUpdate || event == HealthEventMovementUpdate) {
    HealthValue hr = health_service_peek_current_value(HealthMetricHeartRateBPM);
    s_pebble.heart_rate = (hr > 0) ? (int16_t)hr : -1;
    time_t start = time_start_of_today();
    HealthValue steps = health_service_sum(HealthMetricStepCount, start, time(NULL));
    s_pebble.steps = (int32_t)steps;
    changed = true;
  }
  if (event == HealthEventSleepUpdate) {
    time_t start = time_start_of_today();
    HealthValue sleep = health_service_sum(HealthMetricSleepSeconds, start, time(NULL));
    s_pebble.sleep_minutes = (int32_t)(sleep / 60);
    changed = true;
  }
  if (changed && s_top_data_layer) layer_mark_dirty(s_top_data_layer);
}

static void health_init(void) {
  HealthValue hr = health_service_peek_current_value(HealthMetricHeartRateBPM);
  s_pebble.heart_rate = (hr > 0) ? (int16_t)hr : -1;
  time_t start = time_start_of_today();
  HealthValue steps = health_service_sum(HealthMetricStepCount, start, time(NULL));
  s_pebble.steps = (int32_t)steps;
  HealthValue sleep = health_service_sum(HealthMetricSleepSeconds, start, time(NULL));
  s_pebble.sleep_minutes = (int32_t)(sleep / 60);
  health_service_events_subscribe(health_handler, NULL);
}
#endif

static void request_garmin_data(void) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) == APP_MSG_OK) {
    dict_write_uint8(iter, MESSAGE_KEY_REQUEST_GARMIN, 1);
    app_message_outbox_send();
  }
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  Tuple *t;
  bool garmin_updated   = false;
  bool settings_updated = false;
  bool pebble_updated   = false;

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

  // Pebble native data from JS
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_HEART_RATE);
  if (t) { s_pebble.heart_rate = (int16_t)t->value->int32; pebble_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_STEPS);
  if (t) { s_pebble.steps = (int32_t)t->value->int32; pebble_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_SLEEP);
  if (t) { s_pebble.sleep_minutes = (int32_t)t->value->int32; pebble_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_WEATHER_TEMP);
  if (t) { s_pebble.weather_temp = (int8_t)t->value->int32; pebble_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_WEATHER_CODE);
  if (t) { s_pebble.weather_code = (int8_t)t->value->int32; pebble_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_SUNRISE);
  if (t) { s_pebble.sunrise = (uint16_t)t->value->int32; pebble_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_PEBBLE_SUNSET);
  if (t) { s_pebble.sunset = (uint16_t)t->value->int32; pebble_updated = true; }

  // Clay settings - top slots
  t = dict_find(iterator, MESSAGE_KEY_TopSlot0);
  if (t) { strncpy(s_settings.top_slot[0], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_TopSlot1);
  if (t) { strncpy(s_settings.top_slot[1], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_TopSlot2);
  if (t) { strncpy(s_settings.top_slot[2], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }

  // Clay settings - bottom slots
  t = dict_find(iterator, MESSAGE_KEY_Slot0);
  if (t) { strncpy(s_settings.bot_slot[0], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot1);
  if (t) { strncpy(s_settings.bot_slot[1], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot2);
  if (t) { strncpy(s_settings.bot_slot[2], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }
  t = dict_find(iterator, MESSAGE_KEY_Slot3);
  if (t) { strncpy(s_settings.bot_slot[3], t->value->cstring, SLOT_STR_LEN - 1); settings_updated = true; }

  t = dict_find(iterator, MESSAGE_KEY_UpdateInterval);
  if (t) {
    int v = atoi(t->value->cstring);
    if (v > 0) s_settings.interval = (uint8_t)v;
    settings_updated = true;
  }

  t = dict_find(iterator, MESSAGE_KEY_GarminUser);
  bool creds_updated = (t != NULL);
  if (!creds_updated) {
    t = dict_find(iterator, MESSAGE_KEY_GarminPass);
    creds_updated = (t != NULL);
  }

  if (settings_updated) {
    persist_write_data(SETTINGS_KEY, &s_settings, sizeof(WatchSettings));
    if (s_top_data_layer) layer_mark_dirty(s_top_data_layer);
    if (s_data_layer)     layer_mark_dirty(s_data_layer);
  }
  if (creds_updated) {
    request_garmin_data();
  }
  if (garmin_updated) {
    persist_write_data(DATA_KEY, &s_garmin, sizeof(GarminData));
    if (s_garmin_status_layer) layer_mark_dirty(s_garmin_status_layer);
    if (s_data_layer)          layer_mark_dirty(s_data_layer);
  }
  if (pebble_updated) {
    if (s_top_data_layer) layer_mark_dirty(s_top_data_layer);
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
  if (s_top_data_layer) layer_mark_dirty(s_top_data_layer);
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
  s_font_num   = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  s_font_num_small = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  s_font_unit  = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_font_label = fonts_get_system_font(FONT_KEY_ROBOTO_CONDENSED_21);

  int p       = FACE_PADDING;
  int cw      = bounds.size.w - 2 * p;
  int top_h   = 52;
  int stat_h  = 16;
  int gstat_h = 16;
  int data_h  = 46;
  int time_h  = bounds.size.h - 2 * p - top_h - stat_h - gstat_h - data_h;  // 76
  int time_y  = p + top_h + stat_h;
  int gstat_y = time_y + time_h;
  int data_y  = gstat_y + gstat_h;

  s_top_data_layer    = layer_create(GRect(p, p,        cw, top_h));
  s_status_layer      = layer_create(GRect(p, p+top_h,  cw, stat_h));
  s_time_layer        = layer_create(GRect(p, time_y,   cw, time_h));
  s_garmin_status_layer = layer_create(GRect(p, gstat_y, cw, gstat_h));
  s_data_layer        = layer_create(GRect(p, data_y,   cw, data_h));
  s_border_layer      = layer_create(bounds);

  layer_set_update_proc(s_top_data_layer,    top_data_update_proc);
  layer_set_update_proc(s_status_layer,      status_update_proc);
  layer_set_update_proc(s_time_layer,        time_update_proc);
  layer_set_update_proc(s_garmin_status_layer, garmin_status_update_proc);
  layer_set_update_proc(s_data_layer,        data_update_proc);
  layer_set_update_proc(s_border_layer,      border_update_proc);

  layer_add_child(wl, s_top_data_layer);
  layer_add_child(wl, s_status_layer);
  layer_add_child(wl, s_time_layer);
  layer_add_child(wl, s_garmin_status_layer);
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
  layer_destroy(s_top_data_layer);
  layer_destroy(s_status_layer);
  layer_destroy(s_time_layer);
  layer_destroy(s_garmin_status_layer);
  layer_destroy(s_data_layer);
  layer_destroy(s_border_layer);
}

// ── App lifecycle ─────────────────────────────────────────────────────────────

static void init(void) {
  settings_init_defaults();
  garmin_init_defaults();
  pebble_init_defaults();

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
    .load   = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_callback);
  battery_callback(battery_state_service_peek());
  bluetooth_connection_service_subscribe(bluetooth_callback);

#if defined(PBL_HEALTH)
  health_init();
#endif

  app_message_register_inbox_received(inbox_received_callback);
  app_message_open(512, 64);

  request_garmin_data();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
  return 0;
}
