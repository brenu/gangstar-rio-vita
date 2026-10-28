#ifndef GANGSTAR_PEDAL_POLICY_H
#define GANGSTAR_PEDAL_POLICY_H
// -1 leaves touchscreen state alone; 0 releases our pedal; 1 holds it down.
static inline int gangstar_pedal_update(int driving, int pressed, int *active) {
    int next = driving && pressed;
    int output = next ? 1 : *active ? 0 : -1;
    *active = next;
    return output;
}
#endif
