#pragma once
#include "uart/ProtocolSender.h"
#include "SsPlayer.h"
#include "mouse.h"
#include "tp_api.h"
#include <config.h>
/*
*此文件由GUI工具生成
*文件功能：用于处理用户的逻辑相应代码
*功能说明：
*========================onButtonClick_XXXX
当页面中的按键按下后系统会调用对应的函数，XXX代表GUI工具里面的[ID值]名称，
如Button1,当返回值为false的时候系统将不再处理这个按键，返回true的时候系统将会继续处理此按键。比如SYS_BACK.
*========================onSlideWindowItemClick_XXXX(int index) 
当页面中存在滑动窗口并且用户点击了滑动窗口的图标后系统会调用此函数,XXX代表GUI工具里面的[ID值]名称，
如slideWindow1;index 代表按下图标的偏移值
*========================onSeekBarChange_XXXX(int progress) 
当页面中存在滑动条并且用户改变了进度后系统会调用此函数,XXX代表GUI工具里面的[ID值]名称，
如SeekBar1;progress 代表当前的进度值
*========================ogetListItemCount_XXXX() 
当页面中存在滑动列表的时候，更新的时候系统会调用此接口获取列表的总数目,XXX代表GUI工具里面的[ID值]名称，
如List1;返回值为当前列表的总条数
*========================oobtainListItemData_XXXX(ZKListView::ZKListItem *pListItem, int index)
 当页面中存在滑动列表的时候，更新的时候系统会调用此接口获取列表当前条目下的内容信息,XXX代表GUI工具里面的[ID值]名称，
如List1;pListItem 是贴图中的单条目对象，index是列表总目的偏移量。具体见函数说明
*========================常用接口===============
*LOGD(...)  打印调试信息的接口
*mTextXXXPtr->setText("****") 在控件TextXXX上显示文字****
*mButton1Ptr->setSelected(true); 将控件mButton1设置为选中模式，图片会切换成选中图片，按钮文字会切换为选中后的颜色
*mSeekBarPtr->setProgress(12) 在控件mSeekBar上将进度调整到12
*mListView1Ptr->refreshListView() 让mListView1 重新刷新，当列表数据变化后调用
*mDashbroadView1Ptr->setTargetAngle(120) 在控件mDashbroadView1上指针显示角度调整到120度
*
* 在Eclipse编辑器中  使用 “alt + /”  快捷键可以打开智能提示
*/
#if UI_1024_600
#define PANEL_WIDTH		    600
#define PANEL_HEIGHT	    1024

#define WADGET_WIDTH	    600
#define WADGET_HEIGHT	    450

#define TEXT_WADGET_WIDTH	600
#define TEXT_WADGET_HEIGHT	124

#define VIDEO_POS_START     0
#define VIDEO_POS_END       WADGET_HEIGHT

#define JPEG_POS_START      VIDEO_POS_END
#define JPEG_POS_END        (VIDEO_POS_END + WADGET_HEIGHT)

#define TEXT_POS_START      (PANEL_HEIGHT - TEXT_WADGET_HEIGHT)
#define TEXT_POS_END        PANEL_HEIGHT
#endif

static void updateAnimation() {
    char path[64];
    static int animationIndex = 0;
    sprintf(path,"logo_%d.jpg",animationIndex++ % 3);
    if(animationIndex > 3)
    	animationIndex = 0;
#if 0
    static LayoutPosition pos = mTextview1Ptr->getPosition();

    switch(animationIndex){
    case 0:
    	pos.mLeft = 0;
		pos.mTop = 300;
    	pos.mWidth = 480;
    	pos.mHeight = 300;
    	break;
    case 1:
    	pos.mLeft = 10;
		pos.mTop = 310;
    	pos.mWidth = 470;
    	pos.mHeight = 290;
    	break;
    case 2:
    	pos.mLeft = 20;
		pos.mTop = 320;
    	pos.mWidth = 460;
    	pos.mHeight = 280;
    	break;
    case 3:
    	pos.mLeft = 30;
		pos.mTop = 330;
    	pos.mWidth = 450;
    	pos.mHeight = 270;
    	break;
    case 4:
    	pos.mLeft = 40;
		pos.mTop = 340;
    	pos.mWidth = 440;
    	pos.mHeight = 260;
    	break;
    default:
    	break;
    }
    LOGD("++++++++++pic path: %s,width:%d\n",path,pos.mWidth);
    mTextview1Ptr->setPosition(pos);
#endif
    mTextview1Ptr->setBackgroundPic(path);

}

/**
 * 注册定时器
 * 填充数组用于注册定时器
 * 注意：id不能重复
 */
static S_ACTIVITY_TIMEER REGISTER_ACTIVITY_TIMER_TAB[] = {
	//{0,  6000}, //定时器id=0, 时间间隔6秒
	{1,  1000},
};

/**
 * 当界面构造时触发
 */
static void onUI_init(){
    //Tips :添加 UI初始化的显示代码到这里,如:mText1Ptr->setText("123");
    system("echo 12 > /sys/class/gpio/export");
    system("echo out > /sys/class/gpio/gpio12/direction");
    system("echo 1 > /sys/class/gpio/gpio12/value");
}

/**
 * 当切换到该界面时触发
 */
static void onUI_intent(const Intent *intentPtr) {
    if (intentPtr != NULL) {
        //TODO
    }
}

/*
 * 当界面显示时触发
 */
static void onUI_show() {
    #if SS_PLAYER_SWITCH
    tp_player_open ("/customer/demo.mp4",20,30,300,200,1);
    #else
    StartPlayVideo ("/customer/demo.mp4",20,30,300,200);
    #endif
    initMouseDev();
}

/*
 * 当界面隐藏时触发
 */
static void onUI_hide() {

}

/*
 * 当界面完全退出时触发
 */
static void onUI_quit() {

}

/**
 * 串口数据回调接口
 */
static void onProtocolDataUpdate(const SProtocolData &data) {

}

/**
 * 定时器触发函数
 * 不建议在此函数中写耗时操作，否则将影响UI刷新
 * 参数： id
 *         当前所触发定时器的id，与注册时的id相同
 * 返回值: true
 *             继续运行当前定时器
 *         false
 *             停止运行当前定时器
 */
static bool onUI_Timer(int id){
	switch (id) {
	case 1:
		updateAnimation();
		break;

		default:
			break;
	}
    return true;
}

/**
 * 有新的触摸事件时触发
 * 参数：ev
 *         新的触摸事件
 * 返回值：true
 *            表示该触摸事件在此被拦截，系统不再将此触摸事件传递到控件上
 *         false
 *            触摸事件将继续传递到控件上
 */
static bool onmainActivityTouchEvent(const MotionEvent &ev) {
    switch (ev.mActionStatus) {
		case MotionEvent::E_ACTION_DOWN://触摸按下
			LOGD("时刻 = %ld 坐标  x = %d, y = %d", ev.mEventTime, ev.mX, ev.mY);
		{
			//JPEG
			//if(mTextview1Ptr->isVisible() && mTextview1Ptr->getPosition().isHit(ev.mX, ev.mY))
			if((ev.mY > JPEG_POS_START && ev.mY <= JPEG_POS_END))
			{
				static LayoutPosition pos = mTextview1Ptr->getPosition();

		    	pos.mWidth = PANEL_WIDTH - ev.mX;
		    	pos.mHeight = WADGET_HEIGHT * 2 - ev.mY;

		    	pos.mLeft = ev.mX;
		    	pos.mTop = ev.mY;
		    	printf("move jpeg: %d,%d,%d,%d\n",pos.mLeft,pos.mTop,pos.mWidth,pos.mHeight);
		    	mTextview1Ptr->setPosition(pos);

			}
			//VIDEO
			//if(mVideoview1Ptr->isVisible() && mVideoview1Ptr->getPosition().isHit(ev.mX, ev.mY))
			if((ev.mY >= VIDEO_POS_START && ev.mY <= VIDEO_POS_END))
			{
				static LayoutPosition pos = mVideoview1Ptr->getPosition();
		    	pos.mWidth = PANEL_WIDTH - ev.mX;
		    	pos.mHeight = WADGET_HEIGHT - ev.mY;

		    	pos.mLeft = ev.mX;
		    	pos.mTop = ev.mY;

		    	printf("move video: %d,%d,%d,%d\n",pos.mLeft,pos.mTop,pos.mWidth,pos.mHeight);
		    	mVideoview1Ptr->setPosition(pos);

				#if SS_PLAYER_SWITCH
		    	tp_player_close();
		    	usleep(100 * 1000);
		    	tp_player_open ("/customer/demo.mp4",pos.mLeft,pos.mTop,pos.mWidth,pos.mHeight,1);
				#else
		    	StopPlayVideo();
		    	printf("+++++StartPlayVideo+++++++++\n");
		    	StartPlayVideo ("/customer/demo.mp4",pos.mLeft,pos.mTop,pos.mWidth,pos.mHeight);
				#endif
			}
			//TEXT
			//if(mTextview2Ptr->isVisible() && mTextview2Ptr->getPosition().isHit(ev.mX, ev.mY))
			if((ev.mY > TEXT_POS_START && ev.mY <= TEXT_POS_END))
			{
				static LayoutPosition pos = mTextview2Ptr->getPosition();
		    	pos.mWidth = PANEL_WIDTH - ev.mX;
		    	pos.mHeight = PANEL_HEIGHT - ev.mY;

		    	pos.mLeft = ev.mX;
		    	pos.mTop = ev.mY;

		    	printf("move text: %d,%d,%d,%d\n",pos.mLeft,pos.mTop,pos.mWidth,pos.mHeight);
		    	mTextview2Ptr->setPosition(pos);
			}
		}
			break;
		case MotionEvent::E_ACTION_MOVE://触摸滑动
			//LOGD("move 时刻 = %ld 坐标  x = %d, y = %d", ev.mEventTime, ev.mX, ev.mY);
			setMousePos(ev.mX,ev.mY);
			break;
		case MotionEvent::E_ACTION_UP:  //触摸抬起
			break;
		default:
			break;
	}
	return false;
}
