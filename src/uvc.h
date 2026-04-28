#pragma once

#include <pthread.h>
#include <stdbool.h>

#include "hal/support.h"

typedef enum {
    UVC_FORMAT_AUTO,
    UVC_FORMAT_MJPEG,
    UVC_FORMAT_H264
} uvc_format;

typedef struct {
    char device[64];
    unsigned int width;
    unsigned int height;
    unsigned int fps;
    uvc_format format;
} uvc_config;

typedef struct {
    int fd;
    uvc_config config;
    volatile int running;
    pthread_t thread;
    void *buffers[4];
    unsigned int buffer_sizes[4];
    unsigned int buffer_count;
    uvc_format active_format;
} uvc_ctx;

int uvc_capture_init(uvc_config *config);
int uvc_capture_start(void);
void uvc_capture_stop(void);
void uvc_capture_deinit(void);
