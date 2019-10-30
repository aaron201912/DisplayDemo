#include "tp_api.h"
#include "audio.h"
#include "video.h"
#include "player.h"
#include "demux.h"


player_stat_t *g_is = NULL;

int tp_player_open(const char *fp, uint16_t x, uint16_t y, uint16_t width, uint16_t height, int flag)
{
    int ret = -1;
    int tmp_width, tmp_height;

    printf("get in tp_player_open!\n");

    if (g_is == NULL)
    {
        if (width < MIN_WIDTH || width > DEVICE_HEIGHT)
        {
            printf("\033[33;2mset width must be in [%d, %d]\033[0m\n", MIN_WIDTH, DEVICE_HEIGHT);
            if (width < MIN_WIDTH)
                width = MIN_WIDTH;
            else if (width > DEVICE_HEIGHT)
                width = DEVICE_HEIGHT;
        }
        if (height < MIN_HEIGHT || height > DEVICE_WIDTH)
        {
            printf("\033[33;2mset height must be in [%d, %d]\033[0m\n", MIN_HEIGHT, DEVICE_WIDTH);
            if (height < MIN_HEIGHT)
                height = MIN_HEIGHT;
            else if (height > DEVICE_WIDTH)
                height = DEVICE_WIDTH;
        }
            
        g_is = player_init(fp);
        if (g_is == NULL)
        {
            printf("player init failed\n");
            return -1;
        }

        ret = open_demux(g_is);
        if (ret < 0)
        {
            exit_demux(g_is);
            g_is = NULL;
            return -1;
        }

        g_is->out_height = height;
        g_is->out_width  = width;

        printf("set out width : %d, height : %d\n", width, height);

        sstar_audio_init();
        sstar_video_init(g_is, x, y);

        ret = open_video(g_is);
        ret = open_audio(g_is);
    }

    return ret;
}

int tp_player_close(void)
{
    int ret = -1;

    if (g_is)
    {
        printf("close video!\n");
        
        player_deinit(g_is);
        sstar_audio_deinit();
        sstar_video_deinit();
        g_is = NULL;
        ret = 0;
    }

    return ret;
}



