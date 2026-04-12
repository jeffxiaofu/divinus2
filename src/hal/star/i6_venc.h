#pragma once

#include "i6_common.h"

#define I6_VENC_CHN_NUM 9
#define RC_TEXTURE_THR_SIZE 1
typedef enum
{
    I6_VENC_CODEC_H264 = 2,
    I6_VENC_CODEC_H265,
    I6_VENC_CODEC_MJPG,
    I6_VENC_CODEC_END
} i6_venc_codec;

typedef enum
{
    I6_VENC_NALU_H264_PSLICE = 1,
    I6_VENC_NALU_H264_ISLICE = 5,
    I6_VENC_NALU_H264_SEI,
    I6_VENC_NALU_H264_SPS,
    I6_VENC_NALU_H264_PPS,
    I6_VENC_NALU_H264_IPSLICE,
    I6_VENC_NALU_H264_PREFIX = 14,
    I6_VENC_NALU_H264_END
} i6_venc_nalu_h264;

typedef enum
{
    I6_VENC_NALU_H265_PSLICE = 1,
    I6_VENC_NALU_H265_ISLICE = 19,
    I6_VENC_NALU_H265_VPS = 32,
    I6_VENC_NALU_H265_SPS,
    I6_VENC_NALU_H265_PPS,
    I6_VENC_NALU_H265_SEI = 39,
    I6_VENC_NALU_H265_END
} i6_venc_nalu_h265;

typedef enum
{
    I6_VENC_NALU_MJPG_ECS = 5,
    I6_VENC_NALU_MJPG_APP,
    I6_VENC_NALU_MJPG_VDO,
    I6_VENC_NALU_MJPG_PIC,
    I6_VENC_NALU_MJPG_END
} i6_venc_nalu_mjpg;

typedef enum
{
    I6_VENC_SRC_CONF_NORMAL,
    I6_VENC_SRC_CONF_RING_ONE,
    I6_VENC_SRC_CONF_RING_HALF,
    I6_VENC_SRC_CONF_END
} i6_venc_src_conf;

typedef enum
{
    I6_VENC_RATEMODE_H264CBR = 1,
    I6_VENC_RATEMODE_H264VBR,
    I6_VENC_RATEMODE_H264ABR,
    I6_VENC_RATEMODE_H264QP,
    I6_VENC_RATEMODE_H264AVBR,
    I6_VENC_RATEMODE_MJPGCBR,
    I6_VENC_RATEMODE_MJPGQP,
    I6_VENC_RATEMODE_H265CBR,
    I6_VENC_RATEMODE_H265VBR,
    I6_VENC_RATEMODE_H265QP,
    I6_VENC_RATEMODE_H265AVBR,
    I6_VENC_RATEMODE_END
} i6_venc_ratemode;

typedef enum
{
    I6OG_VENC_RATEMODE_H264CBR = 1,
    I6OG_VENC_RATEMODE_H264VBR,
    I6OG_VENC_RATEMODE_H264QP,
    I6OG_VENC_RATEMODE_H264AVBR,
    I6OG_VENC_RATEMODE_MJPGCBR,
    I6OG_VENC_RATEMODE_MJPGQP,
    I6OG_VENC_RATEMODE_H265CBR,
    I6OG_VENC_RATEMODE_H265VBR,
    I6OG_VENC_RATEMODE_H265QP,
    I6OG_VENC_RATEMODE_H265AVBR,
    I6OG_VENC_RATEMODE_END
} i6og_venc_ratemode;

typedef struct
{
    unsigned int maxWidth;
    unsigned int maxHeight;
    unsigned int bufSize;
    unsigned int profile;
    char byFrame;
    unsigned int width;
    unsigned int height;
    unsigned int bFrameNum;
    unsigned int refNum;
} i6_venc_attr_h26x;

typedef struct
{
    unsigned int maxWidth;
    unsigned int maxHeight;
    unsigned int bufSize;
    char byFrame;
    unsigned int width;
    unsigned int height;
    char dcfThumbs;
    unsigned int markPerRow;
} i6_venc_attr_mjpg;

typedef struct
{
    i6_venc_codec codec;
    union
    {
        i6_venc_attr_h26x h264;
        i6_venc_attr_mjpg mjpg;
        i6_venc_attr_h26x h265;
    };
} i6_venc_attrib;

typedef struct
{
    unsigned int gop;
    unsigned int statTime;
    unsigned int fpsNum;
    unsigned int fpsDen;
    unsigned int bitrate;
    unsigned int avgLvl;
} i6_venc_rate_h26xcbr;

typedef struct
{
    unsigned int gop;
    unsigned int statTime;
    unsigned int fpsNum;
    unsigned int fpsDen;
    unsigned int maxBitrate;
    unsigned int maxQual;
    unsigned int minQual;
} i6_venc_rate_h26xvbr;

typedef struct
{
    unsigned int gop;
    unsigned int fpsNum;
    unsigned int fpsDen;
    unsigned int interQual;
    unsigned int predQual;
} i6_venc_rate_h26xqp;

typedef struct
{
    unsigned int gop;
    unsigned int statTime;
    unsigned int fpsNum;
    unsigned int fpsDen;
    unsigned int avgBitrate;
    unsigned int maxBitrate;
} i6_venc_rate_h26xabr;

typedef struct
{
    unsigned int bitrate;
    unsigned int fpsNum;
    unsigned int fpsDen;
} i6_venc_rate_mjpgcbr;

typedef struct
{
    unsigned int fpsNum;
    unsigned int fpsDen;
    unsigned int quality;
} i6_venc_rate_mjpgqp;

typedef struct
{
    i6_venc_ratemode mode;
    union
    {
        i6_venc_rate_h26xcbr h264Cbr;
        i6_venc_rate_h26xvbr h264Vbr;
        i6_venc_rate_h26xqp h264Qp;
        i6_venc_rate_h26xabr h264Abr;
        i6_venc_rate_h26xvbr h264Avbr;
        i6_venc_rate_mjpgcbr mjpgCbr;
        i6_venc_rate_mjpgqp mjpgQp;
        i6_venc_rate_h26xcbr h265Cbr;
        i6_venc_rate_h26xvbr h265Vbr;
        i6_venc_rate_h26xqp h265Qp;
        i6_venc_rate_h26xvbr h265Avbr;
    };
    void *extend;
} i6_venc_rate;

typedef struct
{
    int s32IPQPDelta;
    int s32ChangePos;
    unsigned int u32MaxIQp;
    unsigned int u32MinIQp;
    unsigned int u32MaxIPProp;
} i6_venc_ParamH264Vbr_t;

typedef struct
{
    unsigned int u32MaxQp;
    unsigned int u32MinQp;
    int s32IPQPDelta;
    unsigned int u32MaxIQp;
    unsigned int u32MinIQp;
    unsigned int u32MaxIPProp;
} i6_venc_ParamH264Cbr_t;

typedef struct
{
    int s32IPQPDelta;
    int s32ChangePos;
    unsigned int u32MinIQp;
    unsigned int u32MaxIPProp;
    unsigned int u32MaxIQp;
    unsigned int u32MaxISize;
    unsigned int u32MaxPSize;
    unsigned int u32MinStillPercent;
    unsigned int u32MaxStillQp;
    unsigned int u32MotionSensitivity;
} i6_venc_ParamH264Avbr_t;

typedef struct
{
    unsigned int u32MaxQfactor;
    unsigned int u32MinQfactor;
} i6_venc_ParamMjpegCbr_t;

typedef struct
{
    int s32IPQPDelta;
    int s32ChangePos;
    unsigned int u32MaxIQp;
    unsigned int u32MinIQp;
    unsigned int u32MaxIPProp;
} i6_venc_ParamH265Vbr_t;

typedef struct
{
    unsigned int u32MaxQp;
    unsigned int u32MinQp;
    int s32IPQPDelta;
    unsigned int u32MaxIQp;
    unsigned int u32MinIQp;
    unsigned int u32MaxIPProp;
} i6_venc_ParamH265Cbr_t;

typedef struct
{
    int s32IPQPDelta;
    int s32ChangePos;
    unsigned int u32MinIQp;
    unsigned int u32MaxIPProp;
    unsigned int u32MaxIQp;
    unsigned int u32MaxISize;
    unsigned int u32MaxPSize;
    unsigned int u32MinStillPercent;
    unsigned int u32MaxStillQp;
    unsigned int u32MotionSensitivity;
} i6_venc_ParamH265Avbr_t;

typedef struct
{
    unsigned int au32ThrdI[RC_TEXTURE_THR_SIZE];
    unsigned int au32ThrdP[RC_TEXTURE_THR_SIZE];
    unsigned int u32RowQpDelta;
    union
    {
        i6_venc_ParamH264Cbr_t stParamH264Cbr;
        i6_venc_ParamH264Vbr_t stParamH264VBR;
        i6_venc_ParamH264Avbr_t stParamH264Avbr;
        i6_venc_ParamMjpegCbr_t stParamMjpegCbr;
        i6_venc_ParamH265Cbr_t stParamH265Cbr;
        i6_venc_ParamH265Vbr_t stParamH265Vbr;
        i6_venc_ParamH265Avbr_t stParamH265Avbr;
    };
    void *pRcParam;
} i6_venc_RcParam_t;

typedef struct
{
    i6_venc_attrib attrib;
    i6_venc_rate rate;
} i6_venc_chn;

typedef struct
{
    unsigned int quality;
    unsigned char qtLuma[64];
    unsigned char qtChroma[64];
    unsigned int mcuPerEcs;
} i6_venc_jpg;

typedef union
{
    i6_venc_nalu_h264 h264Nalu;
    i6_venc_nalu_mjpg mjpgNalu;
    i6_venc_nalu_h265 h265Nalu;
} i6_venc_nalu;

typedef struct
{
    i6_venc_nalu packType;
    unsigned int offset;
    unsigned int length;
    unsigned int sliceId;
} i6_venc_packinfo;

typedef struct
{
    unsigned long long addr;
    unsigned char *data;
    unsigned int length;
    unsigned long long timestamp;
    char endFrame;
    i6_venc_nalu naluType;
    unsigned int offset;
    unsigned int packNum;
    i6_venc_packinfo packetInfo[8];
} i6_venc_pack;

typedef struct
{
    unsigned int leftPics;
    unsigned int leftBytes;
    unsigned int leftFrames;
    unsigned int leftMillis;
    unsigned int curPacks;
    unsigned int leftRecvPics;
    unsigned int leftEncPics;
    unsigned int fpsNum;
    unsigned int fpsDen;
    unsigned int bitrate;
} i6_venc_stat;

typedef struct
{
    unsigned int size;
    unsigned int skipMb;
    unsigned int ipcmMb;
    unsigned int iMb16x8;
    unsigned int iMb16x16;
    unsigned int iMb8x16;
    unsigned int iMb8x8;
    unsigned int pMb16;
    unsigned int pMb8;
    unsigned int pMb4;
    unsigned int refSliceType;
    unsigned int refType;
    unsigned int updAttrCnt;
    unsigned int startQual;
} i6_venc_strminfo_h264;

typedef struct
{
    unsigned int size;
    unsigned int iCu64x64;
    unsigned int iCu32x32;
    unsigned int iCu16x16;
    unsigned int iCu8x8;
    unsigned int pCu32x32;
    unsigned int pCu16x16;
    unsigned int pCu8x8;
    unsigned int pCu4x4;
    unsigned int refType;
    unsigned int updAttrCnt;
    unsigned int startQual;
} i6_venc_strminfo_h265;

typedef struct
{
    unsigned int size;
    unsigned int updAttrCnt;
    unsigned int quality;
} i6_venc_strminfo_mjpg;

typedef struct
{
    i6_venc_pack *packet;
    unsigned int count;
    unsigned int sequence;
    int handle;
    union
    {
        i6_venc_strminfo_h264 h264Info;
        i6_venc_strminfo_mjpg mjpgInfo;
        i6_venc_strminfo_h265 h265Info;
    };
} i6_venc_strm;

typedef struct
{
    char Enable;
    unsigned int RefreshLineNum;
    unsigned int ReqIQp;
} i6_venc_IntraRefresh;

#define I6_VENC_ROI_MAX_REGIONS 8

typedef struct
{
    unsigned int u32Index;
    unsigned int bEnable;
    int s32QpDelta;
    i6_common_rect stRect;
} i6_venc_roi_attr;

typedef struct
{
    void *handle;

    int (*fnCreateChannel)(int channel, i6_venc_chn *config);
    int (*fnDestroyChannel)(int channel);
    int (*fnGetChannelConfig)(int channel, i6_venc_chn *config);
    int (*fnGetChannelDeviceId)(int channel, unsigned int *device);
    int (*fnResetChannel)(int channel);
    int (*fnSetChannelConfig)(int channel, i6_venc_chn *config);

    int (*fnSetRcParam)(int channel, i6_venc_RcParam_t *pstRcParam);
    int (*fnGetRcParam)(int channel, i6_venc_RcParam_t *pstRcParam);

    int (*fnFreeDescriptor)(int channel);
    int (*fnGetDescriptor)(int channel);

    int (*fnGetJpegParam)(int channel, i6_venc_jpg *param);
    int (*fnSetJpegParam)(int channel, i6_venc_jpg *param);

    int (*fnFreeStream)(int channel, i6_venc_strm *stream);
    int (*fnGetStream)(int channel, i6_venc_strm *stream, unsigned int timeout);

    int (*fnQuery)(int channel, i6_venc_stat *stats);

    int (*fnSetSourceConfig)(int channel, i6_venc_src_conf *config);

    int (*fnSetIntraRefresh)(int channel, i6_venc_IntraRefresh *config);
    int (*fnGetIntraRefresh)(int channel, i6_venc_IntraRefresh *config);

    int (*fnSetRoiAttr)(int channel, i6_venc_roi_attr *attr);

    int (*fnRequestIdr)(int channel, char instant);
    int (*fnStartReceiving)(int channel);
    int (*fnStartReceivingEx)(int channel, int *count);
    int (*fnStopReceiving)(int channel);
} i6_venc_impl;

static int i6_venc_load(i6_venc_impl *venc_lib)
{
    if (!(venc_lib->handle = dlopen("libmi_venc.so", RTLD_LAZY | RTLD_GLOBAL)))
    {
        HAL_ERROR("i6_venc", "Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_CreateChn", venc_lib->fnCreateChannel);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_DestroyChn", venc_lib->fnDestroyChannel);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetChnAttr", venc_lib->fnGetChannelConfig);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetChnDevid", venc_lib->fnGetChannelDeviceId);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_ResetChn", venc_lib->fnResetChannel);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_SetChnAttr", venc_lib->fnSetChannelConfig);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_SetRcParam", venc_lib->fnSetRcParam);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetRcParam", venc_lib->fnGetRcParam);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_CloseFd", venc_lib->fnFreeDescriptor);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetFd", venc_lib->fnGetDescriptor);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetJpegParam", venc_lib->fnGetJpegParam);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_SetJpegParam", venc_lib->fnSetJpegParam);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_ReleaseStream", venc_lib->fnFreeStream);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetStream", venc_lib->fnGetStream);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_Query", venc_lib->fnQuery);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_SetInputSourceConfig", venc_lib->fnSetSourceConfig);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_SetIntraRefresh", venc_lib->fnSetIntraRefresh);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_GetIntraRefresh", venc_lib->fnGetIntraRefresh);
    {
        // Try multiple possible ROI symbol names across MStar SDK versions
        static const char *roi_syms[] = {
            "MI_VENC_SetRoiAttr",
            "MI_VENC_SetRoiCfg",
            "MI_VENC_SetRoi",
        };
        venc_lib->fnSetRoiAttr = NULL;
        for (int i = 0; i < 3; i++) {
            venc_lib->fnSetRoiAttr = (int(*)(int, i6_venc_roi_attr*))
                dlsym(venc_lib->handle, roi_syms[i]);
            if (venc_lib->fnSetRoiAttr) {
                HAL_INFO("i6_venc", "ROI symbol found: %s\n", roi_syms[i]);
                break;
            }
        }
        if (!venc_lib->fnSetRoiAttr)
            HAL_INFO("i6_venc", "No ROI symbol found in libmi_venc.so\n");
    }
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_RequestIdr", venc_lib->fnRequestIdr);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_StartRecvPic", venc_lib->fnStartReceiving);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_StartRecvPicEx", venc_lib->fnStartReceivingEx);
    LOAD_SYMBOL("i6_venc", venc_lib->handle, "MI_VENC_StopRecvPic", venc_lib->fnStopReceiving);

    return EXIT_SUCCESS;
}

static void i6_venc_unload(i6_venc_impl *venc_lib)
{
    if (venc_lib->handle)
        dlclose(venc_lib->handle);
    venc_lib->handle = NULL;
    memset(venc_lib, 0, sizeof(*venc_lib));
}