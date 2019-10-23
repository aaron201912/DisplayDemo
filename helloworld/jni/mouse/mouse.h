#ifndef __SSTARMOUSE__H__
#define __SSTARMOUSE__H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

#define CURSOR_WIDTH	32
#define CURSOR_HEIGHT	32

int initMouseDev(void);
int setMousePos(int x,int y);
#ifdef __cplusplus
}
#endif // __cplusplus

#endif //__SSTARDISP__H__
