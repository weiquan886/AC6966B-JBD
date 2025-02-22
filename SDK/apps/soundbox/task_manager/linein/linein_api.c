#include "system/includes.h"
#include "app_config.h"
#include "app_task.h"
#include "media/includes.h"
#include "tone_player.h"
#include "audio_dec_linein.h"
#include "asm/audio_linein.h"
#include "app_main.h"
#include "ui_manage.h"
#include "vm.h"
#include "linein/linein_dev.h"
#include "linein/linein.h"
#include "key_event_deal.h"
#include "user_cfg.h"
#include "ui/ui_api.h"
#include "fm_emitter/fm_emitter_manage.h"
#include "clock_cfg.h"
#include "bt.h"
#include "bt_tws.h"
#ifndef CONFIG_MEDIA_NEW_ENABLE
#include "application/audio_output_dac.h"
#endif

/*************************************************************
   此文件函数主要是linein实现api
**************************************************************/


#if TCFG_APP_LINEIN_EN

#define LOG_TAG_CONST       APP_LINEIN
#define LOG_TAG             "[APP_LINEIN]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#include "user_fun_cfg.h"

struct linein_opr {
    void *rec_dev;
    u8 volume : 7;
    u8 onoff  : 1;
};
static struct linein_opr linein_hdl = {0};
#define __this 	(&linein_hdl)


/**@brief   linein 音量设置函数
   @param    需要设置的音量
   @return
   @note    在linein 情况下针对了某些情景进行了处理，设置音量需要使用独立接口
*/
/*----------------------------------------------------------------------------*/
int linein_volume_set(u8 vol)
{
    if (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACL_CH) {
        app_audio_output_ch_analog_gain_set(BIT(0), 0);
        /* app_audio_output_ch_analog_gain_set(BIT(1), vol); */
    } else if (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACR_CH) {
        app_audio_output_ch_analog_gain_set(BIT(1), 0);
        /* app_audio_output_ch_analog_gain_set(BIT(0), vol); */
    }
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, vol, 1);
    log_info("linein vol: %d", __this->volume);
    __this->volume = vol;

#if (TCFG_DEC2TWS_ENABLE)
    bt_tws_sync_volume();
#endif

#if (TCFG_LINEIN_INPUT_WAY != LINEIN_INPUT_WAY_ADC)
    if (__this->volume) {
        audio_linein_mute(0);
    } else {
        audio_linein_mute(1);
    }
#endif

    return true;
}




/*----------------------------------------------------------------------------*/
/**@brief    linein 使用模拟直通方式
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static inline void __linein_way_analog_start()
{
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());//
    if (!app_audio_get_volume(APP_AUDIO_STATE_MUSIC)) {
        audio_linein_mute(1);    //模拟输出时候，dac为0也有数据
    }

    if (TCFG_LINEIN_LR_CH & (BIT(0) | BIT(1))) {
        audio_linein0_open(TCFG_LINEIN_LR_CH, 1);
    } else if (TCFG_LINEIN_LR_CH & (BIT(2) | BIT(3))) {
        audio_linein1_open(TCFG_LINEIN_LR_CH, 1);
    } else if (TCFG_LINEIN_LR_CH & (BIT(4) | BIT(5))) {
        audio_linein2_open(TCFG_LINEIN_LR_CH, 1);
    }
    audio_linein_gain(1);   // high gain
    if (app_audio_get_volume(APP_AUDIO_STATE_MUSIC)) {
        audio_linein_mute(0);
        app_audio_set_volume(APP_AUDIO_STATE_MUSIC, app_audio_get_volume(APP_AUDIO_STATE_MUSIC), 1);//防止无法调整
    }
    //模拟输出时候，dac为0也有数据
}

/*----------------------------------------------------------------------------*/
/**@brief    linein 使用dac IO输入方式
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static inline void __linein_way_dac_analog_start()
{

    if ((TCFG_LINEIN_LR_CH == AUDIO_LIN_DACL_CH) \
        || (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACR_CH)) {
        audio_linein_via_dac_open(TCFG_LINEIN_LR_CH, 1);
    } else {
        ASSERT(0, "linein ch err\n");
    }
    linein_volume_set(app_audio_get_volume(APP_AUDIO_STATE_MUSIC));

}
/*----------------------------------------------------------------------------*/
/**@brief    linein 使用dac IO输入方式
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static inline void user_linein_way_dac_analog_start()
{

    if ((TCFG_LINEIN_LR_CH == AUDIO_LIN_DACL_CH) \
        || (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACR_CH)) {
        audio_linein_via_dac_open(TCFG_LINEIN_LR_CH, 1);
    } else {
        ASSERT(0, "linein ch err\n");
    }

    printf(">>>> %d music vol\n",__this->volume);
    linein_volume_set(__this->volume);

}
/*----------------------------------------------------------------------------*/
/**@brief    linein 使用采集adc输入方式
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
static inline void __linein_way_adc_start()
{

#if (TCFG_LINEIN_MULTIPLEX_WITH_FM && (defined(CONFIG_CPU_BR25)))
    linein_dec_open(AUDIO_LIN1R_CH, 44100);		//696X 系列FM 与 LINEIN复用脚，绑定选择AUDIO_LIN1R_CH
#elif ((TCFG_LINEIN_LR_CH & AUDIO_LIN1R_CH ) && (defined(CONFIG_CPU_BR25)))		//FM 与 LINEIN 复用未使能，不可选择AUDIO_LIN1R_CH
    log_e("FM is not multiplexed with linein. channel selection err\n");
    ASSERT(0, "err\n");
#else
    linein_dec_open(TCFG_LINEIN_LR_CH, 44100);
#endif

}

///*----------------------------------------------------------------------------*/
/**@brief    linein 设置硬件
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
int linein_start(void)
{
    if (__this->onoff == 1) {
        log_info("linein is aleady start\n");
        return true;
    }

#if (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ADC)
    __linein_way_adc_start();
#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ANALOG)
    __linein_way_analog_start();
    audio_dac_vol_mute_lock(1);
#ifndef CONFIG_MEDIA_NEW_ENABLE
#if AUDIO_OUTPUT_AUTOMUTE
    extern void mix_out_automute_skip(u8 skip);
    mix_out_automute_skip(1);
#endif  // #if AUDIO_OUTPUT_AUTOMUTE
#endif

#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_DAC)
    __linein_way_dac_analog_start();
    audio_dac_vol_mute_lock(1);
#ifndef CONFIG_MEDIA_NEW_ENABLE
#if AUDIO_OUTPUT_AUTOMUTE
    extern void mix_out_automute_skip(u8 skip);
    mix_out_automute_skip(1);
#endif  // #if AUDIO_OUTPUT_AUTOMUTE
#endif

#endif
    __this->volume = app_audio_get_volume(APP_AUDIO_STATE_MUSIC);
    __this->onoff = 1;
    UI_REFLASH_WINDOW(false);//刷新主页并且支持打断显示
    return true;
}




/*----------------------------------------------------------------------------*/
/**@brief    linein stop
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/
void linein_stop(void)
{

    if (__this->onoff == 0) {
        log_info("linein is aleady stop\n");
        return;
    }
#if (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ADC)
    linein_dec_close();
#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_ANALOG)

    if (TCFG_LINEIN_LR_CH & (BIT(0) | BIT(1))) {
        audio_linein0_close(TCFG_LINEIN_LR_CH, 0);
    } else if (TCFG_LINEIN_LR_CH & (BIT(2) | BIT(3))) {
        audio_linein1_close(TCFG_LINEIN_LR_CH, 0);
    } else if (TCFG_LINEIN_LR_CH & (BIT(4) | BIT(5))) {
        audio_linein2_close(TCFG_LINEIN_LR_CH, 0);
    }
    audio_dac_vol_mute_lock(0);
#ifndef CONFIG_MEDIA_NEW_ENABLE
#if AUDIO_OUTPUT_AUTOMUTE
    extern void mix_out_automute_skip(u8 skip);
    mix_out_automute_skip(0);
#endif  // #if AUDIO_OUTPUT_AUTOMUTE
#endif

#elif (TCFG_LINEIN_INPUT_WAY == LINEIN_INPUT_WAY_DAC)
    audio_linein_via_dac_close(TCFG_LINEIN_LR_CH, 0);
    audio_dac_vol_mute_lock(0);
#ifndef CONFIG_MEDIA_NEW_ENABLE
#if AUDIO_OUTPUT_AUTOMUTE
    extern void mix_out_automute_skip(u8 skip);
    mix_out_automute_skip(0);
#endif // #if AUDIO_OUTPUT_AUTOMUTE
#endif

#endif
    app_audio_set_volume(APP_AUDIO_STATE_MUSIC, __this->volume, 1);
    __this->onoff = 0;
}

/*----------------------------------------------------------------------------*/
/**@brief    linein start stop 播放暂停切换
   @param    无
   @return   无
   @note
*/
/*----------------------------------------------------------------------------*/

int linein_volume_pp(void)
{
    if (__this->onoff) {
        linein_stop();
    } else {
        linein_start();
    }
    log_info("pp:%d \n", __this->onoff);
    UI_REFLASH_WINDOW(true);
    return  __this->onoff;
}



void user_linein_dec_star(void){
    printf(">>>>>>>>>>> linein onff flag 00 %d\n",__this->onoff);
    if (!__this->onoff) {
        linein_stop();
    } else {
        linein_start();
    }
    printf(">>>>>>>>>>> linein onff flag 11 %d\n",__this->onoff);
}



/**@brief   获取linein 播放状态
   @param    无
   @return   1:当前正在打开 0：当前正在关闭
   @note
*/
/*----------------------------------------------------------------------------*/

u8 linein_get_status(void)
{
    return __this->onoff;
}

void linein_vol_set(void){
    #if USER_SDK_BUG_2
    if (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACL_CH) {
        app_audio_output_ch_analog_gain_set(BIT(0), 0);
        app_audio_output_ch_analog_gain_set(BIT(1), __this->volume);
    } else if (TCFG_LINEIN_LR_CH == AUDIO_LIN_DACR_CH) {
        app_audio_output_ch_analog_gain_set(BIT(0), __this->volume);
        app_audio_output_ch_analog_gain_set(BIT(1), 0);
    }
    #endif
}

u16 user_linein_umute_id = 0;
void user_linein_umute(void){
    //播完提示音之后 声音减小 重新设置一下系统音量
    // linein_volume_set(__this->volume);
    
    user_linein_umute_id = 0;

    if(__this->onoff && __this->volume && !tone_get_status()){
        #if (defined(USER_SDK_BUG_3) && USER_SDK_BUG_3)
        if (__this->onoff) { 
            user_linein_way_dac_analog_start();
        }        
        #elif (defined(USER_SDK_BUG_4) && USER_SDK_BUG_4)
        audio_linein_mute(0);
        #endif

        user_linein_umute_id = 0;
    }else{
        user_linein_umute_id = sys_timeout_add(NULL, user_linein_umute, 600);
    }
}

static void  linein_api_tone_play_end_callback(void *priv, int flag)
{
    u32 index = (u32)priv;

    if (APP_LINEIN_TASK != app_get_curr_task()) {
        log_error("tone callback task out \n");
        return;
    }

    switch (index) {
    case IDEX_TONE_LOW_POWER:
    linein_start();
    app_audio_state_switch(APP_AUDIO_STATE_MUSIC, get_max_sys_vol());
    case IDEX_TONE_MAX_VOL:
        ///提示音播放结束
        puts(">>>>>>> linein tone play end linein statr\n");
        if(__this->onoff && __this->volume){
            if(user_linein_umute_id){
                sys_timeout_del(user_linein_umute_id);
            }
            user_linein_umute_id = sys_timeout_add(NULL, user_linein_umute, 600);
        }
        break;
    default:
        break;
    }
}
/*
   @note    在linein 情况下针对了某些情景进行了处理，设置音量需要使用独立接口
*/

void linein_key_vol_up()
{
    u8 vol;
    if(tone_get_status()){
        return;
    }  
    if (__this->volume < get_max_sys_vol()) {
        __this->volume ++;
        linein_volume_set(__this->volume);
    } else {
        linein_volume_set(__this->volume);
        if (tone_get_status() == 0) {
            /* tone_play(TONE_MAX_VOL); */
#if TCFG_MAX_VOL_PROMPT
    #if (defined(USER_SDK_BUG_3) && USER_SDK_BUG_3)
        tone_play_with_callback_by_name(tone_table[IDEX_TONE_MAX_VOL],USER_TONE_PLAY_MODE?1:0,linein_api_tone_play_end_callback,(void *)IDEX_TONE_MAX_VOL);
    #elif (defined(USER_SDK_BUG_4) && USER_SDK_BUG_4)
        audio_linein_mute(1);
        tone_play_with_callback_by_name(tone_table[IDEX_TONE_MAX_VOL],USER_TONE_PLAY_MODE?1:0,linein_api_tone_play_end_callback,(void *)IDEX_TONE_MAX_VOL);
    #else
        tone_play_by_path(tone_table[IDEX_TONE_MAX_VOL], USER_TONE_PLAY_MODE?1:0);
    #endif
#endif
        }
    }
    vol = __this->volume;
    UI_SHOW_MENU(MENU_MAIN_VOL, 1000, vol, NULL);
    log_info("vol+:%d\n", __this->volume);
}

//linein mode set vol
bool user_linein_set_vol(u8 vol){
    u8 tp_vol= vol;
    if(APP_LINEIN_TASK != app_get_curr_task()){
        return false;
    }

    if(get_max_sys_vol()<tp_vol){
        tp_vol = get_max_sys_vol();
    }

    if(__this){
        linein_volume_set(tp_vol);        
    }
    
    return true;
}

bool user_lienin_play_low_power_tone(void){
    if(APP_LINEIN_TASK != app_get_curr_task()){
        return false;
    }
    
    linein_stop();
    extern void  linein_api_tone_play_end_callback(void *priv, int flag);
    tone_play_with_callback_by_name(tone_table[IDEX_TONE_MAX_VOL],USER_TONE_PLAY_MODE?1:0,linein_api_tone_play_end_callback,(void *)IDEX_TONE_LOW_POWER);

    return true;
}
/*
   @note    在linein 情况下针对了某些情景进行了处理，设置音量需要使用独立接口
*/
void linein_key_vol_down()
{
    u8 vol;
    if(tone_get_status()){
        return;
    }      
    if (__this->volume) {
        __this->volume --;
        linein_volume_set(__this->volume);
    }
    vol = __this->volume;
    UI_SHOW_MENU(MENU_MAIN_VOL, 1000, vol, NULL);
    log_info("vol-:%d\n", __this->volume);
}










#endif
