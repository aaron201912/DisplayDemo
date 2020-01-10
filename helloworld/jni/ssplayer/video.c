﻿#include "video.h"
#include "packet.h"
#include "frame.h"
#include "player.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <poll.h>
#include <poll.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

extern AVPacket v_flush_pkt;

static int sstar_nv12_rotate90(AVFrame *dst, AVFrame *src, int width, int height)
{
    static int tmp_width = 0, tmp_height = 0;
    static int uv_height = 0;
    
    if(width != 0 && height != 0)
    {
        tmp_width  = width;
        tmp_height = height;
        uv_height  = height >> 1;
    }

    //旋转Y
    int k = 0, i = 0, j = 0;
    for(i = 0; i < width; i ++)
    {
        int pos = 0;
        for(j = 0; j < height; j ++)
        {
            dst->data[0][k] = src->data[0][pos + i];
            k++;
            pos += width;
        }
    }
    //旋转UV
    k = 0;
    for(i = 0; i < width; i += 2){
        int pos = 0;
        for(int j = 0; j < uv_height; j ++) {
            dst->data[1][k] = src->data[1][pos + i];
            dst->data[1][k + 1] = src->data[1][pos + i + 1];
            k += 2;
            pos += width;
        }
    }
    
    return 0;
}


static int queue_picture(player_stat_t *is, AVFrame *src_frame, double pts, double duration, int64_t pos)
{
    frame_t *vp;

    if (!(vp = frame_queue_peek_writable(&is->video_frm_queue)))
        return -1;

    vp->sar = src_frame->sample_aspect_ratio;
    vp->uploaded = 0;

    vp->width = src_frame->width;
    vp->height = src_frame->height;
    vp->format = src_frame->format;

    vp->pts = pts;
    vp->duration = duration;
    vp->pos = pos;
    //vp->serial = serial;

    //set_default_window_size(vp->width, vp->height, vp->sar);
    
    // 将AVFrame拷入队列相应位置
    av_frame_move_ref(vp->frame, src_frame);

    //printf("queue frame fomat: %d\n",vp->frame->format);
    // 更新队列计数及写索引
    //printf("before queue ridx: %d,widx: %d,size: %d,maxsize: %d\n ",is->video_frm_queue.rindex,is->video_frm_queue.windex,is->video_frm_queue.size,is->video_frm_queue.max_size);
    frame_queue_push(&is->video_frm_queue);
    //printf("after queue ridx: %d,widx: %d,size: %d,maxsize: %d\n ",is->video_frm_queue.rindex,is->video_frm_queue.windex,is->video_frm_queue.size,is->video_frm_queue.max_size);
    return 0;
}

//static int64_t pktpos, pktpts;
//static struct timeval tvStart;
//static bool decoded;

// 从packet_queue中取一个packet，解码生成frame
static int video_decode_frame(AVCodecContext *p_codec_ctx, packet_queue_t *p_pkt_queue, AVFrame *frame)
{
    int ret;

    while (1)
    {
        AVPacket pkt;

        while (1)
        {
            //printf("get in video_decode_frame!\n");
            if(p_pkt_queue->abort_request)
            {
                return -1;
            }
            // 3. 从解码器接收frame
            // 3.1 一个视频packet含一个视频frame
            //     解码器缓存一定数量的packet后，才有解码后的frame输出
            //     frame输出顺序是按pts的顺序，如IBBPBBP
            //     frame->pkt_pos变量是此frame对应的packet在视频文件中的偏移地址，值同pkt.pos
            ret = avcodec_receive_frame(p_codec_ctx, frame);
            if (ret < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    av_log(NULL, AV_LOG_INFO, "video avcodec_receive_frame(): the decoder has been fully flushed\n");
                    avcodec_flush_buffers(p_codec_ctx);
                    return 0;
                }
                else if (ret == AVERROR(EAGAIN))
                {
                    //av_log(NULL, AV_LOG_INFO, "video avcodec_receive_frame(): output is not available in this state - "
                    //"user must try to send new input\n");
                    //av_log(NULL, AV_LOG_ERROR, "ret : %d, cann't fetch a frame, try again!\n", ret);
                    break;
                }
                else
                {
                    av_log(NULL, AV_LOG_ERROR, "video avcodec_receive_frame(): other errors\n");
                    continue;
                }
            }
            else
            {
                frame->pts = frame->best_effort_timestamp;
                //printf("best_effort_timestamp : %lld.\n", frame->pts);
                //printf("frame pos: %lld\n",frame->pkt_pos);
                //if(frame->pkt_pos == pktpos || frame->pts == pktpts)
                //{
                //    struct timeval tv;
                //    int64_t time;
                //    
                //    gettimeofday(&tv, NULL);
                //    time = ((int64_t)tv.tv_sec * 1000000 + tv.tv_usec) - ((int64_t)tvStart.tv_sec * 1000000 + tvStart.tv_usec);
                //    
                //    printf("fps: %lld\n",time);
                //    decoded = 0;
                //}

                return 1;   // 成功解码得到一个视频帧或一个音频帧，则返回
            }
        }

        // 1. 取出一个packet。使用pkt对应的serial赋值给d->pkt_serial
        if (packet_queue_get(p_pkt_queue, &pkt, true) < 0)
        {
            printf("packet_queue_get fail\n");
            return -1;
        }

        if (pkt.data == v_flush_pkt.data)
        {
            // 复位解码器内部状态/刷新内部缓冲区。
            avcodec_flush_buffers(p_codec_ctx);
        }
        else
        {
            // 如果是最后一个空的packet,只取frame不再送packet
            if (pkt.data == NULL || pkt.size == 0)
            {
                p_codec_ctx->flags |= (1 << 5);
                printf("send a null paket to decoder\n");
            }
            else
            {
                p_codec_ctx->flags &= ~(1 << 5);
            }
            // 2. 将packet发送给解码器
            //    发送packet的顺序是按dts递增的顺序，如IPBBPBB
            //    pkt.pos变量可以标识当前packet在视频文件中的地址偏移
            //printf("send packet to decoder!\n");
            //printf("pkt pos: %lld\n",pkt.pos);
            //if(!decoded)
            //{
            //    pktpos = pkt.pos;
            //    pktpts = pkt.pts;
            //    gettimeofday(&tvStart, NULL);
            //    decoded = 1;
            //    //printf("packet info : dts--%d,pts--%d\n", pkt.dts, pkt.pts);
            //}
            //calctime = av_gettime_relative();

            if (avcodec_send_packet(p_codec_ctx, &pkt) == AVERROR(EAGAIN))
            {
                av_log(NULL, AV_LOG_ERROR, "receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
            }
            av_packet_unref(&pkt);
        }
        //printf("exit out video_decode_frame!\n");
    }
}

// 将视频包解码得到视频帧，然后写入picture队列
static void * video_decode_thread(void *arg)
{
    player_stat_t *is = (player_stat_t *)arg;
    AVFrame *p_frame = av_frame_alloc();
    double pts;
    double duration;
    int ret;
    int got_picture;
    AVRational tb = is->p_video_stream->time_base;
    AVRational frame_rate = av_guess_frame_rate(is->p_fmt_ctx, is->p_video_stream, NULL);
    
    if (p_frame == NULL)
    {
        av_log(NULL, AV_LOG_ERROR, "av_frame_alloc() for p_frame failed\n");
        return NULL;
    }

    printf("video time base : %f ms.\n", 1000 * av_q2d(tb));
    printf("frame rate num : %d. frame rate den : %d.\n", frame_rate.num, frame_rate.den);

    printf("get in video decode thread!\n");

    while (1)
    {
        if(is->abort_request)
        {
            printf("video decode thread exit\n");
            break;
        }
        got_picture = video_decode_frame(is->p_vcodec_ctx, &is->video_pkt_queue, p_frame);
        if (got_picture < 0)
        {
            printf("got pic fail\n");
            goto exit;
        } 
        else if (got_picture > 0)
        {
            duration = (frame_rate.num && frame_rate.den ? av_q2d((AVRational){frame_rate.den, frame_rate.num}) : 0);   // 当前帧播放时长
            pts = (p_frame->pts == AV_NOPTS_VALUE) ? NAN : p_frame->pts * av_q2d(tb);   // 当前帧显示时间戳

            //printf("frame duration : %f. video frame clock : %f.\n", duration, pts);
            ret = queue_picture(is, p_frame, pts, duration, p_frame->pkt_pos);   // 将当前帧压入frame_queue
            av_frame_unref(p_frame);
        }

        if (ret < 0)
        {
            goto exit;
        }

    }

exit:
    av_frame_free(&p_frame);

    return NULL;
}

// 根据视频时钟与同步时钟(如音频时钟)的差值，校正delay值，使视频时钟追赶或等待同步时钟
// 输入参数delay是上一帧播放时长，即上一帧播放后应延时多长时间后再播放当前帧，通过调节此值来调节当前帧播放快慢
// 返回值delay是将输入参数delay经校正后得到的值
static double compute_target_delay(double delay, player_stat_t *is)
{
    double sync_threshold, diff = 0;

    /* update delay to follow master synchronisation source */

    /* if video is slave, we try to correct big delays by
       duplicating or deleting a frame */
    // 视频时钟与同步时钟(如音频时钟)的差异，时钟值是上一帧pts值(实为：上一帧pts + 上一帧至今流逝的时间差)
    if (is->audio_idx >= 0 && is->video_idx >= 0)
        diff = get_clock(&is->video_clk) - get_clock(&is->audio_clk);
    else 
        return delay;
    //printf("audio pts: %lf,video pts: %lf\n",is->audio_clk.pts,is->video_clk.pts);
    //printf("audio clock: %lf,video clock: %lf\n",get_clock(&is->audio_clk),get_clock(&is->video_clk));
    //printf("video pts: %lf,lu: %lf,curtime: %lf\n ",is->video_clk.pts,is->video_clk.last_updated,av_gettime_relative() / 1000000.0);
    //printf("audio pts: %lf,lu: %lf,curtime: %lf\n ",is->audio_clk.pts,is->audio_clk.last_updated,av_gettime_relative() / 1000000.0);
    //printf("video diff audio time: %lf\n",diff);
    // delay是上一帧播放时长：当前帧(待播放的帧)播放时间与上一帧播放时间差理论值
    // diff是视频时钟与同步时钟的差值

    /* skip or repeat frame. We take into account the
       delay to compute the threshold. I still don't know
       if it is the best guess */
    // 若delay < AV_SYNC_THRESHOLD_MIN，则同步域值为AV_SYNC_THRESHOLD_MIN
    // 若delay > AV_SYNC_THRESHOLD_MAX，则同步域值为AV_SYNC_THRESHOLD_MAX
    // 若AV_SYNC_THRESHOLD_MIN < delay < AV_SYNC_THRESHOLD_MAX，则同步域值为delay
    sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));
    if (!isnan(diff))
    {
        if (diff <= -sync_threshold)        // 视频时钟落后于同步时钟，且超过同步域值
            delay = FFMAX(0, delay + diff); // 当前帧播放时刻落后于同步时钟(delay+diff<0)则delay=0(视频追赶，立即播放)，否则delay=delay+diff
        else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD)  // 视频时钟超前于同步时钟，且超过同步域值，但上一帧播放时长超长
            delay = delay + diff;           // 仅仅校正为delay=delay+diff，主要是AV_SYNC_FRAMEDUP_THRESHOLD参数的作用
        else if (diff >= sync_threshold)    // 视频时钟超前于同步时钟，且超过同步域值
            delay = 2 * delay;              // 视频播放要放慢脚步，delay扩大至2倍
    }

    //av_log(NULL, AV_LOG_INFO, "video: delay=%0.3lf A-V=%lf\n", delay, -diff);
    //printf("video: delay=%0.3f A-V=%f\n", delay, -diff);

    return delay;
}

static double vp_duration(player_stat_t *is, frame_t *vp, frame_t *nextvp) {
    if (vp->serial == nextvp->serial)
    {
        double duration = nextvp->pts - vp->pts;
        if (isnan(duration) || duration <= 0)
            return vp->duration;
        else
            return duration;
    } else {
        return 0.0;
    }
}

static void update_video_pts(player_stat_t *is, double pts, int64_t pos, int serial) {
    /* update current video pts */
    set_clock(&is->video_clk, pts, serial);            // 更新vidclock
    //-sync_clock_to_slave(&is->extclk, &is->vidclk);  // 将extclock同步到vidclock
}

static void video_display(player_stat_t *is)
{
    frame_t *vp;
    uint8_t *frame_ydata, *frame_uvdata;

    //av_log(NULL, AV_LOG_ERROR, "rindex : %d, shownidex : %d, size : %d\n", is->video_frm_queue.rindex, is->video_frm_queue.rindex_shown, is->video_frm_queue.size);
    vp = frame_queue_peek_last(&is->video_frm_queue);
    //printf("frame data addr : %p %p\n", vp->frame->data[0], vp->frame->data[1]);
    //vp->frame->linesize[AV_NUM_DATA_POINTERS] = {vp->frame->width, vp->frame->width, 0, 0, 0, 0, 0, 0};
    //printf("get vp disp ridx: %d,format: %d\n",is->video_frm_queue.rindex,vp->frame->format);      

    // 图像转换：p_frm_raw->data ==> p_frm_yuv->data
    // 将源图像中一片连续的区域经过处理后更新到目标图像对应区域，处理的图像区域必须逐行连续
    // plane: 如YUV有Y、U、V三个plane，RGB有R、G、B三个plane
    // slice: 图像中一片连续的行，必须是连续的，顺序由顶部到底部或由底部到顶部
    // stride/pitch: 一行图像所占的字节数，Stride=BytesPerPixel*Width+Padding，注意对齐
    // AVFrame.*data[]: 每个数组元素指向对应plane
    // AVFrame.linesize[]: 每个数组元素表示对应plane中一行图像所占的字节数
    //struct timeval trans_start, trans_tim, trans_end;
    //int64_t time0, time1;
    
    //gettimeofday(&trans_start, NULL);
    
    if (is->decoder_type == SOFT_DECODING) {
        #if 0
        sws_scale(is->img_convert_ctx,                    // sws context
                  (const uint8_t *const *)vp->frame->data,// src slice
                  vp->frame->linesize,                    // src stride
                  0,                                      // src slice y
                  is->p_vcodec_ctx->height,               // src slice height
                  is->p_frm_yuv->data,                    // dst planes
                  is->p_frm_yuv->linesize                 // dst strides
                  );

        frame_ydata  = is->p_frm_yuv->data[0];
        frame_uvdata = is->p_frm_yuv->data[1];
        #endif
    } else if (is->decoder_type == HARD_DECODING) {
        frame_ydata  = vp->frame->data[0];
        frame_uvdata = vp->frame->data[1];
    }

    //gettimeofday(&trans_tim, NULL);

    int ysize, index;
    ysize = vp->frame->width * vp->frame->height;
    //printf("save yuv width: %d,height: %d\n",vp->frame->width,vp->frame->height);

    //put stream to ss disp start
    MI_SYS_BUF_HANDLE hHandle;
    MI_SYS_ChnPort_t  pstSysChnPort;
    MI_SYS_BufConf_t  stBufConf;
    MI_SYS_BufInfo_t  stBufInfo;

    //pstSysChnPort.eModId    = E_MI_MODULE_ID_DIVP;
    pstSysChnPort.eModId    = E_MI_MODULE_ID_DISP;
    pstSysChnPort.u32ChnId  = 0;
    pstSysChnPort.u32DevId  = 0;
    pstSysChnPort.u32PortId = 0;

    memset(&stBufInfo , 0 , sizeof(MI_SYS_BufInfo_t));
    memset(&stBufConf , 0 , sizeof(MI_SYS_BufConf_t));

    stBufConf.eBufType             = E_MI_SYS_BUFDATA_FRAME;
    stBufConf.u64TargetPts         = 0;
    stBufConf.stFrameCfg.u16Width  = ALIGN_BACK(is->out_width,  16);
    stBufConf.stFrameCfg.u16Height = ALIGN_BACK(is->out_height, 64);
    stBufConf.stFrameCfg.eFormat        = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420;
    stBufConf.stFrameCfg.eFrameScanMode = E_MI_SYS_FRAME_SCAN_MODE_PROGRESSIVE;
    stBufConf.stFrameCfg.stFrameBufExtraConf.u16BufHAlignment = 128;   // 宽128对齐
    stBufConf.stFrameCfg.stFrameBufExtraConf.u16BufVAlignment = 64;    // 高32对齐

    if(MI_SUCCESS == MI_SYS_ChnInputPortGetBuf(&pstSysChnPort,&stBufConf,&stBufInfo,&hHandle, -1))
    {
        stBufInfo.stFrameData.eCompressMode = E_MI_SYS_COMPRESS_MODE_NONE;
        stBufInfo.stFrameData.eFieldType    = E_MI_SYS_FIELDTYPE_NONE;
        stBufInfo.stFrameData.eTileMode     = E_MI_SYS_FRAME_TILE_MODE_NONE;
        stBufInfo.bEndOfStream              = FALSE;
        
        //printf("\033[32;2mdisp width : %d, stride : %d, height : %d\033[0m\n", stBufInfo.stFrameData.u16Width, 
        //stBufInfo.stFrameData.u32Stride[0], stBufInfo.stFrameData.u16Height);
      
        //向DIVP中填数据时必须按照stride大小填充
        if (stBufInfo.stFrameData.u32Stride[0] == vp->frame->width) {
            memcpy(stBufInfo.stFrameData.pVirAddr[0], frame_ydata , ysize);
            memcpy(stBufInfo.stFrameData.pVirAddr[1], frame_uvdata, ysize / 2);
        } 
        else
        {
            int valid_width = FFMIN(vp->frame->width, stBufInfo.stFrameData.u32Stride[0]);
            for (index = 0; index < vp->frame->height; index ++)
            {
                memcpy(stBufInfo.stFrameData.pVirAddr[0] + index * stBufInfo.stFrameData.u32Stride[0], 
                       frame_ydata + index * vp->frame->width, 
                       valid_width);
            }
            for (index = 0; index < vp->frame->height / 2; index ++)
            {
                memcpy(stBufInfo.stFrameData.pVirAddr[1] + index * stBufInfo.stFrameData.u32Stride[1], 
                       frame_uvdata + index * vp->frame->width, 
                       valid_width);
            }
        }

        //printf("data0: %p,data1: %p,data2: %p\n",vp->frame->data[0],vp->frame->data[1],vp->frame->data[2]);
        MI_SYS_ChnInputPortPutBuf(hHandle ,&stBufInfo , FALSE);
    }
    //put stream to ss disp end
//    gettimeofday(&trans_end, NULL);
//    time0 = ((int64_t)trans_tim.tv_sec * 1000000 + trans_tim.tv_usec) - ((int64_t)trans_start.tv_sec * 1000000 + trans_start.tv_usec);
//    time1 = ((int64_t)trans_end.tv_sec * 1000000 + trans_end.tv_usec) - ((int64_t)trans_tim.tv_sec * 1000000 + trans_tim.tv_usec);
//    printf("yuv420p to nv12 : %lldus, send to divp : %lldus\n", time0, time1);
}

/* called to display each frame */
static int video_refresh(void *opaque, double *remaining_time)
{
    player_stat_t *is = (player_stat_t *)opaque;
    double time;
    static bool first_frame = true;
    
retry:
    // 暂停处理：不停播放上一帧图像
    if (is->paused)
        goto display;
        
    if (frame_queue_nb_remaining(&is->video_frm_queue) <= 0)  // 所有帧已显示
    {    
        // nothing to do, no picture to display in the queue
        //printf("already last frame: %d\n",is->video_frm_queue.size);
        if (!is->video_complete && is->eof && is->video_pkt_queue.nb_packets == 0)
        {
            is->video_complete = 1;
            if (is->video_complete && is->audio_complete)
                stream_seek(is, is->p_fmt_ctx->start_time, 0, 0);
            //printf("video play completely!\n");
        }
        return 0;
    }

    double last_duration, duration, delay;
    frame_t *vp, *lastvp;

    /* dequeue the picture */
    lastvp = frame_queue_peek_last(&is->video_frm_queue);     // 上一帧：上次已显示的帧
    vp = frame_queue_peek(&is->video_frm_queue);              // 当前帧：当前待显示的帧
    //printf("refresh ridx: %d,rs:%d,widx: %d,size: %d,maxsize: %d\n",is->video_frm_queue.rindex,is->video_frm_queue.rindex_shown,is->video_frm_queue.windex,is->video_frm_queue.size,is->video_frm_queue.max_size);
    //printf("lastpos: %ld,lastpts: %lf,vppos: %ld,vppts: %lf\n",lastvp->pos,lastvp->pts,vp->pos,vp->pts);
    // lastvp和vp不是同一播放序列(一个seek会开始一个新播放序列)，将frame_timer更新为当前时间
    if (first_frame)
    {
        is->frame_timer = av_gettime_relative() / 1000000.0;
        first_frame = false;
    }

    /* compute nominal last_duration */
    last_duration = vp_duration(is, lastvp, vp);        // 上一帧播放时长：vp->pts - lastvp->pts
    delay = compute_target_delay(last_duration, is);    // 根据视频时钟和同步时钟的差值，计算delay值
    //printf("last_duration: %lf,delay: %lf\n",last_duration,delay);
    time= av_gettime_relative()/1000000.0;
    // 当前帧播放时刻(is->frame_timer+delay)大于当前时刻(time)，表示播放时刻未到
    if (time < is->frame_timer + delay) {
        // 播放时刻未到，则更新刷新时间remaining_time为当前时刻到下一播放时刻的时间差
        *remaining_time = FFMIN(is->frame_timer + delay - time, *remaining_time);
        // 播放时刻未到，则不播放，直接返回
        //printf("not ready play\n");
        return 0;
    }
    //printf("remaining time : %f. duration : %f.\n", *remaining_time, last_duration);
    // 更新frame_timer值
    is->frame_timer += delay;
    //printf("frame_timer : %0.6lf, video pts : %0.6lf, mremaining : %0.6lf\n", is->frame_timer, vp->pts, *remaining_time);
    // 校正frame_timer值：若frame_timer落后于当前系统时间太久(超过最大同步域值)，则更新为当前系统时间
    if (delay > 0 && time - is->frame_timer > AV_SYNC_THRESHOLD_MAX)
    {
        is->frame_timer = time;
    }

    pthread_mutex_lock(&is->video_frm_queue.mutex);
    if (!isnan(vp->pts))
    {
        update_video_pts(is, vp->pts, vp->pos, vp->serial); // 更新视频时钟：时间戳、时钟时间
    }
    pthread_mutex_unlock(&is->video_frm_queue.mutex);

    // 是否要丢弃未能及时播放的视频帧
    if (frame_queue_nb_remaining(&is->video_frm_queue) > 1)  // 队列中未显示帧数>1(只有一帧则不考虑丢帧)
    {         
        frame_t *nextvp = frame_queue_peek_next(&is->video_frm_queue);  // 下一帧：下一待显示的帧
        duration = vp_duration(is, vp, nextvp);             // 当前帧vp播放时长 = nextvp->pts - vp->pts
        // 当前帧vp未能及时播放，即下一帧播放时刻(is->frame_timer+duration)小于当前系统时刻(time)
        if (time > is->frame_timer + duration)
        {
            frame_queue_next(&is->video_frm_queue);   // 删除上一帧已显示帧，即删除lastvp，读指针加1(从lastvp更新到vp)
            //av_log(NULL, AV_LOG_INFO, "discard current frame!\n");
            goto retry;
        }
    }

    //AVRational frame_rate = av_guess_frame_rate(is->p_fmt_ctx, is->p_video_stream, NULL);
    //*remaining_time = av_q2d((AVRational){frame_rate.den, frame_rate.num});    //no sync
    //printf("remaining time : %f.\n", (*remaining_time) * AV_TIME_BASE);

    // 删除当前读指针元素，读指针+1。若未丢帧，读指针从lastvp更新到vp；若有丢帧，读指针从vp更新到nextvp
    frame_queue_next(&is->video_frm_queue);

    if (is->step && !is->paused)
    {
        stream_toggle_pause(is);
    }

display:
    video_display(is);                      // 取出当前帧vp(若有丢帧是nextvp)进行播放

    return 0;
}

static void * video_playing_thread(void *arg)
{
    player_stat_t *is = (player_stat_t *)arg;
    double remaining_time = 0.0;
    printf("video_playing_thread in\n");
    while (1)
    {
        if(is->abort_request)
        {
            printf("video play thread exit\n");
            break;
        }
        if (remaining_time > 0.0)
        {
            //printf("delay time: %lf\n",remaining_time);
            av_usleep((unsigned)(remaining_time * 1000000.0));
        }
        remaining_time = REFRESH_RATE;
        // 立即显示当前帧，或延时remaining_time后再显示
        video_refresh(is, &remaining_time);
    }

    return NULL;
}

MI_S32 sstar_sys_bind(ST_Sys_BindInfo_t *pstBindInfo)
{
    MI_SYS_BindChnPort(&pstBindInfo->stSrcChnPort, &pstBindInfo->stDstChnPort, \
        pstBindInfo->u32SrcFrmrate, pstBindInfo->u32DstFrmrate);

    return MI_SUCCESS;
}

MI_S32 sstar_sys_unbind(ST_Sys_BindInfo_t *pstBindInfo)
{
    MI_SYS_UnBindChnPort(&pstBindInfo->stSrcChnPort, &pstBindInfo->stDstChnPort);

    return MI_SUCCESS;
}

#if 0

#ifdef   UI_1024_600
    #include "SAT070CP50_1024x600.h"
#else
    #ifdef USE_MIPI
    #include "ST7701S_480x480_MIPI.h"
    #else
    #include "SAT070AT50_800x480.h"
    #endif
#endif

int sstar_panel_init(MI_DISP_Interface_e eType)
{
    MI_PANEL_LinkType_e eLinkType;
    MI_DISP_InputPortAttr_t stInputPortAttr;
    MI_DISP_PubAttr_t stDispPubAttr;

    stDispPubAttr.eIntfType  = eType;

    if (stDispPubAttr.eIntfType == E_MI_DISP_INTF_LCD)
    {
        stDispPubAttr.stSyncInfo.u16Vact       = stPanelParam.u16Height;
        stDispPubAttr.stSyncInfo.u16Vbb        = stPanelParam.u16VSyncBackPorch;
        stDispPubAttr.stSyncInfo.u16Vfb        = stPanelParam.u16VTotal - (stPanelParam.u16VSyncWidth +
                                                  stPanelParam.u16Height + stPanelParam.u16VSyncBackPorch);
        stDispPubAttr.stSyncInfo.u16Hact       = stPanelParam.u16Width;
        stDispPubAttr.stSyncInfo.u16Hbb        = stPanelParam.u16HSyncBackPorch;
        stDispPubAttr.stSyncInfo.u16Hfb        = stPanelParam.u16HTotal - (stPanelParam.u16HSyncWidth +
                                                  stPanelParam.u16Width + stPanelParam.u16HSyncBackPorch);
        stDispPubAttr.stSyncInfo.u16Bvact      = 0;
        stDispPubAttr.stSyncInfo.u16Bvbb       = 0;
        stDispPubAttr.stSyncInfo.u16Bvfb       = 0;
        stDispPubAttr.stSyncInfo.u16Hpw        = stPanelParam.u16HSyncWidth;
        stDispPubAttr.stSyncInfo.u16Vpw        = stPanelParam.u16VSyncWidth;
        stDispPubAttr.stSyncInfo.u32FrameRate  = stPanelParam.u16DCLK * 1000000 / (stPanelParam.u16HTotal * stPanelParam.u16VTotal);
        stDispPubAttr.eIntfSync                = E_MI_DISP_OUTPUT_USER;
        stDispPubAttr.eIntfType                = E_MI_DISP_INTF_LCD;
        stDispPubAttr.u32BgColor               = YUYV_BLACK;

        #ifdef USE_MIPI
        eLinkType = E_MI_PNL_LINK_MIPI_DSI;
        #else
        eLinkType = E_MI_PNL_LINK_TTL;
        #endif
    }

    memset(&stInputPortAttr, 0, sizeof(MI_DISP_InputPortAttr_t));
    stInputPortAttr.u16SrcWidth         = PANEL_WIDTH;
    stInputPortAttr.u16SrcHeight        = PANEL_HEIGHT;
    stInputPortAttr.stDispWin.u16X      = LOCAL_X;
    stInputPortAttr.stDispWin.u16Y      = LOCAL_Y;
    stInputPortAttr.stDispWin.u16Width  = PANEL_WIDTH;
    stInputPortAttr.stDispWin.u16Height = PANEL_HEIGHT;

    MI_DISP_SetPubAttr(0, &stDispPubAttr);
    MI_DISP_Enable(0);
    MI_DISP_BindVideoLayer(0, 0);
    MI_DISP_EnableVideoLayer(0);

    MI_DISP_SetInputPortAttr(0, 0, &stInputPortAttr);
    MI_DISP_EnableInputPort(0, 0);
    MI_DISP_SetInputPortSyncMode(0, 0, E_MI_DISP_SYNC_MODE_FREE_RUN);

    if (stDispPubAttr.eIntfType == E_MI_DISP_INTF_LCD)
    {
        MI_PANEL_Init(eLinkType);
        MI_PANEL_SetPanelParam(&stPanelParam);
        if(eLinkType == E_MI_PNL_LINK_MIPI_DSI)
        {
            #ifdef USE_MIPI
            MI_PANEL_SetMipiDsiConfig(&stMipiDsiConfig);
            #endif
        }
    }

    return 0;
}

int sstar_panel_deinit(void)
{
    MI_DISP_DisableInputPort(0, 0);
    MI_DISP_DisableVideoLayer(0);
    MI_DISP_UnBindVideoLayer(0, 0);
    MI_DISP_Disable(0);

    MI_PANEL_DeInit();

    return 0;
}
#endif

int sstar_video_init(player_stat_t *is, uint16_t x, uint16_t y)
{
    MI_DISP_VideoLayerAttr_t stLayerAttr;
    MI_DISP_InputPortAttr_t stInputPortAttr;
    MI_DIVP_OutputPortAttr_t stDivpOutAttr;
    MI_DIVP_ChnAttr_t stDivpChnAttr;
    MI_DISP_RotateConfig_t stRotateConfig;;

    // Init DISP module
    memset(&stInputPortAttr, 0, sizeof(MI_DISP_InputPortAttr_t));
    MI_DISP_GetInputPortAttr(0, 0, &stInputPortAttr);
    stInputPortAttr.stDispWin.u16X      = DEVICE_WIDTH - y - is->out_height;
    stInputPortAttr.stDispWin.u16Y      = x;
    stInputPortAttr.stDispWin.u16Width  = is->out_height;
    stInputPortAttr.stDispWin.u16Height = is->out_width;
    //宽高需要对齐
    stInputPortAttr.u16SrcWidth         = ALIGN_BACK(is->out_width,  16);
    stInputPortAttr.u16SrcHeight        = ALIGN_BACK(is->out_height, 64);
    printf("disp input width : %d, height : %d\n", stInputPortAttr.u16SrcWidth, stInputPortAttr.u16SrcHeight);

    // 设置旋转选项
    memset(&stRotateConfig, 0, sizeof(MI_DISP_RotateConfig_t));
    stRotateConfig.eRotateMode          = E_MI_DISP_ROTATE_90;
    MI_DISP_SetVideoLayerRotateMode(0, &stRotateConfig);

    MI_DISP_SetInputPortAttr(0, 0, &stInputPortAttr);
    MI_DISP_EnableInputPort(0, 0);
    MI_DISP_SetInputPortSyncMode(0, 0, E_MI_DISP_SYNC_MODE_FREE_RUN);

    //MI_VDEC_SetOutputPortLayoutMode(E_MI_VDEC_OUTBUF_LAYOUT_TILE);

    return 0;
}

int sstar_video_deinit(void)
{
    MI_DISP_DEV dispDev = 0;
    MI_DISP_LAYER dispLayer = 0;
    MI_U32 u32InputPort = 0;

    MI_DISP_DisableInputPort(dispLayer, u32InputPort);

    return 0;
}


static int open_video_playing(void *arg)
{

    player_stat_t *is = (player_stat_t *)arg;
    int ret;
    int buf_size;
    uint8_t* buffer = NULL;
    const AVPixFmtDescriptor *desc;

    #if 0
    // 为AVFrame.*data[]手工分配缓冲区，用于存储sws_scale()中目的帧视频数据
    buf_size = av_image_get_buffer_size(AV_PIX_FMT_NV12, 
                                        is->out_width,
                                        is->out_height,
                                        1
                                        );
    //printf("alloc size: %d,width: %d,height: %d\n",buf_size,is->p_vcodec_ctx->width,is->p_vcodec_ctx->height);
    // buffer将作为p_frm_yuv的视频数据缓冲区
    buffer = (uint8_t *)av_malloc(buf_size);
    if (buffer == NULL)
    {
        printf("av_malloc() for buffer failed\n");
        return -1;
    }
    // 使用给定参数设定p_frm_yuv->data和p_frm_yuv->linesize
    ret = av_image_fill_arrays(is->p_frm_yuv->data,     // dst data[]
                               is->p_frm_yuv->linesize, // dst linesize[]
                               buffer,                  // src buffer
                               AV_PIX_FMT_NV12,         // pixel format
                               is->out_width,
                               is->out_height,
                               1                        // align
                               );
    if (ret < 0)
    {
        printf("av_image_fill_arrays() failed %d\n", ret);
        return -1;;
    }

    // A2. 初始化SWS context，用于后续图像转换
    //     此处第6个参数使用的是FFmpeg中的像素格式，对比参考注释B3
    //     FFmpeg中的像素格式AV_PIX_FMT_YUV420P对应SDL中的像素格式SDL_PIXELFORMAT_IYUV
    //     如果解码后得到图像的不被SDL支持，不进行图像转换的话，SDL是无法正常显示图像的
    //     如果解码后得到图像的能被SDL支持，则不必进行图像转换
    //     这里为了编码简便，统一转换为SDL支持的格式AV_PIX_FMT_YUV420P==>SDL_PIXELFORMAT_IYUV
    desc = av_pix_fmt_desc_get(is->p_vcodec_ctx->pix_fmt);
    printf("video prefix format : %s.\n", desc->name);

    is->img_convert_ctx = sws_getContext(is->p_vcodec_ctx->width,   // src width
                                         is->p_vcodec_ctx->height,  // src height
                                         is->p_vcodec_ctx->pix_fmt, // src format
                                         is->out_width,
                                         is->out_height,
                                         AV_PIX_FMT_NV12,           // dst format
                                         SWS_POINT,                 // flags
                                         NULL,                      // src filter
                                         NULL,                      // dst filter
                                         NULL                       // param
                                         );
    if (is->img_convert_ctx == NULL)
    {
        printf("sws_getContext() failed\n");
        return -1;
    }
    #endif

    CheckFuncResult(pthread_create(&is->videoPlay_tid, NULL, video_playing_thread, is));

    return 0;
}

static int open_video_stream(player_stat_t *is)
{
    AVCodecParameters* p_codec_par = NULL;
    AVCodec* p_codec = NULL;
    AVCodecContext* p_codec_ctx = NULL;
    AVStream *p_stream = is->p_video_stream;
    int ret;

    // 1. 为视频流构建解码器AVCodecContext
    // 1.1 获取解码器参数AVCodecParameters
    p_codec_par = p_stream->codecpar;

    // 1.2 获取解码器
    switch(p_codec_par->codec_id) 
    {
        case AV_CODEC_ID_H264 : 
            p_codec = avcodec_find_decoder_by_name("ssh264"); 
            is->decoder_type = HARD_DECODING;
            break;

        case AV_CODEC_ID_HEVC : 
            p_codec = avcodec_find_decoder_by_name("sshevc"); 
            is->decoder_type = HARD_DECODING;
            break;

        default : 
            //p_codec = avcodec_find_decoder(p_codec_par->codec_id);
            //is->decoder_type = SOFT_DECODING;
            printf("Error: video is not H264 or H265 encoding!\n");
            break;
    }

    if (p_codec == NULL)
    {
        printf("Cann't find codec!\n");
        return -1;
    }
    printf("open codec: %s\n",p_codec->name);

    // 1.3 构建解码器AVCodecContext
    // 1.3.1 p_codec_ctx初始化：分配结构体，使用p_codec初始化相应成员为默认值
    p_codec_ctx = avcodec_alloc_context3(p_codec);
    if (p_codec_ctx == NULL)
    {
        printf("avcodec_alloc_context3() failed\n");
        return -1;
    }

    // 通过设置这两个参数配置vdec输出宽高
    p_codec_ctx->flags  = ALIGN_BACK(is->out_width,  16);
    p_codec_ctx->flags2 = ALIGN_BACK(is->out_height, 64);
    printf("vdec out width : %d, height : %d\n", p_codec_ctx->flags, p_codec_ctx->flags2);

    // 1.3.2 p_codec_ctx初始化：p_codec_par ==> p_codec_ctx，初始化相应成员
    ret = avcodec_parameters_to_context(p_codec_ctx, p_codec_par);
    if (ret < 0)
    {
        printf("avcodec_parameters_to_context() failed\n");
        return -1;
    }

    // 1.3.3 p_codec_ctx初始化：使用p_codec初始化p_codec_ctx，初始化完成
    ret = avcodec_open2(p_codec_ctx, p_codec, NULL);
    if (ret < 0)
    {
        printf("avcodec_open2() failed %d\n", ret);
        return -1;
    }

    //配置tilemode用于旋转
    MI_VDEC_SetOutputPortLayoutMode(E_MI_VDEC_OUTBUF_LAYOUT_TILE);

    is->p_vcodec_ctx = p_codec_ctx;
    is->p_vcodec_ctx->debug = true;
    is->p_vcodec_ctx->flags  = 0;
    is->p_vcodec_ctx->flags2 = 0;
    printf("codec width: %d,height: %d\n",is->p_vcodec_ctx->width,is->p_vcodec_ctx->height);
    printf("bistream width : %d, height : %d\n", is->p_vcodec_ctx->coded_width,is->p_vcodec_ctx->coded_height);
    // 2. 创建视频解码线程
    CheckFuncResult(pthread_create(&is->videoDecode_tid, NULL, video_decode_thread, is));

    return 0;
}

int open_video(player_stat_t *is)
{
    if (is && is->video_idx >= 0) {
        open_video_stream(is);
 
        open_video_playing(is);
    } 

    return 0;
}

