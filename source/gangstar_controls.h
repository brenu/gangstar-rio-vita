#ifndef GANGSTAR_CONTROLS_H
#define GANGSTAR_CONTROLS_H
#include "reimpl/controls.h"
void gangstar_controls_init(void);
void gangstar_controls_poll(void);
void gangstar_controls_stick(ControlsStickId which, float x, float y);
void gangstar_controls_buttons(uint32_t buttons);
int gangstar_controls_filter_key(int key, ControlsAction action);
#endif
