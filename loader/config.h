#ifndef __CONFIG_H__
#define __CONFIG_H__

// #define REM2

#define LOAD_ADDRESS 0x98000000

#define MEMORY_NEWLIB_MB 240
#define MEMORY_VITAGL_THRESHOLD_MB 12
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_SAMPLES_PER_BUF 8192

#ifdef REM2
#define DATA_PATH "ux0:data/duck/assets"
#define SO_PATH DATA_PATH "/" "libducktales20.so"
#else
#define DATA_PATH "ux0:data/duck"
#define SO_PATH DATA_PATH "/" "libDuckTales.so"
#endif
#define SCREEN_W 960
#define SCREEN_H 544

#endif
