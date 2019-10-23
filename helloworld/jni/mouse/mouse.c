/* Copyright (c) 2018-2019 Sigmastar Technology Corp.
 All rights reserved.
  
  Unless otherwise stipulated in writing, any and all information contained
 herein regardless in any format shall remain the sole proprietary of
 Sigmastar Technology Corp. and be kept in strict confidence
 (��Sigmastar Confidential Information��) by the recipient.
 Any unauthorized act including without limitation unauthorized disclosure,
 copying, use, reproduction, sale, distribution, modification, disassembling,
 reverse engineering and compiling of the contents of Sigmastar Confidential
 Information is unlawful and strictly prohibited. Sigmastar hereby reserves the
 rights to any and all damages, losses, costs and expenses resulting therefrom.
*/
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>


#include "mstarFb.h"
#include "mouse.h"

struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;

/**
 *dump fix info of Framebuffer
 */
void printFixedInfo ()
{
    printf ("Fixed screen info:\n"
            "\tid: %s\n"
            "\tsmem_start: 0x%lx\n"
            "\tsmem_len: %d\n"
            "\ttype: %d\n"
            "\ttype_aux: %d\n"
            "\tvisual: %d\n"
            "\txpanstep: %d\n"
            "\typanstep: %d\n"
            "\tywrapstep: %d\n"
            "\tline_length: %d\n"
            "\tmmio_start: 0x%lx\n"
            "\tmmio_len: %d\n"
            "\taccel: %d\n"
            "\n",
            finfo.id, finfo.smem_start, finfo.smem_len, finfo.type,
            finfo.type_aux, finfo.visual, finfo.xpanstep, finfo.ypanstep,
            finfo.ywrapstep, finfo.line_length, finfo.mmio_start,
            finfo.mmio_len, finfo.accel);
}

/**
 *dump var info of Framebuffer
 */
void printVariableInfo ()
{
    printf ("Variable screen info:\n"
            "\txres: %d\n"
            "\tyres: %d\n"
            "\txres_virtual: %d\n"
            "\tyres_virtual: %d\n"
            "\tyoffset: %d\n"
            "\txoffset: %d\n"
            "\tbits_per_pixel: %d\n"
            "\tgrayscale: %d\n"
            "\tred: offset: %2d, length: %2d, msb_right: %2d\n"
            "\tgreen: offset: %2d, length: %2d, msb_right: %2d\n"
            "\tblue: offset: %2d, length: %2d, msb_right: %2d\n"
            "\ttransp: offset: %2d, length: %2d, msb_right: %2d\n"
            "\tnonstd: %d\n"
            "\tactivate: %d\n"
            "\theight: %d\n"
            "\twidth: %d\n"
            "\taccel_flags: 0x%x\n"
            "\tpixclock: %d\n"
            "\tleft_margin: %d\n"
            "\tright_margin: %d\n"
            "\tupper_margin: %d\n"
            "\tlower_margin: %d\n"
            "\thsync_len: %d\n"
            "\tvsync_len: %d\n"
            "\tsync: %d\n"
            "\tvmode: %d\n"
            "\n",
            vinfo.xres, vinfo.yres, vinfo.xres_virtual, vinfo.yres_virtual,
            vinfo.xoffset, vinfo.yoffset, vinfo.bits_per_pixel,
            vinfo.grayscale, vinfo.red.offset, vinfo.red.length,
            vinfo.red.msb_right, vinfo.green.offset, vinfo.green.length,
            vinfo.green.msb_right, vinfo.blue.offset, vinfo.blue.length,
            vinfo.blue.msb_right, vinfo.transp.offset, vinfo.transp.length,
            vinfo.transp.msb_right, vinfo.nonstd, vinfo.activate,
            vinfo.height, vinfo.width, vinfo.accel_flags, vinfo.pixclock,
            vinfo.left_margin, vinfo.right_margin, vinfo.upper_margin,
            vinfo.lower_margin, vinfo.hsync_len, vinfo.vsync_len,
            vinfo.sync, vinfo.vmode);
}
//static MI_DISP_PubAttr_t stDispPubAttr;
//#define MAKE_YUYV_VALUE(y,u,v)  ((y) << 24) | ((u) << 16) | ((y) << 8) | (v)
//#define YUYV_BLACK              MAKE_YUYV_VALUE(0,128,128)
static unsigned int con_argb8888_to_argb1555(unsigned int color)
{
    return (0x0000ffff & (
        (((color & 0xff000000) >> 24) == 0 ? 0x0000 : 0x8000) |
        (((color & 0x00ff0000) >> 19) << 10) |
        (((color & 0x0000ff00) >> 11) << 5) |
        (((color & 0x000000ff) >> 3))));
}

static int fbFd = 0;
#define PANEL_W  1024
#define PANEL_H  600

int setMousePos(int x,int y)
{
	MI_FB_CursorAttr_t stCursorAttr;
	int tmp;

    stCursorAttr.u32XPos = PANEL_W - y;
    stCursorAttr.u32YPos = x;

    stCursorAttr.u16CursorAttrMask = E_MI_FB_CURSOR_ATTR_MASK_POS;

    if (ioctl(fbFd, FBIOSET_CURSOR_ATTRIBUTE, &stCursorAttr)) {
        perror ("Error FBIOSET_CURSOR_ATTRIBUTE");
        exit(4);
    }
	return 0;
}
int initMouseDev(void)
{
    const char *devfile = "/dev/fb0";

    unsigned int osdWidth = 1920;
    unsigned int osdHeight = 1080;
    //Icon width 44, heigth 56, ARGB8888
    int nIconBytes = CURSOR_WIDTH*CURSOR_HEIGHT*4;
    int nIconPitch = CURSOR_WIDTH;
    int reqPosX = 0;
    int reqPosY = 0;
    FILE* fp = NULL;
    unsigned char buff[CURSOR_WIDTH*CURSOR_HEIGHT*4] = {0};
    MI_FB_CursorAttr_t stCursorAttr;
    memset(&stCursorAttr, 0, sizeof(MI_FB_CursorAttr_t));

    /* Open the file for reading and writing */
    fbFd = open (devfile, O_RDWR);
    if (fbFd == -1)
    {
        perror ("Error: cannot open framebuffer device");
        exit (1);
    }
    //get fb_fix_screeninfo
    if (ioctl (fbFd, FBIOGET_FSCREENINFO, &finfo) == -1)
    {
        perror ("Error reading fixed information");
        exit(2);
    }
    printFixedInfo ();

    //get fb_var_screeninfo
    if (ioctl (fbFd, FBIOGET_VSCREENINFO, &vinfo) == -1)
    {
        perror ("Error reading variable information");
        exit(3);
    }
    printVariableInfo ();
    osdWidth = vinfo.xres;
    osdHeight = vinfo.yres;

    //read icon data
    fp = fopen("/customer/cursor.raw", "rb");
    if (fp == NULL) {
        printf("Can not find cursor.raw in current file!!\n");
        exit(4);
    }
    fread(buff, 1, nIconBytes, fp);
    fclose(fp);
    //get output timing
    MI_FB_DisplayLayerAttr_t displayerAttr;
    memset(&displayerAttr, 0, sizeof(displayerAttr));
    if (ioctl(fbFd, FBIOGET_DISPLAYLAYER_ATTRIBUTES, &displayerAttr) < 0) {
        perror("Error: failed to FBIOGET_DISPLAYLAYER_ATTRIBUTES");
        exit(5);
    }
    //convert data from argb8888 to argb1555
    //Shoule use GE to do bitblit and format convert
    unsigned char pARGB1555Data[CURSOR_WIDTH*CURSOR_HEIGHT*2] = {0};
    unsigned short* pTemp_16 = (unsigned short*)pARGB1555Data;
    unsigned char* pTemp = buff;
    unsigned int pixelVal_32 = 0;
    int height = CURSOR_HEIGHT;
    int width = CURSOR_WIDTH;
    int i=0, j=0;
    for (i=0; i < height; i++)
    {
        for (j=0;j < width;j++) {
            pTemp = buff + 4*(j + i*width);
            pixelVal_32 =
                (((unsigned int)(pTemp[3]) &0x000000ff) << 24) |
                (((unsigned int)(pTemp[2]) &0x000000ff) << 16) |
                (((unsigned int)(pTemp[1]) &0x000000ff) << 8)|
                (((unsigned int)(pTemp[0]) &0x000000ff) );
            pTemp_16[j + i * width] = (unsigned short)(con_argb8888_to_argb1555(pixelVal_32));
        }
    }
    //I2M use gop0 for hwcursor, only support argb1555 argb4444 format
    //set curosr Icon && set positon
    stCursorAttr.stCursorImageInfo.u32Width = CURSOR_WIDTH;
    stCursorAttr.stCursorImageInfo.u32Height = CURSOR_HEIGHT;
    stCursorAttr.stCursorImageInfo.u32Pitch = nIconPitch;
    stCursorAttr.stCursorImageInfo.eColorFmt = E_MI_FB_COLOR_FMT_ARGB1555;
    stCursorAttr.stCursorImageInfo.data = pARGB1555Data;
    stCursorAttr.u32HotSpotX = CURSOR_WIDTH;
    stCursorAttr.u32HotSpotY = 8;
    stCursorAttr.u32XPos = 100;
    stCursorAttr.u32YPos = 100;
    stCursorAttr.stAlpha.bAlphaEnable = TRUE;
    stCursorAttr.stAlpha.u8Alpha0 = 0;
    stCursorAttr.stAlpha.u8Alpha1 = 0xff;
    stCursorAttr.u16CursorAttrMask = E_MI_FB_CURSOR_ATTR_MASK_ICON
        | E_MI_FB_CURSOR_ATTR_MASK_SHOW | E_MI_FB_CURSOR_ATTR_MASK_POS|E_MI_FB_CURSOR_ATTR_MASK_ALPHA;
    if (ioctl(fbFd, FBIOSET_CURSOR_ATTRIBUTE, &stCursorAttr)) {
        perror ("Error FBIOSET_CURSOR_ATTRIBUTE");
        exit(4);
    }
#if 0
    stCursorAttr.u16CursorAttrMask = E_MI_FB_CURSOR_ATTR_MASK_COLORKEY;
    stCursorAttr.stColorKey.bKeyEnable = TRUE;
    stCursorAttr.stColorKey.u8Red = 0xff;
    stCursorAttr.stColorKey.u8Green = 0xff;
    stCursorAttr.stColorKey.u8Blue = 0xff;
    printf("After sleep 5s it will enable colorkey [%x,%x,%x]\n",stCursorAttr.stColorKey.u8Red,
        stCursorAttr.stColorKey.u8Green,stCursorAttr.stColorKey.u8Blue);
    sleep(5);
    if (ioctl(fbFd, FBIOSET_CURSOR_ATTRIBUTE, &stCursorAttr)) {
        perror ("Error FBIOSET_CURSOR_ATTRIBUTE");
        exit(4);
    }

    stCursorAttr.u16CursorAttrMask = E_MI_FB_CURSOR_ATTR_MASK_COLORKEY;
    stCursorAttr.stColorKey.bKeyEnable = FALSE;
    printf("After sleep 5s it will disable colorkey [%x,%x,%x]\n");
    sleep(5);
    if (ioctl(fbFd, FBIOSET_CURSOR_ATTRIBUTE, &stCursorAttr)) {
        perror ("Error FBIOSET_CURSOR_ATTRIBUTE");
        exit(4);
    }

    //i2m used gop0 for hwcursor it can not scale up
    //Hot spot should do same operation as xpos ypos
    int timingWidth = displayerAttr.u32ScreenWidth;
    int timingHeight = displayerAttr.u32ScreenHeight;
    float xpos_scale = (float)timingWidth /osdWidth;
    float ypos_scale = (float)timingHeight /osdHeight;
    while (1)
    {
        reqPosX = rand() % osdWidth;
        reqPosY = rand() % osdHeight;
        //scale positon as scale
        reqPosX = (int)(reqPosX * xpos_scale);
        reqPosY = (int)(reqPosY * ypos_scale);
        stCursorAttr.u32XPos = reqPosX;
        stCursorAttr.u32YPos = reqPosY;
        stCursorAttr.u16CursorAttrMask = E_MI_FB_CURSOR_ATTR_MASK_POS;
        if (ioctl(fbFd, FBIOSET_CURSOR_ATTRIBUTE, &stCursorAttr)) {
            perror ("Error FBIOSET_CURSOR_ATTRIBUTE");
            exit(4);
        }
    }
    close(fbFd);
    exit(0);
#endif
}
