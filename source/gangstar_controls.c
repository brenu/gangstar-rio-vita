/* Vita sticks feed the original game's synchronous input events. */
#include "gangstar_controls.h"
#include "gangstar_analog_policy.h"
#include "gangstar_pedal_policy.h"
#include "gangstar_button_policy.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include <so_util/so_util.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <stdint.h>
#include <string.h>

extern so_module so_mod;
static float sticks[2][2], camera_remainder[2];
static uint64_t last_poll;
static int walking, steering, last_mode;
#define RIO_RIGHT_STICK_CAMERA 1
static int camera_active;
static int accelerating, braking;
static uint32_t buttons;
static int (*game_loaded)(void), (*in_game)(void);
static void *(*hud_instance)(void), *(*event_instance)(void);
static int (*hud_visible)(void *), (*hud_locked)(void *);
static int (*near_shop)(void *), (*near_item)(void *);
static unsigned triangle_action=2;
static void (*raise_event)(void *, const void *);
static void (*move_camera)(void *, int, int);
static void **application;
static uint32_t stick_move_vtable, stick_release_vtable, steer_vtable;
static uint32_t pedal_vtable, virtual_vtable;
static RioPadActions actions;
static uint32_t consumed_keys[8];

typedef struct {
    uint32_t vtable, type;
    float angle, strength;
} StickEvent;
typedef struct {
    uint32_t vtable, type;
    uint8_t right, padding[3];
    float strength;
} SteeringEvent;
_Static_assert(sizeof(StickEvent) == 16, "Android stick event layout");
_Static_assert(sizeof(SteeringEvent) == 16, "Android steering event layout");

static uintptr_t required(const char *name) {
    uintptr_t symbol = so_symbol(&so_mod, name);
    if (!symbol) fatal_error("Gangstar controls: missing %s", name);
    return symbol;
}

static void send_walk(float x, float y, int release) {
    StickEvent event = {
        release ? stick_release_vtable : stick_move_vtable, release ? 8 : 7,
        release ? 0 : gangstar_walk_angle(x, y),
        release ? 0 : fminf(sqrtf(x*x + y*y), 1.0f)
    };
    raise_event(event_instance(), &event);
}

static void send_steering(float x) {
    SteeringEvent event = {steer_vtable, 9, x > 0, {0}, fabsf(x)};
    raise_event(event_instance(), &event);
}
static void send_pedal(int accelerator, int down) {
    // EvUpDownStickMove (11): true calls Character::accelerate, false brake.
    // Its ABI matches the existing left/right event: bool at +8, float +12.
    SteeringEvent event = {pedal_vtable, 11, accelerator != 0, {0}, down ? 1.0f : 0.0f};
    raise_event(event_instance(), &event);
}
void gangstar_controls_buttons(uint32_t state) { buttons = state; }

static void send_button(void *unused, unsigned button, unsigned state) {
    (void)unused;
    if (button==2) {
        if(last_mode==1 && state==0) {
            void *hud=hud_instance();
            triangle_action=rio_triangle_action(last_mode,hud && near_shop(hud),hud && near_item(hud));
            l_info("Gangstar context action: %s",triangle_action==4?"shop":triangle_action==3?"item":"vehicle");
        }
        button=last_mode==2?2:triangle_action;
    }
    struct { uint32_t vtable,type,state,button,context,callback; } event={
        virtual_vtable,13,state,button,0,0};
    _Static_assert(sizeof(event)==24,"Android virtual button layout");
    raise_event(event_instance(),&event);
}
int gangstar_controls_filter_key(int key, ControlsAction action) {
    if(key<0 || key>=256 || key==AKEYCODE_BACK)return 0;
    int mapped=(key>=AKEYCODE_DPAD_UP && key<=AKEYCODE_DPAD_RIGHT) ||
        key==AKEYCODE_BUTTON_A || key==AKEYCODE_BUTTON_B || key==AKEYCODE_BUTTON_X ||
        key==AKEYCODE_BUTTON_Y || key==AKEYCODE_BUTTON_L1 || key==AKEYCODE_BUTTON_R1 ||
        key==AKEYCODE_BUTTON_SELECT;
    unsigned mask=1u<<(key&31);uint32_t *word=&consumed_keys[key>>5];
    if(!mapped || (!last_mode && !(*word&mask)))return 0;
    if(action==CONTROLS_ACTION_DOWN)*word|=mask;else *word&=~mask;
    return 1;
}
static unsigned action_buttons(void) {
    static const struct {unsigned vita,logical;} map[]={
        {SCE_CTRL_CROSS,RIO_CROSS},{SCE_CTRL_SQUARE,RIO_SQUARE},
        {SCE_CTRL_CIRCLE,RIO_CIRCLE},{SCE_CTRL_TRIANGLE,RIO_TRIANGLE},
        {SCE_CTRL_L1,RIO_L},{SCE_CTRL_R1,RIO_R},{SCE_CTRL_UP,RIO_UP},
        {SCE_CTRL_DOWN,RIO_DOWN},{SCE_CTRL_LEFT,RIO_LEFT},{SCE_CTRL_RIGHT,RIO_RIGHT},
        {SCE_CTRL_SELECT,RIO_SELECT},{SCE_CTRL_START,RIO_START}};
    unsigned result=0;
    for(unsigned i=0;i<sizeof(map)/sizeof(*map);++i)if(buttons&map[i].vita)result|=map[i].logical;
    return result;
}

static void ignore_driving_tilt(void *handler, const void *event) {
    (void)handler; (void)event;
    // DrivingControlHandler also receives cached accelerometer events during
    // the engine step. They must not overwrite the stick's steering command.
    // Aircraft/jetpack and all other accelerometer handlers remain original.
}

void gangstar_controls_init(void) {
    game_loaded = (void *)required("_ZN11Application12isGameLoadedEv");
    in_game = (void *)required("_ZN9GS3DStuff8isInGameEv");
    hud_instance = (void *)required("_ZN11CHudManager11getInstanceEv");
    hud_visible = (void *)required("_ZNK11CHudManager9isVisibleEv");
    hud_locked = (void *)required("_ZNK11CHudManager8isLockedEv");
    near_shop = (void *)required("_ZNK11CHudManager10isNearShopEv");
    near_item = (void *)required("_ZNK11CHudManager10isNearItemEv");
    event_instance = (void *)required("_ZN12EventManager11getInstanceEv");
    raise_event = (void *)required("_ZN12EventManager5raiseERK6IEvent");
    move_camera = (void *)required("_ZN11Application17powerA_MoveCameraEii");
    application = (void *)required("_ZN11Application13s_applicationE");
    stick_move_vtable = required("_ZTV11EvStickMove") + 8;
    stick_release_vtable = required("_ZTV15EvStickReleased") + 8;
    steer_vtable = required("_ZTV20EvLeftRightStickMove") + 8;
    pedal_vtable = required("_ZTV17EvUpDownStickMove") + 8;
    virtual_vtable = required("_ZTV15EvVirtualButton") + 8;
    uintptr_t tilt = required("_ZN21DrivingControlHandler18handleAccelerationEPK14EvAcceleration");
    static const uint32_t expected[] = {0xe92d41f0, 0xed2d8b06};
    if (memcmp((void *)tilt, expected, sizeof(expected)))
        fatal_error("Gangstar controls: unsupported driving tilt handler");
    hook_addr(tilt, (uintptr_t)ignore_driving_tilt);
    l_info("Gangstar controls: left stick walk/steer, right stick camera, native action buttons, R accelerate/L brake while driving; car tilt disabled");
}

void gangstar_controls_stick(ControlsStickId which, float x, float y) {
    if (which != CONTROLS_STICK_LEFT && which != CONTROLS_STICK_RIGHT) return;
    sticks[which][0] = x;
    sticks[which][1] = y;
}

void gangstar_controls_poll(void) {
    uint64_t now = sceKernelGetProcessTimeWide();
    float seconds = last_poll ? (now - last_poll) / 1000000.0f : 1.0f / 60.0f;
    last_poll = now;
    unsigned char *hud = NULL;
    int mode = 0;
    if (game_loaded() && in_game()) {
        hud = hud_instance();
        // CHudManager::walking/driving select state pointers at +8/+12.
        if (hud && *(void **)(hud + 4) && hud_visible(hud) && !hud_locked(hud))
            mode = *(void **)(hud + 4) == *(void **)(hud + 12) ? 2 : 1;
    }
    if (mode != last_mode) {
        // Repeat neutral after becoming active: a release sent while the
        // original control manager was locked may have been ignored.
        if (walking || mode) send_walk(0, 0, 1);
        if (steering || mode) send_steering(0);
        if (accelerating || mode == 2) send_pedal(1, 0);
        if (braking || mode == 2) send_pedal(0, 0);
        accelerating = braking = 0;
        walking = steering = 0;
        camera_remainder[0] = camera_remainder[1] = 0;
        if (RIO_RIGHT_STICK_CAMERA && (camera_active || mode) && game_loaded() && *application)
            move_camera(*application, 0, 0);
        camera_active = 0;
        l_info("Gangstar analog input: %s", mode == 2 ? "driving" : mode == 1 ? "on foot/other HUD" : "inactive");
        last_mode = mode;
    }
    rio_pad_actions(&actions,mode,action_buttons(),send_button,NULL);
    if (!mode) return;
    float x = sticks[0][0], y = sticks[0][1];
    if (mode == 2) {
        int gas = gangstar_pedal_update(1, (buttons & SCE_CTRL_R1) != 0, &accelerating);
        int brake = gangstar_pedal_update(1, (buttons & SCE_CTRL_L1) != 0, &braking);
        if (gas >= 0) send_pedal(1, gas);
        if (brake >= 0) send_pedal(0, brake);
        // A zero-strength move explicitly centers the wheels. The original
        // driving handler ignores EvLeftRightStickReleased (event 10).
        send_steering(x);
        steering = 1;
    } else if (x != 0 || y != 0) {
        send_walk(x, y, 0);
        walking = 1;
    } else if (walking) {
        send_walk(0, 0, 1);
        walking = 0;
    }
    if (RIO_RIGHT_STICK_CAMERA) {
    int dx = gangstar_camera_delta(sticks[1][0], seconds, &camera_remainder[0]);
    int dy = gangstar_camera_delta(sticks[1][1], seconds, &camera_remainder[1]);
    if (gangstar_camera_should_send(dx, dy, &camera_active) && *application)
        move_camera(*application, dx, dy);
    }
}
