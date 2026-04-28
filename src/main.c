#include "app_config.h"
#include "hal/macros.h"
#include "http_post.h"
#include "media.h"
#include "network.h"
#include "night.h"
#include "rtsp/rtsp_server.h"
#include "server.h"
#include "uvc.h"
#include "watchdog.h"

#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

rtsp_handle rtspHandle;
char graceful = 0, keepRunning = 1;

void handle_error(int signo) {
    char msg[64];
    sprintf(msg, "Error occured (%d)! Quitting...\n", signo);
    write(STDERR_FILENO, msg, strlen(msg));
    keepRunning = 0;
    exit(EXIT_FAILURE);
}

void handle_exit(int signo) {
    write(STDERR_FILENO, "Graceful shutdown...\n", 21);
    keepRunning = 0;
    graceful = 1;
}

int main(int argc, char *argv[]) {
    {
        char signal_error[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV};
        char signal_exit[] = {SIGINT, SIGQUIT, SIGTERM};
        char signal_null[] = {EPIPE, SIGPIPE};

        for (char *s = signal_error; s < (&signal_error)[1]; s++)
            signal(*s, handle_error);
        for (char *s = signal_exit; s < (&signal_exit)[1]; s++)
            signal(*s, handle_exit);
        for (char *s = signal_null; s < (&signal_null)[1]; s++)
            signal(*s, NULL);
    }

    hal_identify();

    if (!*family)
        HAL_ERROR("hal", "Unsupported chip family! Quitting...\n");

    fprintf(stderr, "\033[7m Divinus for %s \033[0m\n", family);
    fprintf(stderr, "Chip ID: %s\n", chip);

    if (parse_app_config() != CONFIG_OK)
        HAL_ERROR("hal", "Can't load app config 'divinus.yaml'\n");

    if (app_config.watchdog)
        watchdog_start(app_config.watchdog);

    if (app_config.mdns_enable)
        start_mdns();

    start_server();

    if (app_config.rtsp_enable) {
        rtspHandle = rtsp_create(RTSP_MAXIMUM_CONNECTIONS, app_config.rtsp_port, 1);
        HAL_INFO("rtsp", "Started listening for clients...\n");
        if (app_config.rtsp_enable_auth) {
            if (!app_config.rtsp_auth_user || !app_config.rtsp_auth_pass)
                HAL_ERROR("rtsp", "One or both credential fields have been left empty!\n");
            else {
                rtsp_configure_auth(rtspHandle, app_config.rtsp_auth_user, app_config.rtsp_auth_pass);
                HAL_INFO("rtsp", "Authentication enabled!\n");
            }
        }
    }

    if (app_config.stream_enable)
        start_streaming();

    if (start_sdk())
        HAL_ERROR("hal", "Failed to start SDK!\n");

    if (app_config.uvc_enable) {
        uvc_config uvc_conf;
        strncpy(uvc_conf.device, app_config.uvc_device, sizeof(uvc_conf.device) - 1);
        uvc_conf.width = app_config.uvc_width;
        uvc_conf.height = app_config.uvc_height;
        uvc_conf.fps = app_config.uvc_fps;
        if (!strcmp(app_config.uvc_format, "mjpeg"))
            uvc_conf.format = UVC_FORMAT_MJPEG;
        else if (!strcmp(app_config.uvc_format, "h264"))
            uvc_conf.format = UVC_FORMAT_H264;
        else
            uvc_conf.format = UVC_FORMAT_AUTO;

        if (!uvc_capture_init(&uvc_conf))
            uvc_capture_start();
        else
            HAL_DANGER("main", "UVC initialization failed!\n");
    }

    if (app_config.night_mode_enable)
        enable_night();

    if (app_config.http_post_enable)
        start_http_post_send();

    if (app_config.osd_enable)
        start_region_handler();

    while (keepRunning) {
        watchdog_reset();
        sleep(1);
    }

    if (app_config.rtsp_enable) {
        rtsp_finish(rtspHandle);
        HAL_INFO("rtsp", "Server has closed!\n");
    }

    if (app_config.osd_enable)
        stop_region_handler();

    if (app_config.night_mode_enable)
        disable_night();

    stop_sdk();

    if (app_config.uvc_enable)
        uvc_capture_deinit();

    if (app_config.stream_enable)
        stop_streaming();

    stop_server();

    if (app_config.mdns_enable)
        stop_mdns();

    if (app_config.watchdog)
        watchdog_stop();

    if (!graceful)
        restore_app_config();

    fprintf(stderr, "Main thread is shutting down...\n");
    return EXIT_SUCCESS;
}