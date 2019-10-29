#ifndef __SSTARPLAYER__H__
#define __SSTARPLAYER__H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus
//��װ��ʽ
#include "libavformat/avformat.h"
//����
#include "libavcodec/avcodec.h"
//����
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

typedef struct DISP_POS
{
	char filepath[64];
	int x;
	int y;
	int width;
	int height;
}Pos_t;

extern Pos_t pos;

int StartPlayVideo (char *file,int x,int y,int width,int height);
int StopPlayVideo (void);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif //__SSTARDISP__H__
