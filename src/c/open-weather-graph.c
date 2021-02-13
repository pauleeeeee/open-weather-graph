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
static char s_daily_text_highs[7][4];
static char s_daily_text_lows[7][4];
static char s_daily_text_highs_and_lows[7][20];
static char s_today_high_and_low_text[16];
static char s_today_high[4];
static char s_today_low[4];
static int s_current_temperature;
static char s_current_temperature_text[24];
static uint8_t s_day_markers[6];
static uint8_t s_days_of_the_week[7];

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

// static void hour_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
//   int fetch = 1;
//   DictionaryIterator *iter;
//   app_message_outbox_begin(&iter);
//   dict_write_int(iter, GetWeather, &fetch, sizeof(int), false);
//   app_message_outbox_send();
// }

static void in_received_handler(DictionaryIterator *iter, void *context) {

  Tuple *graph_temperature_tuple = dict_find (iter, GraphTemperature);
  if (graph_temperature_tuple) {
      memcpy(s_graph_temperature, graph_temperature_tuple->value->data, 144);
      persist_write_data(GraphTemperature, s_graph_temperature, 144);
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
    // s_current_temperature = current_temperature_tuple->value->int32;
    // snprintf(s_current_temperature_text, sizeof(s_current_temperature_text), "%d", s_current_temperature);
    strncpy(s_current_temperature_text, current_temperature_tuple->value->cstring, sizeof(s_current_temperature_text));
    text_layer_set_text(s_current_temperature_layer, s_current_temperature_text);
    persist_write_string(CurrentTemperature, s_current_temperature_text);
  }

}

static void in_dropped_handler(AppMessageResult reason, void *context){
  //handle failed message
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

static void s_weather_window_layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);

  APP_LOG(APP_LOG_LEVEL_DEBUG, "weather window height %d", bounds.size.h);

  int graphOffset = 30;
  int pixelOffset = 0;

  draw_gradient_rect(ctx, GRect(bounds.origin.x, graphOffset+10, bounds.size.w, 55), GColorWhite, GColorBlack, TOP_TO_BOTTOM);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 30, bounds.size.w, 5), GColorBlack, GColorWhite, DITHER_90_PERCENT);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 35, bounds.size.w, 10), GColorBlack, GColorWhite, DITHER_80_PERCENT);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 45, bounds.size.w, 13), GColorBlack, GColorWhite, DITHER_70_PERCENT);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 58, bounds.size.w, 13), GColorBlack, GColorWhite, DITHER_40_PERCENT);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 71, bounds.size.w, 9), GColorBlack, GColorWhite, DITHER_25_PERCENT);
  // draw_dithered_rect(ctx, GRect(bounds.origin.x, 80, bounds.size.w, 5), GColorBlack, GColorWhite, DITHER_10_PERCENT);

  graphics_context_set_stroke_width(ctx, 1);



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
          if(ii % 4 == 0){
            graphics_context_set_stroke_color(ctx, GColorWhite);
          } else {
            graphics_context_set_stroke_color(ctx, GColorBlack);
          }
          graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset));
        }
      } else if (s_graph_precip_type[i] == 2) {
        for (int ii = 0; ii < s_graph_temperature[i]; ii++){
          if(ii % 4 == 0){
              graphics_context_set_stroke_color(ctx, GColorWhite);
              graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset-1));
              graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset));
          } else {
            graphics_context_set_stroke_color(ctx, GColorBlack);
            graphics_draw_pixel(ctx, GPoint(i,ii+graphOffset+pixelOffset));
          }
        }
        graphics_context_set_stroke_color(ctx, GColorBlack);
        //float prob = s_graph_precip_probability[i] / 10;
        //int pos = round(s_graph_temperature[i] * prob);
        graphics_draw_line(ctx, GPoint(i, graphOffset + s_graph_precip_probability[i]), GPoint(i, graphOffset + s_graph_temperature[i]));
      }

      //temp line black padding from precip type
      graphics_context_set_stroke_color(ctx, GColorBlack);
      graphics_draw_line(ctx, GPoint(i, graphOffset + s_graph_temperature[i]), GPoint(i, graphOffset + s_graph_temperature[i] - 4));
      graphics_draw_line(ctx, GPoint(i, graphOffset + s_graph_temperature[i]), GPoint(i, graphOffset + s_graph_temperature[i] - 4));
      // graphics_context_set_stroke_color(ctx, GColorBlack);
      // graphics_context_set_stroke_width(ctx, 3);
      // graphics_draw_line(ctx, GPoint(i-2, s_graph_temperature[i]+graphOffset-4), GPoint(i+2, s_graph_temperature[i]+graphOffset-4));
        
      //draw wind speeed line  
      //offset is 7
      //7 is top 16 is 
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_draw_line(ctx, GPoint(i,s_graph_wind_speed[i]+4), GPoint(i+1,s_graph_wind_speed[i+1]+4));

    }

    if (s_graph_cloud_cover[i] > 0) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
      graphics_context_set_stroke_width(ctx, 1);
      graphics_draw_line(ctx, GPoint(i, graphOffset), GPoint(i, graphOffset-s_graph_cloud_cover[i]));
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


  //draw day markers, day of the week, and daily highs / lows
  if (s_day_markers[0] > 0) {
    for (int i = 0; i < 6; i++) {
      graphics_draw_line(ctx, GPoint(s_day_markers[i], graphOffset+5), GPoint(s_day_markers[i], 124-graphOffset));      
    }
  }


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
    strncpy(s_current_temperature_text, " ", sizeof(s_current_temperature_text));
  }

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

  //create current temperature
  s_current_temperature_layer = text_layer_create(GRect(0, 12, bounds.size.w/3, 48));
  text_layer_set_background_color(s_current_temperature_layer, GColorClear);
  text_layer_set_text(s_current_temperature_layer, s_current_temperature_text);
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
  snprintf( s_today_high_and_low_text, sizeof(s_today_high_and_low_text), "%d", s_daily_highs[0] );
  snprintf( s_today_low, sizeof(s_today_low), "%d", s_daily_lows[0] );
  strcat(s_today_high_and_low_text, "°");
  strcat(s_today_high_and_low_text, "/");
  strcat(s_today_high_and_low_text, s_today_low);
  strcat(s_today_high_and_low_text, "°");
  text_layer_set_text(s_today_high_and_low_layer, s_today_high_and_low_text);

  layer_add_child(window_layer, text_layer_get_layer(s_today_high_and_low_layer));

  // create weather layer
  GRect weather_layer_bounds = GRect(bounds.origin.x, bounds.origin.y+50, bounds.size.w, bounds.size.h-50);
  s_weather_window_layer = layer_create(weather_layer_bounds);
  layer_set_update_proc(s_weather_window_layer, s_weather_window_layer_update_proc);
  layer_add_child(window_layer, s_weather_window_layer);

  if (s_day_markers[0] > 0) {
    for (int i = 0; i < 6; i++) {

      s_days_of_the_week_text_layers[i] = text_layer_create(GRect(s_day_markers[i], 118-28, 24, 60));
      text_layer_set_background_color(s_days_of_the_week_text_layers[i], GColorClear);
      text_layer_set_text_color(s_days_of_the_week_text_layers[i], GColorWhite);
      text_layer_set_font(s_days_of_the_week_text_layers[i], s_tiny_font);
      text_layer_set_text_alignment(s_days_of_the_week_text_layers[i], GTextAlignmentCenter);

      //convert ints to strings
      snprintf( s_daily_text_highs[i], sizeof(s_daily_text_highs[i]), "%d", s_daily_highs[i] );
      snprintf( s_daily_text_lows[i], sizeof(s_daily_text_lows[i]), "%d", s_daily_lows[i] );
      
      //concatenate the day string, highs, and lows,
      strncpy(s_daily_text_highs_and_lows[i], getDayString(s_days_of_the_week[i]), sizeof(s_daily_text_highs_and_lows[i]));
      strcat(s_daily_text_highs_and_lows[i], "\n");
      strcat(s_daily_text_highs_and_lows[i], s_daily_text_highs[i]);
      strcat(s_daily_text_highs_and_lows[i], "\n");
      strcat(s_daily_text_highs_and_lows[i], s_daily_text_lows[i]);

      //plus one on the i to shift one day forward   
      text_layer_set_text(s_days_of_the_week_text_layers[i], s_daily_text_highs_and_lows[i + 1]);
      layer_add_child(s_weather_window_layer, text_layer_get_layer(s_days_of_the_week_text_layers[i]));
      
    }
  }

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

  app_message_open(1024, 128);


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


