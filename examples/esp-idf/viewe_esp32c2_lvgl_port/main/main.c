/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"

static const char *TAG = "main";


//***************旋钮和按键********** */
#include "iot_knob.h"
#include "iot_button.h"


#define BSP_ENCODER_A         (GPIO_NUM_6)
#define BSP_ENCODER_B         (GPIO_NUM_5)
//***************************** */
void knob_init(uint32_t encoder_a, uint32_t encoder_b);
void button_init(uint32_t button_num);

static button_handle_t *g_btn_handle = NULL;

static void btn_select_sw_cb(void *handle, void *arg)
{
    ESP_LOGI("BTN", "select switch");
    // vTaskDelay(pdMS_TO_TICKS(2*1000));
    esp_restart();
}

void printf_stack()
{
    static char buffer[128];
    sprintf(buffer, "   Biggest /     Free /    Min/    Total\n"
            "\t  SRAM : [%8d / %8d / %8d / %8d]",
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
            heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL),
            heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI("MEM", "%s", buffer);
}

esp_err_t app_btn_init(void)
{
    ESP_ERROR_CHECK((NULL != g_btn_handle));

    int btn_num = 0;
    g_btn_handle = calloc(sizeof(button_handle_t), (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM));
    assert((g_btn_handle) && "memory is insufficient for button");
    return bsp_iot_button_create(g_btn_handle, &btn_num, (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM));
}

esp_err_t app_btn_register_callback(bsp_button_t btn, button_event_t event, button_cb_t callback, void *user_data)
{
    assert((g_btn_handle) && "button not initialized");
    assert((btn < (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM)) && "button id incorrect");

    if (NULL == callback) {
        return iot_button_unregister_cb(g_btn_handle[btn], event);
    }
    return iot_button_register_cb(g_btn_handle[btn], event, callback, user_data);
}

esp_err_t app_btn_register_event_callback(bsp_button_t btn, button_event_config_t cfg, button_cb_t callback, void *user_data)
{
    assert((g_btn_handle) && "button not initialized");
    esp_err_t err = iot_button_register_event_cb(g_btn_handle[btn], cfg, callback, (void *)(cfg.event));
    ESP_ERROR_CHECK(err);
    return ESP_OK;
}

esp_err_t app_btn_rm_all_callback(bsp_button_t btn)
{
    assert((g_btn_handle) && "button not initialized");
    assert((btn < (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM)) && "button id incorrect");

    for (size_t event = 0; event < BUTTON_EVENT_MAX; event++) {
        iot_button_unregister_cb(g_btn_handle[btn], event);
    }
    return ESP_OK;
}

esp_err_t app_btn_rm_event_callback(bsp_button_t btn, size_t event)
{
    assert((g_btn_handle) && "button not initialized");
    assert((btn < (BSP_BUTTON_NUM + BSP_ADC_BUTTON_NUM)) && "button id incorrect");

    iot_button_unregister_cb(g_btn_handle[btn], event);
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    printf_stack();

    knob_init(BSP_ENCODER_A, BSP_ENCODER_B);
    button_init(GPIO_NUM_9);
    //罪魁祸首找到了，旋钮的 B口和 128的CS冲突了，原本都是5
    lv_disp_t* disp_096 = NULL;
    lv_disp_t* disp_128 = NULL;
    bsp_display_start(&disp_096, &disp_128);
   
     ui_init();
     //bsp_display_backlight_on();
    //ui_start(disp_096, disp_128);
}

//***************The following code is for standardizing the event triggering of rotary encoders and buttons.**************** */
//**************旋钮********* */
extern void   LVGL_knob_event(void *event);

extern void   LVGL_button_event(void *event);

static knob_handle_t knob = NULL;

const char *knob_event_table[] = {
    "KNOB_LEFT",
    "KNOB_RIGHT",
    "KNOB_H_LIM",
    "KNOB_L_LIM",
    "KNOB_ZERO",
};

static void knob_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "knob event %s, %d", knob_event_table[(knob_event_t)data], iot_knob_get_count_value(knob));
    LVGL_knob_event(data);
   
}

void knob_init(uint32_t encoder_a, uint32_t encoder_b)
{
    knob_config_t cfg = {
        .default_direction = 0,
        .gpio_encoder_a = encoder_a,
        .gpio_encoder_b = encoder_b,
#if CONFIG_PM_ENABLE
        .enable_power_save = true,
#endif
    };

    knob = iot_knob_create(&cfg);
    assert(knob);
    esp_err_t err = iot_knob_register_cb(knob, KNOB_LEFT, knob_event_cb, (void *)KNOB_LEFT);
    err |= iot_knob_register_cb(knob, KNOB_RIGHT, knob_event_cb, (void *)KNOB_RIGHT);
    err |= iot_knob_register_cb(knob, KNOB_H_LIM, knob_event_cb, (void *)KNOB_H_LIM);
    err |= iot_knob_register_cb(knob, KNOB_L_LIM, knob_event_cb, (void *)KNOB_L_LIM);
    err |= iot_knob_register_cb(knob, KNOB_ZERO, knob_event_cb, (void *)KNOB_ZERO);
    ESP_ERROR_CHECK(err);
}

//******************** */

//*********按键**** */
#if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C2 || CONFIG_IDF_TARGET_ESP32H2 || CONFIG_IDF_TARGET_ESP32C6
#define BOOT_BUTTON_NUM         9
#else
#define BOOT_BUTTON_NUM         0
#endif
#define BUTTON_ACTIVE_LEVEL     0
const char *button_event_table[] = {
    "BUTTON_PRESS_DOWN",
    "BUTTON_PRESS_UP",
    "BUTTON_PRESS_REPEAT",
    "BUTTON_PRESS_REPEAT_DONE",
    "BUTTON_SINGLE_CLICK",
    "BUTTON_DOUBLE_CLICK",
    "BUTTON_MULTIPLE_CLICK",
    "BUTTON_LONG_PRESS_START",
    "BUTTON_LONG_PRESS_HOLD",
    "BUTTON_LONG_PRESS_UP",
    "BUTTON_PRESS_END",
};

static void button_event_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "Button event %s", button_event_table[(button_event_t)data]);
    LVGL_button_event(data);
  
  
}
void button_init(uint32_t button_num)
{
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
#if CONFIG_GPIO_BUTTON_SUPPORT_POWER_SAVE
            .enable_power_save = true,
#endif
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    esp_err_t err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb, (void *)BUTTON_PRESS_DOWN);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb, (void *)BUTTON_PRESS_UP);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb, (void *)BUTTON_PRESS_REPEAT);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb, (void *)BUTTON_PRESS_REPEAT_DONE);
    err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, (void *)BUTTON_SINGLE_CLICK);
    err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb, (void *)BUTTON_DOUBLE_CLICK);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, (void *)BUTTON_LONG_PRESS_START);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, (void *)BUTTON_LONG_PRESS_HOLD);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb, (void *)BUTTON_LONG_PRESS_UP);
    err |= iot_button_register_cb(btn, BUTTON_PRESS_END, button_event_cb, (void *)BUTTON_PRESS_END);

#if CONFIG_ENTER_LIGHT_SLEEP_MODE_MANUALLY
    /*!< For enter Power Save */
    button_power_save_config_t config = {
        .enter_power_save_cb = button_enter_power_save,
    };
    err |= iot_button_register_power_save_cb(&config);
#endif

    ESP_ERROR_CHECK(err);
}



//***************** */
