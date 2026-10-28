#ifndef TEST_VITA_AUDIOOUT_H
#define TEST_VITA_AUDIOOUT_H
#include <stdint.h>
typedef uint32_t SceSize;
typedef enum __attribute__((packed)) { SCE_AUDIO_OUT_PORT_TYPE_MAIN, SCE_AUDIO_OUT_PORT_TYPE_BGM } SceAudioOutPortType;
typedef enum __attribute__((packed)) { SCE_AUDIO_OUT_MODE_MONO, SCE_AUDIO_OUT_MODE_STEREO } SceAudioOutMode;
#endif
