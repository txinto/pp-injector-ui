#ifndef EEZ_LVGL_UI_FONTS_H
#define EEZ_LVGL_UI_FONTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EXT_FONT_DESC_T
#define EXT_FONT_DESC_T
typedef struct _ext_font_desc_t {
    const char *name;
    const void *font_ptr;
} ext_font_desc_t;
#endif

extern ext_font_desc_t fonts[];

// Build-time fallbacks: if these sizes are not enabled in LVGL config,
// map them to Montserrat 14 so generated UI code still compiles.
#if !defined(LV_FONT_MONTSERRAT_20) || !LV_FONT_MONTSERRAT_20
#define lv_font_montserrat_20 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_8) || !LV_FONT_MONTSERRAT_8
#define lv_font_montserrat_8 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_10) || !LV_FONT_MONTSERRAT_10
#define lv_font_montserrat_10 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_12) || !LV_FONT_MONTSERRAT_12
#define lv_font_montserrat_12 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_16) || !LV_FONT_MONTSERRAT_16
#define lv_font_montserrat_16 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_18) || !LV_FONT_MONTSERRAT_18
#define lv_font_montserrat_18 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_22) || !LV_FONT_MONTSERRAT_22
#define lv_font_montserrat_22 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_24) || !LV_FONT_MONTSERRAT_24
#define lv_font_montserrat_24 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_26) || !LV_FONT_MONTSERRAT_26
#define lv_font_montserrat_26 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_28) || !LV_FONT_MONTSERRAT_28
#define lv_font_montserrat_28 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_30) || !LV_FONT_MONTSERRAT_30
#define lv_font_montserrat_30 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_32) || !LV_FONT_MONTSERRAT_32
#define lv_font_montserrat_32 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_34) || !LV_FONT_MONTSERRAT_34
#define lv_font_montserrat_34 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_36) || !LV_FONT_MONTSERRAT_36
#define lv_font_montserrat_36 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_38) || !LV_FONT_MONTSERRAT_38
#define lv_font_montserrat_38 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_40) || !LV_FONT_MONTSERRAT_40
#define lv_font_montserrat_40 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_42) || !LV_FONT_MONTSERRAT_42
#define lv_font_montserrat_42 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_44) || !LV_FONT_MONTSERRAT_44
#define lv_font_montserrat_44 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_46) || !LV_FONT_MONTSERRAT_46
#define lv_font_montserrat_46 lv_font_montserrat_14
#endif

#if !defined(LV_FONT_MONTSERRAT_48) || !LV_FONT_MONTSERRAT_48
#define lv_font_montserrat_48 lv_font_montserrat_14
#endif

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_FONTS_H*/
