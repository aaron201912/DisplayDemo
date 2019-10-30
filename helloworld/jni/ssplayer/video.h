#ifndef __VIDEO_H__
#define __VIDEO_H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus


#include "player.h"

#include "mi_disp.h"
#include "mi_disp_datatype.h"

#include "mi_divp.h"
#include "mi_divp_datatype.h"

#include "mi_panel.h"
#include "mi_panel_datatype.h"

#include "mi_vdec.h"
#include "mi_vdec_datatype.h"


#if UI_1024_600
#define DEVICE_WIDTH        1024
#define DEVICE_HEIGHT       600
#else
#define DEVICE_WIDTH        800
#define DEVICE_HEIGHT       480
#endif

#define MIN_WIDTH           128
#define MIN_HEIGHT          64

#define HARD_DECODING       1
#define SOFT_DECODING       0

#define ENABLE_HDMI         0

#define DISP_DEV            0
#define DISP_LAYER          0
#define DISP_INPUTPORT      0

#define MAKE_YUYV_VALUE(y,u,v) ((y) << 24) | ((u) << 16) | ((y) << 8) | (v)
#define YUYV_BLACK MAKE_YUYV_VALUE(0,128,128)
#define YUYV_WHITE MAKE_YUYV_VALUE(255,128,128)
#define YUYV_RED MAKE_YUYV_VALUE(76,84,255)
#define YUYV_GREEN MAKE_YUYV_VALUE(149,43,21)
#define YUYV_BLUE MAKE_YUYV_VALUE(29,225,107)

#define ALIGN_UP(x, align)          (((x) + ((align) - 1)) & ~((align) - 1))
#define ALIGN_BACK(x, align)        (((x) / (align)) * (align))


typedef struct ST_Sys_BindInfo_s
{
    MI_SYS_ChnPort_t stSrcChnPort;
    MI_SYS_ChnPort_t stDstChnPort;
    MI_U32 u32SrcFrmrate;
    MI_U32 u32DstFrmrate;
} ST_Sys_BindInfo_t;



int open_video(player_stat_t *is);

int sstar_panel_init(MI_DISP_Interface_e eType);
int sstar_panel_deinit(void);

int sstar_video_deinit(void);
int sstar_video_init(player_stat_t *is, uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif

