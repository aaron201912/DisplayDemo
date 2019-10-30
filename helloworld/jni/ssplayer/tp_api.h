#ifndef _TP_API_H_
#define _TP_API_H_

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>


int tp_player_open(const char *fp, uint16_t x, uint16_t y, uint16_t width, uint16_t height, int flag);
int tp_player_close(void);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif

