#pragma once
#include <lvgl.h>
#include "FAppLayoutUnits.h"

// Shared geometry for the legacy native Radio entry. Playback callbacks stay
// unchanged. Geometry uses actual parent content areas on both LVGL versions.
inline void fos_radio_fit_layout(lv_obj_t *root, lv_obj_t *play,
    lv_obj_t *volume, lv_obj_t *progress, lv_obj_t *tabs,
    lv_obj_t *files, lv_obj_t *web) {
  if (!root || !play || !volume || !progress || !tabs || !files || !web) return;
  const auto box = [](lv_obj_t *object, const char *x, const char *y,
                      const char *w, const char *h) {
    lv_obj_t *parent = lv_obj_get_parent(object);
    lv_obj_update_layout(parent);
    const int pw = lv_obj_get_content_width(parent);
    const int ph = lv_obj_get_content_height(parent);
    lv_obj_set_align(object, LV_ALIGN_TOP_LEFT);
    lv_obj_set_size(object, fos_layout_pixels(w,pw,1), fos_layout_pixels(h,ph,1));
    lv_obj_set_pos(object, fos_layout_pixels(x,pw,0), fos_layout_pixels(y,ph,0));
  };
  box(play, "39.875%", "3.75%", "20.25%", "10.416667%");
  box(volume, "3%", "18.75%", "94.25%", "3.75%");
  box(progress, "3%", "25.416667%", "94.25%", "3.75%");
  box(tabs, "0%", "33.333333%", "100%", "66.25%");
  const int bar = fos_layout_pixels("10.416667%",lv_obj_get_content_height(root),50);
#if LVGL_VERSION_MAJOR >= 9
  lv_tabview_set_tab_bar_size(tabs, bar);
#else
  lv_obj_set_height(lv_tabview_get_tab_btns(tabs), bar);
#endif
  lv_obj_update_layout(tabs);
  box(files, "1.5%", "3%", "97%", "94%");
  box(web, "1.5%", "3%", "97%", "94%");
  lv_obj_update_layout(root);
}
