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
static TextLayer *s_time_layer, *s_full_date_layer;
static GFont s_big_font, s_medium_font, s_small_font, s_small_font_bold;

static uint8_t s_graph_temperature[144];
static uint8_t s_graph_cloud_cover[144];
static uint8_t s_graph_precip_type[144];
static uint8_t s_graph_precip_probability[144];
static uint8_t s_graph_humidity[144];
static uint8_t s_graph_pressure[144];

static uint8_t s_daily_highs[6];
static uint8_t s_daily_lows[6];

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

// static void update_date() {
//     // Get a tm structure
//   time_t temp = time(NULL);
//   struct tm *tick_time = localtime(&temp);
  
//   static char day_text[]= "Xxx";
//   static char date_text_day[] = "--";
//   static char date_text_month[] = "--";
  
//   strftime(day_text, sizeof(day_text), "%a", tick_time);
//   text_layer_set_text(s_day_layer, day_text);

//   strftime(date_text_day, sizeof(date_text_day), "%d", tick_time);
//   text_layer_set_text(s_date_layer_day, date_text_day);

//   strftime(date_text_month, sizeof(date_text_month), "%m", tick_time);
//   text_layer_set_text(s_date_layer_month, date_text_month);

//   //update moon
//   moon_layer_set_date(moon_layer, tick_time);

// }

static void update_date() {
    // Get a tm structure
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  // static char day_text[]= "Xxx";
  // static char date_text_day[] = "--";
  // static char date_text_month[] = "--";

  static char full_date_text[] = "Xxx -----";
  
  strftime(full_date_text, sizeof(full_date_text), "%a %m/%d", tick_time);
  text_layer_set_text(s_full_date_layer, full_date_text);

  //update moon
  moon_layer_set_date(moon_layer, tick_time);

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time();
}

static void in_received_handler(DictionaryIterator *iter, void *context) {

  Tuple *graph_temperature_tuple = dict_find (iter, GraphTemperature);
  if (graph_temperature_tuple) {
      memcpy(s_graph_temperature, graph_temperature_tuple->value->data, 144);
  }

  Tuple *graph_precip_type_tuple = dict_find (iter, GraphPrecipType);
  if (graph_precip_type_tuple) {
      memcpy(s_graph_precip_type, graph_precip_type_tuple->value->data, 144);
  }

  Tuple *graph_cloud_cover_tuple = dict_find (iter, GraphCloudCover);
  if (graph_cloud_cover_tuple) {
      memcpy(s_graph_cloud_cover, graph_cloud_cover_tuple->value->data, 144);
      layer_mark_dirty(s_weather_window_layer);
  }

  Tuple *daily_highs_tuple = dict_find (iter, DailyHighs);
  if (daily_highs_tuple) {
      memcpy(s_daily_highs, daily_highs_tuple->value->data, 6);
  }

  Tuple *daily_lows_tuple = dict_find (iter, DailyLows);
  if (daily_lows_tuple) {
      memcpy(s_daily_lows, daily_lows_tuple->value->data, 6);
  }

}

static void in_dropped_handler(AppMessageResult reason, void *context){
  //handle failed message
}

static void s_weather_window_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "weather window height %d", bounds.size.h);

  // draw_gradient_rect(ctx, GRect(bounds.origin.x, graphOffset, bounds.size.w, 55), GColorBlack, GColorWhite, TOP_TO_BOTTOM);
  draw_dithered_rect(ctx, GRect(bounds.origin.x, 30, bounds.size.w, 5), GColorBlack, GColorWhite, DITHER_90_PERCENT);
  draw_dithered_rect(ctx, GRect(bounds.origin.x, 35, bounds.size.w, 10), GColorBlack, GColorWhite, DITHER_80_PERCENT);
  draw_dithered_rect(ctx, GRect(bounds.origin.x, 45, bounds.size.w, 13), GColorBlack, GColorWhite, DITHER_70_PERCENT);
  draw_dithered_rect(ctx, GRect(bounds.origin.x, 58, bounds.size.w, 13), GColorBlack, GColorWhite, DITHER_40_PERCENT);
  draw_dithered_rect(ctx, GRect(bounds.origin.x, 71, bounds.size.w, 9), GColorBlack, GColorWhite, DITHER_25_PERCENT);
  draw_dithered_rect(ctx, GRect(bounds.origin.x, 80, bounds.size.w, 5), GColorBlack, GColorWhite, DITHER_10_PERCENT);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 80, bounds.size.w, 5), GColorBlack, GColorWhite, DITHER_10_PERCENT);
  // draw_random_gradient_rect(ctx, GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, bounds.size.h), GColorWhite, GColorBlack, TOP_TO_BOTTOM);
  graphics_context_set_stroke_width(ctx, 1);

  int graphOffset = 28;
  int pixelOffset = 0;

  for (int i = 0; i < 144; i++){

    if (i%2 == 0) {
      pixelOffset = 0;
    } else {
      pixelOffset = 2;
    }

    if (i < 143){
      //precip type
      if(s_graph_precip_type[i] == 0) {
        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_draw_line(ctx, GPoint(i,graphOffset), GPoint(i,graphOffset+s_graph_temperature[i]));
      } else if (s_graph_precip_type[i] == 1) {
        for (int ii = 0; ii < s_graph_temperature[i]; ii++){
          if(ii%4 == 0){
            graphics_context_set_stroke_color(ctx, GColorWhite);
          } else {
            graphics_context_set_stroke_color(ctx, GColorBlack);
          }
          graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset));
        }
      } else if (s_graph_precip_type[i] == 2) {
        for (int ii = 0; ii < s_graph_temperature[i]; ii++){
          if(ii%4 == 0){
            graphics_context_set_stroke_color(ctx, GColorWhite);
            graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset-1));
            graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset));
          } else {
            graphics_context_set_stroke_color(ctx, GColorBlack);
            graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset));
          }

        }
      } 
      //temp line black padding from precip type
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_line(ctx, GPoint(i, graphOffset + s_graph_temperature[i]), GPoint(i, graphOffset + s_graph_temperature[i] - 4));
      graphics_draw_line(ctx, GPoint(i, graphOffset + s_graph_temperature[i]), GPoint(i, graphOffset + s_graph_temperature[i] - 4));
      // graphics_context_set_stroke_color(ctx, GColorBlack);
      // graphics_context_set_stroke_width(ctx, 3);
      // graphics_draw_line(ctx, GPoint(i-2, s_graph_temperature[i]+graphOffset-4), GPoint(i+2, s_graph_temperature[i]+graphOffset-4));
    }

    if (s_graph_cloud_cover[i] > 0) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, GPoint(i, graphOffset-4), GPoint(i, graphOffset-4-s_graph_cloud_cover[i]));
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
}


static void main_window_load(Window *window) {
  // Get information about the main window
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  s_big_font = fonts_get_system_font(FONT_KEY_BITHAM_42_LIGHT);
  s_medium_font = fonts_get_system_font(FONT_KEY_GOTHIC_18);
  s_small_font = fonts_get_system_font(FONT_KEY_GOTHIC_14);
  s_small_font_bold = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);


  // create moon layer
  moon_layer = moon_layer_create(GPoint(bounds.size.w/2-4, 44));
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
  s_full_date_layer = text_layer_create(GRect(0, 32, bounds.size.w-4, 24));
  text_layer_set_background_color(s_full_date_layer, GColorClear);
  // text_layer_set_text(s_full_date_layer, "--");
  text_layer_set_text_color(s_full_date_layer, GColorWhite);
  text_layer_set_font(s_full_date_layer, s_medium_font);
  text_layer_set_text_alignment(s_full_date_layer, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(s_full_date_layer));

  GRect weather_layer_bounds = GRect(bounds.origin.x, bounds.origin.y+50, bounds.size.w, bounds.size.h-50);
  s_weather_window_layer = layer_create(weather_layer_bounds);
  layer_set_update_proc(s_weather_window_layer, s_weather_window_layer_update_proc);
  layer_add_child(window_layer, s_weather_window_layer);


  window_set_background_color(window, GColorBlack);


}

static void main_window_unload(Window *window) {
  battery_bar_layer_destroy(s_battery_layer);
  bluetooth_layer_destroy(s_bluetooth_layer);
  layer_destroy(s_weather_window_layer);
  text_layer_destroy(s_time_layer);
  text_layer_destroy(s_full_date_layer);
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

  app_message_open(1024, 1024);


  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

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


