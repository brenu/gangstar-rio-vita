#include "gangstar_pedal_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    int gas = 0, brake = 0;
    assert(gangstar_pedal_update(0, 1, &gas) == -1 && !gas);
    assert(gangstar_pedal_update(1, 0, &gas) == -1);
    assert(gangstar_pedal_update(1, 1, &gas) == 1 && gas);
    assert(gangstar_pedal_update(1, 1, &gas) == 1);
    assert(gangstar_pedal_update(1, 1, &brake) == 1 && gas && brake);
    assert(gangstar_pedal_update(1, 0, &gas) == 0 && !gas && brake);
    assert(gangstar_pedal_update(1, 0, &gas) == -1);
    assert(gangstar_pedal_update(0, 1, &brake) == 0 && !brake);
    assert(gangstar_pedal_update(0, 1, &brake) == -1);
    assert(gangstar_pedal_update(1, 1, &brake) == 1);
    puts("Pedals: driving gating, hold, release, simultaneous input and inactive touchscreen preservation passed");
}
