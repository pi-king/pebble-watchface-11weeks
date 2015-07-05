#include <pebble.h>
#include "main.h"

#include "numbers.h"
#include "letters.h"

#include "calendar_layer.h"
#include "sec_layer.h"
#include "watch_battery_layer.h"
#include "phone_battery_layer.h"

#define KEY_PHONE_BATTERY    8

static AppSync s_sync;
static uint8_t* s_sync_buffer;

static Window* s_main_window;
static Layer* s_calendar_layer;
static Layer* s_sec_layer;
static Layer* s_watch_battery_layer;
static Layer* s_phone_battery_layer;

static time_t s_now_t;
static struct tm s_now_tm;

static void init();
static void deinit();
static void main_window_load(Window*);
static void main_window_unload(Window*);
static void tick_handler(struct tm*, TimeUnits);
static void update_time(bool is_init);
static void battery_handler(BatteryChargeState charge_state);

static void init_sync();
static void deinit_sync();
static void sync_changed_handler(const uint32_t key, const Tuple *new_tuple, const Tuple *old_tuple, void *context);
static void sync_error_handler(DictionaryResult dict_error, AppMessageResult app_message_error, void *context);

static void init() {
  // init numbers
  numbers_create();
  // init letters
  letters_create();
  // initialize main window instance
  s_main_window = window_create();
  
  // set the window handlers
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // set background color
  window_set_background_color(s_main_window, GColorBlack);
  
  // register the time tick service
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  
  // register the battery service
  battery_state_service_subscribe(battery_handler);
  
  // init AppSync
  init_sync();
  
  // show window on the watch, with animated = true
  window_stack_push(s_main_window, true);
}

static void deinit() {
  deinit_sync();
  window_destroy(s_main_window);
  letters_destroy();
  numbers_destroy();
}

static void main_window_load(Window* window) {
  // create calendar layer
  calendar_layer_create();
  s_calendar_layer = calendar_layer_get_layer();
  layer_add_child(window_get_root_layer(s_main_window), s_calendar_layer);
  // create sec layer
  sec_layer_create();
  s_sec_layer = sec_layer_get_layer();
  layer_add_child(window_get_root_layer(s_main_window), s_sec_layer);
  // create watch battery layer
  watch_battery_layer_create();
  s_watch_battery_layer = watch_battery_layer_get_layer();
  layer_add_child(window_get_root_layer(s_main_window), s_watch_battery_layer);
  // create phone battery layer
  phone_battery_layer_create();
  s_phone_battery_layer = phone_battery_layer_get_layer();
  layer_add_child(window_get_root_layer(s_main_window), s_phone_battery_layer);
  
  // display time from the beginning
  update_time(true);
  // display battery status from the beginning
  battery_handler(battery_state_service_peek());
}

static void main_window_unload(Window* window) {
  // destroy calendar layer
  calendar_layer_destroy();
  // destroy sec layer
  sec_layer_destroy();
  // destroy watch battery layer
  watch_battery_layer_destroy();
  // destroy phone battery layer
  phone_battery_layer_destroy();
}

static void tick_handler(struct tm* tick_time, TimeUnits units_changed) {
  update_time(false);
}

static void update_time(bool is_init) {
  time(&s_now_t);
  struct tm* st = localtime(&s_now_t);
  memcpy(&s_now_tm, st, sizeof(s_now_tm));
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Now: %d-%d-%d %d:%d:%d",
          s_now_tm.tm_year + 1900, s_now_tm.tm_mon + 1, s_now_tm.tm_mday, s_now_tm.tm_hour, s_now_tm.tm_min, s_now_tm.tm_sec);
  
  sec_layer_update_time(&s_now_t, &s_now_tm);
  layer_mark_dirty(s_sec_layer);
  if (is_init || s_now_tm.tm_sec == 0) {
    calendar_layer_update_time(&s_now_t, &s_now_tm);
    layer_mark_dirty(s_calendar_layer);
  }
}

static void battery_handler(BatteryChargeState charge_state) {
  watch_battery_layer_update(charge_state);
  layer_mark_dirty(s_watch_battery_layer);
}

static void init_sync() {
  // setup AppSync
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
  
  // setup initial value
  Tuplet initial_values[] = {
    TupletInteger(KEY_PHONE_BATTERY, LEVEL_UNKNOWN),
  };
  
  // create buffer
  uint32_t size = dict_calc_buffer_size_from_tuplets(initial_values, ARRAY_LENGTH(initial_values));
  s_sync_buffer = malloc(size * sizeof(uint8_t));
  
  // Begin using AppSync
  app_sync_init(&s_sync, s_sync_buffer, size, initial_values, ARRAY_LENGTH(initial_values), sync_changed_handler, sync_error_handler, NULL);
}

static void deinit_sync() {
  app_sync_deinit(&s_sync);
}

static void sync_changed_handler(const uint32_t key, const Tuple *new_tuple, const Tuple *old_tuple, void *context) {
  switch (key) {
    case KEY_PHONE_BATTERY:
    {
      uint8_t state = (uint8_t) new_tuple->value->int32;
      APP_LOG(APP_LOG_LEVEL_DEBUG, "get battery state: %d, %d", (uint8_t)new_tuple->value->int32 & CHARGING_MASK, (uint8_t)new_tuple->value->int32 & LEVEL_MASK);
      if (state == BATTERY_API_UNSUPPORTED) {
        layer_set_hidden(s_phone_battery_layer, true);
      } else {
        layer_set_hidden(s_phone_battery_layer, false);
        phone_battery_layer_update(state);
        layer_mark_dirty(s_phone_battery_layer);
      }
    }
    break;
    
    default:
    break;
  }
}

static void sync_error_handler(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "phone battery info sync error!");
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}