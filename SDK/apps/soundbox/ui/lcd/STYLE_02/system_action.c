#include "ui/ui.h"
#include "app_config.h"
#include "ui/ui_style.h"
#include "app_task.h"
#include "system/timer.h"
#include "font/language_list.h"
#include "res/resfile.h"
#include "app_power_manage.h"
#include "asm/charge.h"
#include "../ui_sys_param_api.h"

#if TCFG_SPI_LCD_ENABLE

#define STYLE_NAME  JL
REGISTER_UI_STYLE(STYLE_NAME)

extern int ui_hide_main(int id);
extern int ui_show_main(int id);
extern void key_ui_takeover(u8 on);

/************************************************************
                         电池控件事件
 ************************************************************/

static void battery_timer(void *priv)
{
    int  incharge = 0;//充电标志
    int id = (int)(priv);
    static u8 percent = 0;
    static u8 percent_last = 0;
    if (get_charge_online_flag()) { //充电时候图标动态效果
        if (percent > get_vbat_percent()) {
            percent = 0;
        }
        ui_battery_set_level_by_id(id, percent, incharge); //充电标志,ui可以显示不一样的图标
        percent += 20;
    } else {

        percent = get_vbat_percent();
        if (percent != percent_last) {
            ui_battery_set_level_by_id(id, percent, incharge); //充电标志,ui可以显示不一样的图标,需要工具设置
            percent_last = percent;
        }

    }
}

static int battery_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct ui_battery *battery = (struct ui_battery *)ctr;
    static u32 timer = 0;
    int  incharge = 0;//充电标志

    switch (e) {
    case ON_CHANGE_INIT:
        ui_battery_set_level(battery, get_vbat_percent(), incharge);
        if (!timer) {
            timer = sys_timer_add((void *)battery->elm.id, battery_timer, 1 * 1000); //传入的id就是BT_BAT
        }
        break;
    case ON_CHANGE_FIRST_SHOW:
        break;
    case ON_CHANGE_RELEASE:
        if (timer) {
            sys_timer_del(timer);
            timer = 0;
        }
        break;
    default:
        return false;
    }
    return false;
}
REGISTER_UI_EVENT_HANDLER(SYSTEM_BAT)
.onchange = battery_onchange,
 .ontouch = NULL,
};





/************************************************************
                         系统设置
 ************************************************************/


static int sys_mode_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct window *window = (struct window *)ctr;
    static int bt_status_timer = 0;
    switch (e) {
    case ON_CHANGE_INIT:
        puts("\n***sys_mode_onchange***\n");
        key_ui_takeover(1);
        /* ui_register_msg_handler(ID_WINDOW_BT, ui_msg_handler); */
        /*
          * 注册APP消息响应
          */
        break;
    case ON_CHANGE_RELEASE:

        break;
    default:
        return false;
    }
    return false;
}


REGISTER_UI_EVENT_HANDLER(ID_WINDOW_SYS)
.onchange = sys_mode_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int common_layout_onchange(void *ctr, enum element_change_event e, void *arg)
{
    struct layout *layout = (struct layout *)ctr;
    switch (e) {
    case ON_CHANGE_INIT:
        layout_on_focus(layout);
        break;
    case ON_CHANGE_RELEASE:
        layout_lose_focus(layout);
        break;
    default:
        break;
    }
    return FALSE;
}


/************************************************************
                         系统设置入口
 ************************************************************/
static int system_enter_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        sel_item = ui_grid_cur_item(grid);

        switch (sel_item) {
        case 0:
            ui_show(SYSTEM_M_LAYOUT);
            break;
        case 1:
            ui_show(SYS_LANGUAGE);
            break;
        case 2:
            r_printf("UI sys_enter_soft_poweroff\n");
            ui_show(SYS_POWEROFF);
            break;
        case 3:
            ui_show(SYS_BACKLIGHT);
            break;
        case 4:
            ui_hide_main(ID_WINDOW_SYS);
            ui_show_main(ID_WINDOW_MAIN);
            break;
        }


        break;
    default:
        return false;
    }
    return TRUE;
}


REGISTER_UI_EVENT_HANDLER(SYSTEM_SET_LIST)
.onchange = NULL,
 .onkey = system_enter_callback,
  .ontouch = NULL,
};


/************************************************************
                         系统菜单
 ************************************************************/


REGISTER_UI_EVENT_HANDLER(SYSTEM_M_LAYOUT)
.onchange = common_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int system_menu_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        sel_item = ui_grid_cur_item(grid);

        switch (sel_item) {
        case 0:
            ui_show(SYS_MSG_LAYOUT);
            return TRUE;
            break;
        default:
            ui_hide(SYSTEM_M_LAYOUT);
            break;
        }

        break;
    default:
        return false;
    }
    return TRUE;
}


REGISTER_UI_EVENT_HANDLER(SYSTEM_M_LIST)
.onchange = NULL,
 .onkey = system_menu_callback,
  .ontouch = NULL,
};



/************************************************************
                        系统信息
************************************************************/
/*
 * 版本号
 */
#define VERSION_LOG "20200522"
#define SPACE_LOG   "1024 MB"

static int version_ascii_onchange(void *ctr, enum element_change_event e, void *arg)
{
    int index;
    struct ui_text *text = (struct ui_text *)ctr;
    struct ui_text_attrs *text_attrs = &(text->attrs);
    u8 vol;
    switch (e) {
    case ON_CHANGE_INIT:
        text_attrs->str = VERSION_LOG;
        break;
    case ON_CHANGE_RELEASE:
        break;
    default:
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(SYSTEM_31)
.onchange = version_ascii_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};

static int space_ascii_onchange(void *ctr, enum element_change_event e, void *arg)
{
    int index;
    struct ui_text *text = (struct ui_text *)ctr;
    struct ui_text_attrs *text_attrs = &(text->attrs);
    u8 vol;
    switch (e) {
    case ON_CHANGE_INIT:
        text_attrs->str = SPACE_LOG;
        break;
    case ON_CHANGE_RELEASE:
        break;
    default:
        break;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(SYSTEM_35)
.onchange = space_ascii_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


static int  system_info_menu_enter_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        ui_hide(SYS_MSG_LAYOUT);
        return false;
    }
    return TRUE;
}

REGISTER_UI_EVENT_HANDLER(SYS_MSG_INFO_LIST)
.onchange = NULL,
 .onkey = system_info_menu_enter_callback,
  .ontouch = NULL,
};



/************************************************************
                         语言设置
 ************************************************************/

/*
 * system语言设置
 */
static const u8 table_system_language[] = {
    Chinese_Simplified,  /* 简体中文 */
    Chinese_Traditional, /* 繁体中文 */
    English,             /* 英文 */
};


static int  language_enter_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        sel_item = ui_grid_cur_item(grid);
        if (sel_item >= 0 && sel_item < sizeof(table_system_language) / sizeof(table_system_language[0])) {
            ui_language_set(table_system_language[sel_item]);
        } else {
            ui_hide(SYS_LANGUAGE);
        }

        return false;
    default:
        return false;
    }
    return TRUE;
}
REGISTER_UI_EVENT_HANDLER(SYS_LANGUAGE_LIST)
.onchange = NULL,
 .onkey = language_enter_callback,
  .ontouch = NULL,
};



REGISTER_UI_EVENT_HANDLER(SYS_LANGUAGE)
.onchange = common_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};




/************************************************************
                         背光设置
 ************************************************************/

static int  backlight_menu_enter_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        sel_item = ui_grid_cur_item(grid);
        switch (sel_item) {
        case 0:
            ui_show(SYS_BACKLIGHT_TIME);
            break;
        case 1:
            ui_show(SYS_BACKLIGHT_VALUE);
            break;
        case 2:
            ui_hide(SYS_BACKLIGHT);
            break;

        }
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(SYS_BACKLIGHT_LIST)
.onchange = NULL,
 .onkey = backlight_menu_enter_callback,
  .ontouch = NULL,
};


REGISTER_UI_EVENT_HANDLER(SYS_BACKLIGHT)
.onchange = common_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


/*
 * system屏幕保护设置
 */
static int  backlight_menu_time_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        ui_hide(SYS_BACKLIGHT_TIME);
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(SYS_BACKLIGHT_TIME_LIST)
.onchange = NULL,
 .onkey = backlight_menu_time_callback,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(SYS_BACKLIGHT_TIME)
.onchange = common_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};




/*
 * system屏幕亮度设置
 */
static int  backlight_menu_value_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        ui_hide(SYS_BACKLIGHT_VALUE);
        return false;
    }
    return false;
}

REGISTER_UI_EVENT_HANDLER(SYS_BACKLIGHT_VALUE_LIST)
.onchange = NULL,
 .onkey = backlight_menu_value_callback,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(SYS_BACKLIGHT_VALUE)
.onchange = common_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};




/************************************************************
                         系统关机设置
 ************************************************************/

/*
 * system自动关机设置
 */
static int power_menu_enter_callback(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        sel_item = ui_grid_cur_item(grid);
        switch (sel_item) {
        case 0:
            auto_power_off_timer_close();
            ui_hide(SYS_POWEROFF);
            break;
        case 1:
        case 2:
        case 3:
        case 4:
            set_auto_poweroff_timer(sel_item);
        case 5:
            ui_hide(SYS_POWEROFF);
            break;

        }
        return false;
    default:
        return false;
    }
    return TRUE;
}

REGISTER_UI_EVENT_HANDLER(SYS_POWEROFF_LIST)
.onchange = NULL,
 .onkey = power_menu_enter_callback,
  .ontouch = NULL,
};

REGISTER_UI_EVENT_HANDLER(SYS_POWEROFF)
.onchange = common_layout_onchange,
 .onkey = NULL,
  .ontouch = NULL,
};


#if 0
struct grid_setting {
    int id;
    int parent_id;
    void (*handler)(void *hd, int args, int index);
};


static const  struct grid_setting grid_function_table[] = {
    { SYSTEM_MENU_LIST, 0, system_enter_callback},
    { LANGUAGE_MENU_LIST, 0, language_enter_callback},
    { POWER_MENU_LIST, 0, power_menu_enter_callback},
    { BACK_MENU_LIST, NULL},
    { 0, NULL}, //必须要要0结束
};

static const  struct grid_setting grid_function_table_1[] = {
    { SYS_BACKLIGHT_0, SYS_BACKLIGHT, backlight_menu_enter_callback},
    { SYS_BACKLIGHT_1, SYS_BACKLIGHT, backlight_menu_enter_callback},
    { SYS_BACKLIGHT_2, SYS_BACKLIGHT, backlight_menu_enter_callback},
    { 0, NULL}, //必须要要0结束
};



static void ui_list_callback_common(const struct grid_setting *table, int id, int key, int index)
{
    if (!table) {
        return;
    }
    for (; table->id; table++) {
        if (table->id == id) {
            if (table->handler) {
                table->handler(table, key, index);
            }
            break;
        }
    }
}

static int ui_list_onkey_common(void *ctr, struct element_key_event *e)
{
    struct ui_grid *grid = (struct ui_grid *)ctr;
    printf("ui key %s %d\n", __FUNCTION__, e->value);
    int sel_item;
    switch (e->value) {
    case KEY_OK:
        sel_item = ui_grid_cur_item(grid);
        if (sel_item >= 0) {
            ui_list_callback_common(grid_function_table, grid->item[sel_item].elm.id, e->value, sel_item);
        }
        break;
    default:
        return false;
    }
    return TRUE;
}

#endif
#endif //TCFG_SPI_LCD_ENABLE

