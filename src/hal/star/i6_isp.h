#pragma once

#include "i6_common.h"
typedef enum __attribute__((aligned(4)))
{
    SS_AE_MODE_A,  // auto
    SS_AE_MODE_AV, // aperture priority
    SS_AE_MODE_SV, // 增益先决模式
    SS_AE_MODE_TV, // shutter priority
    SS_AE_MODE_M,  // manual mode
    SS_AE_MODE_MAX
} i6_ISP_AE_MODE_TYPE_e;

typedef struct
{
    unsigned int u32FNx10;      // 光圈值×10 (ex：光圈F1.8则此项等于18)
    unsigned int u32SensorGain; // Sensor增益值 (含sensor类比增益与sensor数位增益，1024等于1倍)
    unsigned int u32ISPGain;    // ISP数字增益(1024等于1倍)
    unsigned int u32US;         // 曝光时间(μsec)
} i6_ISP_AE_EXPO_VALUE_TYPE_t;

typedef struct
{
    void *handle, *handleCus3a, *handleIspAlgo;

    int (*fnLoadChannelConfig)(int channel, char *path, unsigned int key);
    int (*fnSetColorToGray)(int channel, char *enable);
    int (*fnSetExpoMode)(int channel, i6_ISP_AE_MODE_TYPE_e *data);
    int (*fnGetExpoMode)(int channel, i6_ISP_AE_MODE_TYPE_e *data);
    int (*fnSetManualExpo)(int channel, i6_ISP_AE_EXPO_VALUE_TYPE_t *data);
    int (*fnGetManualExpo)(int channel, i6_ISP_AE_EXPO_VALUE_TYPE_t *data);
} i6_isp_impl;

static int i6_isp_load(i6_isp_impl *isp_lib)
{
    isp_lib->handleIspAlgo = dlopen("libispalgo.so", RTLD_LAZY | RTLD_GLOBAL);

    isp_lib->handleCus3a = dlopen("libcus3a.so", RTLD_LAZY | RTLD_GLOBAL);

    if (!(isp_lib->handle = dlopen("libmi_isp.so", RTLD_LAZY | RTLD_GLOBAL)))
    {
        HAL_ERROR("i6_isp", "Failed to load library!\nError: %s\n", dlerror());
        return EXIT_FAILURE;
    }
    
    LOAD_SYMBOL("i6_isp", isp_lib->handle, "MI_ISP_API_CmdLoadBinFile", isp_lib->fnLoadChannelConfig);
    LOAD_SYMBOL("i6_isp", isp_lib->handle, "MI_ISP_IQ_SetColorToGray", isp_lib->fnSetColorToGray);
    LOAD_SYMBOL("i6_isp", isp_lib->handle, "MI_ISP_AE_SetExpoMode", isp_lib->fnSetExpoMode);
    LOAD_SYMBOL("i6_isp", isp_lib->handle, "MI_ISP_AE_GetExpoMode", isp_lib->fnGetExpoMode);
    LOAD_SYMBOL("i6_isp", isp_lib->handle, "MI_ISP_AE_SetManualExpo", isp_lib->fnSetManualExpo);
    LOAD_SYMBOL("i6_isp", isp_lib->handle, "MI_ISP_AE_GetManualExpo", isp_lib->fnGetManualExpo);

    return EXIT_SUCCESS;
}

static void i6_isp_unload(i6_isp_impl *isp_lib)
{
    if (isp_lib->handle)
        dlclose(isp_lib->handle);
    isp_lib->handle = NULL;
    if (isp_lib->handleCus3a)
        dlclose(isp_lib->handleCus3a);
    isp_lib->handleCus3a = NULL;
    if (isp_lib->handleIspAlgo)
        dlclose(isp_lib->handleIspAlgo);
    isp_lib->handleIspAlgo = NULL;
    memset(isp_lib, 0, sizeof(*isp_lib));
}