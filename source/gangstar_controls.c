/* Vita sticks feed the original game's synchronous input events. */
#include "gangstar_controls.h"
#include "gangstar_analog_policy.h"
#include "gangstar_pedal_policy.h"
#include "gangstar_button_policy.h"
#include "utils/dialog.h"
#include "utils/logger.h"
#include <so_util/so_util.h>
#include <kubridge.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include "gangstar_startup_trampoline.h"
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
static int (*hud_visible)(void *), (*hud_locked)(void *), (*hud_flying)(void *);
static void *(*script_instance)(void);
static int (*in_cinematic)(void *), (*controls_locked)(void *);
static int (*near_shop)(void *), (*near_item)(void *), (*getup_active)(void *);
static unsigned triangle_action=2;
static void (*raise_event)(void *, const void *);
static void (*move_camera)(void *, int, int);
static void **application, **player_instance;
static int (*player_fallen)(void *);
static uint32_t stick_move_vtable, stick_release_vtable, steer_vtable;
static uint32_t pedal_vtable, virtual_vtable;
static RioPadActions actions, recovery_actions;
static int recovering;
static uint32_t consumed_keys[8];
static uintptr_t touch_button_vtables[4], touch_stick_vtables[3];
static void (*touch_button_update_original)(void *,double);

static void physical_button_touch_update(void *button, double seconds) {
    unsigned type;
    memcpy(&type,(const char *)button+0x68,sizeof(type));
    if(rio_touch_button_mapped(type))return;
    // VirtualButton::update independently hit-tests the stored rectangle,
    // bypassing getTouchRegion. Preserve that path for UI/context buttons.
    touch_button_update_original(button,seconds);
}
static void physical_stick_touch_update(void *stick, double seconds) {
    (void)stick;(void)seconds;
}

// HudElement::getTouchRegion returns a 16-byte rect through an ARM hidden
// result pointer (r0), followed by this (r1). Keep native HUD state intact:
// visibility is also queried by contextual physical actions such as get-up.
static void physical_control_touch_region(float *region, const void *element) {
    uint32_t vtable, type;
    memcpy(&vtable, element, sizeof(vtable));
    int blocked=0;
    for(unsigned i=0;i<sizeof(touch_stick_vtables)/sizeof(*touch_stick_vtables);++i)
        if(vtable==touch_stick_vtables[i])blocked=1;
    for(unsigned i=0;i<sizeof(touch_button_vtables)/sizeof(*touch_button_vtables);++i)
        if(vtable==touch_button_vtables[i]) {
            memcpy(&type,(const char *)element+0x68,sizeof(type));
            blocked=rio_touch_button_mapped(type);
        }
    if(blocked) {
        // A valid rectangle entirely outside the touchscreen coordinate range.
        region[0]=region[1]=-10000.0f;
        region[2]=region[3]=-9999.0f;
    } else memcpy(region,(const char *)element+0x58,4*sizeof(float));
}

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
            // Latch the context on press so release reaches the same action.
            // Native get-up button 21 advances recovery on release (state 2).
            unsigned char *player=*player_instance;
            int hud_getup=hud && getup_active(hud),fallen=player && player_fallen(player);
            unsigned vehicle=0;int blocked=0;
            if(player) {blocked=player[0x77d]!=0;memcpy(&vehicle,player+0x778,sizeof(vehicle));}
            int recovery=rio_recovery_available(hud_getup,fallen,blocked,vehicle!=0);
            triangle_action=rio_triangle_action(last_mode,recovery,hud && near_shop(hud),hud && near_item(hud));
            l_info("Gangstar recovery context: hud=%d fallen=%d blocked=%d vehicle=%d",hud_getup,fallen,blocked,vehicle!=0);
            l_info("Gangstar context action: %s",triangle_action==21?"get up":triangle_action==4?"shop":triangle_action==3?"item":"vehicle");
        }
        // Cancellation must reach the action latched before a HUD-mode change.
        button=state==3?triangle_action:last_mode>=2?2:triangle_action;
    }
    struct { uint32_t vtable,type,state,button,context,callback; } event={
        virtual_vtable,13,state,button,0,0};
    _Static_assert(sizeof(event)==24,"Android virtual button layout");
    unsigned before=0,after=0;
    unsigned char *player=button==21 && state==2 ? *player_instance : NULL;
    if(player)memcpy(&before,player+0x908,sizeof(before));
    raise_event(event_instance(),&event);
    if(player) {
        memcpy(&after,player+0x908,sizeof(after));
        l_info("Gangstar recovery release: before=%u after=%u",before,after);
    }
}
static void send_recovery_button(void *unused,unsigned button,unsigned state) {
    (void)button;send_button(unused,21,state);
}
int gangstar_controls_filter_key(int key, ControlsAction action) {
    if(key<0 || key>=256 || key==AKEYCODE_BACK)return 0;
    int mapped=(key>=AKEYCODE_DPAD_UP && key<=AKEYCODE_DPAD_RIGHT) ||
        key==AKEYCODE_BUTTON_A || key==AKEYCODE_BUTTON_B || key==AKEYCODE_BUTTON_X ||
        key==AKEYCODE_BUTTON_Y || key==AKEYCODE_BUTTON_L1 || key==AKEYCODE_BUTTON_R1 ||
        key==AKEYCODE_BUTTON_SELECT;
    unsigned mask=1u<<(key&31);uint32_t *word=&consumed_keys[key>>5];
    if(!mapped)return 0;
    return rio_key_consume(word,mask,last_mode!=0 || recovering,action==CONTROLS_ACTION_DOWN);
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
    // The aircraft handler is also disabled below; jetpack remains original.
}

static void aircraft_stick(void *handler,const StickEvent *event) {
    // Verified FlyingControlHandler::handleAcceleration writes target +0x90
    // and +0x94. Player Helicopter::UpdatePhysic converts these axes into rotor
    // angle/strength every tick, overriding the stock handleStickMove result.
    // Feed that same physics path directly, without motion sensor input.
    unsigned char *target=*(unsigned char **)((char *)handler+4);
    if(!target)return;
    float x,y;
    gangstar_aircraft_axes(event->angle,event->strength,&x,&y);
    memcpy(target+0x90,&x,sizeof(x));memcpy(target+0x94,&y,sizeof(y));
}

void gangstar_controls_init(void) {
    game_loaded = (void *)required("_ZN11Application12isGameLoadedEv");
    in_game = (void *)required("_ZN9GS3DStuff8isInGameEv");
    hud_instance = (void *)required("_ZN11CHudManager11getInstanceEv");
    hud_visible = (void *)required("_ZNK11CHudManager9isVisibleEv");
    hud_locked = (void *)required("_ZNK11CHudManager8isLockedEv");
    hud_flying = (void *)required("_ZNK11CHudManager10isInFlyHudEv");
    script_instance = (void *)required("_ZN13ScriptManager11getInstanceEv");
    in_cinematic = (void *)required("_ZNK13ScriptManager13isInCinematicEv");
    controls_locked = (void *)required("_ZNK15CControlManager8isLockedEv");
    near_shop = (void *)required("_ZNK11CHudManager10isNearShopEv");
    near_item = (void *)required("_ZNK11CHudManager10isNearItemEv");
    player_instance=(void *)required("_ZN6Player8s_playerE");
    player_fallen=(void *)required("_ZN9Character10isFallDownEv");
    getup_active = (void *)required("_ZNK11CHudManager19isGetupButtonActiveEv");
    event_instance = (void *)required("_ZN12EventManager11getInstanceEv");
    raise_event = (void *)required("_ZN12EventManager5raiseERK6IEvent");
    move_camera = (void *)required("_ZN11Application17powerA_MoveCameraEii");
    application = (void *)required("_ZN11Application13s_applicationE");
    stick_move_vtable = required("_ZTV11EvStickMove") + 8;
    stick_release_vtable = required("_ZTV15EvStickReleased") + 8;
    steer_vtable = required("_ZTV20EvLeftRightStickMove") + 8;
    pedal_vtable = required("_ZTV17EvUpDownStickMove") + 8;
    virtual_vtable = required("_ZTV15EvVirtualButton") + 8;
    static const char *touch_buttons[]={"_ZTV13VirtualButton","_ZTV12SprintButton",
        "_ZTV12ToggleButton","_ZTV14AnimatedButton"};
    static const char *touch_sticks[]={"_ZTV11AnalogStick","_ZTV12SlideControl","_ZTV5Wheel"};
    for(unsigned i=0;i<sizeof(touch_buttons)/sizeof(*touch_buttons);++i)
        touch_button_vtables[i]=required(touch_buttons[i])+8;
    for(unsigned i=0;i<sizeof(touch_sticks)/sizeof(*touch_sticks);++i)
        touch_stick_vtables[i]=required(touch_sticks[i])+8;
    uintptr_t touch_region=required("_ZN10HudElement14getTouchRegionEv");
    static const uint32_t region_expected[]={0xedd17a16,0xedc07a00};
    if(memcmp((void *)touch_region,region_expected,sizeof(region_expected)))
        fatal_error("Gangstar controls: unsupported HUD touch region layout");
    hook_addr(touch_region,(uintptr_t)physical_control_touch_region);
    uintptr_t button_update=required("_ZN13VirtualButton6updateEd");
    static const uint32_t update_expected[]={0xe92d4070,0xe5903024};
    if(memcmp((void *)button_update,update_expected,sizeof(update_expected)))
        fatal_error("Gangstar controls: unsupported button touch update layout");
    uint32_t trampoline[6],literals[2]={0};
    uintptr_t target=(so_mod.patch_head+3)&~(uintptr_t)3;
    if(!rio_startup_trampoline(trampoline,button_update,update_expected,literals) ||
       target<so_mod.patch_base || target>so_mod.patch_base+so_mod.patch_size ||
       sizeof(trampoline)>so_mod.patch_base+so_mod.patch_size-target)
        fatal_error("Gangstar controls: no space for button update trampoline");
    memcpy((void *)target,trampoline,sizeof(trampoline));
    so_mod.patch_head=target+sizeof(trampoline);
    kuKernelFlushCaches((void *)target,sizeof(trampoline));
    touch_button_update_original=(void *)target;
    hook_addr(button_update,(uintptr_t)physical_button_touch_update);
    kuKernelFlushCaches((void *)button_update,8);
    static const char *stick_updates[]={"_ZN11AnalogStick6updateEd",
        "_ZN12SlideControl6updateEd","_ZN5Wheel6updateEd"};
    static const uint32_t stick_update_expected[]={0xe59f30c8,0xeddf7a30};
    for(unsigned i=0;i<sizeof(stick_updates)/sizeof(*stick_updates);++i) {
        uintptr_t update=required(stick_updates[i]);
        if(memcmp((void *)update,stick_update_expected,sizeof(stick_update_expected)))
            fatal_error("Gangstar controls: unsupported stick touch update layout");
        hook_addr(update,(uintptr_t)physical_stick_touch_update);
    }
    l_info("Gangstar mapped gameplay touch regions disabled; UI and touch camera retained");
    uintptr_t tilt = required("_ZN21DrivingControlHandler18handleAccelerationEPK14EvAcceleration");
    static const uint32_t expected[] = {0xe92d41f0, 0xed2d8b06};
    if (memcmp((void *)tilt, expected, sizeof(expected)))
        fatal_error("Gangstar controls: unsupported driving tilt handler");
    hook_addr(tilt, (uintptr_t)ignore_driving_tilt);
    uintptr_t flying_tilt=required("_ZN20FlyingControlHandler18handleAccelerationEPK14EvAcceleration");
    static const uint32_t flying_expected[]={0xe92d4070,0xed2d8b04};
    if(memcmp((void *)flying_tilt,flying_expected,sizeof(flying_expected)))
        fatal_error("Gangstar controls: unsupported aircraft tilt handler");
    hook_addr(flying_tilt,(uintptr_t)ignore_driving_tilt);
    uintptr_t flying_stick=required("_ZN20FlyingControlHandler15handleStickMoveEPK11EvStickMove");
    static const uint32_t stick_expected[]={0xe59f3054,0xe92d4010};
    if(memcmp((void *)flying_stick,stick_expected,sizeof(stick_expected)))
        fatal_error("Gangstar controls: unsupported aircraft stick handler");
    hook_addr(flying_stick,(uintptr_t)aircraft_stick);
    l_info("Gangstar controls: left stick walk/steer, right stick camera, Square attack only, R accelerate/L brake while driving; car/aircraft tilt disabled; left stick aircraft pitch/roll");
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
    recovering=0;
    if (game_loaded() && in_game()) {
        hud = hud_instance();
        // CHudManager::walking/driving select state pointers at +8/+12.
        // OnFootControlHandler::onEvent independently checks the control
        // manager at Application+0x17af4 and ScriptManager::isInCinematic.
        // HUD visibility alone can become true before either gate clears.
        void *controls=*application ? *(void **)((char *)*application+0x17af4) : NULL;
        void *script=script_instance();
        unsigned char *player=*player_instance;
        // Native OnFootControlHandler::onEvent explicitly accepts button 21
        // while controls are locked when this ejection/recovery flag is set.
        recovering=player && player[0x77e] && script && !in_cinematic(script);
        if (hud && *(void **)(hud + 4) && hud_visible(hud) && !hud_locked(hud) &&
            controls && !controls_locked(controls) && script && !in_cinematic(script))
            mode = hud_flying(hud) ? 3 : *(void **)(hud + 4) == *(void **)(hud + 12) ? 2 : 1;
    }
    if(recovering)mode=0; // Recovery accepts only Triangle; other controls remain cancelled.
    if (mode != last_mode) {
        // Repeat neutral after becoming active: a release sent while the
        // original control manager was locked may have been ignored.
        // FlyingControlHandler ignores EvStickReleased (8). A zero-strength
        // EvStickMove (7) clears the retained rotor/airplane stick command.
        if (last_mode==3 || mode==3) send_walk(0,0,0);
        else if (walking || mode) send_walk(0, 0, 1);
        if (steering || mode) send_steering(0);
        if (accelerating || mode >= 2) send_pedal(1, 0);
        if (braking || mode >= 2) send_pedal(0, 0);
        accelerating = braking = 0;
        walking = steering = 0;
        camera_remainder[0] = camera_remainder[1] = 0;
        if (RIO_RIGHT_STICK_CAMERA && (camera_active || mode) && game_loaded() && *application)
            move_camera(*application, 0, 0);
        camera_active = 0;
        l_info("Gangstar analog input: %s", mode == 3 ? "flying" : mode == 2 ? "driving" : mode == 1 ? "on foot/other HUD" : "inactive");
        last_mode = mode;
    }
    unsigned physical=action_buttons();
    rio_pad_actions(&actions,mode==3?2:mode,physical,send_button,NULL);
    rio_pad_actions(&recovery_actions,recovering?1:0,physical&RIO_TRIANGLE,send_recovery_button,NULL);
    if (!mode) return;
    float x = sticks[0][0], y = sticks[0][1];
    if (mode >= 2) {
        int gas = gangstar_pedal_update(1, (buttons & SCE_CTRL_R1) != 0, &accelerating);
        int brake = gangstar_pedal_update(1, (buttons & SCE_CTRL_L1) != 0, &braking);
        if (gas >= 0) send_pedal(1, gas);
        if (brake >= 0) send_pedal(0, brake);
        // A zero-strength move explicitly centers the wheels. The original
        // driving handler ignores EvLeftRightStickReleased (event 10).
        if(mode==3) {
            // The aircraft adapter feeds native physics axes. Send every
            // frame, including centered input.
            send_walk(x,y,0);
            walking=1;
        } else {
            send_steering(x);
            steering=1;
        }
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
