#include <furi.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <notification/notification_messages.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define FPS 10

typedef enum{
    EventTypeInput,
    EventTypeClockTick,
} EventType;

typedef struct{
    EventType type;
    InputEvent input;
} AppEvent;

typedef struct{
    FuriMutex* mutex;

    int8_t radius;
    int8_t posX, posY;
    int8_t dirX, dirY;
} Ball;

static void input_callback(InputEvent* i_event, void* ctx){
    FuriMessageQueue* queue = ctx;

    AppEvent event = {.type = EventTypeInput, .input = *i_event};

    furi_message_queue_put(queue, &event, FuriWaitForever);
}

static void render_callback(Canvas* canvas, void* ctx){
    Ball* ball = ctx;
    furi_mutex_acquire(ball->mutex, FuriWaitForever);

    canvas_draw_frame(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    canvas_draw_circle(canvas, ball->posX, ball->posY, ball->radius);

    furi_mutex_release(ball->mutex);
}

static void clock_tick(void* ctx){
    furi_assert(ctx);
    FuriMessageQueue* queue = ctx;

    AppEvent event = {.type = EventTypeClockTick};
    furi_message_queue_put(queue, &event, 0);
}

uint8_t init_ball_direction(){
    uint8_t random_uint8[1];
    int8_t value = 0;

    furi_hal_random_fill_buf(random_uint8, 1);
    random_uint8[0] &= 0B1;
    value = (random_uint8[0] < 1) ? -1 : 1;

    return value;
}

int32_t bounce_ball_app(void* p){
    UNUSED(p);

    AppEvent event;
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    
    Ball ball;
    ball.posX = SCREEN_WIDTH / 2;
    ball.posY = SCREEN_HEIGHT / 2;
    ball.dirX = init_ball_direction();
    ball.dirY = init_ball_direction();
    ball.radius = 4;

    ball.mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!ball.mutex){
        furi_message_queue_free(queue);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_input_callback_set(view_port, input_callback, queue);
    view_port_draw_callback_set(view_port, render_callback, &ball);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    FuriTimer* timer = furi_timer_alloc(clock_tick, FuriTimerTypePeriodic, queue);
    furi_timer_start(timer, 1000 / FPS);

    bool processing = true;

    while(processing){
        FuriStatus event_status = furi_message_queue_get(queue, &event, FuriWaitForever);
        furi_mutex_acquire(ball.mutex, FuriWaitForever);

        if(event_status == FuriStatusOk){
            if(event.type == EventTypeInput){
                if(event.input.key == InputKeyBack)
                    processing = false;
            }
            else if(event.type == EventTypeClockTick){
                // +1 and -2 to ajust for the frame
                if(ball.posX - ball.radius <= 1){
                    ball.dirX *= -1;
                    ball.posX += ball.dirX;
                    notification_message(notification, &sequence_blink_red_100);
                }
                else if(ball.posX + ball.radius >= SCREEN_WIDTH - 2){
                    ball.dirX *= -1;
                    ball.posX += ball.dirX;
                    notification_message(notification, &sequence_blink_green_100);
                }

                if(ball.posY - ball.radius <= 1){
                    ball.dirY *= -1;
                    ball.posY += ball.dirY;
                    notification_message(notification, &sequence_blink_blue_100);
                }
                else if(ball.posY + ball.radius >= SCREEN_HEIGHT - 2){
                    ball.dirY *= -1;
                    ball.posY += ball.dirY;
                    notification_message(notification, &sequence_blink_white_100);
                }

                ball.posX += ball.dirX;
                ball.posY += ball.dirY;
            }
        }

        furi_mutex_release(ball.mutex);
        view_port_update(view_port);
    }

    notification_message(notification, &sequence_reset_rgb);

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(ball.mutex);
    furi_timer_free(timer);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}