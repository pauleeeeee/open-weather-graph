#include <pebble.h>
#include <stdio.h>
#include <math.h>
#include "./open-weather-graph.h"
#include "./dither.h"
#include <pebble-battery-bar/pebble-battery-bar.h>
#include <pebble-bluetooth-icon/pebble-bluetooth-icon.h>
#include <pebble-moon-layer/moon-layer.h>

static BluetoothLayer *s_bluetooth_layer;
static BatteryBarLayer *s_battery_layer;
static MoonLayer *moon_layer;
static Window *s_main_window;
static Layer *s_weather_window_layer;
static TextLayer *s_time_layer, *s_full_date_layer, *s_current_temperature_layer, *s_today_high_and_low_layer;
static TextLayer *s_days_of_the_week_text_layers[6];
static GFont s_current_temperature_font, s_big_font, s_medium_font, s_small_font, s_small_font_bold, s_tiny_font;

static uint8_t s_graph_temperature[144];
static uint8_t s_graph_cloud_cover[144];
static uint8_t s_graph_precip_type[144];
static uint8_t s_graph_precip_probability[144];
static uint8_t s_graph_humidity[144];
static uint8_t s_graph_pressure[144];
static uint8_t s_graph_wind_speed[144];

static int8_t s_daily_highs[7];
static int8_t s_daily_lows[7];
/* One combined label string per day (day letter + high + low); 6 labels for days 0..5 */
static char s_daily_text_highs_and_lows[6][20];
static char s_today_high_and_low_text[16];
static char s_today_low[4];
static int s_current_temperature;
static char s_current_temperature_text[24];
static uint8_t s_day_markers[6];
static uint8_t s_horizon_days;
#define WEATHER_PLACEHOLDER " "
#define WEATHER_AWAITING_STR "Awaiting data"

static bool has_weather_data(void) {
  return s_current_temperature_text[0] != '\0' &&
         s_current_temperature_text[0] != ' ' &&
         strstr(s_current_temperature_text, "\xc2\xb0") != NULL;  /* UTF-8 degree symbol */
}
static uint8_t s_days_of_the_week[7];

static void create_day_label_layers(void);
static void update_day_label_positions(void);
static void update_day_label_text(void);
static void update_today_high_low_layer(void);
static void update_day_label_visibility(void);

static void update_time() {
  // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  // Write the current hours and minutes into a buffer
  static char s_buffer[8];
  strftime(s_buffer, sizeof(s_buffer), "%l:%M", tick_time);

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, s_buffer);
  // text_layer_set_text(s_time_layer, "12:00");

}


static void update_date() {
    // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  static char full_date_text[] = "Xxx -----";
  
  strftime(full_date_text, sizeof(full_date_text), "%a %m/%d", tick_time);
  text_layer_set_text(s_full_date_layer, full_date_text);

  //update moon
  moon_layer_set_date(moon_layer, tick_time);

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void request_weather_callback(void *data) {
  DictionaryIterator *iter;
  if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "GetWeather: outbox_begin failed");
    return;
  }
  int fetch = 1;
  dict_write_int(iter, GetWeather, &fetch, sizeof(int), false);
  app_message_outbox_send();
  APP_LOG(APP_LOG_LEVEL_INFO, "GetWeather request sent to phone");
}

static void in_received_handler(DictionaryIterator *iter, void *context) {
  bool got_weather = false;

  Tuple *graph_temperature_tuple = dict_find (iter, GraphTemperature);
  if (graph_temperature_tuple) {
      memcpy(s_graph_temperature, graph_temperature_tuple->value->data, 144);
      persist_write_data(GraphTemperature, s_graph_temperature, 144);
      got_weather = true;
  }

  Tuple *graph_precip_type_tuple = dict_find (iter, GraphPrecipType);
  if (graph_precip_type_tuple) {
      memcpy(s_graph_precip_type, graph_precip_type_tuple->value->data, 144);
      persist_write_data(GraphPrecipType, s_graph_precip_type, 144);
  }

  Tuple *graph_cloud_cover_tuple = dict_find (iter, GraphCloudCover);
  if (graph_cloud_cover_tuple) {
      memcpy(s_graph_cloud_cover, graph_cloud_cover_tuple->value->data, 144);
      persist_write_data(GraphCloudCover, s_graph_cloud_cover, 144);
  }

  Tuple *graph_wind_speed_tuple = dict_find (iter, GraphWindSpeed);
  if (graph_wind_speed_tuple) {
      memcpy(s_graph_wind_speed, graph_wind_speed_tuple->value->data, 144);
      persist_write_data(GraphWindSpeed, s_graph_wind_speed, 144);
  }

  Tuple *graph_precip_probability_tuple = dict_find (iter, GraphPrecipProb);
  if (graph_precip_probability_tuple) {
      memcpy(s_graph_precip_probability, graph_precip_probability_tuple->value->data, 144);
      persist_write_data(GraphPrecipProb, s_graph_precip_probability, 144);
  }

  Tuple *daily_highs_tuple = dict_find (iter, DailyHighs);
  if (daily_highs_tuple) {
      memcpy(s_daily_highs, daily_highs_tuple->value->data, 7);
      persist_write_data(DailyHighs, s_daily_highs, 7);
  }

  Tuple *daily_lows_tuple = dict_find (iter, DailyLows);
  if (daily_lows_tuple) {
      memcpy(s_daily_lows, daily_lows_tuple->value->data, 7);
      persist_write_data(DailyLows, s_daily_lows, 7);
  }

  Tuple *day_markers_tuple = dict_find (iter, DayMarkers);
  if (day_markers_tuple) {
      memcpy(s_day_markers, day_markers_tuple->value->data, 6);
      persist_write_data(DayMarkers, s_day_markers, 6);
  }

  Tuple *days_of_the_week_tuple = dict_find (iter, DaysOfTheWeek);
  if (days_of_the_week_tuple) {
      memcpy(s_days_of_the_week, days_of_the_week_tuple->value->data, 7);
      persist_write_data(DaysOfTheWeek, s_days_of_the_week, 7);
      layer_mark_dirty(s_weather_window_layer);
  }

  Tuple *current_temperature_tuple = dict_find (iter, CurrentTemperature);
  if (current_temperature_tuple) {
    strncpy(s_current_temperature_text, current_temperature_tuple->value->cstring, sizeof(s_current_temperature_text));
    s_current_temperature_text[sizeof(s_current_temperature_text) - 1] = '\0';
    text_layer_set_text(s_current_temperature_layer, s_current_temperature_text);
    persist_write_string(CurrentTemperature, s_current_temperature_text);
  }

  Tuple *horizon_days_tuple = dict_find(iter, HorizonDays);
  if (horizon_days_tuple) {
    int32_t v = horizon_days_tuple->value->int32;
    if (v < 1) v = 1;
    if (v > 6) v = 6;
    s_horizon_days = (uint8_t)v;
    persist_write_data(HorizonDays, &s_horizon_days, 1);
  }

    if (got_weather) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Weather data received: temp[0]=%d temp[72]=%d day_markers[0]=%d current=%s",
        (int)s_graph_temperature[0], (int)s_graph_temperature[72],
        (int)s_day_markers[0], s_current_temperature_text);
    create_day_label_layers();   /* create day labels if not yet created (e.g. first fetch) */
    update_day_label_text();     /* refresh highs/lows/day when data changes */
    update_day_label_positions(); /* recenter labels when day_markers or horizon change */
    update_day_label_visibility();
    update_today_high_low_layer();
    layer_mark_dirty(s_weather_window_layer);
  }
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "AppMessage dropped: reason=%d", (int)reason);
}

static char *getDayString(int day){
  switch(day){
    case 0:
      return "S";
    case 1:
      return "M";
    case 2:
      return "T";
    case 3:
      return "W";
    case 4:
      return "T";
    case 5:
      return "F";
    case 6:
      return "S";
    default:
      return "";
  }
}

#define DAY_LABEL_WIDTH 24
#define DAY_LABEL_HEIGHT 60
#define DAY_LABEL_TOP (118 - 28)
#define GRAPH_WIDTH 144

/** Position day label layers so each is centered in the middle of its day segment.
 * Day segments: day 0 = 0 to s_day_markers[0], day i = s_day_markers[i-1] to s_day_markers[i], last = s_day_markers[horizon-2] to GRAPH_WIDTH.
 * Center = midpoint of segment; origin = center - half label width. */
static void update_day_label_positions(void) {
  for (int i = 0; i < 6; i++) {
    if (s_days_of_the_week_text_layers[i] == NULL) continue;
    if (i >= (int)s_horizon_days) continue;
    int center_x;
    if (i == 0) {
      center_x = s_day_markers[0] / 2;
    } else if (i < (int)s_horizon_days - 1) {
      center_x = (s_day_markers[i - 1] + s_day_markers[i]) / 2;
    } else {
      center_x = (s_day_markers[s_horizon_days - 2] + GRAPH_WIDTH) / 2;
    }
    int origin_x = center_x - DAY_LABEL_WIDTH / 2;
    if (origin_x < 0) origin_x = 0;
    if (origin_x > GRAPH_WIDTH - DAY_LABEL_WIDTH) origin_x = GRAPH_WIDTH - DAY_LABEL_WIDTH;
    layer_set_frame(text_layer_get_layer(s_days_of_the_week_text_layers[i]),
        GRect(origin_x, DAY_LABEL_TOP, DAY_LABEL_WIDTH, DAY_LABEL_HEIGHT));
  }
}

/** Create and add day label text layers when we have day_markers. Safe to call multiple times; only creates if layers don't exist yet. */
static void create_day_label_layers(void) {
  if (s_day_markers[0] == 0) return;
  if (s_days_of_the_week_text_layers[0] != NULL) return;  /* already created */

  for (int i = 0; i < 6; i++) {
    s_days_of_the_week_text_layers[i] = text_layer_create(GRect(0, DAY_LABEL_TOP, DAY_LABEL_WIDTH, DAY_LABEL_HEIGHT));
    text_layer_set_background_color(s_days_of_the_week_text_layers[i], GColorClear);
    text_layer_set_text_color(s_days_of_the_week_text_layers[i], GColorWhite);
    text_layer_set_font(s_days_of_the_week_text_layers[i], s_tiny_font);
    text_layer_set_text_alignment(s_days_of_the_week_text_layers[i], GTextAlignmentCenter);

    snprintf(s_daily_text_highs_and_lows[i], sizeof(s_daily_text_highs_and_lows[i]), "%s\n%d\n%d",
        getDayString(s_days_of_the_week[i]), s_daily_highs[i], s_daily_lows[i]);

    text_layer_set_text(s_days_of_the_week_text_layers[i], s_daily_text_highs_and_lows[i]);
    layer_add_child(s_weather_window_layer, text_layer_get_layer(s_days_of_the_week_text_layers[i]));
  }
  update_day_label_positions();
  update_day_label_visibility();
}

/** Refresh day label text from current s_daily_highs/s_daily_lows/s_days_of_the_week. */
static void update_day_label_text(void) {
  for (int i = 0; i < 6; i++) {
    if (s_days_of_the_week_text_layers[i] == NULL) continue;
    snprintf(s_daily_text_highs_and_lows[i], sizeof(s_daily_text_highs_and_lows[i]), "%s\n%d\n%d",
        getDayString(s_days_of_the_week[i]), s_daily_highs[i], s_daily_lows[i]);
    text_layer_set_text(s_days_of_the_week_text_layers[i], s_daily_text_highs_and_lows[i]);
  }
}

/** Set day label layers visible for days 0..(horizon-1), i.e. all horizon_days labels. */
static void update_day_label_visibility(void) {
  for (int i = 0; i < 6; i++) {
    if (s_days_of_the_week_text_layers[i] != NULL) {
      layer_set_hidden(text_layer_get_layer(s_days_of_the_week_text_layers[i]), (i >= (int)s_horizon_days));
    }
  }
}

/** Update today high/low text layer from current s_daily_highs/s_daily_lows. */
static void update_today_high_low_layer(void) {
  if (!has_weather_data()) {
    text_layer_set_text(s_today_high_and_low_layer, "--/--");
    return;
  }
  snprintf(s_today_high_and_low_text, sizeof(s_today_high_and_low_text), "%d", s_daily_highs[0]);
  strcat(s_today_high_and_low_text, "°");
  strcat(s_today_high_and_low_text, "/");
  snprintf(s_today_low, sizeof(s_today_low), "%d", s_daily_lows[0]);
  strcat(s_today_high_and_low_text, s_today_low);
  strcat(s_today_high_and_low_text, "°");
  text_layer_set_text(s_today_high_and_low_layer, s_today_high_and_low_text);
}

static void s_weather_window_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  static bool s_logged_data_state = false;

#if defined(PBL_COLOR)
  graphics_context_set_antialiased(ctx, true);
#endif

  int graphOffset = 30;
  GRect graph_rect = GRect(bounds.origin.x, graphOffset + 10, bounds.size.w, 55);

  /* When there is no weather data on the watch (first launch before first fetch),
   * skip the gradient and graph and draw a simple dark placeholder instead.
   * This avoids showing a big dithered square and flat lines. */
  if (!persist_exists(GraphTemperature)) {
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, graph_rect, 0, GCornerNone);
    return;
  }

  /* Log once whether we have graph data (so we see if wavy line should appear) */
  if (!s_logged_data_state) {
    int t0 = (int)s_graph_temperature[0], t72 = (int)s_graph_temperature[72];
    APP_LOG(APP_LOG_LEVEL_INFO, "Graph draw: bounds.h=%d temp[0]=%d temp[72]=%d day_markers[0]=%d",
        bounds.size.h, t0, t72, (int)s_day_markers[0]);
    s_logged_data_state = true;
  }

  draw_gradient_rect(ctx, graph_rect, GColorWhite, GColorBlack, TOP_TO_BOTTOM);

  /* Fill the band above the temp curve with black so the gradient doesn't show as gray. */
  graphics_context_set_fill_color(ctx, GColorBlack);
  for (int i = 0; i < 144; i++) {
    int h = (int)s_graph_temperature[i];
    if (h > 0) {
      graphics_fill_rect(ctx, GRect(i, graphOffset, 1, h), 0, GCornerNone);
    }
  }

  graphics_context_set_stroke_width(ctx, 1);

  for (int i = 0; i < 144; i++){
    if (i < 143){
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, GPoint(i,s_graph_wind_speed[i]+4), GPoint(i+1,s_graph_wind_speed[i+1]+4));
    }

    if (s_graph_cloud_cover[i] > 0) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, GPoint(i, graphOffset), GPoint(i, graphOffset-s_graph_cloud_cover[i]));
    }
  }

  /* Precipitation: dotted vertical line from just below cloud to just above temp line.
   * Rain = 2 px on, 2 off; snow = 1 on, 1 off; each column offset by its index. */
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < 144; i++) {
    if (s_graph_precip_probability[i] == 0 || s_graph_precip_type[i] == 0) continue;

    int y_top = graphOffset + 1;   /* just below cloud bottom */
    int y_bottom = (int)s_graph_temperature[i] + graphOffset - 1;  /* just above temp line */
    if (y_bottom <= y_top) continue;

    int period, on_len;
    if (s_graph_precip_type[i] == 1) {
      period = 4; on_len = 2;  /* rain: 2 on, 2 off */
    } else {
      period = 2; on_len = 1;  /* snow (or sleet): 1 on, 1 off */
    }
    int col_offset = i % period;

    for (int y = y_top; y <= y_bottom; y++) {
      int y_index = y - y_top;
      int pattern_index = (col_offset + y_index) % period;
      if (pattern_index < on_len) {
        graphics_fill_rect(ctx, GRect(i, y, 1, 1), 0, GCornerNone);
      }
    }
  }

  //temp line gets its own loop to ensure prettiness
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 1);
  for (int i = 0; i < 144; i++){
    if( i < 143 ) {
      graphics_draw_line(ctx, GPoint(i, s_graph_temperature[i]+graphOffset), GPoint(i+1, s_graph_temperature[i+1]+graphOffset));
    }
  }

  // static char high_low[24];
  // static char low[8];


  /* Draw day separator lines at each boundary (pixel positions from JS).
   * Boundaries: s_day_markers[0]=24h, [1]=48h, ... [horizon_days-2]=(horizon_days-1)*24h.
   * Draw all horizon_days-1 separators so the first one always renders when horizon_days > 1. */
  // if (s_horizon_days > 1) {
  //   for (int i = 0; i < 6 && i < (int)s_horizon_days - 1; i++) {
  //     graphics_draw_line(ctx, GPoint(s_day_markers[i], graphOffset+5), GPoint(s_day_markers[i], 124-graphOffset));
  //   }
  // }


}


static void main_window_load(Window *window) {
  // Get information about the main window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  //pull storage 
  if(persist_exists(GraphTemperature)){
    persist_read_data(GraphTemperature, s_graph_temperature, sizeof(s_graph_temperature));
  } 
  if(persist_exists(GraphCloudCover)){
    persist_read_data(GraphCloudCover, s_graph_cloud_cover, sizeof(s_graph_cloud_cover));
  } 
  if(persist_exists(GraphWindSpeed)){
    persist_read_data(GraphWindSpeed, s_graph_wind_speed, sizeof(s_graph_wind_speed));
  } 
  if(persist_exists(GraphPrecipType)){
    persist_read_data(GraphPrecipType, s_graph_precip_type, sizeof(s_graph_precip_type));
  } 
  if(persist_exists(GraphPrecipProb)){
    persist_read_data(GraphPrecipProb, s_graph_precip_probability, sizeof(s_graph_precip_probability));
  } 
  if(persist_exists(DailyHighs)){
    persist_read_data(DailyHighs, s_daily_highs, sizeof(s_daily_highs));
  } 
  if(persist_exists(DailyLows)){
    persist_read_data(DailyLows, s_daily_lows, sizeof(s_daily_lows));
  } 
  if(persist_exists(DayMarkers)){
    persist_read_data(DayMarkers, s_day_markers, sizeof(s_day_markers));
  } 
  if(persist_exists(DaysOfTheWeek)){
    persist_read_data(DaysOfTheWeek, s_days_of_the_week, sizeof(s_days_of_the_week));
  }
  if(persist_exists(CurrentTemperature)){
    persist_read_string(CurrentTemperature, s_current_temperature_text, sizeof(s_current_temperature_text));
  } else {
    s_current_temperature_text[0] = ' ';
    s_current_temperature_text[1] = '\0';
  }
  if (persist_exists(HorizonDays)) {
    persist_read_data(HorizonDays, &s_horizon_days, 1);
    if (s_horizon_days < 1 || s_horizon_days > 6) s_horizon_days = 6;
  } else {
    s_horizon_days = 6;
  }

  APP_LOG(APP_LOG_LEVEL_INFO, "Window load: persist temp=%d persist_day_markers=%d has_weather=%d",
      persist_exists(GraphTemperature) ? (int)s_graph_temperature[0] : -1,
      persist_exists(DayMarkers) ? (int)s_day_markers[0] : -1,
      has_weather_data() ? 1 : 0);

  //declare fonts
  s_big_font = fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT);
  s_current_temperature_font = fonts_get_system_font(FONT_KEY_GOTHIC_28);
  s_medium_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_small_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_small_font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  s_tiny_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_TYPE_WRITER_8));

  // create moon layer
  moon_layer = moon_layer_create(GPoint(bounds.size.w/2-4, 46));
  moon_layer_set_hemisphere(moon_layer, MoonLayerHemisphereNorthern);
  // moon_layer_set_border_color(moon_layer, GColorDarkGray);
  layer_add_child(window_layer, moon_layer_get_layer(moon_layer));

  //create battery bar
  s_battery_layer = battery_bar_layer_create();
  battery_bar_set_position(GPoint(0, 5));
  battery_bar_set_colors(GColorWhite, GColorDarkGray, GColorDarkGray, GColorWhite);
  battery_bar_set_percent_hidden(true);
  layer_add_child(window_layer, s_battery_layer);

  //create bluetooth indicator
  s_bluetooth_layer = bluetooth_layer_create();
  bluetooth_set_position(GPoint(6, 4));
  bluetooth_vibe_disconnect(false);
  bluetooth_vibe_connect(false);
  //void bluetooth_set_colors(GColor connected_circle, GColor connected_icon, GColor disconnected_circle, GColor disconnected_icon);
  bluetooth_set_colors(GColorBlack, GColorWhite, GColorDarkGray, GColorClear);
  layer_add_child(window_layer, s_bluetooth_layer);

  // create time text layer
  s_time_layer = text_layer_create(GRect(0, -8, bounds.size.w-4, bounds.size.h));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_text_color(s_time_layer, GColorWhite);
  text_layer_set_font(s_time_layer, s_big_font);
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_time_layer));

  //create date layer
  s_full_date_layer = text_layer_create(GRect(0, 34, bounds.size.w-4, 24));
  text_layer_set_background_color(s_full_date_layer, GColorClear);
  // text_layer_set_text(s_full_date_layer, "--");
  text_layer_set_text_color(s_full_date_layer, GColorWhite);
  text_layer_set_font(s_full_date_layer, s_medium_font);
  text_layer_set_text_alignment(s_full_date_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_full_date_layer));

  //create current temperature (show "Awaiting data" when no weather yet)
  s_current_temperature_layer = text_layer_create(GRect(0, 12, bounds.size.w/3, 48));
  text_layer_set_background_color(s_current_temperature_layer, GColorClear);
  text_layer_set_text(s_current_temperature_layer,
      has_weather_data() ? s_current_temperature_text : WEATHER_AWAITING_STR);
  text_layer_set_text_color(s_current_temperature_layer, GColorWhite);
  text_layer_set_font(s_current_temperature_layer, s_current_temperature_font);
  text_layer_set_text_alignment(s_current_temperature_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_current_temperature_layer));

  //create current high low layer
  s_today_high_and_low_layer = text_layer_create(GRect(0, 40, bounds.size.w/3 + 6, 24));
  text_layer_set_background_color(s_today_high_and_low_layer, GColorClear);
  //  text_layer_set_text(s_today_high_and_low_layer, s_today_high_and_low_text);
  text_layer_set_text_color(s_today_high_and_low_layer, GColorWhite);
  text_layer_set_font(s_today_high_and_low_layer, s_small_font);
  text_layer_set_text_alignment(s_today_high_and_low_layer, GTextAlignmentCenter);
  if (has_weather_data()) {
    snprintf(s_today_high_and_low_text, sizeof(s_today_high_and_low_text), "%d", s_daily_highs[0]);
    strcat(s_today_high_and_low_text, "°");
    strcat(s_today_high_and_low_text, "/");
    snprintf(s_today_low, sizeof(s_today_low), "%d", s_daily_lows[0]);
    strcat(s_today_high_and_low_text, s_today_low);
    strcat(s_today_high_and_low_text, "°");
    text_layer_set_text(s_today_high_and_low_layer, s_today_high_and_low_text);
  } else {
    text_layer_set_text(s_today_high_and_low_layer, "--/--");
  }
  layer_add_child(window_layer, text_layer_get_layer(s_today_high_and_low_layer));

  // create weather layer
  GRect weather_layer_bounds = GRect(bounds.origin.x, bounds.origin.y+50, bounds.size.w, bounds.size.h-50);
  s_weather_window_layer = layer_create(weather_layer_bounds);
  layer_set_update_proc(s_weather_window_layer, s_weather_window_layer_update_proc);
  layer_add_child(window_layer, s_weather_window_layer);

  create_day_label_layers();  /* create day labels if we have persisted data */
  update_day_label_positions();
  update_day_label_visibility();

  window_set_background_color(window, GColorBlack);


}

static void main_window_unload(Window *window) {
  moon_layer_destroy(moon_layer);
  battery_bar_layer_destroy(s_battery_layer);
  bluetooth_layer_destroy(s_bluetooth_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_full_date_layer);
  text_layer_destroy(s_current_temperature_layer);
  text_layer_destroy(s_today_high_and_low_layer);
  for (int i = 0; i < 6; i++) {
    if (s_days_of_the_week_text_layers[i] != NULL) {
      text_layer_destroy(s_days_of_the_week_text_layers[i]);
      s_days_of_the_week_text_layers[i] = NULL;
    }
  }
  layer_destroy(s_weather_window_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  //instantiate appmessages
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);

  app_message_open(1024, 128);

  (void)s_graph_humidity;
  (void)s_graph_pressure;
  (void)s_current_temperature;

  // Request weather from phone on startup (short delay so connection is ready)
  app_timer_register(400, request_weather_callback, NULL);

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  // tick_timer_service_subscribe(HOUR_UNIT, hour_tick_handler);

  update_time();
  update_date();

}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}


