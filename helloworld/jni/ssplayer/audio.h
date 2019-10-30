#ifndef __AUDIO_H__
#define __AUDIO_H__

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus


#include "player.h"

#include "mi_ao.h"


int open_audio(player_stat_t *is);
int sstar_audio_deinit(void);
int sstar_audio_init(void);

#ifdef __cplusplus
}
#endif // __cplusplus


#endif
