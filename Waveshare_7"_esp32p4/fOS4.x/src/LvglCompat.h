#ifndef FOS_LVGL9_COMPAT_H
#define FOS_LVGL9_COMPAT_H
#pragma once
// Only application-side adapters for APIs removed between LVGL 8 and 9.
// SquareLine's new LVGL 9 export includes the real LVGL header directly.
static inline lv_obj_t *fos_tabview_create(lv_obj_t *parent, lv_dir_t position, int32_t size) {
  lv_obj_t *tabs = lv_tabview_create(parent);
  lv_tabview_set_tab_bar_position(tabs, position);
  lv_tabview_set_tab_bar_size(tabs, size);
  return tabs;
}

static inline void lv_canvas_set_px_color(lv_obj_t *canvas, int32_t x, int32_t y, lv_color_t color) {
  lv_canvas_set_px(canvas, x, y, color, LV_OPA_COVER);
}
static inline void lv_canvas_draw_line(lv_obj_t *canvas, const lv_point_t *points,
                                      uint32_t count, const lv_draw_line_dsc_t *style) {
  lv_layer_t layer;
  lv_canvas_init_layer(canvas, &layer);
  for (uint32_t i = 1; i < count; ++i) {
    lv_draw_line_dsc_t dsc = *style;
    dsc.p1.x = points[i - 1].x; dsc.p1.y = points[i - 1].y;
    dsc.p2.x = points[i].x; dsc.p2.y = points[i].y;
    lv_draw_line(&layer, &dsc);
  }
  lv_canvas_finish_layer(canvas, &layer);
}
static inline void lv_canvas_draw_rect(lv_obj_t *canvas, int32_t x, int32_t y,
                                      int32_t w, int32_t h, const lv_draw_rect_dsc_t *style) {
  lv_layer_t layer;
  lv_area_t area = {x, y, x + w - 1, y + h - 1};
  lv_canvas_init_layer(canvas, &layer);
  lv_draw_rect(&layer, style, &area);
  lv_canvas_finish_layer(canvas, &layer);
}
static inline void lv_canvas_draw_arc(lv_obj_t *canvas, int32_t x, int32_t y, int32_t radius,
                                     int32_t start, int32_t end, const lv_draw_arc_dsc_t *style) {
  lv_layer_t layer;
  lv_draw_arc_dsc_t dsc = *style;
  dsc.center.x = x; dsc.center.y = y; dsc.radius = radius;
  dsc.start_angle = start; dsc.end_angle = end;
  lv_canvas_init_layer(canvas, &layer);
  lv_draw_arc(&layer, &dsc);
  lv_canvas_finish_layer(canvas, &layer);
}
static inline void lv_canvas_draw_text(lv_obj_t *canvas, int32_t x, int32_t y, int32_t w,
                                      const lv_draw_label_dsc_t *style, const char *text) {
  lv_layer_t layer;
  lv_draw_label_dsc_t dsc = *style;
  dsc.text = text;
  const lv_draw_buf_t *buffer = lv_canvas_get_draw_buf(canvas);
  if (!buffer) return;
  lv_area_t area = {x, y, x + w - 1, (int32_t)buffer->header.h - 1};
  lv_canvas_init_layer(canvas, &layer);
  lv_draw_label(&layer, &dsc, &area);
  lv_canvas_finish_layer(canvas, &layer);
}

#endif
