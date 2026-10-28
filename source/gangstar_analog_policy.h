#ifndef GANGSTAR_ANALOG_POLICY_H
#define GANGSTAR_ANALOG_POLICY_H
#include <math.h>

static inline void gangstar_stick_normalize(float *x, float *y, float deadzone) {
    float length = sqrtf(*x * *x + *y * *y);
    if (length <= deadzone) { *x = 0; *y = 0; return; }
    float strength = (fminf(length, 1.0f) - deadzone) / (1.0f - deadzone);
    *x *= strength / length;
    *y *= strength / length;
}

// Native EvStickMove: forward is zero, right is -pi/2; Vita Y grows down.
static inline float gangstar_walk_angle(float x, float y) {
    return -atan2f(x, -y);
}

// Match the Android PowerA camera's 20 units/frame at 60 Hz. Accumulate the
// fractional part so slow stick movement survives its integer scroll API.
static inline int gangstar_camera_delta(float axis, float seconds, float *remainder) {
    if (axis == 0) { *remainder = 0; return 0; }
    float value = *remainder + axis * 1200.0f * fminf(seconds, 0.05f);
    int delta = (int)value;
    *remainder = value - delta;
    return delta;
}
// Rio retains camera angular input. Send one explicit neutral after movement,
// including a zero quantized frame, without taking over idle touch controls.
static inline int gangstar_camera_should_send(int dx, int dy, int *active) {
    int moving = dx != 0 || dy != 0;
    int send = moving || *active;
    *active = moving;
    return send;
}
#endif
