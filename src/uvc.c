#include "uvc.h"
#include "app_config.h"
#include "stream.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

static uvc_ctx *g_uvc_ctx = NULL;

#define UVC_BUFFER_COUNT 4
#define UVC_H264_NAL_MAX 32

static const char *format_to_string(uvc_format fmt)
{
    switch (fmt)
    {
    case UVC_FORMAT_MJPEG: return "MJPEG";
    case UVC_FORMAT_H264:  return "H.264";
    default:               return "auto";
    }
}

static uvc_format v4l2_to_uvc_format(unsigned int pixfmt)
{
    if (pixfmt == V4L2_PIX_FMT_MJPEG)
        return UVC_FORMAT_MJPEG;
    if (pixfmt == V4L2_PIX_FMT_H264)
        return UVC_FORMAT_H264;
    return UVC_FORMAT_AUTO;
}

static unsigned int uvc_to_v4l2_format(uvc_format fmt)
{
    switch (fmt)
    {
    case UVC_FORMAT_MJPEG: return V4L2_PIX_FMT_MJPEG;
    case UVC_FORMAT_H264:  return V4L2_PIX_FMT_H264;
    default:               return 0;
    }
}

static int xioctl(int fd, unsigned long request, void *arg)
{
    int r;
    do
        r = ioctl(fd, request, arg);
    while (r == -1 && errno == EINTR);
    return r;
}

static uvc_format detect_best_format(int fd)
{
    struct v4l2_fmtdesc fmtdesc;
    int has_h264 = 0, has_mjpeg = 0;

    memset(&fmtdesc, 0, sizeof(fmtdesc));
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    while (xioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc) == 0)
    {
        if (fmtdesc.pixelformat == V4L2_PIX_FMT_H264)
            has_h264 = 1;
        else if (fmtdesc.pixelformat == V4L2_PIX_FMT_MJPEG)
            has_mjpeg = 1;
        fmtdesc.index++;
    }

    if (has_h264)
        return UVC_FORMAT_H264;
    if (has_mjpeg)
        return UVC_FORMAT_MJPEG;
    return UVC_FORMAT_AUTO;
}

static void find_jpeg_sof_dimensions(const unsigned char *data, int size,
                                     unsigned short *w, unsigned short *h)
{
    for (int i = 0; i < size - 9; i++)
    {
        if (data[i] != 0xFF)
            continue;
        unsigned char marker = data[i + 1];
        if (marker != 0xC0 && marker != 0xC2)
            continue;
        int len = (data[i + 2] << 8) | data[i + 3];
        if (len < 9)
            continue;
        *h = (data[i + 5] << 8) | data[i + 6];
        *w = (data[i + 7] << 8) | data[i + 8];
        return;
    }
}

static void send_h264_buffer(const unsigned char *data, int size,
                             unsigned int timestamp)
{
    const unsigned char *ptr = data;
    const unsigned char *end = data + size;

    while (ptr < end - 4)
    {
        if (ptr[0] != 0 || ptr[1] != 0)
        {
            ptr++;
            continue;
        }

        int start_code_len = 0;
        if (ptr[2] == 1)
            start_code_len = 3;
        else if (ptr[2] == 0 && ptr + 3 < end && ptr[3] == 1)
            start_code_len = 4;

        if (!start_code_len)
        {
            ptr++;
            continue;
        }

        const unsigned char *nal_start = ptr + start_code_len;

        const unsigned char *next = nal_start + 1;
        while (next < end - 3)
        {
            if (next[0] == 0 && next[1] == 0 && (next[2] == 1 || (next[2] == 0 && next + 3 < end && next[3] == 1)))
                break;
            next++;
        }
        if (next >= end - 3)
            next = end;

        int nal_size = next - nal_start;
        unsigned char nal_type = nal_start[0] & 0x1F;
        int is_keyframe = (nal_type == 5 || nal_type == 7 || nal_type == 8);

        udp_stream_send_nal((const char *)ptr, next - ptr, timestamp, is_keyframe, 0);
        ptr = next;
    }
}

static void *uvc_capture_thread(void *data)
{
    uvc_ctx *ctx = (uvc_ctx *)data;
    fd_set fds;
    struct timeval tv;
    unsigned int frame_seq = 0;

    HAL_INFO("uvc", "Capture thread started (format: %s, %ux%u @ %u fps)\n",
             format_to_string(ctx->active_format),
             ctx->config.width, ctx->config.height, ctx->config.fps);

    while (ctx->running)
    {
        FD_ZERO(&fds);
        FD_SET(ctx->fd, &fds);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(ctx->fd + 1, &fds, NULL, NULL, &tv);
        if (ret == -1)
        {
            if (errno == EINTR)
                continue;
            HAL_DANGER("uvc", "select() error: %s\n", strerror(errno));
            break;
        }
        if (ret == 0)
            continue;

        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (xioctl(ctx->fd, VIDIOC_DQBUF, &buf) < 0)
        {
            if (errno == EAGAIN)
                continue;
            HAL_DANGER("uvc", "VIDIOC_DQBUF failed: %s\n", strerror(errno));
            continue;
        }

        unsigned char *frame_data = (unsigned char *)ctx->buffers[buf.index];
        unsigned int frame_size = buf.bytesused;
        unsigned int timestamp = frame_seq * (90000 / ctx->config.fps);
        frame_seq++;

        if (ctx->active_format == UVC_FORMAT_MJPEG)
        {
            unsigned short w = ctx->config.width;
            unsigned short h = ctx->config.height;
            find_jpeg_sof_dimensions(frame_data, frame_size, &w, &h);
            udp_stream_send_jpeg(frame_data, frame_size, timestamp, w, h);
        }
        else if (ctx->active_format == UVC_FORMAT_H264)
        {
            send_h264_buffer(frame_data, frame_size, timestamp);
        }

        xioctl(ctx->fd, VIDIOC_QBUF, &buf);
    }

    HAL_INFO("uvc", "Capture thread stopped\n");
    return NULL;
}

int uvc_capture_init(uvc_config *config)
{
    if (g_uvc_ctx)
    {
        HAL_DANGER("uvc", "Already initialized!\n");
        return EXIT_FAILURE;
    }

    if (!(g_uvc_ctx = (uvc_ctx *)calloc(1, sizeof(uvc_ctx))))
        HAL_ERROR("uvc", "Failed to allocate context!\n");

    memcpy(&g_uvc_ctx->config, config, sizeof(uvc_config));

    int fd = open(config->device, O_RDWR | O_NONBLOCK);
    if (fd < 0)
    {
        HAL_DANGER("uvc", "Cannot open %s: %s\n", config->device, strerror(errno));
        free(g_uvc_ctx);
        g_uvc_ctx = NULL;
        return EXIT_FAILURE;
    }
    g_uvc_ctx->fd = fd;

    struct v4l2_capability cap;
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0)
    {
        HAL_DANGER("uvc", "VIDIOC_QUERYCAP failed: %s\n", strerror(errno));
        goto error;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
    {
        HAL_DANGER("uvc", "Device does not support video capture!\n");
        goto error;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING))
    {
        HAL_DANGER("uvc", "Device does not support streaming!\n");
        goto error;
    }

    HAL_INFO("uvc", "Device: %s (%s)\n", cap.card, cap.driver);

    /* Detect best format if auto */
    uvc_format use_format = config->format;
    if (use_format == UVC_FORMAT_AUTO)
    {
        use_format = detect_best_format(fd);
        if (use_format == UVC_FORMAT_AUTO)
        {
            HAL_DANGER("uvc", "No supported format found (need MJPEG or H.264)!\n");
            goto error;
        }
        HAL_INFO("uvc", "Auto-detected format: %s\n", format_to_string(use_format));
    }
    g_uvc_ctx->active_format = use_format;

    /* Set format */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config->width;
    fmt.fmt.pix.height = config->height;
    fmt.fmt.pix.pixelformat = uvc_to_v4l2_format(use_format);
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0)
    {
        HAL_DANGER("uvc", "VIDIOC_S_FMT failed: %s\n", strerror(errno));
        goto error;
    }

    g_uvc_ctx->config.width = fmt.fmt.pix.width;
    g_uvc_ctx->config.height = fmt.fmt.pix.height;

    if (fmt.fmt.pix.pixelformat != uvc_to_v4l2_format(use_format))
    {
        g_uvc_ctx->active_format = v4l2_to_uvc_format(fmt.fmt.pix.pixelformat);
        HAL_INFO("uvc", "Format adjusted to: %s\n",
                 format_to_string(g_uvc_ctx->active_format));
    }

    /* Set framerate */
    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = config->fps;

    if (xioctl(fd, VIDIOC_S_PARM, &parm) < 0)
        HAL_DANGER("uvc", "VIDIOC_S_PARM failed: %s\n", strerror(errno));
    else
        g_uvc_ctx->config.fps = parm.parm.capture.timeperframe.denominator /
                                parm.parm.capture.timeperframe.numerator;

    /* Request buffers */
    struct v4l2_requestbuffers req;
    memset(&req, 0, sizeof(req));
    req.count = UVC_BUFFER_COUNT;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;

    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0)
    {
        HAL_DANGER("uvc", "VIDIOC_REQBUFS failed: %s\n", strerror(errno));
        goto error;
    }

    if (req.count < 2)
    {
        HAL_DANGER("uvc", "Insufficient buffer memory!\n");
        goto error;
    }

    /* Map buffers */
    g_uvc_ctx->buffer_count = req.count;
    for (unsigned int i = 0; i < req.count; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0)
        {
            HAL_DANGER("uvc", "VIDIOC_QUERYBUF failed: %s\n", strerror(errno));
            goto error_unmap;
        }

        g_uvc_ctx->buffer_sizes[i] = buf.length;
        g_uvc_ctx->buffers[i] = mmap(NULL, buf.length,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

        if (g_uvc_ctx->buffers[i] == MAP_FAILED)
        {
            HAL_DANGER("uvc", "mmap failed: %s\n", strerror(errno));
            g_uvc_ctx->buffer_count = i;
            goto error_unmap;
        }
    }

    HAL_INFO("uvc", "Initialized %s %ux%u @ %u fps (buffers: %u)\n",
             format_to_string(g_uvc_ctx->active_format),
             g_uvc_ctx->config.width, g_uvc_ctx->config.height,
             g_uvc_ctx->config.fps, g_uvc_ctx->buffer_count);

    return EXIT_SUCCESS;

error_unmap:
    for (unsigned int i = 0; i < g_uvc_ctx->buffer_count; i++)
    {
        if (g_uvc_ctx->buffers[i] && g_uvc_ctx->buffers[i] != MAP_FAILED)
            munmap(g_uvc_ctx->buffers[i], g_uvc_ctx->buffer_sizes[i]);
    }
error:
    close(fd);
    free(g_uvc_ctx);
    g_uvc_ctx = NULL;
    return EXIT_FAILURE;
}

int uvc_capture_start(void)
{
    if (!g_uvc_ctx)
        return EXIT_FAILURE;

    /* Queue all buffers */
    for (unsigned int i = 0; i < g_uvc_ctx->buffer_count; i++)
    {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        if (xioctl(g_uvc_ctx->fd, VIDIOC_QBUF, &buf) < 0)
        {
            HAL_DANGER("uvc", "VIDIOC_QBUF failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }
    }

    /* Start streaming */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(g_uvc_ctx->fd, VIDIOC_STREAMON, &type) < 0)
    {
        HAL_DANGER("uvc", "VIDIOC_STREAMON failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    g_uvc_ctx->running = 1;
    if (pthread_create(&g_uvc_ctx->thread, NULL, uvc_capture_thread, g_uvc_ctx) != 0)
    {
        HAL_DANGER("uvc", "Failed to create capture thread!\n");
        g_uvc_ctx->running = 0;
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        xioctl(g_uvc_ctx->fd, VIDIOC_STREAMOFF, &type);
        return EXIT_FAILURE;
    }

    HAL_INFO("uvc", "Capture started\n");
    return EXIT_SUCCESS;
}

void uvc_capture_stop(void)
{
    if (!g_uvc_ctx || !g_uvc_ctx->running)
        return;

    g_uvc_ctx->running = 0;
    pthread_join(g_uvc_ctx->thread, NULL);

    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(g_uvc_ctx->fd, VIDIOC_STREAMOFF, &type);

    HAL_INFO("uvc", "Capture stopped\n");
}

void uvc_capture_deinit(void)
{
    if (!g_uvc_ctx)
        return;

    if (g_uvc_ctx->running)
        uvc_capture_stop();

    for (unsigned int i = 0; i < g_uvc_ctx->buffer_count; i++)
    {
        if (g_uvc_ctx->buffers[i] && g_uvc_ctx->buffers[i] != MAP_FAILED)
            munmap(g_uvc_ctx->buffers[i], g_uvc_ctx->buffer_sizes[i]);
    }

    if (g_uvc_ctx->fd >= 0)
        close(g_uvc_ctx->fd);

    free(g_uvc_ctx);
    g_uvc_ctx = NULL;

    HAL_INFO("uvc", "Deinitialized\n");
}
