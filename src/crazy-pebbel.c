#include <pebble.h>

#define RETRY_SEND_MS 100

static Window *window;
static GRect window_frame;
static Layer *disc_layer;
static TextLayer *text_layer;

static AppTimer *timer;

typedef struct Vec2d {
    double x;
    double y;
} Vec2d;

typedef struct Disc {
    Vec2d pos;
    double radius;
} Disc;

static Disc disc;
static int btn = -1;


static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = 2;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = 1;
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	btn = 0;
}

static void long_up_click_handler(ClickRecognizerRef recognizer, void *context) {
	btn = 5;
}

static void long_select_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = 4;
}

static void long_down_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = 3;
}

static void click_config_provider(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
    window_long_click_subscribe(BUTTON_ID_SELECT, 0, long_select_click_handler, NULL);
    window_long_click_subscribe(BUTTON_ID_UP, 0, long_up_click_handler, NULL);
    window_long_click_subscribe(BUTTON_ID_DOWN, 0, long_down_click_handler, NULL);
}

static void sendData(void *data) {	
    AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
    DictionaryIterator *iter;
    
    if (app_message_outbox_begin(&iter) != APP_MSG_OK) {
  	    timer = app_timer_register(RETRY_SEND_MS, sendData, NULL);
  	    return;
    }

    accel_service_peek(&accel);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %f",(double) accel.x/(double) 4000);

    //disc_apply_accel(disc, accel);
    //disc.pos.x =(double) accel.x/(double) 1500 * (window_frame.size.w-2*disc.radius)+disc.radius+window_frame.size.w/2;
    //disc.pos.y =-(double) accel.y/(double) 1500 * (window_frame.size.h-2*disc.radius)+disc.radius+window_frame.size.h/2;
    
    //layer_mark_dirty(disc_layer);

	dict_write_int(iter, 1, &accel.x, 2, 1);
	dict_write_int(iter, 2, &accel.y, 2 ,1);
	dict_write_int(iter, 3, &accel.z, 2 ,1);
	dict_write_uint8(iter, 0, btn);
	btn = -1;
	app_message_outbox_send();
}

void out_sent_handler(DictionaryIterator *sent, void *context) {
	sendData(NULL);
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
	timer = app_timer_register(RETRY_SEND_MS, sendData, NULL);
}

static void disc_layer_update_callback(Layer *me, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, GPoint(disc.pos.x, disc.pos.y), disc.radius);
}

static void disc_init() {
    GRect frame = window_frame;
    disc.pos.x = frame.size.w/2;
    disc.pos.y = frame.size.h/2;
    disc.radius = 3;
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect frame = window_frame = layer_get_frame(window_layer);
    disc_layer = layer_create(frame);
  
    layer_set_update_proc(disc_layer, disc_layer_update_callback);
    layer_add_child(window_layer, disc_layer);
  
    disc_init();
  
    //layer_add_child(window_layer, text_layer_get_layer(text_layer));
}

static void window_unload(Window *window) {
    text_layer_destroy(text_layer);
}

static void app_message_init(void) {
    app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
    app_message_open(16, 64);
    app_message_register_outbox_failed(out_failed_handler);
    app_message_register_outbox_sent(out_sent_handler);
}

static void init(void) {
    window = window_create();
    window_set_click_config_provider(window, click_config_provider);
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
  
    window_stack_push(window, true);
  
    accel_data_service_subscribe(0, NULL);
  
    sendData(NULL);
}

static void deinit(void) {
    window_destroy(window);
}

int main(void) {
	app_message_init();
	init();

	APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);
	
	app_event_loop();
	deinit();
}
