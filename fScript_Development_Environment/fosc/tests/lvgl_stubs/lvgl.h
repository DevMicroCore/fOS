#pragma once

#include <stdint.h>

typedef int32_t lv_coord_t;
typedef int lv_event_code_t;
typedef int lv_anim_enable_t;
typedef uint32_t lv_color_t;
struct lv_font_t {};
struct lv_point_t { lv_coord_t x; lv_coord_t y; };
struct lv_area_t { lv_coord_t x1; lv_coord_t y1; lv_coord_t x2; lv_coord_t y2; };
struct lv_indev_t {};
struct lv_draw_line_dsc_t { lv_color_t color = 0; lv_coord_t width = 1; int round_start = 0; int round_end = 0; };
struct lv_draw_rect_dsc_t { lv_color_t bg_color = 0; lv_color_t border_color = 0; int bg_opa = 0; int border_opa = 0; lv_coord_t border_width = 0; lv_coord_t radius = 0; };
struct lv_draw_arc_dsc_t { lv_color_t color = 0; lv_coord_t width = 1; };
struct lv_draw_label_dsc_t { lv_color_t color = 0; const lv_font_t * font = nullptr; };
struct lv_obj_t { uint32_t flags = 0; uint32_t state = 0; const char * text = ""; lv_obj_t * child = nullptr; };
struct lv_event_t { lv_event_code_t code = 0; lv_obj_t * target = nullptr; void * user = nullptr; };
typedef void (*lv_event_cb_t)(lv_event_t *);

#define LV_STATE_CHECKED (1u << 0)
#define LV_STATE_DISABLED (1u << 1)
#define LV_STATE_FOCUSED (1u << 2)
#define LV_OBJ_FLAG_HIDDEN (1u << 0)
#define LV_EVENT_ALL 0
#define LV_EVENT_CLICKED 1
#define LV_EVENT_VALUE_CHANGED 2
#define LV_EVENT_PRESSED 3
#define LV_EVENT_RELEASED 4
#define LV_EVENT_READY 5
#define LV_EVENT_CANCEL 6
#define LV_EVENT_FOCUSED 7
#define LV_EVENT_PRESSING 8
#define LV_EVENT_PRESS_LOST 9
#define LV_ANIM_OFF 0
#define LV_COORD_MAX 0x3fffffff
#define LV_ROLLER_MODE_NORMAL 0
#define LV_OPA_TRANSP 0
#define LV_OPA_COVER 255
#define LV_RADIUS_CIRCLE 32767

static lv_font_t lv_font_montserrat_20;
static lv_font_t lv_font_montserrat_24;
static lv_font_t lv_font_montserrat_40;
#define LV_FONT_DEFAULT (&lv_font_montserrat_20)

inline lv_obj_t * lv_obj_get_child(lv_obj_t * object, int) { return object == nullptr ? nullptr : object->child; }
inline bool lv_obj_has_state(lv_obj_t * object, uint32_t state) { return object != nullptr && (object->state & state) != 0; }
inline bool lv_obj_has_flag(lv_obj_t * object, uint32_t flag) { return object != nullptr && (object->flags & flag) != 0; }
inline void lv_obj_add_state(lv_obj_t * object, uint32_t state) { if (object != nullptr) object->state |= state; }
inline void lv_obj_clear_state(lv_obj_t * object, uint32_t state) { if (object != nullptr) object->state &= ~state; }
inline void lv_obj_add_flag(lv_obj_t * object, uint32_t flag) { if (object != nullptr) object->flags |= flag; }
inline void lv_obj_clear_flag(lv_obj_t * object, uint32_t flag) { if (object != nullptr) object->flags &= ~flag; }
inline const char * lv_label_get_text(lv_obj_t * object) { return object == nullptr ? "" : object->text; }
inline const char * lv_textarea_get_text(lv_obj_t * object) { return lv_label_get_text(object); }
inline const char * lv_checkbox_get_text(lv_obj_t * object) { return lv_label_get_text(object); }
inline void lv_label_set_text(lv_obj_t * object, const char * text) { if (object != nullptr) object->text = text; }
inline void lv_textarea_set_text(lv_obj_t * object, const char * text) { lv_label_set_text(object, text); }
inline void lv_checkbox_set_text(lv_obj_t * object, const char * text) { lv_label_set_text(object, text); }
inline void lv_roller_get_selected_str(lv_obj_t *, char * output, uint32_t capacity) { if (capacity > 0) output[0] = '\0'; }
inline void lv_dropdown_get_selected_str(lv_obj_t *, char * output, uint32_t capacity) { if (capacity > 0) output[0] = '\0'; }
inline uint16_t lv_roller_get_selected(lv_obj_t *) { return 0; }
inline uint16_t lv_dropdown_get_selected(lv_obj_t *) { return 0; }
inline void lv_roller_set_options(lv_obj_t *, const char *, int) {}
inline void lv_dropdown_set_options(lv_obj_t *, const char *) {}
inline void lv_roller_set_selected(lv_obj_t *, uint16_t, lv_anim_enable_t) {}
inline void lv_dropdown_set_selected(lv_obj_t *, uint16_t) {}
inline void lv_group_focus_obj(lv_obj_t *) {}
inline void lv_obj_move_foreground(lv_obj_t *) {}
inline void lv_obj_scroll_to_y(lv_obj_t *, lv_coord_t, lv_anim_enable_t) {}
inline void lv_obj_add_event_cb(lv_obj_t *, lv_event_cb_t, lv_event_code_t, void *) {}
inline void * lv_event_get_user_data(lv_event_t * event) { return event == nullptr ? nullptr : event->user; }
inline lv_obj_t * lv_event_get_target(lv_event_t * event) { return event == nullptr ? nullptr : event->target; }
inline lv_event_code_t lv_event_get_code(lv_event_t * event) { return event == nullptr ? 0 : event->code; }
inline lv_color_t lv_color_hex(uint32_t value) { return value; }
inline void lv_canvas_fill_bg(lv_obj_t *, lv_color_t, int) {}
inline void lv_canvas_set_px_color(lv_obj_t *, lv_coord_t, lv_coord_t, lv_color_t) {}
inline void lv_draw_line_dsc_init(lv_draw_line_dsc_t * descriptor) { *descriptor = lv_draw_line_dsc_t{}; }
inline void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t * descriptor) { *descriptor = lv_draw_rect_dsc_t{}; }
inline void lv_draw_arc_dsc_init(lv_draw_arc_dsc_t * descriptor) { *descriptor = lv_draw_arc_dsc_t{}; }
inline void lv_draw_label_dsc_init(lv_draw_label_dsc_t * descriptor) { *descriptor = lv_draw_label_dsc_t{}; }
inline void lv_canvas_draw_line(lv_obj_t *, const lv_point_t *, uint32_t, const lv_draw_line_dsc_t *) {}
inline void lv_canvas_draw_rect(lv_obj_t *, lv_coord_t, lv_coord_t, lv_coord_t, lv_coord_t, const lv_draw_rect_dsc_t *) {}
inline void lv_canvas_draw_arc(lv_obj_t *, lv_coord_t, lv_coord_t, lv_coord_t, int, int, const lv_draw_arc_dsc_t *) {}
inline void lv_canvas_draw_text(lv_obj_t *, lv_coord_t, lv_coord_t, lv_coord_t, const lv_draw_label_dsc_t *, const char *) {}
inline lv_coord_t lv_obj_get_width(lv_obj_t *) { return 100; }
inline void lv_obj_invalidate(lv_obj_t *) {}
inline lv_indev_t * lv_indev_get_act() { static lv_indev_t input; return &input; }
inline void lv_indev_get_point(lv_indev_t *, lv_point_t * point) { point->x = 0; point->y = 0; }
inline void lv_obj_get_coords(lv_obj_t *, lv_area_t * area) { area->x1 = 0; area->y1 = 0; area->x2 = 99; area->y2 = 99; }
