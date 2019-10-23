#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <arpa/inet.h>
//封装格式
#include "libavformat/avformat.h"
//解码
#include "libavcodec/avcodec.h"
//缩放
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

#include "mi_common_datatype.h"
#include "mi_sys.h"
#include "mi_ao.h"

//sstar sdk lib
#include "mi_vdec.h"
#include "mi_vdec_datatype.h"
#include "mi_common.h"
#include "mi_common_datatype.h"
#include "mi_sys.h"
#include "mi_sys_datatype.h"
//#include "mi_divp.h"
//#include "mi_divp_datatype.h"
#include "mi_hdmi.h"
#include "mi_hdmi_datatype.h"
#include "mi_disp.h"
#include "mi_disp_datatype.h"

#include "sstardisp.h"

//sdk audio input/outout param
#define     AUDIO_INPUT_SAMPRATE       48000
#define     AUDIO_INPUT_CHLAYOUT       AV_CH_LAYOUT_MONO
#define     AUDIO_INPUT_SAMPFMT        AV_SAMPLE_FMT_S16

#define     AUDIO_OUTPUT_SAMPRATE       E_MI_AUDIO_SAMPLE_RATE_48000
#define     AUDIO_OUTPUT_CHLAYOUT       E_MI_AUDIO_SOUND_MODE_MONO
#define     AUDIO_OUTPUT_SAMPFMT        E_MI_AUDIO_BIT_WIDTH_16


//func return value
#define     SUCCESS     0
#define     FAIL        1

typedef struct ST_Sys_BindInfo_s
{
    MI_SYS_ChnPort_t stSrcChnPort;
    MI_SYS_ChnPort_t stDstChnPort;
    MI_U32 u32SrcFrmrate;
    MI_U32 u32DstFrmrate;
} ST_Sys_BindInfo_t;

#define STCHECKRESULT(result)\
    if (result != MI_SUCCESS)\
    {\
        printf("[%s %d]exec function failed\n", __FUNCTION__, __LINE__);\
        return 1;\
    }\
    else\
    {\
        printf("(%s %d)exec function pass\n", __FUNCTION__,__LINE__);\
    }

#define ExecFunc(func, _ret_) \
    printf("%d Start test: %s\n", __LINE__, #func);\
    if (func != _ret_)\
    {\
        printf("AO_TEST [%d] %s exec function failed\n",__LINE__, #func);\
        return 1;\
    }\
    else\
    {\
        printf("AO_TEST [%d] %s  exec function pass\n", __LINE__, #func);\
    }\
    printf("%d End test: %s\n", __LINE__, #func);

#define MI_AUDIO_SAMPLE_PER_FRAME 1024

#define WAV_G711A 0x06
#define WAV_G711U 0x7
#define WAV_G726 0x45
#define WAV_PCM  0x1

#define G726_16 2
#define G726_24 3
#define G726_32 4
#define G726_40 5

#define DMA_BUF_SIZE_8K (8000)
#define DMA_BUF_SIZE_16K    (16000)
#define DMA_BUF_SIZE_32K    (32000)
#define DMA_BUF_SIZE_48K    (48000)
/*=============================================================*/
// Global Variable definition
/*=============================================================*/
// WAVE file header format
typedef struct _WavHeader_s{
    MI_U8   riff[4];                // RIFF string
    MI_U32  ChunkSize;              // overall size of file in bytes
    MI_U8   wave[4];                // WAVE string
    MI_U8   fmt_chunk_marker[4];    // fmt string with trailing null char
    MI_U32  length_of_fmt;          // length of the format data
    MI_U16  format_type;            // format type. 1-PCM, 3- IEEE float, 6 - 8bit A law, 7 - 8bit mu law
    MI_U16  channels;               // no.of channels
    MI_U32  sample_rate;            // sampling rate (blocks per second)
    MI_U32  byterate;               // SampleRate * NumChannels * BitsPerSample/8
    MI_U16  block_align;            // NumChannels * BitsPerSample/8
    MI_U16  bits_per_sample;        // bits per sample, 8- 8bits, 16- 16 bits etc
    MI_U8   data_chunk_header [4];  // DATA string or FLLR string
    MI_U32  data_size;              // NumSamples * NumChannels * BitsPerSample/8 - size of the next chunk that will be read
}_WavHeader_t;

typedef struct WAVE_FORMAT
{
    signed short wFormatTag;
    signed short wChannels;
    unsigned int  dwSamplesPerSec;
    unsigned int  dwAvgBytesPerSec;
    signed short wBlockAlign;
    signed short wBitsPerSample;
} WaveFormat_t;

typedef struct WAVEFILEHEADER
{
    char chRIFF[4];
    unsigned int  dwRIFFLen;
    char chWAVE[4];
    char chFMT[4];
    unsigned int  dwFMTLen;
    WaveFormat_t wave;
    char chDATA[4];
    unsigned int  dwDATALen;
} WaveFileHeader_t;


typedef enum
{
    E_SOUND_MODE_MONO =0, /* mono */
    E_SOUND_MODE_STERO =1, /* stereo */
}SoundMode_e;

typedef enum
{
    E_SAMPLE_RATE_8000 =8000, /* 8kHz sampling rate */
    E_SAMPLE_RATE_16000 =16000, /* 16kHz sampling rate */
    E_SAMPLE_RATE_32000 =32000, /* 32kHz sampling rate */
    E_SAMPLE_RATE_48000 =48000, /* 48kHz sampling rate */
}SampleRate_e;

typedef enum
{
    E_ANEC_TYPE_G711A = 0,
    E_ANEC_TYPE_G711U,
    E_AENC_TYPE_G726_16,
    E_AENC_TYPE_G726_24,
    E_AENC_TYPE_G726_32,
    E_AENC_TYPE_G726_40,
    PCM,
}AencType_e;


#define ALIGN_BACK(x, a)            (((x) / (a)) * (a))
#define DISP_WIDTH_ALIGN    2
#define DISP_HEIGHT_ALIGN   2

#define NALU_PACKET_SIZE            512*1024
#define MI_U32VALUE(pu8Data, index) (pu8Data[index]<<24)|(pu8Data[index+1]<<16)|(pu8Data[index+2]<<8)|(pu8Data[index+3])

#define MAKE_YUYV_VALUE(y,u,v) ((y) << 24) | ((u) << 16) | ((y) << 8) | (v)
#define YUYV_BLACK MAKE_YUYV_VALUE(0,128,128)
#define YUYV_WHITE MAKE_YUYV_VALUE(255,128,128)
#define YUYV_RED MAKE_YUYV_VALUE(76,84,255)
#define YUYV_GREEN MAKE_YUYV_VALUE(149,43,21)
#define YUYV_BLUE MAKE_YUYV_VALUE(29,225,107)


#define SAVE_YUV_TO_FILE 0

static int bExit;
static pthread_t PlayThreadID;

static MI_S32 ST_Sys_Init(void)
{
    MI_SYS_Version_t stVersion;
    MI_U64 u64Pts = 0;

    STCHECKRESULT(MI_SYS_Init());

    memset(&stVersion, 0x0, sizeof(MI_SYS_Version_t));

    STCHECKRESULT(MI_SYS_GetVersion(&stVersion));

    STCHECKRESULT(MI_SYS_GetCurPts(&u64Pts));

    u64Pts = 0xF1237890F1237890;
    STCHECKRESULT(MI_SYS_InitPtsBase(u64Pts));

    u64Pts = 0xE1237890E1237890;
    STCHECKRESULT(MI_SYS_SyncPts(u64Pts));

    return 0;
}

static MI_S32 ST_Sys_UnBind(ST_Sys_BindInfo_t *pstBindInfo)
{
    STCHECKRESULT(MI_SYS_UnBindChnPort(&pstBindInfo->stSrcChnPort, &pstBindInfo->stDstChnPort));

    return 0;
}

static MI_S32 Hdmi_callback_impl(MI_HDMI_DeviceId_e eHdmi, MI_HDMI_EventType_e Event, void *pEventParam, void *pUsrParam)
{
    switch (Event)
    {
        case E_MI_HDMI_EVENT_HOTPLUG:
            printf("E_MI_HDMI_EVENT_HOTPLUG.\n");
            STCHECKRESULT(MI_HDMI_Start(eHdmi));
            break;
        case E_MI_HDMI_EVENT_NO_PLUG:
            printf("E_MI_HDMI_EVENT_NO_PLUG.\n");
            STCHECKRESULT(MI_HDMI_Stop(eHdmi));
            break;
        default:
            printf("Unsupport event.\n");
            break;
    }

    return MI_SUCCESS;
}

static MI_S32 ST_Hdmi_Init(void)
{
    MI_HDMI_InitParam_t stInitParam;
    MI_HDMI_DeviceId_e eHdmi = E_MI_HDMI_ID_0;

    stInitParam.pCallBackArgs = NULL;
    stInitParam.pfnHdmiEventCallback = Hdmi_callback_impl;

    STCHECKRESULT(MI_HDMI_Init(&stInitParam));
    STCHECKRESULT(MI_HDMI_Open(eHdmi));

	return MI_SUCCESS;
}

static MI_S32 ST_Hdmi_DeInit(MI_HDMI_DeviceId_e eHdmi)
{

    STCHECKRESULT(MI_HDMI_Stop(eHdmi));
    STCHECKRESULT(MI_HDMI_Close(eHdmi));
    STCHECKRESULT(MI_HDMI_DeInit());

    return MI_SUCCESS;
}

/*
 * Default: HDMI MODE, YUV444, NoID(color depth)
 */
static MI_S32 ST_Hdmi_Start(MI_HDMI_DeviceId_e eHdmi, MI_HDMI_TimingType_e eTimingType)
{
    MI_HDMI_Attr_t stAttr;

    memset(&stAttr, 0, sizeof(MI_HDMI_Attr_t));
    stAttr.stEnInfoFrame.bEnableAudInfoFrame  = FALSE;
    stAttr.stEnInfoFrame.bEnableAviInfoFrame  = FALSE;
    stAttr.stEnInfoFrame.bEnableSpdInfoFrame  = FALSE;
    stAttr.stAudioAttr.bEnableAudio = TRUE;
    stAttr.stAudioAttr.bIsMultiChannel = 0;
    stAttr.stAudioAttr.eBitDepth = E_MI_HDMI_BIT_DEPTH_16;
    stAttr.stAudioAttr.eCodeType = E_MI_HDMI_ACODE_PCM;
    stAttr.stAudioAttr.eSampleRate = E_MI_HDMI_AUDIO_SAMPLERATE_48K;
    stAttr.stVideoAttr.bEnableVideo = TRUE;
    stAttr.stVideoAttr.eColorType = E_MI_HDMI_COLOR_TYPE_RGB444;//default color type
    stAttr.stVideoAttr.eDeepColorMode = E_MI_HDMI_DEEP_COLOR_MAX;
    stAttr.stVideoAttr.eTimingType = eTimingType;
    stAttr.stVideoAttr.eOutputMode = E_MI_HDMI_OUTPUT_MODE_HDMI;
    STCHECKRESULT(MI_HDMI_SetAttr(eHdmi, &stAttr));

    STCHECKRESULT(MI_HDMI_Start(eHdmi));

    return MI_SUCCESS;
}


int add_wave_header(WaveFileHeader_t* tWavHead, AencType_e eAencType, SoundMode_e eSoundMode, SampleRate_e eSampleRate, int raw_len)
{
    tWavHead->chRIFF[0] = 'R';
    tWavHead->chRIFF[1] = 'I';
    tWavHead->chRIFF[2] = 'F';
    tWavHead->chRIFF[3] = 'F';

    tWavHead->chWAVE[0] = 'W';
    tWavHead->chWAVE[1] = 'A';
    tWavHead->chWAVE[2] = 'V';
    tWavHead->chWAVE[3] = 'E';

    tWavHead->chFMT[0] = 'f';
    tWavHead->chFMT[1] = 'm';
    tWavHead->chFMT[2] = 't';
    tWavHead->chFMT[3] = 0x20;
    tWavHead->dwFMTLen = 0x10;

    if(eAencType == E_ANEC_TYPE_G711A)
    {
        tWavHead->wave.wFormatTag = 0x06;
    }

    if(eAencType == E_ANEC_TYPE_G711U)
    {
        tWavHead->wave.wFormatTag = 0x07;
    }

    if(eAencType == E_ANEC_TYPE_G711U || eAencType == E_ANEC_TYPE_G711A)
    {
        if(eSoundMode == E_SOUND_MODE_MONO)
            tWavHead->wave.wChannels = 0x01;
        else
            tWavHead->wave.wChannels = 0x02;

        tWavHead->wave.wBitsPerSample = 8;//bitWidth;g711encode鍑烘潵鏄?bit锛岃繖閲岄渶瑕佸啓姝?        tWavHead->wave.dwSamplesPerSec = eSampleRate;
        tWavHead->wave.dwAvgBytesPerSec = (tWavHead->wave.wBitsPerSample  * tWavHead->wave.dwSamplesPerSec * tWavHead->wave.wChannels) / 8;
        tWavHead->wave.wBlockAlign = (tWavHead->wave.wBitsPerSample  * tWavHead->wave.wChannels) / 8;
    }
    else if(eAencType == PCM)
    {
        if(eSoundMode == E_SOUND_MODE_MONO)
            tWavHead->wave.wChannels = 0x01;
        else
            tWavHead->wave.wChannels = 0x02;

        tWavHead->wave.wFormatTag = 0x1;
        tWavHead->wave.wBitsPerSample = 16; //16bit
        tWavHead->wave.dwSamplesPerSec = eSampleRate;
        tWavHead->wave.dwAvgBytesPerSec = (tWavHead->wave.wBitsPerSample  * tWavHead->wave.dwSamplesPerSec * tWavHead->wave.wChannels) / 8;
        tWavHead->wave.wBlockAlign = 1024 ;
    }
    else //g726
    {
        tWavHead->wave.wFormatTag = 0x45;
        tWavHead->wave.wChannels  = 0x01;
        switch(eAencType)
        {
            case E_AENC_TYPE_G726_40:
                tWavHead->wave.wBitsPerSample = 5;
                tWavHead->wave.wBlockAlign =  5;
                break;
            case E_AENC_TYPE_G726_32:
                tWavHead->wave.wBitsPerSample = 4;
                tWavHead->wave.wBlockAlign =  4;
                break;
            case E_AENC_TYPE_G726_24:
                tWavHead->wave.wBitsPerSample = 3;
                tWavHead->wave.wBlockAlign =  3;
                break;
            case E_AENC_TYPE_G726_16:
                tWavHead->wave.wBitsPerSample = 2;
                tWavHead->wave.wBlockAlign =  2;
                break;
            default:
                printf("eAencType error:%d\n", eAencType);
                return -1;
        }

        tWavHead->wave.dwSamplesPerSec = eSampleRate;
        tWavHead->wave.dwAvgBytesPerSec = (tWavHead->wave.wBitsPerSample * tWavHead->wave.dwSamplesPerSec * tWavHead->wave.wChannels) / 8;
    }

    tWavHead->chDATA[0] = 'd';
    tWavHead->chDATA[1] = 'a';
    tWavHead->chDATA[2] = 't';
    tWavHead->chDATA[3] = 'a';
    tWavHead->dwDATALen = raw_len;
    tWavHead->dwRIFFLen = raw_len + sizeof(WaveFileHeader_t) - 8;

    return -1;
}

int ss_ao_Deinit(void)
{
    MI_AUDIO_DEV AoDevId = 0;
    MI_AO_CHN AoChn = 0;

    /* disable ao channel of */
    ExecFunc(MI_AO_DisableChn(AoDevId, AoChn), MI_SUCCESS);

    /* disable ao device */
    ExecFunc(MI_AO_Disable(AoDevId), MI_SUCCESS);
    
    return 0;
}
int ss_ao_Init(void)
{
    MI_AUDIO_Attr_t stSetAttr;
    MI_AUDIO_Attr_t stGetAttr;
    MI_AUDIO_DEV AoDevId = 0;
    MI_AO_CHN AoChn = 0;

    MI_S32 s32SetVolumeDb;
    MI_S32 s32GetVolumeDb;

    //set Ao Attr struct
    memset(&stSetAttr, 0, sizeof(MI_AUDIO_Attr_t));
    stSetAttr.eBitwidth = AUDIO_OUTPUT_SAMPFMT;
    stSetAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_MASTER;
    stSetAttr.u32FrmNum = 6;
    stSetAttr.u32PtNumPerFrm = MI_AUDIO_SAMPLE_PER_FRAME;
    stSetAttr.u32ChnCnt = 1;

    if(stSetAttr.u32ChnCnt == 2)
    {
        stSetAttr.eSoundmode = E_MI_AUDIO_SOUND_MODE_STEREO;
    }
    else if(stSetAttr.u32ChnCnt == 1)
    {
        stSetAttr.eSoundmode = AUDIO_OUTPUT_CHLAYOUT;
    }

    stSetAttr.eSamplerate = AUDIO_OUTPUT_SAMPRATE;

    /* set ao public attr*/
    ExecFunc(MI_AO_SetPubAttr(AoDevId, &stSetAttr), MI_SUCCESS);

    /* get ao device*/
    ExecFunc(MI_AO_GetPubAttr(AoDevId, &stGetAttr), MI_SUCCESS);

    /* enable ao device */
    ExecFunc(MI_AO_Enable(AoDevId), MI_SUCCESS);

    /* enable ao channel of device*/
    ExecFunc(MI_AO_EnableChn(AoDevId, AoChn), MI_SUCCESS);


    /* if test AO Volume */
    s32SetVolumeDb = -20;
    ExecFunc(MI_AO_SetVolume(AoDevId, s32SetVolumeDb), MI_SUCCESS);
    ExecFunc(MI_AO_SetVolume(AoDevId, s32SetVolumeDb), MI_SUCCESS);
    /* get AO volume */
    ExecFunc(MI_AO_GetVolume(AoDevId, &s32GetVolumeDb), MI_SUCCESS);

    return 0;
}

static MI_S32 CreateDispDev(AVCodecContext *pVideoCodeCtx)
{
    MI_DISP_PubAttr_t stPubAttr;
    //MI_DISP_VideoLayerAttr_t stLayerAttr;
    MI_DISP_InputPortAttr_t stInputPortAttr;
    MI_DISP_RotateConfig_t stRotateConfig;

    //init disp
    memset(&stPubAttr, 0, sizeof(stPubAttr));
    stPubAttr.u32BgColor = YUYV_BLACK;
    stPubAttr.eIntfSync = E_MI_DISP_OUTPUT_1080P60;
    stPubAttr.eIntfType = E_MI_DISP_INTF_HDMI;
    STCHECKRESULT(MI_DISP_SetPubAttr(0, &stPubAttr));
    STCHECKRESULT(MI_DISP_Enable(0));
#if 0    //i2m unneeded
    memset(&stLayerAttr, 0, sizeof(stLayerAttr));

    stLayerAttr.stVidLayerSize.u16Width  = pCropC->cropwidth;
    stLayerAttr.stVidLayerSize.u16Height = pCropC->cropheight;


    stLayerAttr.ePixFormat = E_MI_SYS_PIXEL_FRAME_YUV_MST_420;
    stLayerAttr.stVidLayerDispWin.u16X      = pCropC->x;
    stLayerAttr.stVidLayerDispWin.u16Y      = pCropC->y;
    stLayerAttr.stVidLayerDispWin.u16Width  = pCropC->cropwidth;
    stLayerAttr.stVidLayerDispWin.u16Height = pCropC->cropheight;
#endif
    STCHECKRESULT(MI_DISP_BindVideoLayer(0, 0));
    //STCHECKRESULT(MI_DISP_SetVideoLayerAttr(0, &stLayerAttr));
    //STCHECKRESULT(MI_DISP_GetVideoLayerAttr(0, &stLayerAttr));
    
    STCHECKRESULT(MI_DISP_EnableVideoLayer(0));
    
    //init disp ch
    memset(&stInputPortAttr, 0, sizeof(stInputPortAttr));
    STCHECKRESULT(MI_DISP_GetInputPortAttr(0, 0, &stInputPortAttr));
    stInputPortAttr.stDispWin.u16X      = 0;
    stInputPortAttr.stDispWin.u16Y      = 0;

    stInputPortAttr.stDispWin.u16Width  = pVideoCodeCtx->width;
    stInputPortAttr.stDispWin.u16Height = pVideoCodeCtx->height;

    stInputPortAttr.u16SrcWidth = pVideoCodeCtx->width;

    stInputPortAttr.u16SrcHeight = pVideoCodeCtx->height;
    STCHECKRESULT(MI_DISP_SetInputPortAttr(0, 0, &stInputPortAttr));
    
    stRotateConfig.eRotateMode = E_MI_DISP_ROTATE_NONE;
    STCHECKRESULT(MI_DISP_SetVideoLayerRotateMode(0, &stRotateConfig));
    STCHECKRESULT(MI_DISP_EnableInputPort(0, 0));
    STCHECKRESULT(MI_DISP_SetInputPortSyncMode(0, 0, E_MI_DISP_SYNC_MODE_FREE_RUN));
    
    //init HDMI
    STCHECKRESULT(ST_Hdmi_Init());
    STCHECKRESULT(ST_Hdmi_Start(E_MI_HDMI_ID_0, E_MI_HDMI_TIMING_1080_60P));

    return 0;
}

int sdk_Unbind(void)
{
    ST_Sys_BindInfo_t stBindInfo;

    memset(&stBindInfo, 0, sizeof(ST_Sys_BindInfo_t));

    stBindInfo.stSrcChnPort.eModId = E_MI_MODULE_ID_VDEC;
    stBindInfo.stSrcChnPort.u32DevId = 0;
    stBindInfo.stSrcChnPort.u32ChnId = 0;
    stBindInfo.stSrcChnPort.u32PortId = 0;

    stBindInfo.stDstChnPort.eModId = E_MI_MODULE_ID_DISP;
    stBindInfo.stDstChnPort.u32DevId = 0;
    stBindInfo.stDstChnPort.u32ChnId = 0;
    stBindInfo.stDstChnPort.u32PortId = 0;
    
    STCHECKRESULT(ST_Sys_UnBind(&stBindInfo));

    return SUCCESS;
}
static MI_DISP_PubAttr_t stDispPubAttr;

int sdk_Deinit(void)
{
    //ST_Hdmi_DeInit(0);
    MI_DISP_DisableInputPort(0 ,0);
    
    MI_DISP_DisableVideoLayer(0);
    MI_DISP_UnBindVideoLayer(0, 0);

     MI_DISP_Disable(0);

    ss_ao_Deinit();
    //STCHECKRESULT(MI_SYS_Exit());

    return SUCCESS;

}

int sdk_bind(void)
{
    MI_SYS_ChnPort_t stSrcChnPort;
    MI_SYS_ChnPort_t stDstChnPort;

    //bind vdec to disp
    stSrcChnPort.eModId = E_MI_MODULE_ID_VDEC;
    stSrcChnPort.u32DevId = 0;
    stSrcChnPort.u32ChnId = 0;
    stSrcChnPort.u32PortId = 0;

    stDstChnPort.eModId = E_MI_MODULE_ID_DISP;
    stDstChnPort.u32DevId = 0;
    stDstChnPort.u32ChnId = 0;
    stDstChnPort.u32PortId = 0;
    MI_SYS_BindChnPort(&stSrcChnPort, &stDstChnPort, 30, 30);

    return SUCCESS;
}

int sdk_Init(AVCodecContext *pVideoCodeCtx)
{
    MI_VDEC_ChnAttr_t stVdecChnAttr;
    MI_SYS_ChnPort_t stChnPort;
    MI_SYS_ChnPort_t stSrcChnPort;
    MI_SYS_ChnPort_t stDstChnPort;

    //ST_Sys_Init();
#if 0
    //init vdec
    memset(&stVdecChnAttr, 0, sizeof(MI_VDEC_ChnAttr_t));
    stVdecChnAttr.eCodecType =E_MI_VDEC_CODEC_TYPE_H264;
    stVdecChnAttr.stVdecVideoAttr.u32RefFrameNum = 5;
    stVdecChnAttr.eVideoMode = E_MI_VDEC_VIDEO_MODE_FRAME;
    stVdecChnAttr.u32BufSize = 1 * 1024 * 1024;
    stVdecChnAttr.u32PicWidth = 1920;//avctx->width;
    stVdecChnAttr.u32PicHeight = 1080;//avctx->height;
    stVdecChnAttr.eInplaceMode = E_MI_VDEC_INPLACE_MODE_OFF;
    stVdecChnAttr.u32Priority = 0;
    MI_VDEC_CreateChn(0, &stVdecChnAttr);
    MI_VDEC_StartChn(0);

    MI_VDEC_OutputPortAttr_t stOutputPortAttr;
    stOutputPortAttr.u16Width = 640;
    stOutputPortAttr.u16Height = 352;
    MI_VDEC_SetOutputPortAttr(0, &stOutputPortAttr);

    memset(&stChnPort, 0x0, sizeof(MI_SYS_ChnPort_t));
    stChnPort.eModId = E_MI_MODULE_ID_VDEC;
    stChnPort.u32DevId = 0;
    stChnPort.u32ChnId = 0;
    stChnPort.u32PortId = 0;

    STCHECKRESULT(MI_SYS_SetChnOutputPortDepth(&stChnPort, 0, 6));
#endif
    //init disp
    stDispPubAttr.eIntfType = E_MI_DISP_INTF_LCD;
    stDispPubAttr.eIntfSync = E_MI_DISP_OUTPUT_USER;
    stDispPubAttr.u32BgColor = YUYV_BLACK;

    sstar_disp_init(&stDispPubAttr,pVideoCodeCtx->flags,pVideoCodeCtx->flags2);
    
    //usleep(2*1000*1000);
#if 0
    //bind vdec to disp
    stSrcChnPort.eModId = E_MI_MODULE_ID_VDEC;
    stSrcChnPort.u32DevId = 0;
    stSrcChnPort.u32ChnId = 0;
    stSrcChnPort.u32PortId = 0;

    stDstChnPort.eModId = E_MI_MODULE_ID_DISP;
    stDstChnPort.u32DevId = 0;
    stDstChnPort.u32ChnId = 0;
    stDstChnPort.u32PortId = 0;
    MI_SYS_BindChnPort(&stSrcChnPort, &stDstChnPort, 30, 30);
#endif
    //init audio
    ss_ao_Init();
    
	return 0;
}

static void *PlayVideoThread (void *args)
{
    const char *filename, *outAudiofilename,*outVideofilename;
    struct SwsContext *sws_ctx;
    AVFrame *pFrameYUV;
    uint8_t *Vout_buffer;

    printf("pos x: %d, y: %d,width: %d,height: %d\n",pos.x,pos.y,pos.width,pos.height);

    //1.注册组件
    av_register_all();
    //封装格式上下文
    AVFormatContext *pFormatCtx = avformat_alloc_context();

    //2.打开输入音频文件
    if (avformat_open_input(&pFormatCtx, pos.filepath, NULL, NULL) != 0) {
        printf("%s %d fail!\n",__FUNCTION__,__LINE__);
        return FAIL;
    }

    //3.获取音频信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("%s %d fail!\n",__FUNCTION__,__LINE__);
        return FAIL;
    }

    //音频解码，需要找到对应的AVStream所在的pFormatCtx->streams的索引位置
    int audio_stream_idx = -1;
    int video_stream_idx = -1;
    int i = 0;
    for (; i < pFormatCtx->nb_streams; i++) {
        //根据类型判断是否是音频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            break;
        }
    }
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        //根据类型判断是否是音频流
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            break;
        }
    }

    //4.获取解码器
    //根据索引拿到对应的流,根据流拿到解码器上下文
    AVCodecContext *pAudioCodeCtx = pFormatCtx->streams[audio_stream_idx]->codec;

    AVCodecContext *pVideoCodeCtx = pFormatCtx->streams[video_stream_idx]->codec;

    printf("video code width: %d,height: %d,codeid: %d\n",pVideoCodeCtx->width,pVideoCodeCtx->height,pVideoCodeCtx->codec_id);

	if((pVideoCodeCtx->width > pos.width) || (pVideoCodeCtx->height > pos.height))
	{
		if(pVideoCodeCtx->width > pVideoCodeCtx->height)
		{
			pVideoCodeCtx->flags = ALIGN_BACK(pos.width,32); //scale down width
			pVideoCodeCtx->flags2 = ALIGN_BACK(pVideoCodeCtx->height * pos.width / pVideoCodeCtx->width,32);
		}
		else
		{
			pVideoCodeCtx->flags2 = ALIGN_BACK(pos.height,32); //scale down height
			pVideoCodeCtx->flags = ALIGN_BACK(pVideoCodeCtx->width * pos.height / pVideoCodeCtx->height,32);
		}
	}
	else
	{
		pVideoCodeCtx->flags = ALIGN_BACK(pVideoCodeCtx->width,32);
		pVideoCodeCtx->flags2 = ALIGN_BACK(pVideoCodeCtx->height,32);
	}

	printf("pVideoCodeCtx flags: %d,flags2: %d\n",pVideoCodeCtx->flags,pVideoCodeCtx->flags2);

    sdk_Init(pVideoCodeCtx);

    //再根据上下文拿到编解码id，通过该id拿到解码器
    AVCodec *pAudioCodec = avcodec_find_decoder(pAudioCodeCtx->codec_id);
    if (pAudioCodec == NULL) {
        printf("%s %d fail!\n",__FUNCTION__,__LINE__);
        return FAIL;
    }

    AVCodec *pVideoCodec = avcodec_find_decoder(pVideoCodeCtx->codec_id);
    if (pVideoCodec == NULL) {
        printf("%s %d fail!\n",__FUNCTION__,__LINE__);
        return FAIL;
    }

    //5.打开解码器
    if (avcodec_open2(pAudioCodeCtx, pAudioCodec, NULL) < 0) {
        printf("%s %d fail!\n",__FUNCTION__,__LINE__);
        return FAIL;
    }

    if (avcodec_open2(pVideoCodeCtx, pVideoCodec, NULL) < 0) {
        printf("%s %d fail!\n",__FUNCTION__,__LINE__);
        return FAIL;
    }
    //编码数据
    AVPacket *packet = av_malloc(sizeof(AVPacket));
    //解压缩数据
    AVFrame *frame = av_frame_alloc();

    //none h264 sw decode need transcode to NV12 for disp input
    if(pVideoCodeCtx->codec_id != AV_CODEC_ID_H264)
    {
        //转换成YUV420P
        pFrameYUV = av_frame_alloc();
        //只有指定了AVFrame的像素格式、画面大小才能真正分配内存
        //缓冲区分配内存
        Vout_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_NV12, pVideoCodeCtx->width, pVideoCodeCtx->height));
        //初始化缓冲区
        avpicture_fill((AVPicture *)pFrameYUV, Vout_buffer, AV_PIX_FMT_NV12, pVideoCodeCtx->width, pVideoCodeCtx->height);

        //用于转码（缩放）的参数，转之前的宽高，转之后的宽高，格式等
        sws_ctx = sws_getContext(pVideoCodeCtx->width,pVideoCodeCtx->height,pVideoCodeCtx->pix_fmt,
                                                pVideoCodeCtx->width, pVideoCodeCtx->height, AV_PIX_FMT_NV12,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    }

    //frame->16bit 44100 PCM 统一音频采样格式与采样率
    SwrContext *swrCtx = swr_alloc();
    //重采样设置选项-----------------------------------------------------------start
    //输入的采样格式
    enum AVSampleFormat in_sample_fmt = pAudioCodeCtx->sample_fmt;
    //输出的采样格式 16bit PCM
    enum AVSampleFormat out_sample_fmt = AUDIO_INPUT_SAMPFMT;
    //输入的采样率
    int in_sample_rate = pAudioCodeCtx->sample_rate;
    //输出的采样率
    int out_sample_rate = AUDIO_INPUT_SAMPRATE;

    //输入的声道布局
    uint64_t in_ch_layout = pAudioCodeCtx->channel_layout;
    //输出的声道布局
    uint64_t out_ch_layout = AUDIO_INPUT_CHLAYOUT;
#if 0
    printf("out_ch_layout: %d\n",out_ch_layout);
    printf("out_sample_fmt: %d\n",out_sample_fmt);
    printf("out_sample_rate: %d\n",out_sample_rate);

    printf("in_ch_layout: %d\n",in_ch_layout);
    printf("in_sample_fmt: %d\n",in_sample_fmt);
    printf("in_sample_rate: %d\n",in_sample_rate);
#endif
    swr_alloc_set_opts(swrCtx, out_ch_layout, out_sample_fmt, out_sample_rate, in_ch_layout, in_sample_fmt,
            in_sample_rate, 0, NULL);
    swr_init(swrCtx);
    //重采样设置选项-----------------------------------------------------------end

    //获取输出的声道个数
    int out_channel_nb = av_get_channel_layout_nb_channels(out_ch_layout);

    //存储pcm数据
    uint8_t *out_buffer = (uint8_t *) av_malloc(2 * AUDIO_INPUT_SAMPRATE);

    int ret;

    //h264 hw decode need bind vdec 2 disp
    if(pVideoCodeCtx->codec_id == AV_CODEC_ID_H264)
    {
        //Patch for skip avformat_find_stream_info send frame to ss hw decode
        pVideoCodeCtx->debug = 1;
        sdk_bind();
    }

    //FILE *fp_yuv = fopen(outVideofilename, "wb+");
    //FILE *fp_pcm = fopen(outAudiofilename, "wb+");
PLAY:
    //6.一帧一帧读取压缩的音频数据AVPacket
    while (av_read_frame(pFormatCtx, packet) >= 0 && !bExit) {
    
        if(packet->stream_index == audio_stream_idx) 
        {
        
            MI_AUDIO_Frame_t stAoSendFrame;
            MI_S32 s32RetSendStatus = 0;
            MI_AUDIO_DEV AoDevId = 0;
            MI_AO_CHN AoChn = 0;
            
            //解码AVPacket->AVFrame
            ret = avcodec_send_packet(pAudioCodeCtx, packet);
            if(ret < 0)
            {
                printf("avcodec_send_packet fail!\n");
                continue;
            }

            ret = avcodec_receive_frame(pAudioCodeCtx, frame);
            if (ret < 0 && ret != AVERROR(EAGAIN))
            {
                printf("avcodec_receive_frame fail\n");
                return ret;
            }
            if(ret >= 0)
            {
                swr_convert(swrCtx, &out_buffer, 2 * AUDIO_INPUT_SAMPRATE, (const uint8_t **)frame->data, frame->nb_samples);
                int out_buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb, frame->nb_samples,
                out_sample_fmt, 1);

                //read data and send to AO module
                stAoSendFrame.u32Len = out_buffer_size;
                stAoSendFrame.apVirAddr[0] = out_buffer;
                stAoSendFrame.apVirAddr[1] = NULL;

                do{
                    s32RetSendStatus = MI_AO_SendFrame(AoDevId, AoChn, &stAoSendFrame, 128);
                }while(s32RetSendStatus == MI_AO_ERR_NOBUF);

                if(s32RetSendStatus != MI_SUCCESS)
                {
                    printf("[Warning]: MI_AO_SendFrame fail, error is 0x%x: \n",s32RetSendStatus);
                }
            }
        }

        if(packet->stream_index == video_stream_idx)
        {
            //detect avi format
            if(pVideoCodeCtx->codec_id == AV_CODEC_ID_H264)
            {
                const char start_code[4] = { 0, 0, 0, 1 };
                if(memcmp(start_code, packet->data, 4) != 0)
                {//is avc1 code, have no start code of H264
                    int len = 0;
                    uint8_t *p = packet->data;

                    do 
                    {//add start_code for each NAL, one frame may have multi NALs.
                        len = ntohl(*((long*)p));
                        memcpy(p, start_code, 4);

                        p += 4;
                        p += len;
                        if(p >= packet->data + packet->size)
                        {
                            break;
                        }
                    } while (1);
                }
            }
            //7.解码一帧视频压缩数据
            ret = avcodec_send_packet(pVideoCodeCtx, packet);
            if(ret < 0)
            {
                printf("avcodec_send_packet fail!\n");
                continue;
            }

            ret = avcodec_receive_frame(pVideoCodeCtx, frame);

            if (ret < 0 && ret != AVERROR(EAGAIN))
            {
                printf("avcodec_receive_frame fail\n");
                return ret;
            }
            
            //none h264 sw decode need put frame to disp
            if(pVideoCodeCtx->codec_id != AV_CODEC_ID_H264)
            {
                if(ret >= 0)
                {
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, pVideoCodeCtx->height,pFrameYUV->data, pFrameYUV->linesize);

                    int y_size = pVideoCodeCtx->width * pVideoCodeCtx->height;

                    MI_SYS_BUF_HANDLE hHandle;
                    MI_SYS_ChnPort_t pstSysChnPort;
                    MI_SYS_BufConf_t stBufConf;
                    MI_SYS_BufInfo_t stBufInfo;

                    pstSysChnPort.eModId = E_MI_MODULE_ID_DISP;
                    pstSysChnPort.u32ChnId = 0;
                    pstSysChnPort.u32DevId = 0;
                    pstSysChnPort.u32PortId = 0;

                    memset(&stBufInfo , 0 , sizeof(MI_SYS_BufInfo_t));
                    memset(&stBufConf , 0 , sizeof(MI_SYS_BufConf_t));

                    stBufConf.eBufType = E_MI_SYS_BUFDATA_FRAME;
                    stBufConf.u64TargetPts = 0;
                    stBufConf.stFrameCfg.u16Width = pVideoCodeCtx->width;
                    stBufConf.stFrameCfg.u16Height = pVideoCodeCtx->height;
                    stBufConf.stFrameCfg.eFormat = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420;
                    stBufConf.stFrameCfg.eFrameScanMode = E_MI_SYS_FRAME_SCAN_MODE_PROGRESSIVE;

                    if(MI_SUCCESS == MI_SYS_ChnInputPortGetBuf(&pstSysChnPort,&stBufConf,&stBufInfo,&hHandle, -1))
                    {
                        stBufInfo.stFrameData.eCompressMode = E_MI_SYS_COMPRESS_MODE_NONE;
                        stBufInfo.stFrameData.eFieldType = E_MI_SYS_FIELDTYPE_NONE;
                        stBufInfo.stFrameData.eTileMode = E_MI_SYS_FRAME_TILE_MODE_NONE;
                        stBufInfo.bEndOfStream = FALSE;

                        memcpy(stBufInfo.stFrameData.pVirAddr[0],pFrameYUV->data[0],y_size);
                        memcpy(stBufInfo.stFrameData.pVirAddr[1],pFrameYUV->data[1],y_size/2);

                        //stBufInfo.stFrameData.pVirAddr[0] = pFrameYUV->data[0];
                        //stBufInfo.stFrameData.pVirAddr[1] = pFrameYUV->data[1];

                        MI_SYS_ChnInputPortPutBuf(hHandle ,&stBufInfo , FALSE);
                    }
                }
            }
        }
        av_free_packet(packet);
    }
	int seekflag;
	seekflag &= ~AVSEEK_FLAG_BYTE;
	avformat_seek_file(pFormatCtx, -1, INT64_MIN, pFormatCtx->start_time, INT64_MAX, seekflag);
	if(!bExit)
		goto PLAY;
	
    if(pVideoCodeCtx->codec_id == AV_CODEC_ID_H264)
    {
        sdk_Unbind();
    }
    sdk_Deinit();

    av_frame_free(&frame);
    printf("11\n");
    av_free(out_buffer);
    printf("22\n");
    if(pVideoCodeCtx->codec_id != AV_CODEC_ID_H264)
    {
    	av_frame_free(&pFrameYUV);
    	av_free(Vout_buffer);
    }
	printf("33\n");
    swr_free(&swrCtx);
    printf("44\n");
    avcodec_close(pAudioCodeCtx);
    printf("55\n");
    avformat_close_input(&pFormatCtx);
    printf("66\n");
    return SUCCESS;
}

int StartPlayVideo (char *file,int x,int y,int width,int height)
{
	strcpy(pos.filepath,file);
	pos.x = x;
	pos.y = y;
	pos.width = width;
	pos.height = height;
	printf("input file: %s,x: %d,y: %d,width: %d,height: %d\n",pos.filepath,pos.x,pos.y,pos.width,pos.height);
	bExit = false;
	pthread_create(&PlayThreadID, NULL, PlayVideoThread, NULL);
	printf("start play video success\n");
	return 0;
}
int StopPlayVideo (void)
{
	bExit = true;
	pthread_join(PlayThreadID, NULL);
	return 0;
}
