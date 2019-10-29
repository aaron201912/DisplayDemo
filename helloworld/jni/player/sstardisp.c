#include "string.h"
#include "stdio.h"
#include "stdlib.h"

#include "mi_sys.h"
#include "sstardisp.h"
#include "SsPlayer.h"

#include "mi_panel_datatype.h"
#include "mi_panel.h"
#include "mi_disp_datatype.h"
#include "mi_disp.h"

#include <config.h>

#if UI_1024_600
#include "SAT070CP50_1024x600.h"
#else
#if USE_MIPI
#include "EK79007_1024x600_MIPI.h"
#else
#include "SAT070AT50_800x480.h"
#endif
#endif

// loacl play res
#if UI_1024_600
//#define LOCAL_VIDEO_W  822
//#define LOCAL_VIDEO_H  464
//#define LOCAL_VIDEO_X  100
//#define LOCAL_VIDEO_Y  60
#define LOCAL_VIDEO_W  1024
#define LOCAL_VIDEO_H  600
#define LOCAL_VIDEO_X  0
#define LOCAL_VIDEO_Y  0
#else
//#define LOCAL_VIDEO_W  640
//#define LOCAL_VIDEO_H  360
//#define LOCAL_VIDEO_X  100
//#define LOCAL_VIDEO_Y  60
#define LOCAL_VIDEO_W  800
#define LOCAL_VIDEO_H  480
#define LOCAL_VIDEO_X  0
#define LOCAL_VIDEO_Y  0
#endif

#if defined(__cplusplus)||defined(c_plusplus)
extern "C"{
#endif
int sstar_disp_init(MI_DISP_PubAttr_t *pstDispPubAttr,int inputwidth,int inputheight)
{
    //MI_PANEL_LinkType_e eLinkType;
    MI_DISP_InputPortAttr_t stInputPortAttr;

    memset(&stInputPortAttr, 0, sizeof(stInputPortAttr));

    //MI_SYS_Init();

    MI_DISP_DisableInputPort(0, 0);
	MI_DISP_RotateConfig_t stRotateConfig;
	memset(&stRotateConfig, 0, sizeof(MI_DISP_RotateConfig_t));
	stRotateConfig.eRotateMode      = E_MI_DISP_ROTATE_90;
	
	MI_DISP_SetVideoLayerRotateMode(0, &stRotateConfig);
	 
    stInputPortAttr.u16SrcWidth = inputwidth;//LOCAL_VIDEO_W;
    stInputPortAttr.u16SrcHeight = inputheight;//LOCAL_VIDEO_H;
    stInputPortAttr.stDispWin.u16X = LOCAL_VIDEO_W - pos.height -pos.y;//LOCAL_VIDEO_X;
    stInputPortAttr.stDispWin.u16Y = pos.x;//LOCAL_VIDEO_Y;
    stInputPortAttr.stDispWin.u16Width = pos.height;//inputheight;//pos.height;
    stInputPortAttr.stDispWin.u16Height = pos.width;//inputwidth;//pos.width;

    MI_DISP_SetInputPortAttr(0, 0, &stInputPortAttr);
    MI_DISP_EnableInputPort(0, 0);
    MI_DISP_SetInputPortSyncMode(0, 0, E_MI_DISP_SYNC_MODE_FREE_RUN);

    return 0;
}
int sstar_disp_Deinit(MI_DISP_PubAttr_t *pstDispPubAttr)
{

    MI_DISP_DisableInputPort(0, 0);
    MI_DISP_DisableVideoLayer(0);
    MI_DISP_UnBindVideoLayer(0, 0);
    MI_DISP_Disable(0);

    switch(pstDispPubAttr->eIntfType) {
        case E_MI_DISP_INTF_HDMI:
            break;

        case E_MI_DISP_INTF_VGA:
            break;

        case E_MI_DISP_INTF_LCD:
        default:
            MI_PANEL_DeInit();

    }

    MI_SYS_Exit();
    printf("sstar_disp_Deinit...\n");

    return 0;
}

#if defined(__cplusplus)||defined(c_plusplus)
}
#endif

