#include "stream.h"
static struct udp_stream_ctx *g_udp_ctx = NULL;

static unsigned long long get_timestamp_us();
static void *udp_client_manager_thread(void *data);
static int add_rtp_header(unsigned char *packet, int pay_size,
                          unsigned short seq, unsigned int tstamp,
                          unsigned int ssrc, int marker, int pay_type);

/**
 * Initializes the UDP streaming module
 * @param port UDP port to be used (0 = prefer the default value)
 * @param mcast_addr Multicast address to be used (NULL = disabled)
 * @return EXIT_SUCCESS (0) or EXIT_FAILURE (-1)
 */
int udp_stream_init(unsigned short port, const char *mcast_addr)
{
    struct sockaddr_in addr;
    int enable = 1;

    if (!(g_udp_ctx = (struct udp_stream_ctx *)calloc(1, sizeof(struct udp_stream_ctx))))
        HAL_ERROR("stream", "Failed to allocate UDP stream context!\n");

    g_udp_ctx->port = port ? port : UDP_DEFAULT_PORT;
    g_udp_ctx->running = 0;
    g_udp_ctx->client_count = 0;
    g_udp_ctx->is_mcast = 0;

    if (pthread_mutex_init(&g_udp_ctx->mutex, NULL))
    {
        free(g_udp_ctx);
        g_udp_ctx = NULL;
        HAL_ERROR("stream", "Failed to initialize mutex!\n");
    }

    if ((g_udp_ctx->socket_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        HAL_DANGER("stream", "Failed to create UDP socket: %s\n", strerror(errno));
        goto error;
    }

    if (setsockopt(g_udp_ctx->socket_fd, SOL_SOCKET, SO_REUSEADDR,
                   &enable, sizeof(int)) < 0)
    {
        HAL_DANGER("stream", "Failed to set socket options: %s\n", strerror(errno));
        goto error;
    }

    if (mcast_addr)
    {
        g_udp_ctx->is_mcast = 1;
        g_udp_ctx->mcast_addr = inet_addr(mcast_addr);

        int ttl = 32;
        if (setsockopt(g_udp_ctx->socket_fd, IPPROTO_IP, IP_MULTICAST_TTL,
                       &ttl, sizeof(ttl)) < 0)
            HAL_DANGER("stream", "Failed to set multicast TTL: %s\n", strerror(errno));
    }

    memset(&addr, 0, sizeof(addr));
    addr = (struct sockaddr_in){.sin_family = AF_INET,
                                .sin_addr.s_addr = INADDR_ANY,
                                .sin_port = htons(g_udp_ctx->port)};

    if (bind(g_udp_ctx->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        HAL_DANGER("stream", "Failed to bind UDP socket: %s\n", strerror(errno));
        goto error;
    }

    fcntl(g_udp_ctx->socket_fd, F_SETFL,
          fcntl(g_udp_ctx->socket_fd, F_GETFL, 0) | O_NONBLOCK);

    g_udp_ctx->running = 1;
    if (pthread_create(&g_udp_ctx->thread, NULL,
                       udp_client_manager_thread, g_udp_ctx) != 0)
    {
        HAL_DANGER("stream", "Failed to create UDP client manager thread!\n");
        goto error;
    }

    HAL_INFO("stream", "UDP streaming initialized on port %d\n", g_udp_ctx->port);
    if (g_udp_ctx->is_mcast)
    {
        char ip_str[INET_ADDRSTRLEN];
        struct in_addr addr;
        addr.s_addr = g_udp_ctx->mcast_addr;
        inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
        HAL_INFO("stream", "UDP multicast streaming to %s\n", ip_str);
    }

    return EXIT_SUCCESS;

error:
    if (g_udp_ctx)
    {
        if (g_udp_ctx->socket_fd >= 0)
            close(g_udp_ctx->socket_fd);
        pthread_mutex_destroy(&g_udp_ctx->mutex);
        free(g_udp_ctx);
        g_udp_ctx = NULL;
    }

    return EXIT_FAILURE;
}

/**
 * Closes and cleans up the UDP streaming module
 */
void udp_stream_close()
{
    if (!g_udp_ctx)
        return;

    g_udp_ctx->running = 0;
    pthread_join(g_udp_ctx->thread, NULL);

    close(g_udp_ctx->socket_fd);

    pthread_mutex_destroy(&g_udp_ctx->mutex);

    free(g_udp_ctx);
    g_udp_ctx = NULL;

    HAL_INFO("stream", "UDP streaming closed\n");
}

/**
 * Adds a new UDP client
 * @param host Client hostname or IP address
 * @param port Client port
 * @return Client ID or -1 on error
 */
int udp_stream_add_client(const char *host, unsigned short port)
{
    if (!g_udp_ctx)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0)
    {
        HAL_DANGER("stream", "Invalid address: %s\n", host);
        return -1;
    }

    pthread_mutex_lock(&g_udp_ctx->mutex);

    for (int i = 0; i < UDP_MAX_CLIENTS; i++)
    {
        if (!g_udp_ctx->clients[i].active)
            continue;

        if (g_udp_ctx->clients[i].addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
            g_udp_ctx->clients[i].addr.sin_port == addr.sin_port)
        {
            g_udp_ctx->clients[i].last_act = time(NULL);
            pthread_mutex_unlock(&g_udp_ctx->mutex);
            return i;
        }
    }

    for (int i = 0; i < UDP_MAX_CLIENTS; i++)
    {
        if (g_udp_ctx->clients[i].active)
            continue;

        g_udp_ctx->clients[i] = (udp_client_t){
            .addr = addr,
            .active = 1,
            .ssrc = rand(),
            .seq = rand() & 0xFFFF,
            .tstamp = rand(),
            .last_act = time(NULL)};

        g_udp_ctx->client_count++;

        HAL_INFO("stream", "Added UDP client %s:%d (ID %d)\n",
                 host, port, i);

        pthread_mutex_unlock(&g_udp_ctx->mutex);
        return i;
    }

    HAL_DANGER("stream", "Maximum number of UDP clients reached!\n");
    pthread_mutex_unlock(&g_udp_ctx->mutex);
    return -1;
}

/**
 * Deletes a UDP client
 * @param client_id Client ID to be removed
 */
void udp_stream_remove_client(int client_id)
{
    if (!g_udp_ctx)
        return;
    if (client_id < 0 || client_id >= UDP_MAX_CLIENTS)
        return;

    pthread_mutex_lock(&g_udp_ctx->mutex);

    if (g_udp_ctx->clients[client_id].active)
    {
        char ip_str[INET_ADDRSTRLEN];
        struct sockaddr_in *addr = &g_udp_ctx->clients[client_id].addr;
        uint16_t port = ntohs(addr->sin_port);

        inet_ntop(AF_INET, &addr->sin_addr, ip_str, INET_ADDRSTRLEN);
        g_udp_ctx->clients[client_id].active = 0;
        g_udp_ctx->client_count--;

        HAL_INFO("stream", "Removed UDP client %s:%d (ID %d)\n",
                 ip_str, port, client_id);
    }

    pthread_mutex_unlock(&g_udp_ctx->mutex);
}

/**
 * Send a RTP-encapsulated NAL unit to all clients
 * @param nal_data NAL unit data
 * @param nal_size Size of the NAL unit
 * @param timestamp Timestamp of the NAL unit
 * @param is_keyframe Indicates if the NAL unit is a keyframe
 * @param is_h265 Indicates if the NAL unit is using the H.265 codec
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int udp_stream_send_nal(const char *nal_data_ext, int nal_size_ext, unsigned int timestamp,
                        int is_keyframe, int is_h265)
{
    char *nal_data = nal_data_ext;
    int nal_size = nal_size_ext;
    if (!g_udp_ctx || !nal_data || nal_size <= 0)
        return EXIT_FAILURE;

    if (nal_data[0] == 0 && nal_data[1] == 0 &&
        nal_data[2] == 0 && nal_data[3] == 1)
    {
        nal_data = &nal_data[4];
        nal_size -= 4;
    }
    else if (nal_data[0] == 0 && nal_data[1] == 0 &&
             nal_data[2] == 1)
    {
        nal_data = &nal_data[3];
        nal_size -= 3;
    }

    unsigned char nal_type;

    if (is_h265)
    {
        nal_type = (nal_data[0] >> 1) & 0x3F;
    }
    else
    {
        nal_type = nal_data[0] & 0x1F;
    }
    int is_VPS_SPS_PPS_SEI = (nal_type == 32 || nal_type == 33 || nal_type == 34 || nal_type == 39) ? 1 : 0;

    static unsigned char packet[MAX_UDP_PACKET_SIZE];
    int payload_type = 96;
    int total_clients = 0;

    pthread_mutex_lock(&g_udp_ctx->mutex);
    total_clients = g_udp_ctx->client_count;
    pthread_mutex_unlock(&g_udp_ctx->mutex);

    if (total_clients == 0 && !g_udp_ctx->is_mcast)
        return EXIT_SUCCESS;

    if (nal_size + 1 + RTP_HEADER_SIZE <= MAX_UDP_PACKET_SIZE)
    {
        // HAL_INFO("stream", "single packet\n");
        pthread_mutex_lock(&g_udp_ctx->mutex);

        if (g_udp_ctx->is_mcast)
        {
            struct sockaddr_in mcast_addr;
            memset(&mcast_addr, 0, sizeof(mcast_addr));
            mcast_addr.sin_family = AF_INET;
            mcast_addr.sin_addr.s_addr = g_udp_ctx->mcast_addr;
            mcast_addr.sin_port = htons(g_udp_ctx->port);

            int packet_size = add_rtp_header(packet, nal_size, 0, timestamp, 0, !is_VPS_SPS_PPS_SEI, payload_type);

            memcpy(packet + RTP_HEADER_SIZE, nal_data, nal_size);
            packet_size += nal_size;

            // HAL_INFO("stream", "single packet mcast send %d\n",packet_size);
            sendto(g_udp_ctx->socket_fd, packet, packet_size, 0,
                   (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
        }
        else
        {
            for (int i = 0; i < UDP_MAX_CLIENTS; i++)
            {
                if (!g_udp_ctx->clients[i].active)
                    continue;

                int packet_size = add_rtp_header(packet, nal_size, g_udp_ctx->clients[i].seq++, timestamp, g_udp_ctx->clients[i].ssrc, !is_VPS_SPS_PPS_SEI, payload_type);

                memcpy(packet + RTP_HEADER_SIZE, nal_data, nal_size);
                packet_size += nal_size;

                // HAL_INFO("stream", "single packet !mcast send %d\n",packet_size);
                sendto(g_udp_ctx->socket_fd, packet, packet_size, 0,
                       (struct sockaddr *)&g_udp_ctx->clients[i].addr,
                       sizeof(struct sockaddr_in));
            }
        }
        pthread_mutex_unlock(&g_udp_ctx->mutex);
    }
    else
    {
        // HAL_INFO("stream", "more packet\n");
        int nal_header_size = is_h265 ? 2 : 1;

        int fragments = (nal_size - nal_header_size) / (MAX_UDP_PACKET_SIZE - nal_header_size - RTP_HEADER_SIZE - 1) + 1;
        int bytes_left = nal_size - nal_header_size;
        const unsigned char *data_ptr = nal_data + nal_header_size;

        pthread_mutex_lock(&g_udp_ctx->mutex);

        for (int i = 0; i < UDP_MAX_CLIENTS || g_udp_ctx->is_mcast; i++)
        {
            if (g_udp_ctx->is_mcast || g_udp_ctx->clients[i].active)
            {
                int remaining = bytes_left;
                const unsigned char *frag_ptr = data_ptr;
                unsigned short seq = g_udp_ctx->is_mcast ? 0 : g_udp_ctx->clients[i].seq;
                unsigned int ssrc = g_udp_ctx->is_mcast ? 0 : g_udp_ctx->clients[i].ssrc;

                // unsigned long long timestamp = 0;
                for (int frag = 0; frag < fragments; frag++)
                {
                    int is_first = (frag == 0);
                    int is_last = (frag == fragments - 1);

                    // if (is_first)
                    // {
                    //     timestamp= get_timestamp_us();
                    // }

                    // HAL_INFO("stream", "is_last:%d\n",is_last);
                    int payload_size = (is_last) ? remaining : (MAX_UDP_PACKET_SIZE - nal_header_size - RTP_HEADER_SIZE - 1);
                    // HAL_INFO("stream", "payload_size:%d\n", payload_size);
                    int packet_size = 0;
                    //HAL_INFO("stream", "is_h265:%d\n", is_h265);
                    if (is_h265)
                    {
                        // fu_indicator is 2 bytes
                        packet[RTP_HEADER_SIZE] = 49 << 1;                                                     // fu_indicator[0],FU type=49
                        packet[RTP_HEADER_SIZE + 1] = nal_data[1];                                             // fu_indicator[1],h265 need nal_data[1]
                        packet[RTP_HEADER_SIZE + 2] = (is_first ? 0x80 : 0) | (is_last ? 0x40 : 0) | nal_type; // fu_header
                    }
                    else
                    {
                        // fu_indicator is 1 bytes
                        packet[RTP_HEADER_SIZE] = (nal_data[0] & 0xE0) | 28;                                   // fu_indicator[0],FU-A type=28
                        packet[RTP_HEADER_SIZE + 1] = (is_first ? 0x80 : 0) | (is_last ? 0x40 : 0) | nal_type; // fu_header
                    }

                    packet_size = add_rtp_header(packet, payload_size + nal_header_size + 1, seq++, timestamp, ssrc, is_last, payload_type);
                    packet_size += payload_size + nal_header_size + 1;
                    memcpy(packet + RTP_HEADER_SIZE + nal_header_size + 1, frag_ptr, payload_size);

                    frag_ptr += payload_size;
                    remaining -= payload_size;

                    // HAL_INFO("stream", "UDP packet size:%d\n",packet_size);
                    if (g_udp_ctx->is_mcast)
                    {
                        struct sockaddr_in mcast_addr;
                        memset(&mcast_addr, 0, sizeof(mcast_addr));
                        mcast_addr.sin_family = AF_INET;
                        mcast_addr.sin_addr.s_addr = g_udp_ctx->mcast_addr;
                        mcast_addr.sin_port = htons(g_udp_ctx->port);

                        // HAL_INFO("stream", "more packet mcast send %d\n",packet_size);
                        sendto(g_udp_ctx->socket_fd, packet, packet_size, 0,
                               (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
                    }
                    else
                    {
                        // HAL_INFO("stream", "more packet !mcast send %d\n",packet_size);
                        sendto(g_udp_ctx->socket_fd, packet, packet_size, 0,
                               (struct sockaddr *)&g_udp_ctx->clients[i].addr,
                               sizeof(struct sockaddr_in));
                    }

                    // if (is_last)
                    // {
                    //     timestamp= get_timestamp_us()-timestamp;
                    //     HAL_INFO("stream", "more packet send time %d\n",timestamp);
                    // }
                    // usleep(100);
                }

                if (!g_udp_ctx->is_mcast)
                {
                    g_udp_ctx->clients[i].seq = seq;
                }
            }

            if (g_udp_ctx->is_mcast)
                break;
        }

        pthread_mutex_unlock(&g_udp_ctx->mutex);
    }

    pthread_mutex_lock(&g_udp_ctx->mutex);
    for (int i = 0; i < UDP_MAX_CLIENTS; i++)
    {
        if (!g_udp_ctx->clients[i].active)
            continue;
        g_udp_ctx->clients[i].tstamp = timestamp;
    }
    pthread_mutex_unlock(&g_udp_ctx->mutex);

    return EXIT_SUCCESS;
}

/**
 * Thread handler for managing UDP clients (inactivity check)
 */
static void *udp_client_manager_thread(void *data)
{
    struct udp_stream_ctx *ctx = (struct udp_stream_ctx *)data;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[1024];
    int recv_len;
    time_t now;

    while (ctx->running)
    {
        now = time(NULL);

        pthread_mutex_lock(&ctx->mutex);
        for (int i = 0; i < UDP_MAX_CLIENTS; i++)
        {
            if (ctx->clients[i].active)
            {
                // if (difftime(now, ctx->clients[i].last_act) > 60)
                // {
                //     ctx->clients[i].active = 0;
                //     ctx->client_count--;

                //     char ip_str[INET_ADDRSTRLEN];
                //     inet_ntop(AF_INET, &ctx->clients[i].addr.sin_addr,
                //               ip_str, INET_ADDRSTRLEN);
                //     HAL_INFO("stream", "Removed inactive UDP client %s:%d (ID %d)\n",
                //              ip_str, ntohs(ctx->clients[i].addr.sin_port), i);
                // }
            }
        }
        pthread_mutex_unlock(&ctx->mutex);

        recv_len = recvfrom(ctx->socket_fd, buffer, sizeof(buffer), 0,
                            (struct sockaddr *)&client_addr, &addr_len);

        if (recv_len > 0)
        {
            pthread_mutex_lock(&ctx->mutex);

            int client_found = 0;
            for (int i = 0; i < UDP_MAX_CLIENTS; i++)
            {
                if (!ctx->clients[i].active)
                    continue;

                if (ctx->clients[i].addr.sin_addr.s_addr == client_addr.sin_addr.s_addr &&
                    ctx->clients[i].addr.sin_port == client_addr.sin_port)
                {
                    ctx->clients[i].last_act = now;
                    client_found = 1;
                    break;
                }
            }

            if (!client_found && ctx->client_count < UDP_MAX_CLIENTS)
            {
                for (int i = 0; i < UDP_MAX_CLIENTS; i++)
                {
                    if (ctx->clients[i].active)
                        continue;

                    ctx->clients[i].addr = client_addr;
                    ctx->clients[i].active = 1;
                    ctx->clients[i].ssrc = rand();
                    ctx->clients[i].seq = rand() & 0xFFFF;
                    ctx->clients[i].tstamp = rand();
                    ctx->clients[i].last_act = now;

                    ctx->client_count++;

                    char ip_str[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
                    HAL_INFO("stream", "Auto-added UDP client %s:%d (ID %d)\n",
                             ip_str, ntohs(client_addr.sin_port), i);
                    break;
                }
            }

            pthread_mutex_unlock(&ctx->mutex);
        }

        usleep(500000);
    }

    return NULL;
}

/**
 * Prefixes the RTP header to a given packet
 */
static int add_rtp_header(unsigned char *packet, int pay_size,
                          unsigned short seq, unsigned int tstamp,
                          unsigned int ssrc, int marker, int pay_type)
{
    if (!packet || pay_size <= 0)
        return 0;

    // RTP Header (12 bytes)
    packet[0] = 0x80; // Version=2, Padding=0, Extension=0, CSRC count=0
    packet[1] = (marker ? 0x80 : 0x00) | (pay_type & 0x7F);

    // Sequence number (16 bits)
    packet[2] = (seq >> 8) & 0xFF;
    packet[3] = seq & 0xFF;

    // Timestamp (32 bits)
    packet[4] = (tstamp >> 24) & 0xFF;
    packet[5] = (tstamp >> 16) & 0xFF;
    packet[6] = (tstamp >> 8) & 0xFF;
    packet[7] = tstamp & 0xFF;

    // SSRC (32 bits)
    packet[8] = (ssrc >> 24) & 0xFF;
    packet[9] = (ssrc >> 16) & 0xFF;
    packet[10] = (ssrc >> 8) & 0xFF;
    packet[11] = ssrc & 0xFF;

    return RTP_HEADER_SIZE;
}

/**
 * Obtains a timestamp in microseconds
 */
static unsigned long long get_timestamp_us()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Standard JPEG luminance quantization table (Q=50 baseline) */
static const unsigned char std_lum_quant[64] = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99
};

/* Standard JPEG chrominance quantization table (Q=50 baseline) */
static const unsigned char std_chr_quant[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

/**
 * Send a JPEG frame over RTP using RFC 2435 payload format
 * @param jpeg_data JPEG frame data
 * @param jpeg_size Size of the JPEG frame
 * @param timestamp RTP timestamp
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @return EXIT_SUCCESS or EXIT_FAILURE
 */
int udp_stream_send_jpeg(const unsigned char *jpeg_data, int jpeg_size,
                         unsigned int timestamp, unsigned short width, unsigned short height)
{
    if (!g_udp_ctx || !jpeg_data || jpeg_size <= 0)
        return EXIT_FAILURE;

    int total_clients = 0;
    pthread_mutex_lock(&g_udp_ctx->mutex);
    total_clients = g_udp_ctx->client_count;
    pthread_mutex_unlock(&g_udp_ctx->mutex);

    if (total_clients == 0 && !g_udp_ctx->is_mcast)
        return EXIT_SUCCESS;

    unsigned int fragment_offset = 0;
    /* First packet has extra 68 bytes for quantization table header */
    unsigned int payload_space_first = MAX_UDP_PACKET_SIZE - RTP_HEADER_SIZE - 8 - 68;
    unsigned int payload_space = MAX_UDP_PACKET_SIZE - RTP_HEADER_SIZE - 8;

    while (fragment_offset < (unsigned int)jpeg_size)
    {
        unsigned int space = (fragment_offset == 0) ? payload_space_first : payload_space;
        unsigned int remaining = jpeg_size - fragment_offset;
        unsigned int is_last = (remaining <= space);
        unsigned int frag_size = is_last ? remaining : space;

        pthread_mutex_lock(&g_udp_ctx->mutex);

        if (g_udp_ctx->is_mcast)
        {
            struct sockaddr_in mcast_addr;
            memset(&mcast_addr, 0, sizeof(mcast_addr));
            mcast_addr.sin_family = AF_INET;
            mcast_addr.sin_addr.s_addr = g_udp_ctx->mcast_addr;
            mcast_addr.sin_port = htons(g_udp_ctx->port);

            unsigned char packet[MAX_UDP_PACKET_SIZE];
            int rtp_size = add_rtp_header(packet, 0, 0, timestamp, 0, is_last, 26);

            /* JPEG Main Header (8 bytes) */
            packet[rtp_size++] = 0x00;  /* Type Specific */
            packet[rtp_size++] = (fragment_offset >> 16) & 0xFF;
            packet[rtp_size++] = (fragment_offset >> 8) & 0xFF;
            packet[rtp_size++] = fragment_offset & 0xFF;
            packet[rtp_size++] = 0xFF;  /* Q = 0xFF: custom quantization tables follow */
            packet[rtp_size++] = (width + 7) / 8;   /* Width in 8-pixel units */
            packet[rtp_size++] = (height + 7) / 8;  /* Height in 8-pixel units */

            if (fragment_offset == 0)
            {
                /* Quantization Table Header (68 bytes) */
                packet[rtp_size++] = 0x00;  /* MBZ */
                packet[rtp_size++] = 0x00;  /* Precision (8-bit) */
                packet[rtp_size++] = 0x00;  /* Length high */
                packet[rtp_size++] = 0x80;  /* Length low (128 = 64+64) */
                memcpy(packet + rtp_size, std_lum_quant, 64);
                rtp_size += 64;
                memcpy(packet + rtp_size, std_chr_quant, 64);
                rtp_size += 64;
            }

            memcpy(packet + rtp_size, jpeg_data + fragment_offset, frag_size);
            rtp_size += frag_size;

            sendto(g_udp_ctx->socket_fd, packet, rtp_size, 0,
                   (struct sockaddr *)&mcast_addr, sizeof(mcast_addr));
        }
        else
        {
            for (int i = 0; i < UDP_MAX_CLIENTS; i++)
            {
                if (!g_udp_ctx->clients[i].active)
                    continue;

                unsigned char packet[MAX_UDP_PACKET_SIZE];
                unsigned short seq = g_udp_ctx->clients[i].seq++;
                int rtp_size = add_rtp_header(packet, 0, seq, timestamp,
                                               g_udp_ctx->clients[i].ssrc, is_last, 26);

                /* JPEG Main Header (8 bytes) */
                packet[rtp_size++] = 0x00;  /* Type Specific */
                packet[rtp_size++] = (fragment_offset >> 16) & 0xFF;
                packet[rtp_size++] = (fragment_offset >> 8) & 0xFF;
                packet[rtp_size++] = fragment_offset & 0xFF;
                packet[rtp_size++] = 0xFF;  /* Q = 0xFF: custom quantization tables */
                packet[rtp_size++] = (width + 7) / 8;
                packet[rtp_size++] = (height + 7) / 8;

                if (fragment_offset == 0)
                {
                    /* Quantization Table Header (68 bytes) */
                    packet[rtp_size++] = 0x00;
                    packet[rtp_size++] = 0x00;
                    packet[rtp_size++] = 0x00;
                    packet[rtp_size++] = 0x80;
                    memcpy(packet + rtp_size, std_lum_quant, 64);
                    rtp_size += 64;
                    memcpy(packet + rtp_size, std_chr_quant, 64);
                    rtp_size += 64;
                }

                memcpy(packet + rtp_size, jpeg_data + fragment_offset, frag_size);
                rtp_size += frag_size;

                sendto(g_udp_ctx->socket_fd, packet, rtp_size, 0,
                       (struct sockaddr *)&g_udp_ctx->clients[i].addr,
                       sizeof(struct sockaddr_in));
            }
        }

        pthread_mutex_unlock(&g_udp_ctx->mutex);
        fragment_offset += frag_size;
    }

    return EXIT_SUCCESS;
}