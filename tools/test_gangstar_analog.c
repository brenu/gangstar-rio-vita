#include "gangstar_analog_policy.h"
#include "reimpl/controls.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int camera_distance(int fps, float axis) {
    float remainder = 0;
    int total = 0;
    for (int frame = 0; frame < fps; ++frame)
        total += gangstar_camera_delta(axis, 1.0f / fps, &remainder);
    return total;
}

int main(void) {
    int camera_active = 0;
    assert(!gangstar_camera_should_send(0, 0, &camera_active));
    assert(gangstar_camera_should_send(0, 5, &camera_active) && camera_active);
    assert(gangstar_camera_should_send(0, 0, &camera_active) && !camera_active);
    assert(!gangstar_camera_should_send(0, 0, &camera_active));
    assert(gangstar_camera_should_send(3, 0, &camera_active));
    assert(gangstar_camera_should_send(0, -2, &camera_active));
    assert(gangstar_camera_should_send(0, 0, &camera_active));
    assert(!gangstar_camera_should_send(0, 0, &camera_active));
    // Camera drift inside the configured threshold is neutral, while walking
    // input still passes the left stick's existing 16% threshold.
    float drift_x = RIGHT_ANALOG_DEADZONE * 0.8f, drift_y = RIGHT_ANALOG_DEADZONE * 0.3f;
    float walk_x = drift_x, walk_y = drift_y;
    gangstar_stick_normalize(&drift_x, &drift_y, RIGHT_ANALOG_DEADZONE);
    gangstar_stick_normalize(&walk_x, &walk_y, LEFT_ANALOG_DEADZONE);
    assert(drift_x == 0 && drift_y == 0);
    assert(walk_x > 0 && walk_y > 0);
    drift_x = RIGHT_ANALOG_DEADZONE + 0.01f; drift_y = 0;
    gangstar_stick_normalize(&drift_x, &drift_y, RIGHT_ANALOG_DEADZONE);
    assert(fabsf(drift_x - 0.01f / (1.0f - RIGHT_ANALOG_DEADZONE)) < 0.00001f && drift_y == 0);
    drift_x = 1; drift_y = 0;
    gangstar_stick_normalize(&drift_x, &drift_y, RIGHT_ANALOG_DEADZONE);
    assert(fabsf(drift_x - 1) < 0.00001f);
    float x = 0.10f, y = -0.10f;
    gangstar_stick_normalize(&x, &y, 0.16f);
    assert(x == 0 && y == 0);
    x = y = 1;
    gangstar_stick_normalize(&x, &y, 0.16f);
    assert(fabsf(sqrtf(x*x + y*y) - 1) < 0.00001f); // diagonal is not faster
    x = 0.58f; y = 0;
    gangstar_stick_normalize(&x, &y, 0.16f);
    assert(fabsf(x - 0.5f) < 0.00001f && y == 0);
    const float pi = 3.141592653589793f;
    assert(fabsf(gangstar_walk_angle(0, -1)) < 0.00001f);
    assert(fabsf(gangstar_walk_angle(1, 0) + pi/2) < 0.00001f);
    assert(fabsf(gangstar_walk_angle(-1, 0) - pi/2) < 0.00001f);
    assert(fabsf(fabsf(gangstar_walk_angle(0, 1)) - pi) < 0.00001f);
    assert(abs(camera_distance(30, 0.7f) - camera_distance(60, 0.7f)) <= 1);
    assert(abs(camera_distance(30, -0.7f) - camera_distance(60, -0.7f)) <= 1);
    assert(camera_distance(60, 0.01f) >= 11); // fractional slow look survives
    float remainder = 0.9f;
    assert(gangstar_camera_delta(0, 1.0f/60, &remainder) == 0 && remainder == 0);
    assert(gangstar_camera_delta(1, 10, &remainder) <= 60); // no resume jump
    puts("Gangstar analog: deadzone, diagonal speed, walking directions and frame-independent camera passed");
}
