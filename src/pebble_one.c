/*
 * Copyright (c) 2015 Szabolcs (Sam) Ban
 *
 * fork of PebbleONE code from Bert Freudenberg
 * see https://github.com/bertfreudenberg/PebbleONE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <pebble.h>
#include <time.h>

#define CENTER_X    71
#define CENTER_Y    71
#define DOTS_RADIUS 67
#define DOTS_SIZE    4
#define HOUR_RADIUS 40
#define MIN_RADIUS  60
#define SEC_RADIUS  62

static TextLayer *battp_layer;
#define BATTP_BUFFER_BYTES 32
static char battp_buffer[BATTP_BUFFER_BYTES];

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

static Window *window;
static Layer *background_layer;
static Layer *hands_layer;
static Layer *date_layer;

static GBitmap *logo;
static BitmapLayer *logo_layer;

static GBitmap *battery_images[22];
static BitmapLayer *battery_layer;

static GBitmap *bluetooth_images[4];
static BitmapLayer *bluetooth_layer;

static struct tm *now = NULL;
static int date_wday = -1;
static int date_mday = -1;
static bool was_connected = true;

static GFont *font;
#define DATE_BUFFER_BYTES 32
static char date_buffer[DATE_BUFFER_BYTES];

const GPathInfo HOUR_POINTS = {
  8,
  (GPoint []) {
    { 6,-37},
    { 5,-39},
    { 3,-40},
    {-3,-40},
    {-5,-39},
    {-6,-37},
    {-6,  0},
    { 6,  0},
  }
};
const GPathInfo HOUR_IN_POINTS = {
  8,
  (GPoint []) {
    { 3,-36},
    { 2,-37},
    { 1,-38},
    {-1,-38},
    {-2,-37},
    {-3,-36},
    {-3,  0},
    { 3,  0},
  }
};
static GPath *hour_path;
static GPath *hour_in_path;

const GPathInfo MIN_POINTS = {
  6,
  (GPoint []) {
    { 5,-57},
    { 3,-61},
    {-3,-61},
    {-5,-57},
    {-5,  0},
    { 5,  0},
  }
};
static GPath *min_path;

const GPathInfo SEC_POINTS = {
  4,
  (GPoint []) {
    { 2,  0},
    { 2,-61},
    {-2,-61},
    {-2,  0},
  }
};
static GPath *sec_path;

const char WEEKDAY_NAMES[7][5] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

const char TIMECHARS[12][2] = {"4","5","6","7","8","9","10","11","12","1","2","3"};

static void background_layer_update_callback(Layer *layer, GContext* ctx) {
    graphics_context_set_fill_color(ctx, GColorWhite);

  int32_t angle = 0;
  for (int i = 0; i < 12; i++) {
    angle += TRIG_MAX_ANGLE / 12;
    GPoint pos = GPoint(
      CENTER_X + DOTS_RADIUS * cos_lookup(angle) / TRIG_MAX_RATIO,
      CENTER_Y + DOTS_RADIUS * sin_lookup(angle) / TRIG_MAX_RATIO);
    if (i == 11 || i == 5) {
      graphics_draw_text(ctx,
        TIMECHARS[i],
        font,
        GRect(pos.x-5, pos.y-16, 32, 32),
        GTextOverflowModeTrailingEllipsis,
        GTextAlignmentLeft,
        NULL);
    } else {
         graphics_fill_circle(ctx, pos, DOTS_SIZE);
    }
  }
}

static void hands_layer_update_callback(Layer *layer, GContext* ctx) {
#if SCREENSHOT
  now->tm_hour = 10;
  now->tm_min = 9;
  now->tm_sec = 36;
#endif

  GPoint center = GPoint(CENTER_X, CENTER_Y);

  // hours and minutes
//  int32_t hour_angle = TRIG_MAX_ANGLE * (now->tm_hour * 5 + now->tm_min / 12) / 60;
  int32_t hour_angle = (TRIG_MAX_ANGLE * (((now->tm_hour % 12) * 6) + (now->tm_min / 10))) / (12 * 6);
  int32_t min_angle = TRIG_MAX_ANGLE * now->tm_min / 60;
  gpath_rotate_to(hour_path, hour_angle);
  gpath_rotate_to(hour_in_path, hour_angle);
  gpath_rotate_to(min_path, min_angle);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, hour_path);
  gpath_draw_outline(ctx, hour_path);
  graphics_context_set_fill_color(ctx, GColorBlack);
  gpath_draw_filled(ctx, hour_in_path);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_draw_circle(ctx, center, DOTS_SIZE+4);
  gpath_draw_filled(ctx, min_path);
  gpath_draw_outline(ctx, min_path);
  graphics_fill_circle(ctx, center, DOTS_SIZE+3);

  // center dot
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_circle(ctx, center, DOTS_SIZE);
}

static void date_layer_update_callback(Layer *layer, GContext* ctx) {

#if SCREENSHOT
  now->tm_wday = 0;
  now->tm_mday = 25;
#endif

  graphics_context_set_text_color(ctx, GColorWhite);
  GRect info_rect = GRect(0, -2, 144, 32);
  // weekday
  //strftime(date_buffer, DATE_BUFFER_BYTES, "%a", now);
  graphics_draw_text(ctx,
    WEEKDAY_NAMES[now->tm_wday],
    font,
    info_rect,
    GTextOverflowModeWordWrap,
    GTextAlignmentLeft,
    NULL);

  static char s_time_text[] = "00:00"; 
  strftime(s_time_text, sizeof(s_time_text), "%R", now);
  graphics_draw_text(ctx,
    s_time_text,
    font,
    info_rect,
    GTextOverflowModeWordWrap,
    GTextAlignmentCenter,
    NULL);

  // day of month
  strftime(date_buffer, DATE_BUFFER_BYTES, "%e", now);
  graphics_draw_text(ctx,
    date_buffer,
    font,
    info_rect,
    GTextOverflowModeWordWrap,
    GTextAlignmentRight,
    NULL);

  date_wday = now->tm_wday;
  date_mday = now->tm_mday;
}

static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
  now = tick_time;
  layer_mark_dirty(hands_layer);
  if (now->tm_wday != date_wday || now->tm_mday != date_mday) {
    layer_mark_dirty(date_layer);
  }
}

static void lost_connection_warning(void *);

static void handle_bluetooth(bool connected) {
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_images[connected ? 1 : 0]);
  if (was_connected && !connected) {
      lost_connection_warning((void*) 0);
  }
  was_connected = connected;
}

static void lost_connection_warning(void *data) {
  int count = (int) data;
  bool on_off = count & 1;
  // blink icon
  bitmap_layer_set_bitmap(bluetooth_layer, bluetooth_images[on_off ? 1 : 0]);
  layer_set_hidden(bitmap_layer_get_layer(bluetooth_layer), false);
  // buzz 3 times
  if (count < 6 && !on_off)
    vibes_short_pulse();
  if (count < 50) // blink for 15 seconds
    app_timer_register(300, lost_connection_warning, (void*) (count+1));
  else // restore bluetooth icon
    handle_bluetooth(bluetooth_connection_service_peek());
}

static void handle_battery(BatteryChargeState charge_state) {
  snprintf(battp_buffer, BATTP_BUFFER_BYTES, "%s%d%%",  charge_state.is_charging ? "+" : "", charge_state.charge_percent);
  text_layer_set_text(battp_layer, battp_buffer);
  bitmap_layer_set_bitmap(battery_layer, battery_images[
    (charge_state.is_charging ? 11 : 0) + min(charge_state.charge_percent / 10, 10)]);
}

static void handle_init() {
  time_t clock = time(NULL);
  now = localtime(&clock);
  window = window_create();
  window_stack_push(window, true /* Animated */);
  window_set_background_color(window, GColorBlack);

  date_layer = layer_create(GRect(0, 144, 144, 24));
  layer_set_update_proc(date_layer, &date_layer_update_callback);
  layer_add_child(window_get_root_layer(window), date_layer);

  background_layer = layer_create(GRect(0, 0, 144, 144));
  layer_set_update_proc(background_layer, &background_layer_update_callback);
  layer_add_child(window_get_root_layer(window), background_layer);

  hands_layer = layer_create(layer_get_frame(background_layer));
  layer_set_update_proc(hands_layer, &hands_layer_update_callback);
  layer_add_child(background_layer, hands_layer);

  for (int i = 0; i < 22; i++) {
    battery_images[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_0 + i);  
  }
  battery_layer = bitmap_layer_create(GRect(144-16-3, 3, 16, 10));
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(battery_layer));

  for (int i = 0; i < 2; i++)
    bluetooth_images[i] = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BLUETOOTH_OFF + i);  
  bluetooth_layer = bitmap_layer_create(GRect(66, 0, 13, 13));
  layer_add_child(background_layer, bitmap_layer_get_layer(bluetooth_layer));

  battp_layer = text_layer_create(GRect(0, 0, 32, 16));
  strcpy(battp_buffer, "");
  text_layer_set_text(battp_layer, battp_buffer);
  text_layer_set_text_color(battp_layer, GColorWhite);
  text_layer_set_background_color(battp_layer, GColorBlack);
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(battp_layer));
  
  hour_path = gpath_create(&HOUR_POINTS);
  hour_in_path = gpath_create(&HOUR_IN_POINTS);
  gpath_move_to(hour_path, GPoint(CENTER_X, CENTER_Y));
  gpath_move_to(hour_in_path, GPoint(CENTER_X, CENTER_Y));
  min_path = gpath_create(&MIN_POINTS);
  gpath_move_to(min_path, GPoint(CENTER_X, CENTER_Y));
  sec_path = gpath_create(&SEC_POINTS);
  gpath_move_to(sec_path, GPoint(CENTER_X, CENTER_Y));

  font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_20));

  tick_timer_service_subscribe(MINUTE_UNIT, &handle_tick);
  battery_state_service_subscribe(&handle_battery);
  handle_battery(battery_state_service_peek());
  bluetooth_connection_service_subscribe(&handle_bluetooth);
  handle_bluetooth(bluetooth_connection_service_peek());
}

static void handle_deinit() {
  app_message_deregister_callbacks();
  battery_state_service_unsubscribe();
  tick_timer_service_unsubscribe();
  fonts_unload_custom_font(font);
  gpath_destroy(sec_path);
  gpath_destroy(min_path);
  gpath_destroy(hour_path);
  gpath_destroy(hour_in_path);
#if DEBUG
  text_layer_destroy(debug_layer);
#endif
  layer_destroy(hands_layer);
  bitmap_layer_destroy(logo_layer);
  gbitmap_destroy(logo);
  bitmap_layer_destroy(battery_layer);
  for (int i = 0; i < 22; i++)
    gbitmap_destroy(battery_images[i]);
  bitmap_layer_destroy(bluetooth_layer);
  for (int i = 0; i < 2; i++)
    gbitmap_destroy(bluetooth_images[i]);
  layer_destroy(background_layer);
  layer_destroy(date_layer);
  window_destroy(window);
}

int main(void) {
  handle_init();
  app_event_loop();
  handle_deinit();
}
