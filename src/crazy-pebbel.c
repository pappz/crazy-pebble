#include <pebble.h>

#define RETRY_SEND_MS 100
#define RESEND_LIMIT 50
#define SQRT_MAGIC_F 0x5f3759df

#define BTN_DOWN 0
#define BTN_SELECT 1
#define BTN_UP 2
#define BTN_DOWN_LONG 3
#define BTN_SELECT_LONG 4
#define BTN_UP_LONG 5

static Window *window;
static Layer *pointer_layer;

static AppTimer *timer;

typedef struct Pointer {
    double x;
    double y;
    double radius;
} Pointer;

static Pointer pointer;

//For the coords calculation
typedef struct WindowDiv {
	float w;
	float whalf;
	float h;
	float hhalf;
} WindowDiv;

static WindowDiv WDiv;

static AccelData accelSent;

static int btn = -1;
static int btnSent = -1;

/*
 *  http://www.codeproject.com/Articles/69941/Best-Square-Root-Method-Algorithm-Function-Precisi
 */
float my_sqrt(const float x) {
    const float xhalf = 0.5f*x;
    union // get bits for floating value
    {
        float x;
        int i;
    } u;
    u.x = x;
    u.i = SQRT_MAGIC_F - (u.i >> 1);  // gives initial guess y0
    return x*u.x*(1.5f - xhalf*u.x*u.x);// Newton step, repeating increases accuracy 
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = BTN_UP;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = BTN_SELECT;
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
	btn = BTN_DOWN;
}

static void long_up_click_handler(ClickRecognizerRef recognizer, void *context) {
	btn = BTN_UP_LONG;
}

static void long_select_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = BTN_SELECT_LONG;
}

static void long_down_click_handler(ClickRecognizerRef recognizer, void *context) {
    btn = BTN_DOWN_LONG;
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

    if ((accel.x-accelSent.x) > -RESEND_LIMIT &&
        (accel.x-accelSent.x) <  RESEND_LIMIT &&
        (accel.y-accelSent.y) > -RESEND_LIMIT &&
        (accel.y-accelSent.y) <  RESEND_LIMIT &&
        (accel.z-accelSent.z) > -RESEND_LIMIT &&
        (accel.z-accelSent.z) <  RESEND_LIMIT &&
        btn == btnSent)
    {
        app_timer_register(20, sendData, NULL);
        return;
    }

    accelSent.x = accel.x;
    accelSent.y = accel.y;
    accelSent.z = accel.z;
    btnSent = btn;

    float d = my_sqrt((float) (accel.x * accel.x + accel.y * accel.y + accel.z * accel.z));
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "coord: %lf", d);
    //disc.pos.x =(double) accel.x/(double) 1500 * (window_frame.size.w-2*disc.radius)+disc.radius+window_frame.size.w/2;
    //disc.pos.y =-(double) accel.y/(double) 1500 * (window_frame.size.h-2*disc.radius)+disc.radius+window_frame.size.h/2;
    
    pointer.x = accel.x / d * WDiv.w + WDiv.whalf;
    pointer.y = accel.y*-1 / d * WDiv.h + WDiv.hhalf;

    layer_mark_dirty(pointer_layer);

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

static void pointer_layer_update_callback(Layer *me, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_circle(ctx, GPoint(pointer.x, pointer.y), pointer.radius);
}

static void circle_layer_update_callback(Layer *me, GContext* ctx) {
    GRect bounds = layer_get_bounds(me);
    graphics_draw_circle(ctx, GPoint(bounds.size.w/2, bounds.size.h/2), bounds.size.w/2);
}

static void pointer_init() {
    pointer.x = 0;
    pointer.y = 0;
    pointer.radius = 3;
}

static void app_message_init(void) {
    app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);
    app_message_open(16, 64);
    app_message_register_outbox_failed(out_failed_handler);
    app_message_register_outbox_sent(out_sent_handler);
}

static void window_load(Window *window) {
    Layer *window_layer = window_get_root_layer(window);
    GRect frame = layer_get_frame(window_layer);

    pointer_layer = layer_create(frame);
  
    layer_set_update_proc(pointer_layer, pointer_layer_update_callback);
    layer_add_child(window_layer, pointer_layer);
  
    pointer_init();
    
    //For the coords calculation
    WDiv.w = (frame.size.w-2*pointer.radius)/2;
    WDiv.whalf = frame.size.w/2;
    WDiv.h = (frame.size.h-2*pointer.radius)/2;
    WDiv.hhalf = frame.size.h/2;

    //The background circle
    Layer *circle_layer = layer_create(frame);
    layer_set_update_proc(circle_layer, &circle_layer_update_callback);
    layer_add_child(window_layer, circle_layer);
}

static void window_unload(Window *window){
    layer_destroy(pointer_layer);
}

static void init(void) {
	app_message_init();
    window = window_create();
    window_set_click_config_provider(window, click_config_provider);
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
    });
  
    window_stack_push(window, true);
  
    accel_data_service_subscribe(0, NULL);

    accelSent = (AccelData) { .x = 0, .y = 0, .z = 0 };
 
    sendData(NULL);
}

static void deinit(void) {
    window_destroy(window);
    accel_data_service_unsubscribe();
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
