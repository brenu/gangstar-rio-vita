#ifndef GANGSTAR_DEVICE_MODEL_H
#define GANGSTAR_DEVICE_MODEL_H
/* The Android Build.MODEL value the port reports, through Java
 * (GloftG4HM.getPhoneModel) and through the direct native answer. */
#define RIO_PHONE_MODEL "PS Vita"
/* Diagnostic builds: AndroidGetDeviceModel calls since the previous report. */
unsigned gangstar_device_model_calls(void);
#endif
