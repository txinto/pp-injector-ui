/*
 * LVGL 9 port layer for TouchScreen component.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "lvgl_port.h"

static const char *TAG = "lv_port";

static SemaphoreHandle_t s_lvgl_mux;
static TaskHandle_t s_lvgl_task_handle;
static esp_lcd_panel_handle_t s_lcd_handle;
static esp_lcd_touch_handle_t s_touch_handle;
static lv_display_t *s_display;
static lv_indev_t *s_touch_indev;
static esp_timer_handle_t s_tick_timer;
static lv_color_t *s_draw_buf_1;
static lv_color_t *s_draw_buf_2;
static uint8_t *s_rot_buf;
static size_t s_rot_buf_bytes;
static bool s_inited;

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static inline uint32_t clamp_delay_ms(uint32_t delay_ms)
{
    if (delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
        return LVGL_PORT_TASK_MIN_DELAY_MS;
    }
    if (delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
        return LVGL_PORT_TASK_MAX_DELAY_MS;
    }
    return delay_ms;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    if (!s_lcd_handle) {
        lv_display_flush_ready(disp);
        return;
    }

    const lv_area_t *draw_area = area;
    uint8_t *draw_px = px_map;
    lv_area_t rotated_area;
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    if (rotation != LV_DISPLAY_ROTATION_0) {
        lv_color_format_t cf = lv_display_get_color_format(disp);
        rotated_area = *area;
        lv_display_rotate_area(disp, &rotated_area);

        uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
        uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);
        int32_t src_w = lv_area_get_width(area);
        int32_t src_h = lv_area_get_height(area);

        size_t needed = (size_t)dest_stride * (size_t)lv_area_get_height(&rotated_area);
        if (s_rot_buf && needed <= s_rot_buf_bytes) {
            lv_draw_sw_rotate(px_map, s_rot_buf, src_w, src_h, src_stride, dest_stride, rotation, cf);
            draw_area = &rotated_area;
            draw_px = s_rot_buf;
        } else {
            ESP_LOGW(TAG, "rotation buffer too small (%u > %u), drawing unrotated",
                     (unsigned)needed, (unsigned)s_rot_buf_bytes);
        }
    }

    const int x1 = draw_area->x1;
    const int y1 = draw_area->y1;
    const int x2 = draw_area->x2 + 1;
    const int y2 = draw_area->y2 + 1;
    esp_lcd_panel_draw_bitmap(s_lcd_handle, x1, y1, x2, y2, draw_px);
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static lv_coord_t last_x;
    static lv_coord_t last_y;

    data->state = (lv_indev_state_t)0;
    data->point.x = last_x;
    data->point.y = last_y;
    data->continue_reading = false;

    if (!s_touch_handle) {
        return;
    }

    esp_lcd_touch_read_data(s_touch_handle);

    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint16_t strength[1] = {0};
    uint8_t count = 0;

    bool pressed = esp_lcd_touch_get_coordinates(s_touch_handle, x, y, strength, &count, 1);
    if (pressed && count > 0) {
        last_x = (lv_coord_t)x[0];
        last_y = (lv_coord_t)y[0];
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = (lv_indev_state_t)1;
    }
}

static esp_err_t setup_display_buffers(void)
{
    const uint32_t line_count = LVGL_PORT_BUFFER_HEIGHT;
    const size_t buf_pixels = (size_t)LVGL_PORT_H_RES * line_count;
    const size_t buf_bytes = buf_pixels * sizeof(lv_color_t);

    s_draw_buf_1 = heap_caps_malloc(buf_bytes, LVGL_PORT_BUFFER_MALLOC_CAPS);
    if (!s_draw_buf_1) {
        ESP_LOGE(TAG, "draw buf1 alloc failed (%u bytes)", (unsigned)buf_bytes);
        return ESP_ERR_NO_MEM;
    }

    s_draw_buf_2 = heap_caps_malloc(buf_bytes, LVGL_PORT_BUFFER_MALLOC_CAPS);
    if (!s_draw_buf_2) {
        ESP_LOGE(TAG, "draw buf2 alloc failed (%u bytes)", (unsigned)buf_bytes);
        heap_caps_free(s_draw_buf_1);
        s_draw_buf_1 = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_rot_buf = heap_caps_malloc(buf_bytes, LVGL_PORT_BUFFER_MALLOC_CAPS);
    if (!s_rot_buf) {
        ESP_LOGE(TAG, "rotation buf alloc failed (%u bytes)", (unsigned)buf_bytes);
        heap_caps_free(s_draw_buf_2);
        heap_caps_free(s_draw_buf_1);
        s_draw_buf_2 = NULL;
        s_draw_buf_1 = NULL;
        return ESP_ERR_NO_MEM;
    }
    s_rot_buf_bytes = buf_bytes;

    lv_display_set_buffers(s_display, s_draw_buf_1, s_draw_buf_2, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return ESP_OK;
}

static void lvgl_task(void *arg)
{
    (void)arg;
    while (true) {
        uint32_t delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        if (lvgl_port_lock(-1)) {
            delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }
        delay_ms = clamp_delay_ms(delay_ms);

        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(delay_ms));
    }
}

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle)
{
    if (!lcd_handle) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_inited) {
        return ESP_OK;
    }

    s_lcd_handle = lcd_handle;
    s_touch_handle = tp_handle;

    lv_init();
    s_display = lv_display_create(LVGL_PORT_H_RES, LVGL_PORT_V_RES);
    if (!s_display) {
        return ESP_FAIL;
    }
    lv_display_set_flush_cb(s_display, flush_cb);

    esp_err_t err = setup_display_buffers();
    if (err != ESP_OK) {
        return err;
    }

#if LVGL_PORT_ROTATION_DEGREE == 90
    lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_90);
#elif LVGL_PORT_ROTATION_DEGREE == 180
    lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_180);
#elif LVGL_PORT_ROTATION_DEGREE == 270
    lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_270);
#else
    lv_display_set_rotation(s_display, LV_DISPLAY_ROTATION_0);
#endif

    if (s_touch_handle) {
        s_touch_indev = lv_indev_create();
        if (!s_touch_indev) {
            return ESP_FAIL;
        }
        lv_indev_set_type(s_touch_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_touch_indev, touch_read_cb);
    }

    s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    if (!s_lvgl_mux) {
        return ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t tick_timer_args = {
        .callback = lvgl_tick_cb,
        .name = "lvgl_tick",
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &s_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000));

    BaseType_t core_id = LVGL_PORT_TASK_CORE < 0 ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ok = xTaskCreatePinnedToCore(
        lvgl_task,
        "lvgl",
        LVGL_PORT_TASK_STACK_SIZE,
        NULL,
        LVGL_PORT_TASK_PRIORITY,
        &s_lvgl_task_handle,
        core_id
    );
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "cannot create LVGL task");
        return ESP_FAIL;
    }

    s_inited = true;
    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms)
{
    assert(s_lvgl_mux && "lvgl_port_init must be called first");
    TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    assert(s_lvgl_mux && "lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(s_lvgl_mux);
}

bool lvgl_port_notify_rgb_vsync(void)
{
    if (!s_lvgl_task_handle) {
        return false;
    }
    BaseType_t need_yield = pdFALSE;
    vTaskNotifyGiveFromISR(s_lvgl_task_handle, &need_yield);
    return need_yield == pdTRUE;
}
