#include "PPInjectorUI_prd_ui.h"
#include "PPInjectorUI_display_comms.h"
#include "PPInjectorUI.h"
#include "ui/eez-flow.h"
#include "ui/fonts.h"
#include "ui/screens.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_spiffs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cerrno>
#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
static const char *TAG = "PPInjectorUI_PRD";

class SerialShim {
public:
  void println(const char *msg) const {
    ESP_LOGD(TAG, "%s", msg ? msg : "");
  }

  void printf(const char *fmt, ...) const {
    if (!fmt) {
      return;
    }
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    ESP_LOGD(TAG, "%s", buf);
  }
};

static const SerialShim Serial;

static inline uint32_t millis(void) {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

static inline void delay(uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

namespace Storage {
static bool s_inited = false;
static constexpr const char *kSpiffsBasePath = "/spiffs";
static constexpr const char *kSpiffsPartition = "spiffs_storage";
static constexpr const char *kMouldsPath = "/spiffs/moulds.bin";

void init() {
  if (s_inited) return;

  const esp_vfs_spiffs_conf_t conf = {
      .base_path = kSpiffsBasePath,
      .partition_label = kSpiffsPartition,
      .max_files = 8,
      .format_if_mount_failed = true,
  };

  esp_err_t err = esp_vfs_spiffs_register(&conf);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
    return;
  }

  size_t total = 0, used = 0;
  err = esp_spiffs_info(kSpiffsPartition, &total, &used);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "SPIFFS mounted (%u/%u bytes used)",
             (unsigned)used, (unsigned)total);
  } else {
    ESP_LOGW(TAG, "SPIFFS info failed: %s", esp_err_to_name(err));
  }

  s_inited = true;
}

void loadMoulds(DisplayComms::MouldParams *moulds, int &count, int maxCount) {
  count = 0;
  if (!s_inited || !moulds || maxCount <= 0) return;

  FILE *f = fopen(kMouldsPath, "rb");
  if (!f) {
    ESP_LOGI(TAG, "Storage: %s not found, starting empty", kMouldsPath);
    return;
  }

  uint32_t storedCount = 0;
  if (fread(&storedCount, sizeof(storedCount), 1, f) != 1) {
    ESP_LOGW(TAG, "Storage: invalid header in %s", kMouldsPath);
    fclose(f);
    return;
  }

  if (storedCount > (uint32_t)maxCount) {
    ESP_LOGW(TAG, "Storage: truncating mould count %u -> %d",
             (unsigned)storedCount, maxCount);
    storedCount = (uint32_t)maxCount;
  }

  size_t readCount = fread(
      moulds, sizeof(DisplayComms::MouldParams), (size_t)storedCount, f);
  fclose(f);

  count = (int)readCount;
  for (int i = 0; i < count; ++i) {
    moulds[i].name[sizeof(moulds[i].name) - 1] = '\0';
    moulds[i].mode[sizeof(moulds[i].mode) - 1] = '\0';
  }
  ESP_LOGI(TAG, "Storage: loaded %d mould profiles", count);
  for (int i = 0; i < count; ++i) {
    const DisplayComms::MouldParams &p = moulds[i];
    ESP_LOGI(TAG,
             "Storage load [%d] name=%s mode=%s fillVol=%.2f fillSpd=%.2f packVol=%.2f torque=%.2f",
             i, p.name, p.mode, p.fillVolume, p.fillSpeed, p.packVolume,
             p.injectTorque);
  }
}

void saveMoulds(const DisplayComms::MouldParams *moulds, int count) {
  if (!s_inited || !moulds || count < 0) return;

  FILE *f = fopen(kMouldsPath, "wb");
  if (!f) {
    ESP_LOGE(TAG, "Storage: cannot open %s for write (errno=%d: %s)",
             kMouldsPath, errno, strerror(errno));
    return;
  }

  uint32_t storedCount = (uint32_t)count;
  if (fwrite(&storedCount, sizeof(storedCount), 1, f) != 1) {
    ESP_LOGE(TAG, "Storage: failed writing header");
    fclose(f);
    return;
  }

  size_t written = fwrite(
      moulds, sizeof(DisplayComms::MouldParams), (size_t)count, f);
  fclose(f);

  if (written != (size_t)count) {
    ESP_LOGE(TAG, "Storage: short write (%u/%u)",
             (unsigned)written, (unsigned)count);
    return;
  }

  ESP_LOGI(TAG, "Storage: saved %d mould profiles", count);
}
} // namespace Storage

#ifndef SCREEN_DIAG_ONLY
#define SCREEN_DIAG_ONLY 0
#endif

#ifndef SCREEN_DIAG_ENABLE_PRD_MOULD_EDIT
#if SCREEN_DIAG_ONLY
#define SCREEN_DIAG_ENABLE_PRD_MOULD_EDIT 1
#else
#define SCREEN_DIAG_ENABLE_PRD_MOULD_EDIT 1
#endif
#endif

#ifndef SCREEN_DIAG_SKIP_LAST_PROFILE_ON_RENDER
#define SCREEN_DIAG_SKIP_LAST_PROFILE_ON_RENDER 0
#endif

#ifndef SCREEN_DIAG_FORCE_KEYBOARD_CENTER
#define SCREEN_DIAG_FORCE_KEYBOARD_CENTER 0
#endif

constexpr lv_coord_t LEFT_X = 8;
// ... (rest of the file constants)

// ...

//...
constexpr lv_coord_t LEFT_WIDTH = 114;
constexpr lv_coord_t RIGHT_X = 130;
constexpr lv_coord_t RIGHT_WIDTH = 350;
constexpr lv_coord_t SCREEN_HEIGHT = 800;
constexpr int MAX_MOULD_PROFILES = 16;
constexpr uint32_t DOUBLE_TAP_MS = 420;
constexpr uint32_t NETWORK_HOLD_GRAY_MS = 3000;
constexpr uint32_t NETWORK_HOLD_OTA_MS = 6000;
constexpr uint32_t NETWORK_HOLD_REPROVISION_MS = 10000;

// Shared geometry constants.
constexpr float TURNS_TO_TOP = 22.53f;
constexpr float TURNS_TO_BOTTOM = 360.5f;
constexpr float TURNS_MIN = 0.0f;
constexpr float TURNS_MAX = TURNS_TO_BOTTOM;
constexpr int BARREL_INNER_TOP_Y = 0;
constexpr int BARREL_INNER_HEIGHT = 793;
constexpr int TIP_HEIGHT = 80;
// Plunger calibration (keep aligned with the previously tuned behavior).
constexpr int PLUNGER_TIP_ANCHOR_Y = 711;
constexpr int PLUNGER_TIP_TRAVEL_PX = PLUNGER_TIP_ANCHOR_Y; // 711
constexpr float PLUNGER_PX_PER_TURN =
    static_cast<float>(PLUNGER_TIP_TRAVEL_PX) /
    (TURNS_TO_BOTTOM - TURNS_TO_TOP);
constexpr float REFILL_PX_PER_TURN = PLUNGER_PX_PER_TURN;
constexpr int REFILL_STACK_BOTTOM_Y = 770;

const char *COMMON_FIELD_NAMES[] = {
    "Trap Accel",          "Compress Torque",  "Micro Interval (ms)",
    "Micro Duration (ms)", "Purge Up",         "Purge Down",
    "Purge Current",       "Antidrip Vel",     "Antidrip Current",
    "Release Dist",        "Release Trap Vel", "Release Current",
    "Contactor Cycles",    "Contactor Limit",
};

const bool COMMON_FIELD_IS_INTEGER[] = {false, false, true,  true,  false,
                                        false, false, false, false, false,
                                        false, false, true,  true};

constexpr int COMMON_FIELD_COUNT =
    sizeof(COMMON_FIELD_NAMES) / sizeof(COMMON_FIELD_NAMES[0]);

// ... (Common fields)

const char *MOULD_FIELD_NAMES[] = {
    "Name",         "Fill Volume",  "Fill Speed",    "Fill Pressure",
    "Pack Volume",  "Pack Speed",   "Pack Pressure", "Pack Time",
    "Cooling Time", "Fill Accel",   "Fill Decel",    "Pack Accel",
    "Pack Decel",   "Mode (2D/3D)", "Inject Torque",
};

// 0=String, 1=Float, 2=Integer (none currently)
constexpr int MOULD_FIELD_TYPE[] = {0, 1, 1, 1, 1, 1, 1, 1,
                                    1, 1, 1, 1, 1, 0, 1};

constexpr int MOULD_FIELD_COUNT =
    sizeof(MOULD_FIELD_NAMES) / sizeof(MOULD_FIELD_NAMES[0]);

struct RefillBlock {
  float volume; // cm3
  uint32_t addedMs;
  bool active;

  RefillBlock() : volume(0), addedMs(0), active(false) {}
  RefillBlock(float v, uint32_t a, bool act)
      : volume(v), addedMs(a), active(act) {}
};

struct UiState {
  bool initialized = false;

  lv_obj_t *rightPanelMain = nullptr;
  lv_obj_t *rightPanelMould = nullptr;
  lv_obj_t *rightPanelMouldEdit = nullptr; // New
  lv_obj_t *rightPanelCommon = nullptr;

  lv_obj_t *posLabelMain = nullptr;
  lv_obj_t *tempLabelMain = nullptr;
  lv_obj_t *posLabelMould = nullptr;
  lv_obj_t *tempLabelMould = nullptr;
  lv_obj_t *posLabelCommon = nullptr;
  lv_obj_t *tempLabelCommon = nullptr;

  lv_obj_t *stateValue = nullptr;
  lv_obj_t *stateAction1 = nullptr;
  lv_obj_t *stateAction2 = nullptr;
  lv_obj_t *mainErrorLabel = nullptr;

  lv_obj_t *mouldList = nullptr;
  lv_obj_t *mouldNotice = nullptr;

  lv_obj_t *mouldButtonBack = nullptr;
  lv_obj_t *mouldButtonSend = nullptr;
  lv_obj_t *mouldButtonEdit = nullptr;
  lv_obj_t *mouldButtonNew = nullptr;
  lv_obj_t *mouldButtonDelete = nullptr;
  lv_obj_t *mouldProfileButtons[MAX_MOULD_PROFILES] = {};
  DisplayComms::MouldParams mouldProfiles[MAX_MOULD_PROFILES] = {};
  int mouldProfileCount = 0;
  int selectedMould = -1;
  int lastTappedMould = -1;
  uint32_t lastTapMs = 0;

  lv_obj_t *commonScroll = nullptr;
  lv_obj_t *commonNotice = nullptr;
  lv_obj_t *commonButtonBack = nullptr;
  lv_obj_t *commonButtonSend = nullptr;
  lv_obj_t *commonKeyboard = nullptr;
  lv_obj_t *commonInputs[COMMON_FIELD_COUNT] = {};
  lv_obj_t *commonDiscardOverlay = nullptr;
  bool commonDirty = false;
  bool suppressCommonEvents = false;

  // Mould Edit State
  lv_obj_t *mouldEditScroll = nullptr;
  lv_obj_t *mouldEditInputs[MOULD_FIELD_COUNT] = {};
  lv_obj_t *mouldEditKeyboard =
      nullptr; // Separate keyboard for safety/simplicity
  DisplayComms::MouldParams editMouldSnapshot = {};
  bool mouldEditDirty = false;

  char lastMouldName[32] = {0};

  lv_obj_t *networkGestureZone = nullptr;
  lv_obj_t *networkGestureIndicator = nullptr;
  bool networkGestureActive = false;
  lv_point_t networkGestureStartPoint = {0, 0};
  uint32_t networkGestureStartMs = 0;

  RefillBlock refillBlocks[16];
  int blockCount = 0;
  char lastState[24] = "";
  float startRefillPos = 0;
  float lastFramePos = 0;
  bool isRefilling = false;
  bool refillSequenceActive = false;
};

UiState ui;

inline bool isObjReady(lv_obj_t *obj) { return obj && lv_obj_is_valid(obj); }

inline void uiYield() { delay(0); }

void disablePlungerAreaScroll() {
  lv_obj_t *locked[] = {
      objects.main,
      objects.mould_settings,
      objects.common_settings,
      objects.plunger_tip,
      objects.obj0,
      objects.obj2,
      objects.obj5,
  };

  for (lv_obj_t *obj : locked) {
    if (!isObjReady(obj)) {
      continue;
    }
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
  }
}

static void styleKeyboardChrome(lv_obj_t *keyboard) {
  if (!isObjReady(keyboard)) {
    return;
  }

  lv_obj_set_style_border_color(keyboard, lv_color_hex(0xffec18),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(keyboard, 2,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(keyboard, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x1a222b),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
}

void logUiState(const char *tag) {
  Serial.printf(
      "PRD_UI[%s]: screen=%d count=%d selected=%d lastTapped=%d lastName='%s'\n",
      tag ? tag : "?", static_cast<int>(g_currentScreen), ui.mouldProfileCount,
      ui.selectedMould, ui.lastTappedMould, ui.lastMouldName);
}

inline void hideIfPresent(lv_obj_t *obj) {
  if (isObjReady(obj)) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static void async_scroll_to_view(void *target) {
  lv_obj_t *obj = static_cast<lv_obj_t *>(target);
  if (isObjReady(obj)) {
    lv_obj_scroll_to_view_recursive(obj, LV_ANIM_ON);
  }
}

float turnsToCm3(float turns) {
  static const float TURNS_PER_CM3 = 0.99925f;
  if (TURNS_PER_CM3 == 0.0f) {
    return 0.0f;
  }
  return turns / TURNS_PER_CM3;
}

void setButtonEnabled(lv_obj_t *button, bool enabled) {
  if (!isObjReady(button)) {
    return;
  }
  if (enabled) {
    lv_obj_clear_state(button, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(button, LV_STATE_DISABLED);
  }
}

void setLabelTextIfChanged(lv_obj_t *label, const char *text) {
  if (!label || !text) {
    return;
  }
  const char *current = lv_label_get_text(label);
  if (!current || strcmp(current, text) != 0) {
    lv_label_set_text(label, text);
  }
}

void setTextareaTextIfChanged(lv_obj_t *textarea, const char *text) {
  if (!textarea || !text) {
    return;
  }
  const char *current = lv_textarea_get_text(textarea);
  if (!current || strcmp(current, text) != 0) {
    lv_textarea_set_text(textarea, text);
  }
}

void setNotice(lv_obj_t *label, const char *text,
               lv_color_t color = lv_color_hex(0xffd6d6d6)) {
  if (!label) {
    return;
  }
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN | LV_STATE_DEFAULT);
  setLabelTextIfChanged(label, text ? text : "");
}

void keepTextareaVisibleAboveKeyboard(lv_obj_t *scrollContainer, lv_obj_t *textarea,
                                      lv_obj_t *keyboard) {
  if (!isObjReady(scrollContainer) || !isObjReady(textarea) ||
      !isObjReady(keyboard)) {
    return;
  }

  // First let LVGL bring the focused object into view in its scroll chain.
  lv_obj_scroll_to_view_recursive(textarea, LV_ANIM_ON);

  lv_area_t ta;
  lv_area_t kb;
  lv_obj_get_coords(textarea, &ta);
  lv_obj_get_coords(keyboard, &kb);

  const lv_coord_t margin = 10;
  auto get_overlap_and_pref_sign = [&](int *prefSign) -> lv_coord_t {
    lv_area_t ta2;
    lv_area_t kb2;
    lv_obj_get_coords(textarea, &ta2);
    lv_obj_get_coords(keyboard, &kb2);
    const bool keyboard_is_below = kb2.y1 >= ta2.y1;
    if (keyboard_is_below) {
      if (prefSign) {
        *prefSign = -1; // Move content up.
      }
      return (ta2.y2 + margin) - kb2.y1;
    }

    if (prefSign) {
      *prefSign = 1; // Move content down.
    }
    return (kb2.y2 + margin) - ta2.y1;
  };

  int prefSign = -1;
  lv_coord_t overlap = get_overlap_and_pref_sign(&prefSign);
  if (overlap <= 0) {
    return;
  }

  lv_obj_scroll_by(scrollContainer, 0, prefSign * overlap, LV_ANIM_OFF);
  lv_coord_t after = get_overlap_and_pref_sign(nullptr);
  if (after > 0 && after >= overlap) {
    // Wrong direction: undo and apply opposite.
    lv_obj_scroll_by(scrollContainer, 0, -prefSign * overlap, LV_ANIM_OFF);
    lv_obj_scroll_by(scrollContainer, 0, -prefSign * overlap, LV_ANIM_ON);
  } else if (after > 0) {
    // Improved but still overlapping: finish the remaining distance.
    lv_obj_scroll_by(scrollContainer, 0, prefSign * after, LV_ANIM_ON);
  }
}

void alignKeyboardForField(lv_obj_t *keyboard, lv_obj_t *screen,
                           lv_obj_t *textarea) {
  if (!isObjReady(keyboard) || !isObjReady(screen) || !isObjReady(textarea)) {
    return;
  }

  lv_obj_set_parent(keyboard, screen);

  lv_coord_t kb_h = lv_obj_get_height(keyboard);
  if (kb_h <= 0) {
    kb_h = 250;
  }
  lv_obj_set_size(keyboard, lv_pct(100), kb_h);

#if CONFIG_PPINJECTORUI_KEYBOARD_DYNAMIC_ANCHOR
  lv_area_t screenArea;
  lv_area_t fieldArea;
  lv_obj_get_coords(screen, &screenArea);
  lv_obj_get_coords(textarea, &fieldArea);

  const lv_coord_t screenMidY = screenArea.y1 + ((screenArea.y2 - screenArea.y1) / 2);
  const lv_coord_t fieldY = fieldArea.y1;
  const bool useBottom = fieldY < screenMidY;

  if (useBottom) {
    Serial.printf("PRD_UI: Aligning keyboard BOTTOM (field_y=%d mid_y=%d)\n",
                  (int)fieldY, (int)screenMidY);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
  } else {
    Serial.printf("PRD_UI: Aligning keyboard TOP (field_y=%d mid_y=%d)\n",
                  (int)fieldY, (int)screenMidY);
    lv_obj_align(keyboard, LV_ALIGN_TOP_MID, 0, 0);
  }
#else
  Serial.println("PRD_UI: Aligning keyboard BOTTOM (dynamic anchor disabled)");
  lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
#endif
}

lv_obj_t *createButton(lv_obj_t *parent, const char *text, lv_coord_t x,
                       lv_coord_t y, lv_coord_t w, lv_coord_t h,
                       lv_event_cb_t cb, void *userData = nullptr) {
  Serial.printf("PRD_UI: createButton begin text='%s' parent=%p cb=%p user=%p\n",
                text ? text : "(null)", (void *)parent, (void *)cb, userData);
  lv_obj_t *button = lv_button_create(parent);
  Serial.printf("PRD_UI: createButton after lv_button_create button=%p\n",
                (void *)button);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, w, h);
  lv_obj_set_style_radius(button, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1f5ea8),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(button, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(button, lv_color_hex(0xffffff),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  if (cb) {
    Serial.println("PRD_UI: createButton before lv_obj_add_event_cb");
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, userData);
    Serial.println("PRD_UI: createButton after lv_obj_add_event_cb");
  }

  Serial.println("PRD_UI: createButton before lv_label_create");
  lv_obj_t *label = lv_label_create(button);
  Serial.printf("PRD_UI: createButton after lv_label_create label=%p\n",
                (void *)label);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(label);
  Serial.println("PRD_UI: createButton end");
  return button;
}

void hideLegacyWidgets() {
  // Keep main screen from the known-good demo for now.
  // Replace only Mould/Common in this step.
  hideIfPresent(objects.obj3);
  hideIfPresent(objects.obj4);
  hideIfPresent(objects.button_to_mould_settings_1);
  // Legacy "Common Settings" button on main screen.
  hideIfPresent(objects.button_to_mould_settings_3);

  // Common settings
  hideIfPresent(objects.obj6);
  hideIfPresent(objects.button_to_mould_settings_2);

  // Legacy refill bands from the generated plunger widget (debug/demo colors).
  // They can appear as red blocks and are not used by the current PrdUi flow.
  lv_obj_t *legacy_refill_bands[] = {
      objects.plunger_tip__refill_band_0,  objects.plunger_tip__refill_band_1,
      objects.plunger_tip__refill_band_2,  objects.plunger_tip__refill_band_3,
      objects.plunger_tip__refill_band_4,  objects.plunger_tip__refill_band_5,
      objects.plunger_tip__refill_band_6,  objects.plunger_tip__refill_band_7,
      objects.plunger_tip__refill_band_8,  objects.plunger_tip__refill_band_9,
      objects.plunger_tip__refill_band_10, objects.plunger_tip__refill_band_11,
      objects.plunger_tip__refill_band_12, objects.plunger_tip__refill_band_13,
      objects.plunger_tip__refill_band_14, objects.plunger_tip__refill_band_15,

      objects.obj0__refill_band_0,  objects.obj0__refill_band_1,
      objects.obj0__refill_band_2,  objects.obj0__refill_band_3,
      objects.obj0__refill_band_4,  objects.obj0__refill_band_5,
      objects.obj0__refill_band_6,  objects.obj0__refill_band_7,
      objects.obj0__refill_band_8,  objects.obj0__refill_band_9,
      objects.obj0__refill_band_10, objects.obj0__refill_band_11,
      objects.obj0__refill_band_12, objects.obj0__refill_band_13,
      objects.obj0__refill_band_14, objects.obj0__refill_band_15,

      objects.obj2__refill_band_0,  objects.obj2__refill_band_1,
      objects.obj2__refill_band_2,  objects.obj2__refill_band_3,
      objects.obj2__refill_band_4,  objects.obj2__refill_band_5,
      objects.obj2__refill_band_6,  objects.obj2__refill_band_7,
      objects.obj2__refill_band_8,  objects.obj2__refill_band_9,
      objects.obj2__refill_band_10, objects.obj2__refill_band_11,
      objects.obj2__refill_band_12, objects.obj2__refill_band_13,
      objects.obj2__refill_band_14, objects.obj2__refill_band_15,

      objects.obj5__refill_band_0,  objects.obj5__refill_band_1,
      objects.obj5__refill_band_2,  objects.obj5__refill_band_3,
      objects.obj5__refill_band_4,  objects.obj5__refill_band_5,
      objects.obj5__refill_band_6,  objects.obj5__refill_band_7,
      objects.obj5__refill_band_8,  objects.obj5__refill_band_9,
      objects.obj5__refill_band_10, objects.obj5__refill_band_11,
      objects.obj5__refill_band_12, objects.obj5__refill_band_13,
      objects.obj5__refill_band_14, objects.obj5__refill_band_15,
  };

  for (lv_obj_t *band : legacy_refill_bands) {
    hideIfPresent(band);
  }

  // Redundant numeric labels on top of plungers (large 2-line legacy text).
  hideIfPresent(objects.plunger_tip__obj0);
  hideIfPresent(objects.obj0__obj0);
  hideIfPresent(objects.obj2__obj0);
  hideIfPresent(objects.obj5__obj0);
}

void hideNetworkGestureIndicator() {
  if (!isObjReady(ui.networkGestureIndicator)) {
    return;
  }
  lv_obj_add_flag(ui.networkGestureIndicator, LV_OBJ_FLAG_HIDDEN);
}

void updateNetworkGestureIndicator(uint32_t heldMs) {
  if (!isObjReady(ui.networkGestureIndicator)) {
    return;
  }

  if (heldMs >= NETWORK_HOLD_REPROVISION_MS) {
    lv_obj_set_style_bg_color(ui.networkGestureIndicator, lv_color_hex(0xc62828),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui.networkGestureIndicator, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui.networkGestureIndicator, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (heldMs >= NETWORK_HOLD_OTA_MS && PPInjectorUI_wifi_credentials_available()) {
    lv_obj_set_style_bg_color(ui.networkGestureIndicator, lv_color_hex(0x2e7d32),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui.networkGestureIndicator, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui.networkGestureIndicator, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (heldMs >= NETWORK_HOLD_GRAY_MS) {
    lv_obj_set_style_bg_color(ui.networkGestureIndicator, lv_color_hex(0x909090),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui.networkGestureIndicator, LV_OPA_COVER,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui.networkGestureIndicator, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_add_flag(ui.networkGestureIndicator, LV_OBJ_FLAG_HIDDEN);
}

void resetNetworkGesture() {
  ui.networkGestureActive = false;
  ui.networkGestureStartMs = 0;
  ui.networkGestureStartPoint.x = 0;
  ui.networkGestureStartPoint.y = 0;
  hideNetworkGestureIndicator();
}

void onNetworkGestureEvent(lv_event_t *event) {
  const lv_event_code_t code = lv_event_get_code(event);

  if (code == LV_EVENT_PRESSED) {
    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point = {0, 0};
    if (indev) {
      lv_indev_get_point(indev, &point);
    }
    ui.networkGestureStartPoint = point;
    ui.networkGestureStartMs = millis();
    ui.networkGestureActive = true;
    updateNetworkGestureIndicator(0);
    return;
  }

  if (code == LV_EVENT_PRESSING) {
    if (!ui.networkGestureActive) {
      return;
    }

    lv_indev_t *indev = lv_event_get_indev(event);
    lv_point_t point = ui.networkGestureStartPoint;
    if (indev) {
      lv_indev_get_point(indev, &point);
    }

    uint32_t heldMs = millis() - ui.networkGestureStartMs;
    updateNetworkGestureIndicator(heldMs);
    return;
  }

  if ((code == LV_EVENT_RELEASED) || (code == LV_EVENT_PRESS_LOST)) {
    if (ui.networkGestureActive) {
      uint32_t heldMs = millis() - ui.networkGestureStartMs;
      if (heldMs >= NETWORK_HOLD_REPROVISION_MS) {
        ESP_LOGW(TAG, "Gesture action: re-provision requested");
        PPInjectorUI_request_system_action(PPInjectorUI_system_action_reprovision);
      } else if (heldMs >= NETWORK_HOLD_OTA_MS &&
                 PPInjectorUI_wifi_credentials_available()) {
        ESP_LOGW(TAG, "Gesture action: OTA requested");
        PPInjectorUI_request_system_action(PPInjectorUI_system_action_ota);
      }
    }
    resetNetworkGesture();
    return;
  }
}

void createNetworkGestureUi() {
  lv_obj_t *top = lv_layer_top();
  if (!isObjReady(top)) {
    return;
  }

  ui.networkGestureZone = lv_obj_create(top);
  lv_obj_remove_style_all(ui.networkGestureZone);
  lv_obj_set_size(ui.networkGestureZone, 140, 140);
  lv_obj_align(ui.networkGestureZone, LV_ALIGN_TOP_RIGHT, -4, 4);
  lv_obj_add_flag(ui.networkGestureZone, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui.networkGestureZone, onNetworkGestureEvent, LV_EVENT_ALL,
                      nullptr);

  ui.networkGestureIndicator = lv_obj_create(top);
  lv_obj_remove_style_all(ui.networkGestureIndicator);
  lv_obj_set_size(ui.networkGestureIndicator, 78, 78);
  lv_obj_align(ui.networkGestureIndicator, LV_ALIGN_TOP_RIGHT, -12, 12);
  lv_obj_set_style_radius(ui.networkGestureIndicator, 5,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui.networkGestureIndicator, LV_OPA_COVER,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(ui.networkGestureIndicator, lv_color_hex(0x909090),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_add_flag(ui.networkGestureIndicator, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *createRightPanel(lv_obj_t *screen) {
  if (!isObjReady(screen)) {
    Serial.println("PRD_UI: createRightPanel skipped (invalid screen)");
    return nullptr;
  }
  lv_obj_t *panel = lv_obj_create(screen);
  lv_obj_set_pos(panel, RIGHT_X, 0);
  lv_obj_set_size(panel, RIGHT_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x11151a),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

void updateErrorFrames(const DisplayComms::Status &status) {
  bool hasError = (status.errorCode != 0) || (status.errorMsg[0] != '\0');
  lv_obj_t *panels[] = {ui.rightPanelMain, ui.rightPanelMould,
                        ui.rightPanelCommon, ui.rightPanelMouldEdit};

  for (lv_obj_t *panel : panels) {
    if (!panel) {
      continue;
    }
    lv_obj_set_style_border_width(panel, hasError ? 4 : 0,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xffc62828),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  if (ui.mainErrorLabel) {
    if (hasError) {
      char text[120];
      if (status.errorMsg[0] != '\0') {
        snprintf(text, sizeof(text), "ERROR 0x%X: %s", status.errorCode,
                 status.errorMsg);
      } else {
        snprintf(text, sizeof(text), "ERROR 0x%X", status.errorCode);
      }
      setLabelTextIfChanged(ui.mainErrorLabel, text);
      lv_obj_clear_flag(ui.mainErrorLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
      setLabelTextIfChanged(ui.mainErrorLabel, "");
      lv_obj_add_flag(ui.mainErrorLabel, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void updateLeftReadouts(const DisplayComms::Status &status) {
  char posText[40];
  char tempText[40];

  snprintf(posText, sizeof(posText), "%.2f cm3",
           turnsToCm3(status.encoderTurns));
  snprintf(tempText, sizeof(tempText), "%.1f C", status.tempC);

  setLabelTextIfChanged(ui.posLabelMain, posText);
  setLabelTextIfChanged(ui.posLabelMould, posText);
  setLabelTextIfChanged(ui.posLabelCommon, posText);

  setLabelTextIfChanged(ui.tempLabelMain, tempText);
  setLabelTextIfChanged(ui.tempLabelMould, tempText);
  setLabelTextIfChanged(ui.tempLabelCommon, tempText);
}

void updatePlungerPosition(float turns) {
  // Calibrated linear mapping from encoder turns to on-screen plunger Y.
  // This calibration is intentionally kept as the previously tuned plunger behavior.
  static const int MIN_Y_OFFSET = -750;
  static const int MAX_Y_OFFSET = 13;

  float clampedTurns = turns;
  if (clampedTurns < TURNS_MIN) {
    clampedTurns = TURNS_MIN;
  }
  if (clampedTurns > TURNS_MAX) {
    clampedTurns = TURNS_MAX;
  }

  const int targetTipY =
      static_cast<int>((clampedTurns - TURNS_TO_TOP) * PLUNGER_PX_PER_TURN);
  int yOffset = targetTipY - PLUNGER_TIP_ANCHOR_Y;

  if (yOffset < MIN_Y_OFFSET) {
    yOffset = MIN_Y_OFFSET;
  }
  if (yOffset > MAX_Y_OFFSET) {
    yOffset = MAX_Y_OFFSET;
  }

  if (isObjReady(objects.plunger_tip__plunger)) {
    lv_obj_set_y(objects.plunger_tip__plunger, yOffset);
  }
  if (isObjReady(objects.obj0__plunger)) {
    lv_obj_set_y(objects.obj0__plunger, yOffset);
  }
  if (isObjReady(objects.obj2__plunger)) {
    lv_obj_set_y(objects.obj2__plunger, yOffset);
  }
  if (isObjReady(objects.obj5__plunger)) {
    lv_obj_set_y(objects.obj5__plunger, yOffset);
  }
}

void updateRefillBlocks(const DisplayComms::Status &status) {
  float currentPos = status.encoderTurns;
  const char *state = status.state;

  if (strcmp(state, "REFILL") == 0) {
    if (!ui.refillSequenceActive) {
      ui.refillSequenceActive = true;
      ui.startRefillPos = currentPos;
      Serial.printf("PRD_UI: Refill sequence started at %.2f\n", currentPos);
    }
    ui.isRefilling = true;
  } else {
    ui.isRefilling = false;
  }

  if (strcmp(state, "READY_TO_INJECT") == 0 &&
      strcmp(ui.lastState, "READY_TO_INJECT") != 0 && ui.refillSequenceActive) {
    float spaceBelowPlunger = 360.5f - currentPos;
    if (spaceBelowPlunger < 0) {
      spaceBelowPlunger = 0;
    }

    float existingVolume = 0.0f;
    for (int i = 0; i < ui.blockCount; i++) {
      existingVolume += ui.refillBlocks[i].volume;
    }

    float delta = spaceBelowPlunger - existingVolume;
    if (delta > 0.5f) {
      if (ui.blockCount < 16) {
        ui.refillBlocks[ui.blockCount] = RefillBlock(delta, millis(), true);
        ui.blockCount++;
        Serial.printf("PRD_UI: Refill block added vol=%.2f count=%d\n", delta,
                      ui.blockCount);
      } else {
        Serial.println("PRD_UI: Refill block limit reached");
      }
    } else {
      Serial.printf("PRD_UI: Refill block ignored delta=%.2f\n", delta);
    }

    ui.refillSequenceActive = false;
  }

  if (currentPos > ui.lastFramePos) {
    float consumedCm3 = currentPos - ui.lastFramePos;
    if (consumedCm3 > 0.001f && consumedCm3 < 100.0f) {
      while (consumedCm3 > 0.001f && ui.blockCount > 0) {
        if (ui.refillBlocks[0].volume > consumedCm3) {
          ui.refillBlocks[0].volume -= consumedCm3;
          consumedCm3 = 0;
        } else {
          consumedCm3 -= ui.refillBlocks[0].volume;
          for (int i = 0; i < ui.blockCount - 1; i++) {
            ui.refillBlocks[i] = ui.refillBlocks[i + 1];
          }
          ui.blockCount--;
          ui.refillBlocks[ui.blockCount] = RefillBlock();
        }
      }
    }
  }

  ui.lastFramePos = currentPos;
  strncpy(ui.lastState, state, sizeof(ui.lastState) - 1);
}

void renderRefillBlocksForBands(lv_obj_t **bands) {
  if (!bands) {
    return;
  }

  const float pxPerTurn = REFILL_PX_PER_TURN;
  int y = REFILL_STACK_BOTTOM_Y;
  uint32_t now = millis();

  for (int i = 0; i < 16; i++) {
    lv_obj_t *band = bands[i];
    if (!isObjReady(band)) {
      continue;
    }

    if (i < ui.blockCount) {
      int h = static_cast<int>(ui.refillBlocks[i].volume * pxPerTurn);
      if (h < 1) {
        h = 1;
      }
      y -= h;
      lv_obj_set_size(band, 80, h);
      lv_obj_set_pos(band, -8, y);
      lv_obj_clear_flag(band, LV_OBJ_FLAG_HIDDEN);

      uint32_t age = now - ui.refillBlocks[i].addedMs;
      lv_color_t color;
      if (age < 10000) {
        color = lv_color_hex(0x3498db);
      } else if (age < 30000) {
        color = lv_color_hex(0xe67e22);
      } else {
        color = lv_color_hex(0xe74c3c);
      }
      lv_obj_set_style_bg_color(band, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
      lv_obj_add_flag(band, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void renderAllPlungers() {
  lv_obj_t **allObjects = (lv_obj_t **)&objects;
  renderRefillBlocksForBands(&allObjects[6]);
  renderRefillBlocksForBands(&allObjects[29]);
  renderRefillBlocksForBands(&allObjects[58]);
  renderRefillBlocksForBands(&allObjects[104]);
}

void onNavigate(lv_event_t *event) {
  intptr_t target = reinterpret_cast<intptr_t>(lv_event_get_user_data(event));
  Serial.printf("PRD_UI: onNavigate request target=%d from screen=%d\n",
                static_cast<int>(target), static_cast<int>(g_currentScreen));
  logUiState("onNavigate.before");
  auto navigate_async = [](void *userData) {
    const intptr_t asyncTarget = reinterpret_cast<intptr_t>(userData);
    eez_flow_set_screen(static_cast<int16_t>(asyncTarget), LV_SCR_LOAD_ANIM_NONE, 0,
                        0);
    Serial.printf("PRD_UI: onNavigate async applied target=%d now screen=%d\n",
                  static_cast<int>(asyncTarget), static_cast<int>(g_currentScreen));
    logUiState("onNavigate.after");
  };
  lv_async_call(navigate_async, reinterpret_cast<void *>(target));
}

void onStateActionQueryState(lv_event_t *) { DisplayComms::sendQueryState(); }

void onStateActionQueryError(lv_event_t *) { DisplayComms::sendQueryError(); }

void onMouldProfileSelect(lv_event_t *event) {
  int index = static_cast<int>(
      reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  if (index < 0 || index >= ui.mouldProfileCount) {
    return;
  }

  uint32_t now = millis();
  bool isDoubleTap =
      (ui.lastTappedMould == index) && ((now - ui.lastTapMs) <= DOUBLE_TAP_MS);
  ui.lastTappedMould = index;
  ui.lastTapMs = now;
  ui.selectedMould = index;

  for (int i = 0; i < ui.mouldProfileCount; i++) {
    Serial.printf("PRD_UI: rebuild idx=%d/%d\n", i + 1, ui.mouldProfileCount);
    if (!ui.mouldProfileButtons[i]) {
      continue;
    }
    if (i == ui.selectedMould) {
      lv_obj_set_style_bg_color(ui.mouldProfileButtons[i],
                                lv_color_hex(0x2d7dd2),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
      lv_obj_set_style_bg_color(ui.mouldProfileButtons[i],
                                lv_color_hex(0x26303a),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    }
  }

  if (isDoubleTap) {
    setNotice(ui.mouldNotice, "Edit flow pending: use Send for now.");
  }
  Serial.printf("PRD_UI: onMouldProfileSelect index=%d doubleTap=%d\n", index,
                static_cast<int>(isDoubleTap));
  logUiState("onMouldProfileSelect");
}

void syncMouldSendEditEnablement() {
  bool hasSelection =
      ui.selectedMould >= 0 && ui.selectedMould < ui.mouldProfileCount;

  Serial.printf("PRD_UI: sync begin sel=%d count=%d hasSel=%d edit=%p send=%p del=%p\n",
                ui.selectedMould, ui.mouldProfileCount,
                static_cast<int>(hasSelection),
                (void *)ui.mouldButtonEdit, (void *)ui.mouldButtonSend,
                (void *)ui.mouldButtonDelete);

  Serial.println("PRD_UI: sync before edit");
  setButtonEnabled(ui.mouldButtonEdit, hasSelection);
  Serial.println("PRD_UI: sync after edit");

  Serial.println("PRD_UI: sync before safeForUpdate");
  bool safeForUpdate = DisplayComms::isSafeForUpdate();
  Serial.printf("PRD_UI: sync safeForUpdate=%d\n", static_cast<int>(safeForUpdate));

  Serial.println("PRD_UI: sync before send");
  setButtonEnabled(ui.mouldButtonSend, hasSelection && safeForUpdate);
  Serial.println("PRD_UI: sync after send");

  Serial.println("PRD_UI: sync before delete");
  setButtonEnabled(ui.mouldButtonDelete, hasSelection);
  Serial.println("PRD_UI: sync after delete");
}

void rebuildMouldList() {
  lv_mem_monitor_t mon_before;
  lv_mem_monitor(&mon_before);
  ESP_LOGD(TAG,
           "LVGL mem before rebuild: free=%u used_pct=%u frag_pct=%u largest=%u",
           (unsigned)mon_before.free_size, (unsigned)mon_before.used_pct,
           (unsigned)mon_before.frag_pct,
           (unsigned)mon_before.free_biggest_size);

  Serial.printf("PRD_UI: rebuildMouldList begin count=%d selected=%d\n", ui.mouldProfileCount, ui.selectedMould);
  if (!ui.mouldList) {
    Serial.println("PRD_UI: rebuildMouldList aborted (mouldList null)");
    return;
  }
  if (!isObjReady(ui.mouldList)) {
    Serial.println("PRD_UI: rebuildMouldList aborted (mouldList invalid)");
    return;
  }

  lv_obj_clean(ui.mouldList);
  Serial.println("PRD_UI: rebuildMouldList cleaned list");

  for (int i = 0; i < MAX_MOULD_PROFILES; i++) {
    ui.mouldProfileButtons[i] = nullptr;
  }

  int renderCount = ui.mouldProfileCount;
#if SCREEN_DIAG_SKIP_LAST_PROFILE_ON_RENDER
  if (renderCount > 0) {
    Serial.printf("PRD_UI: SCREEN_DIAG_SKIP_LAST_PROFILE_ON_RENDER=1 -> rendering %d/%d profiles\n",
                  renderCount - 1, renderCount);
    renderCount -= 1;
  }
#endif

  int y = 8;
  for (int i = 0; i < renderCount; i++) {
    Serial.printf("PRD_UI: rebuild idx=%d/%d\n", i + 1, ui.mouldProfileCount);
    char safeName[sizeof(ui.mouldProfiles[i].name)];
    safeName[0] = '\0';
    strncpy(safeName, ui.mouldProfiles[i].name, sizeof(safeName) - 1);
    safeName[sizeof(safeName) - 1] = '\0';
    const char *name = safeName[0] != '\0' ? safeName : "Unnamed Mould";
    lv_obj_t *button =
        createButton(ui.mouldList, name, 8, y, 286, 46, onMouldProfileSelect,
                     reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_set_style_bg_color(button, lv_color_hex(0x26303a),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(button, lv_color_hex(0x41505f),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    ui.mouldProfileButtons[i] = button;
    y += 54;
    if ((i & 1) == 1) {
      uiYield();
    }
  }

  if (ui.selectedMould >= ui.mouldProfileCount) {
    ui.selectedMould = -1;
  }
  Serial.println("PRD_UI: rebuild after loop before sync");
  syncMouldSendEditEnablement();
  Serial.println("PRD_UI: rebuild after sync");
  logUiState("rebuildMouldList");
  lv_mem_monitor_t mon_after;
  lv_mem_monitor(&mon_after);
  ESP_LOGD(TAG,
           "LVGL mem after rebuild: free=%u used_pct=%u frag_pct=%u largest=%u",
           (unsigned)mon_after.free_size, (unsigned)mon_after.used_pct,
           (unsigned)mon_after.frag_pct,
           (unsigned)mon_after.free_biggest_size);
  Serial.println("PRD_UI: rebuild end");
}

void onMouldSend(lv_event_t *) {
  if (ui.selectedMould < 0 || ui.selectedMould >= ui.mouldProfileCount) {
    setNotice(ui.mouldNotice, "Select a mould first.", lv_color_hex(0xfff0a0));
    return;
  }
  if (!DisplayComms::isSafeForUpdate()) {
    setNotice(ui.mouldNotice, "Unsafe machine state for MOULD send.",
              lv_color_hex(0xffff7a));
    return;
  }

  if (DisplayComms::sendMould(ui.mouldProfiles[ui.selectedMould])) {
    setNotice(ui.mouldNotice, "MOULD command sent.", lv_color_hex(0xff9be7a5));
  } else {
    setNotice(ui.mouldNotice, "Failed to send MOULD command.",
              lv_color_hex(0xffff7a));
  }
}

// ... Mould Edit Implementation ...

void showMouldEditKeyboard(lv_obj_t *textarea) {
  if (!ui.mouldEditKeyboard || !textarea) {
    return;
  }

#if !SCREEN_DIAG_FORCE_KEYBOARD_CENTER
  alignKeyboardForField(ui.mouldEditKeyboard, objects.mould_settings, textarea);
#else
  lv_obj_center(ui.mouldEditKeyboard);
#endif

  lv_keyboard_set_textarea(ui.mouldEditKeyboard, textarea);

  // Check if text input (Name/Mode) or Number
  // Simple heuristic: Name is index 0, Mode is index 13
  // But wait, we don't have index here easily unless we store it.
  // We can check userdata.
  intptr_t index = reinterpret_cast<intptr_t>(lv_obj_get_user_data(textarea));
  if (MOULD_FIELD_TYPE[index] == 0) {
    lv_keyboard_set_mode(ui.mouldEditKeyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
  } else {
    lv_keyboard_set_mode(ui.mouldEditKeyboard, LV_KEYBOARD_MODE_NUMBER);
  }

  lv_obj_clear_flag(ui.mouldEditKeyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.mouldEditKeyboard);
  keepTextareaVisibleAboveKeyboard(ui.mouldEditScroll, textarea,
                                   ui.mouldEditKeyboard);
  lv_async_call(async_scroll_to_view, textarea);
}

void hideMouldEditKeyboard() {
  if (ui.mouldEditKeyboard) {
    lv_keyboard_set_textarea(ui.mouldEditKeyboard, nullptr);
    lv_obj_add_flag(ui.mouldEditKeyboard, LV_OBJ_FLAG_HIDDEN);
  }
}

void onMouldEditInputFocus(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = lv_event_get_target_obj(event);

  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    showMouldEditKeyboard(target);
    return;
  }

  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL ||
      code == LV_EVENT_DEFOCUSED) {
    hideMouldEditKeyboard();
  }
}

void onMouldEditFieldChanged(lv_event_t *event) { ui.mouldEditDirty = true; }

void onMouldEditCancel(lv_event_t *) {
  hideMouldEditKeyboard();
  lv_obj_add_flag(ui.rightPanelMouldEdit, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui.rightPanelMould, LV_OBJ_FLAG_HIDDEN);
}

void onMouldEditSave(lv_event_t *) {
  hideMouldEditKeyboard();

  // Save inputs back to snapshot -> ui.mouldProfiles
  DisplayComms::MouldParams &p = ui.mouldProfiles[ui.selectedMould];

  for (int i = 0; i < MOULD_FIELD_COUNT; i++) {
    if (!ui.mouldEditInputs[i])
      continue;
    const char *txt = lv_textarea_get_text(ui.mouldEditInputs[i]);
    if (!txt)
      continue;

    if (MOULD_FIELD_TYPE[i] == 0) { // String
      if (i == 0) {
        strncpy(p.name, txt, sizeof(p.name) - 1);
        p.name[sizeof(p.name) - 1] = 0;
      }
      if (i == 13) {
        strncpy(p.mode, txt, sizeof(p.mode) - 1);
        p.mode[sizeof(p.mode) - 1] = 0;
      }
    } else { // Float
      float val = atof(txt);
      switch (i) {
      case 1:
        p.fillVolume = val;
        break;
      case 2:
        p.fillSpeed = val;
        break;
      case 3:
        p.fillPressure = val;
        break;
      case 4:
        p.packVolume = val;
        break;
      case 5:
        p.packSpeed = val;
        break;
      case 6:
        p.packPressure = val;
        break;
      case 7:
        p.packTime = val;
        break;
      case 8:
        p.coolingTime = val;
        break;
      case 9:
        p.fillAccel = val;
        break;
      case 10:
        p.fillDecel = val;
        break;
      case 11:
        p.packAccel = val;
        break;
      case 12:
        p.packDecel = val;
        break;
      case 14:
        p.injectTorque = val;
        break;
      }
    }
  }

  Storage::saveMoulds(ui.mouldProfiles, ui.mouldProfileCount);
  rebuildMouldList(); // Refresh list names

  // If this is the active mould, update controller?
  // User must click "Send" explicitly from the list to update controller.

  lv_obj_add_flag(ui.rightPanelMouldEdit, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui.rightPanelMould, LV_OBJ_FLAG_HIDDEN);
  setNotice(ui.mouldNotice, "Profile saved.", lv_color_hex(0xff9be7a5));
  logUiState("onMouldEditSave");
}

bool ensureMouldEditPanel();

void onMouldEdit(lv_event_t *) {
  if (ui.selectedMould < 0 || ui.selectedMould >= ui.mouldProfileCount) {
    setNotice(ui.mouldNotice, "Select a mould first.", lv_color_hex(0xfff0a0));
    return;
  }

  if (!ensureMouldEditPanel()) {
    setNotice(ui.mouldNotice, "Cannot open editor (LVGL mem).",
              lv_color_hex(0xffff7a));
    return;
  }

  // Populate fields
  DisplayComms::MouldParams &p = ui.mouldProfiles[ui.selectedMould];
  for (int i = 0; i < MOULD_FIELD_COUNT; i++) {
    if (!ui.mouldEditInputs[i])
      continue;
    char buf[32];
    if (MOULD_FIELD_TYPE[i] == 0) { // String
      const char *s = (i == 0) ? p.name : p.mode;
      strncpy(buf, s, sizeof(buf));
    } else { // Float
      float val = 0.0f;
      switch (i) {
      case 1:
        val = p.fillVolume;
        break;
      case 2:
        val = p.fillSpeed;
        break;
      case 3:
        val = p.fillPressure;
        break;
      case 4:
        val = p.packVolume;
        break;
      case 5:
        val = p.packSpeed;
        break;
      case 6:
        val = p.packPressure;
        break;
      case 7:
        val = p.packTime;
        break;
      case 8:
        val = p.coolingTime;
        break;
      case 9:
        val = p.fillAccel;
        break;
      case 10:
        val = p.fillDecel;
        break;
      case 11:
        val = p.packAccel;
        break;
      case 12:
        val = p.packDecel;
        break;
      case 14:
        val = p.injectTorque;
        break;
      }
      snprintf(buf, sizeof(buf), "%.2f", val);
    }
    lv_textarea_set_text(ui.mouldEditInputs[i], buf);
  }

  // Show panel
  lv_obj_add_flag(ui.rightPanelMould, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ui.rightPanelMouldEdit, LV_OBJ_FLAG_HIDDEN);
}

void onMouldNew(lv_event_t *) {
  Serial.println("PRD_UI: onMouldNew begin");
  logUiState("onMouldNew.begin");
  if (ui.mouldProfileCount >= MAX_MOULD_PROFILES) {
    setNotice(ui.mouldNotice, "Profile limit reached.", lv_color_hex(0xffff7a));
    Serial.println("PRD_UI: onMouldNew limit reached");
    return;
  }

  DisplayComms::MouldParams newProfile = {};
  snprintf(newProfile.name, sizeof(newProfile.name), "Local %d",
           ui.mouldProfileCount + 1);
  newProfile.mode[0] = '2';
  newProfile.mode[1] = 'D';
  newProfile.mode[2] = '\0';

  // Set default values for new profile
  newProfile.fillVolume = 10.0f;
  newProfile.fillSpeed = 5.0f;
  newProfile.fillPressure = 50.0f;
  newProfile.packVolume = 2.0f;
  newProfile.packSpeed = 2.0f;
  newProfile.packPressure = 40.0f;
  newProfile.packTime = 2.0f;
  newProfile.coolingTime = 5.0f;
  newProfile.fillAccel = 100.0f;
  newProfile.fillDecel = 100.0f;
  newProfile.packAccel = 100.0f;
  newProfile.packDecel = 100.0f;
  newProfile.injectTorque = 0.5f;

  ui.mouldProfiles[ui.mouldProfileCount] = newProfile;
  ui.mouldProfileCount++;
  Serial.printf("PRD_UI: onMouldNew after append count=%d\n", ui.mouldProfileCount);
  delay(0);
  rebuildMouldList();
  Serial.println("PRD_UI: onMouldNew after rebuild");
  delay(0);
  Storage::saveMoulds(ui.mouldProfiles, ui.mouldProfileCount);
  Serial.println("PRD_UI: onMouldNew after save");
  setNotice(ui.mouldNotice, "Created local mould profile.",
            lv_color_hex(0xff9be7a5));
  Serial.println("PRD_UI: onMouldNew after notice");
  logUiState("onMouldNew");
}

void onMouldDelete(lv_event_t *) {
  if (ui.selectedMould < 0 || ui.selectedMould >= ui.mouldProfileCount) {
    setNotice(ui.mouldNotice, "Select a mould first.", lv_color_hex(0xfff0a0));
    return;
  }

  const int removeIndex = ui.selectedMould;
  for (int i = removeIndex; i < ui.mouldProfileCount - 1; i++) {
    ui.mouldProfiles[i] = ui.mouldProfiles[i + 1];
  }
  if (ui.mouldProfileCount > 0) {
    ui.mouldProfileCount--;
  }

  if (removeIndex == 0) {
    ui.lastMouldName[0] = '\0';
  }

  ui.selectedMould = -1;
  ui.lastTappedMould = -1;
  rebuildMouldList();
  Storage::saveMoulds(ui.mouldProfiles, ui.mouldProfileCount);
  setNotice(ui.mouldNotice, "Profile deleted.", lv_color_hex(0xfff0a0));
  logUiState("onMouldDelete");
}

double commonFieldValue(const DisplayComms::CommonParams &common,
                        int fieldIndex) {
  switch (fieldIndex) {
  case 0:
    return common.trapAccel;
  case 1:
    return common.compressTorque;
  case 2:
    return common.microIntervalMs;
  case 3:
    return common.microDurationMs;
  case 4:
    return common.purgeUp;
  case 5:
    return common.purgeDown;
  case 6:
    return common.purgeCurrent;
  case 7:
    return common.antidripVel;
  case 8:
    return common.antidripCurrent;
  case 9:
    return common.releaseDist;
  case 10:
    return common.releaseTrapVel;
  case 11:
    return common.releaseCurrent;
  case 12:
    return common.contactorCycles;
  case 13:
    return common.contactorLimit;
  default:
    return 0.0;
  }
}

void assignCommonField(DisplayComms::CommonParams &common, int fieldIndex,
                       const char *text) {
  if (!text) {
    return;
  }
  if (COMMON_FIELD_IS_INTEGER[fieldIndex]) {
    uint32_t value = static_cast<uint32_t>(strtoul(text, nullptr, 10));
    switch (fieldIndex) {
    case 2:
      common.microIntervalMs = value;
      break;
    case 3:
      common.microDurationMs = value;
      break;
    case 12:
      common.contactorCycles = value;
      break;
    case 13:
      common.contactorLimit = value;
      break;
    default:
      break;
    }
    return;
  }

  float value = static_cast<float>(atof(text));
  switch (fieldIndex) {
  case 0:
    common.trapAccel = value;
    break;
  case 1:
    common.compressTorque = value;
    break;
  case 4:
    common.purgeUp = value;
    break;
  case 5:
    common.purgeDown = value;
    break;
  case 6:
    common.purgeCurrent = value;
    break;
  case 7:
    common.antidripVel = value;
    break;
  case 8:
    common.antidripCurrent = value;
    break;
  case 9:
    common.releaseDist = value;
    break;
  case 10:
    common.releaseTrapVel = value;
    break;
  case 11:
    common.releaseCurrent = value;
    break;
  default:
    break;
  }
}

void syncCommonInputsFromModel(const DisplayComms::CommonParams &common) {
  ui.suppressCommonEvents = true;
  for (int i = 0; i < COMMON_FIELD_COUNT; i++) {
    if (!ui.commonInputs[i]) {
      continue;
    }
    char buffer[32];
    if (COMMON_FIELD_IS_INTEGER[i]) {
      snprintf(buffer, sizeof(buffer), "%lu",
               static_cast<unsigned long>(commonFieldValue(common, i)));
    } else {
      snprintf(buffer, sizeof(buffer), "%.3f", commonFieldValue(common, i));
    }
    setTextareaTextIfChanged(ui.commonInputs[i], buffer);
  }
  ui.suppressCommonEvents = false;
}

void syncCommonSendEnablement() {
  setButtonEnabled(ui.commonButtonSend,
                   ui.commonDirty && DisplayComms::isSafeForUpdate());
}

void hideCommonKeyboard() {
  if (!ui.commonKeyboard) {
    return;
  }
  lv_keyboard_set_textarea(ui.commonKeyboard, nullptr);
  lv_obj_add_flag(ui.commonKeyboard, LV_OBJ_FLAG_HIDDEN);
}

void showCommonKeyboard(lv_obj_t *textarea) {
  if (!ui.commonKeyboard || !textarea) {
    return;
  }
#if !SCREEN_DIAG_FORCE_KEYBOARD_CENTER
  alignKeyboardForField(ui.commonKeyboard, objects.common_settings, textarea);
#else
  lv_obj_center(ui.commonKeyboard);
#endif

  lv_keyboard_set_textarea(ui.commonKeyboard, textarea);
  lv_keyboard_set_mode(ui.commonKeyboard, LV_KEYBOARD_MODE_NUMBER);
  lv_obj_clear_flag(ui.commonKeyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.commonKeyboard);
  keepTextareaVisibleAboveKeyboard(ui.commonScroll, textarea, ui.commonKeyboard);
  lv_async_call(async_scroll_to_view, textarea);
}

void onCommonInputFocus(lv_event_t *event) {
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *target = lv_event_get_target_obj(event);

  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
    showCommonKeyboard(target);
    return;
  }

  if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL ||
      code == LV_EVENT_DEFOCUSED) {
    hideCommonKeyboard();
  }
}

void onCommonFieldChanged(lv_event_t *event) {
  if (ui.suppressCommonEvents) {
    return;
  }
  if (lv_event_get_code(event) == LV_EVENT_VALUE_CHANGED ||
      lv_event_get_code(event) == LV_EVENT_READY) {
    ui.commonDirty = true;
    syncCommonSendEnablement();
  }
}

bool sendCommonFromInputs() {
  if (!DisplayComms::isSafeForUpdate()) {
    setNotice(ui.commonNotice, "Unsafe machine state for COMMON send.",
              lv_color_hex(0xffff7a));
    return false;
  }

  DisplayComms::CommonParams toSend = DisplayComms::getCommon();
  for (int i = 0; i < COMMON_FIELD_COUNT; i++) {
    if (!ui.commonInputs[i]) {
      continue;
    }
    assignCommonField(toSend, i, lv_textarea_get_text(ui.commonInputs[i]));
  }

  if (DisplayComms::sendCommon(toSend)) {
    setNotice(ui.commonNotice, "COMMON command sent.",
              lv_color_hex(0xff9be7a5));
    ui.commonDirty = false;
    syncCommonSendEnablement();
    return true;
  }

  setNotice(ui.commonNotice, "Failed to send COMMON command.",
            lv_color_hex(0xffff7a));
  return false;
}

void onCommonSend(lv_event_t *) { sendCommonFromInputs(); }

void showCommonDiscardOverlay(bool show) {
  if (!ui.commonDiscardOverlay) {
    return;
  }
  if (show) {
    hideCommonKeyboard();
    lv_obj_clear_flag(ui.commonDiscardOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui.commonDiscardOverlay);
  } else {
    lv_obj_add_flag(ui.commonDiscardOverlay, LV_OBJ_FLAG_HIDDEN);
  }
}

void onCommonBack(lv_event_t *) {
  hideCommonKeyboard();
  if (ui.commonDirty) {
    showCommonDiscardOverlay(true);
    return;
  }
  eez_flow_set_screen(SCREEN_ID_MAIN, LV_SCR_LOAD_ANIM_NONE, 0, 0);
}

void onCommonDiscardSend(lv_event_t *) {
  if (sendCommonFromInputs()) {
    hideCommonKeyboard();
    showCommonDiscardOverlay(false);
    eez_flow_set_screen(SCREEN_ID_MAIN, LV_SCR_LOAD_ANIM_NONE, 0, 0);
  }
}

void onCommonDiscardCancel(lv_event_t *) {
  hideCommonKeyboard();
  syncCommonInputsFromModel(DisplayComms::getCommon());
  ui.commonDirty = false;
  syncCommonSendEnablement();
  showCommonDiscardOverlay(false);
  eez_flow_set_screen(SCREEN_ID_MAIN, LV_SCR_LOAD_ANIM_NONE, 0, 0);
}

void createLeftReadouts(lv_obj_t *screen, lv_obj_t **posLabel,
                        lv_obj_t **tempLabel) {
  *posLabel = lv_label_create(screen);
  lv_obj_set_pos(*posLabel, LEFT_X, 10);
  lv_obj_set_size(*posLabel, LEFT_WIDTH, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(*posLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(*posLabel, &lv_font_montserrat_16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(*posLabel, "-- cm3");

  *tempLabel = lv_label_create(screen);
  lv_obj_set_pos(*tempLabel, LEFT_X, 770);
  lv_obj_set_size(*tempLabel, LEFT_WIDTH, LV_SIZE_CONTENT);
  lv_obj_set_style_text_align(*tempLabel, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(*tempLabel, &lv_font_montserrat_16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(*tempLabel, "--.- C");
}

void createMainPanel() {
  ui.rightPanelMain = createRightPanel(objects.main);
  if (!ui.rightPanelMain) {
    return;
  }
  lv_obj_t *title = lv_label_create(ui.rightPanelMain);
  lv_obj_set_pos(title, 18, 12);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(title, "Main");

  lv_obj_t *stateHeader = lv_label_create(ui.rightPanelMain);
  lv_obj_set_pos(stateHeader, 18, 60);
  lv_obj_set_style_text_font(stateHeader, &lv_font_montserrat_16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(stateHeader, lv_color_hex(0xff9fb2c7),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(stateHeader, "Machine State");

  ui.stateValue = lv_label_create(ui.rightPanelMain);
  lv_obj_set_pos(ui.stateValue, 18, 90);
  lv_obj_set_width(ui.stateValue, RIGHT_WIDTH - 36);
  lv_label_set_long_mode(ui.stateValue, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(ui.stateValue, &lv_font_montserrat_30,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui.stateValue, "--");

  ui.stateAction1 = createButton(ui.rightPanelMain, "Refresh State", 18, 235,
                                 150, 52, onStateActionQueryState);
  ui.stateAction2 = createButton(ui.rightPanelMain, "Refresh Error", 182, 235,
                                 150, 52, onStateActionQueryError);

  createButton(ui.rightPanelMain, "Mould Settings", 18, 720, 150, 58,
               onNavigate,
               reinterpret_cast<void *>(
                   static_cast<intptr_t>(SCREEN_ID_MOULD_SETTINGS)));
  createButton(ui.rightPanelMain, "Common Settings", 182, 720, 150, 58,
               onNavigate,
               reinterpret_cast<void *>(
                   static_cast<intptr_t>(SCREEN_ID_COMMON_SETTINGS)));

  ui.mainErrorLabel = lv_label_create(ui.rightPanelMain);
  lv_obj_set_pos(ui.mainErrorLabel, 18, 640);
  lv_obj_set_width(ui.mainErrorLabel, RIGHT_WIDTH - 36);
  lv_label_set_long_mode(ui.mainErrorLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(ui.mainErrorLabel, &lv_font_montserrat_16,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(ui.mainErrorLabel, lv_color_hex(0xffff6b6b),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui.mainErrorLabel, "");
  lv_obj_add_flag(ui.mainErrorLabel, LV_OBJ_FLAG_HIDDEN);
}

void createMouldPanel() {
  ui.rightPanelMould = createRightPanel(objects.mould_settings);
  if (!ui.rightPanelMould) {
    return;
  }

  lv_obj_t *title = lv_label_create(ui.rightPanelMould);
  lv_obj_set_pos(title, 18, 12);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(title, "Mould Selection");

  ui.mouldList = lv_obj_create(ui.rightPanelMould);
  lv_obj_set_pos(ui.mouldList, 18, 54);
  lv_obj_set_size(ui.mouldList, RIGHT_WIDTH - 36, 530);
  lv_obj_set_style_bg_color(ui.mouldList, lv_color_hex(0x1a222b),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui.mouldList, lv_color_hex(0x3a4a5a),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui.mouldList, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui.mouldList, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_scrollbar_mode(ui.mouldList, LV_SCROLLBAR_MODE_ACTIVE);

  ui.mouldNotice = lv_label_create(ui.rightPanelMould);
  lv_obj_set_pos(ui.mouldNotice, 18, 592);
  lv_obj_set_width(ui.mouldNotice, RIGHT_WIDTH - 36);
  lv_label_set_long_mode(ui.mouldNotice, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(ui.mouldNotice, lv_color_hex(0xffd6d6d6),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui.mouldNotice, "Select a profile.");

  ui.mouldButtonBack = createButton(
      ui.rightPanelMould, "Back", 18, 648, 96, 52, onNavigate,
      reinterpret_cast<void *>(static_cast<intptr_t>(SCREEN_ID_MAIN)));
  ui.mouldButtonSend =
      createButton(ui.rightPanelMould, "Send", 127, 648, 96, 52, onMouldSend);
  ui.mouldButtonEdit =
      createButton(ui.rightPanelMould, "Edit", 236, 648, 96, 52, onMouldEdit);
  ui.mouldButtonNew =
      createButton(ui.rightPanelMould, "New", 70, 712, 96, 52, onMouldNew);
  ui.mouldButtonDelete = 
      createButton(ui.rightPanelMould, "Delete", 184, 712, 96, 52, onMouldDelete);
}

void createMouldEditPanel() {
  Serial.println("PRD_UI: createMouldEditPanel begin");
  ui.rightPanelMouldEdit = createRightPanel(objects.mould_settings);
  if (!ui.rightPanelMouldEdit)
    return;
  lv_obj_add_flag(ui.rightPanelMouldEdit, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = lv_label_create(ui.rightPanelMouldEdit);
  lv_obj_set_pos(title, 18, 12);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(title, "Edit Mould");

  ui.mouldEditScroll = lv_obj_create(ui.rightPanelMouldEdit);
  lv_obj_set_pos(ui.mouldEditScroll, 18, 54);
  lv_obj_set_size(ui.mouldEditScroll, RIGHT_WIDTH - 36, 598);
  lv_obj_set_style_bg_color(ui.mouldEditScroll, lv_color_hex(0x1a222b),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui.mouldEditScroll, lv_color_hex(0x3a4a5a),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui.mouldEditScroll, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui.mouldEditScroll, 6,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_scrollbar_mode(ui.mouldEditScroll, LV_SCROLLBAR_MODE_ACTIVE);

  int y = 6;
  for (int i = 0; i < MOULD_FIELD_COUNT; i++) {
    lv_obj_t *label = lv_label_create(ui.mouldEditScroll);
    lv_obj_set_pos(label, 4, y + 8);
    lv_obj_set_size(label, 160, LV_SIZE_CONTENT);
    lv_label_set_text(label, MOULD_FIELD_NAMES[i]);

    lv_obj_t *input = lv_textarea_create(ui.mouldEditScroll);
    lv_obj_set_pos(input, 168, y);
    lv_obj_set_size(input, 130, 34);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_max_length(input, (i == 0) ? 20 : 10);

    const char *accepted =
        (MOULD_FIELD_TYPE[i] == 0) ? nullptr : "0123456789.-";
    if (accepted)
      lv_textarea_set_accepted_chars(input, accepted);

    lv_textarea_set_text(input, (MOULD_FIELD_TYPE[i] == 0) ? "" : "0");
    lv_obj_set_style_text_align(input, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_user_data(input,
                         reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_add_event_cb(input, onMouldEditInputFocus, LV_EVENT_CLICKED,
                        nullptr);
    lv_obj_add_event_cb(input, onMouldEditInputFocus, LV_EVENT_FOCUSED,
                        nullptr);
    lv_obj_add_event_cb(input, onMouldEditInputFocus, LV_EVENT_DEFOCUSED,
                        nullptr);
    lv_obj_add_event_cb(input, onMouldEditInputFocus, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(input, onMouldEditInputFocus, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_event_cb(input, onMouldEditFieldChanged, LV_EVENT_VALUE_CHANGED,
                        nullptr);

    ui.mouldEditInputs[i] = input;
    y += 42;
    if ((i & 1) == 1) {
      uiYield();
    }
  }

  createButton(ui.rightPanelMouldEdit, "Save", 18, 720, 150, 58,
               onMouldEditSave);
  createButton(ui.rightPanelMouldEdit, "Cancel", 182, 720, 150, 58,
               onMouldEditCancel);

  ui.mouldEditKeyboard = lv_keyboard_create(ui.rightPanelMouldEdit);
  lv_obj_set_pos(ui.mouldEditKeyboard, 0, SCREEN_HEIGHT - 250);
  lv_obj_set_size(ui.mouldEditKeyboard, 480, 250);
  styleKeyboardChrome(ui.mouldEditKeyboard);
  lv_keyboard_set_mode(ui.mouldEditKeyboard, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(ui.mouldEditKeyboard, nullptr);
  lv_obj_add_flag(ui.mouldEditKeyboard, LV_OBJ_FLAG_HIDDEN);
#if SCREEN_DIAG_FORCE_KEYBOARD_CENTER
  lv_obj_set_size(ui.mouldEditKeyboard, 280, 180);
  lv_obj_center(ui.mouldEditKeyboard);
  lv_obj_clear_flag(ui.mouldEditKeyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.mouldEditKeyboard);
  Serial.println("PRD_UI: DIAG force mould keyboard centered");
#endif
  Serial.println("PRD_UI: createMouldEditPanel end");
}

bool ensureMouldEditPanel() {
  if (isObjReady(ui.rightPanelMouldEdit)) {
    return true;
  }
  Serial.println("PRD_UI: ensureMouldEditPanel -> creating lazily");
  createMouldEditPanel();
  return isObjReady(ui.rightPanelMouldEdit);
}

void createCommonPanel() {
  ui.rightPanelCommon = createRightPanel(objects.common_settings);
  if (!ui.rightPanelCommon) {
    return;
  }

  lv_obj_t *title = lv_label_create(ui.rightPanelCommon);
  lv_obj_set_pos(title, 18, 12);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_24,
                             LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(title, "Common Settings");

  ui.commonScroll = lv_obj_create(ui.rightPanelCommon);
  lv_obj_set_pos(ui.commonScroll, 18, 54);
  lv_obj_set_size(ui.commonScroll, RIGHT_WIDTH - 36, 598);
  lv_obj_set_style_bg_color(ui.commonScroll, lv_color_hex(0x1a222b),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(ui.commonScroll, lv_color_hex(0x3a4a5a),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui.commonScroll, 1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(ui.commonScroll, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_scrollbar_mode(ui.commonScroll, LV_SCROLLBAR_MODE_ACTIVE);

  int y = 6;
  for (int i = 0; i < COMMON_FIELD_COUNT; i++) {
    lv_obj_t *label = lv_label_create(ui.commonScroll);
    lv_obj_set_pos(label, 4, y + 8);
    lv_obj_set_size(label, 160, LV_SIZE_CONTENT);
    lv_label_set_text(label, COMMON_FIELD_NAMES[i]);

    lv_obj_t *input = lv_textarea_create(ui.commonScroll);
    lv_obj_set_pos(input, 168, y);
    lv_obj_set_size(input, 130, 34);
    lv_textarea_set_one_line(input, true);
    lv_textarea_set_max_length(input, 20);
    lv_textarea_set_accepted_chars(
        input, COMMON_FIELD_IS_INTEGER[i] ? "0123456789" : "0123456789.-");
    lv_textarea_set_text(input, "0");
    lv_obj_set_style_text_align(input, LV_TEXT_ALIGN_RIGHT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(input, onCommonInputFocus, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(input, onCommonInputFocus, LV_EVENT_FOCUSED, nullptr);
    lv_obj_add_event_cb(input, onCommonInputFocus, LV_EVENT_DEFOCUSED, nullptr);
    lv_obj_add_event_cb(input, onCommonInputFocus, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(input, onCommonInputFocus, LV_EVENT_CANCEL, nullptr);
    lv_obj_add_event_cb(input, onCommonFieldChanged, LV_EVENT_VALUE_CHANGED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_add_event_cb(input, onCommonFieldChanged, LV_EVENT_READY,
                        reinterpret_cast<void *>(static_cast<intptr_t>(i)));

    ui.commonInputs[i] = input;
    y += 42;
    if ((i & 1) == 1) {
      uiYield();
    }
  }

  ui.commonNotice = lv_label_create(ui.rightPanelCommon);
  lv_obj_set_pos(ui.commonNotice, 18, 660);
  lv_obj_set_width(ui.commonNotice, RIGHT_WIDTH - 36);
  lv_label_set_long_mode(ui.commonNotice, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_color(ui.commonNotice, lv_color_hex(0xffd6d6d6),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(ui.commonNotice, "Edit and press Send.");

  ui.commonButtonBack =
      createButton(ui.rightPanelCommon, "Back", 18, 720, 150, 58, onCommonBack);
  ui.commonButtonSend = createButton(ui.rightPanelCommon, "Send", 182, 720, 150,
                                     58, onCommonSend);

  ui.commonKeyboard = lv_keyboard_create(ui.rightPanelCommon);
  lv_obj_set_pos(ui.commonKeyboard, 0, SCREEN_HEIGHT - 250);
  lv_obj_set_size(ui.commonKeyboard, 480, 250);
  styleKeyboardChrome(ui.commonKeyboard);
  lv_keyboard_set_mode(ui.commonKeyboard, LV_KEYBOARD_MODE_NUMBER);
  lv_keyboard_set_textarea(ui.commonKeyboard, nullptr);
  lv_obj_add_flag(ui.commonKeyboard, LV_OBJ_FLAG_HIDDEN);
#if SCREEN_DIAG_FORCE_KEYBOARD_CENTER
  lv_obj_set_size(ui.commonKeyboard, 280, 180);
  lv_obj_center(ui.commonKeyboard);
  lv_obj_clear_flag(ui.commonKeyboard, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.commonKeyboard);
  Serial.println("PRD_UI: DIAG force common keyboard centered");
#endif

  ui.commonDiscardOverlay = lv_obj_create(ui.rightPanelCommon);
  lv_obj_set_pos(ui.commonDiscardOverlay, 0, 0);
  lv_obj_set_size(ui.commonDiscardOverlay, RIGHT_WIDTH, SCREEN_HEIGHT);
  lv_obj_set_style_bg_color(ui.commonDiscardOverlay, lv_color_hex(0x000000),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(ui.commonDiscardOverlay, LV_OPA_60,
                          LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(ui.commonDiscardOverlay, 0,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *dialog = lv_obj_create(ui.commonDiscardOverlay);
  lv_obj_set_size(dialog, 300, 180);
  lv_obj_center(dialog);
  lv_obj_set_style_bg_color(dialog, lv_color_hex(0x1f2630),
                            LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(dialog, lv_color_hex(0x4a5d71),
                                LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(dialog, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *dialogText = lv_label_create(dialog);
  lv_obj_set_pos(dialogText, 14, 18);
  lv_obj_set_width(dialogText, 272);
  lv_label_set_long_mode(dialogText, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(dialogText, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(dialogText, "You have unsent changes.\nSend or Cancel?");

  createButton(dialog, "Send", 24, 112, 110, 46, onCommonDiscardSend);
  createButton(dialog, "Cancel", 166, 112, 110, 46, onCommonDiscardCancel);
  showCommonDiscardOverlay(false);
  uiYield();
}

void updateStateWidgets(const DisplayComms::Status &status) {
  const char *stateText = status.state[0] != '\0' ? status.state : "--";
  setLabelTextIfChanged(ui.stateValue, stateText);

  bool hasState = status.state[0] != '\0';
  if (hasState) {
    if (ui.stateAction1) {
      lv_obj_clear_flag(ui.stateAction1, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui.stateAction2) {
      lv_obj_clear_flag(ui.stateAction2, LV_OBJ_FLAG_HIDDEN);
    }
  } else {
    if (ui.stateAction1) {
      lv_obj_add_flag(ui.stateAction1, LV_OBJ_FLAG_HIDDEN);
    }
    if (ui.stateAction2) {
      lv_obj_add_flag(ui.stateAction2, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

void updateMouldListFromComms(const DisplayComms::MouldParams &mould) {
  if (mould.name[0] == '\0') {
    return;
  }

  if (ui.mouldProfileCount == 0) {
    ui.mouldProfileCount = 1;
    ui.mouldProfiles[0] = mould;
    strncpy(ui.lastMouldName, mould.name, sizeof(ui.lastMouldName) - 1);
    ui.lastMouldName[sizeof(ui.lastMouldName) - 1] = '\0';
    rebuildMouldList();
    return;
  }

  if (strcmp(ui.lastMouldName, mould.name) != 0) {
    ui.mouldProfiles[0] = mould;
    strncpy(ui.lastMouldName, mould.name, sizeof(ui.lastMouldName) - 1);
    ui.lastMouldName[sizeof(ui.lastMouldName) - 1] = '\0';
    rebuildMouldList();
    return;
  }

  // Keep active profile data fresh even when the name doesn't change.
  ui.mouldProfiles[0] = mould;
}

} // namespace

namespace PrdUi {

void storageSelfTest() {
  DisplayComms::MouldParams out[3] = {};

  Storage::init();

  for (int i = 0; i < 3; ++i) {
    DisplayComms::MouldParams &p = out[i];
    snprintf(p.name, sizeof(p.name), "TEST_%d", i + 1);
    p.fillVolume = 10.0f + (float)i;
    p.fillSpeed = 5.0f + (float)i;
    p.fillPressure = 50.0f + (float)i;
    p.packVolume = 2.0f + (float)i;
    p.packSpeed = 3.0f + (float)i;
    p.packPressure = 25.0f + (float)i;
    p.packTime = 0.5f + (float)i * 0.1f;
    p.coolingTime = 4.0f + (float)i;
    p.fillAccel = 100.0f + (float)i;
    p.fillDecel = 90.0f + (float)i;
    p.packAccel = 80.0f + (float)i;
    p.packDecel = 70.0f + (float)i;
    strncpy(p.mode, (i % 2 == 0) ? "2D" : "3D", sizeof(p.mode) - 1);
    p.mode[sizeof(p.mode) - 1] = '\0';
    p.injectTorque = 12.5f + (float)i;
  }

  Storage::saveMoulds(out, 3);
  ESP_LOGI(TAG, "Storage self-test: wrote 3 synthetic profiles");
}

void storageReadDump() {
  DisplayComms::MouldParams in[MAX_MOULD_PROFILES] = {};
  int inCount = 0;

  Storage::init();
  Storage::loadMoulds(in, inCount, MAX_MOULD_PROFILES);
  ESP_LOGI(TAG, "Storage read-dump: loaded=%d", inCount);
}

void init() {
  if (ui.initialized) {
    return;
  }

  if (!isObjReady(objects.main) || !isObjReady(objects.mould_settings) ||
      !isObjReady(objects.common_settings)) {
    Serial.printf("PRD_UI: screen validity m=%d ms=%d cs=%d\n",
                  static_cast<int>(isObjReady(objects.main)),
                  static_cast<int>(isObjReady(objects.mould_settings)),
                  static_cast<int>(isObjReady(objects.common_settings)));
    return;
  }

  // Load persisted moulds
  Storage::init();
  Storage::loadMoulds(ui.mouldProfiles, ui.mouldProfileCount,
                      MAX_MOULD_PROFILES);

  // Keep the plunger column static (touch drag should not scroll the screen).
  disablePlungerAreaScroll();

  Serial.println("PRD_UI: init hideLegacyWidgets");
  hideLegacyWidgets();
  uiYield();

  Serial.println("PRD_UI: init createLeftReadouts");
  createLeftReadouts(objects.main, &ui.posLabelMain, &ui.tempLabelMain);
  createLeftReadouts(objects.mould_settings, &ui.posLabelMould,
                     &ui.tempLabelMould);
  createLeftReadouts(objects.common_settings, &ui.posLabelCommon,
                     &ui.tempLabelCommon);
  uiYield();

  Serial.println("PRD_UI: init createMainPanel");
  createMainPanel();
  Serial.println("PRD_UI: init createMouldPanel");
  createMouldPanel();
#if SCREEN_DIAG_ENABLE_PRD_MOULD_EDIT
  Serial.println("PRD_UI: init deferred createMouldEditPanel (lazy on Edit)");
#else
  Serial.println("PRD_UI: init SCREEN_DIAG_ENABLE_PRD_MOULD_EDIT=0 -> skip edit");
#endif
  Serial.println("PRD_UI: init createCommonPanel");
  createCommonPanel();
  createNetworkGestureUi();
  Serial.println("PRD_UI: init before uiYield after panel creation");
  uiYield();
  Serial.println("PRD_UI: init before rebuildMouldList");

  if (ui.mouldProfileCount <= 0) {
    ui.mouldProfileCount = 1;
    strncpy(ui.mouldProfiles[0].name, "Awaiting QUERY_MOULD",
            sizeof(ui.mouldProfiles[0].name) - 1);
    ui.mouldProfiles[0].name[sizeof(ui.mouldProfiles[0].name) - 1] = '\0';
  }
  rebuildMouldList();
  ui.selectedMould = -1;

  setButtonEnabled(ui.mouldButtonSend, false);
  setButtonEnabled(ui.mouldButtonEdit, false);
  setButtonEnabled(ui.commonButtonSend, false);

  ui.initialized = true;
  Serial.println("PRD_UI: init complete");
}

void tick() {
  if (!ui.initialized) {
    return;
  }

  const DisplayComms::Status &status = DisplayComms::getStatus();
  const DisplayComms::MouldParams &mould = DisplayComms::getMould();
  const DisplayComms::CommonParams &common = DisplayComms::getCommon();

  updateRefillBlocks(status);
  updatePlungerPosition(status.encoderTurns);
  renderAllPlungers();
  updateLeftReadouts(status);
  updateStateWidgets(status);
  updateErrorFrames(status);
  updateMouldListFromComms(mould);
  syncMouldSendEditEnablement();

  if (!ui.commonDirty) {
    syncCommonInputsFromModel(common);
  }
  syncCommonSendEnablement();
}

bool isInitialized() { return ui.initialized; }

} // namespace PrdUi
