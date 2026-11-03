#pragma once
// Internal to the UI. Screen size is 466 x 466 (round AMOLED).
#include "lvgl.h"
#include "ui.h"
#include "ui_app.h"
#include "assets/assets.h"

#define SCR_W 466
#define SCR_H 466

// Colours (same palette as the browser prototype)
#define C_INK     0xF1F3FF
#define C_DIM     0x8B91AE
#define C_BLUE    0x6AA7FF
#define C_VIOLET  0x9B7BFF
#define C_OK      0x7BE3A4
#define C_BAD     0xFF7A8A

// Shared styles
extern lv_style_t st_screen, st_glass, st_glass_sel, st_glass_on, st_title, st_dim, st_btn_round, st_btn_on, st_pressed;
void theme_init(void);
lv_obj_t *mk_screen(void);                                    // black screen with the glow background
lv_obj_t *mk_label(lv_obj_t *p, const lv_font_t *f, uint32_t color, const char *txt);
lv_obj_t *mk_icon(lv_obj_t *p, const lv_image_dsc_t *img, uint32_t color);
lv_obj_t *mk_round_btn(lv_obj_t *p, int size, const lv_image_dsc_t *icon, bool on);
lv_obj_t *mk_title(lv_obj_t *scr, const char *text, bool bottom);   // "‹ Title", tap = back
const lv_image_dsc_t *tile_icon(const char *key);

// Navigation
typedef enum { SCR_HOME, SCR_MENU, SCR_LIST, SCR_CONTROL, SCR_QR, SCR_SETUP, SCR_VOICE } scr_kind_t;
typedef enum { L_CATEGORY, L_ROOMS, L_ROOM, L_FAVS, L_MORE, L_INFO, L_COMPANION } list_kind_t;
typedef struct {
    scr_kind_t kind;
    list_kind_t list;
    char arg[ID_LEN];      // tile key, area index, or entity id
    int32_t scroll;        // remembered scroll position
} scr_ref_t;

void nav_push(const scr_ref_t *r);
void nav_replace_home(void);
scr_ref_t *nav_top(void);
void nav_gesture_back(lv_obj_t *scr);          // swipe right = back

// Screen builders and refreshers (refresh is called when the model or status changes)
void home_build(lv_obj_t *scr);   void home_refresh(void);   void home_happy(int ms); void home_mood_override(int mood, int ms);
void menu_build(lv_obj_t *scr);   void menu_refresh(void);
void list_build(lv_obj_t *scr, scr_ref_t *r); void list_refresh(void);
void ctl_build(lv_obj_t *scr, scr_ref_t *r);  void ctl_refresh(void);
void qr_build(lv_obj_t *scr);     void qr_refresh(void);
void setup_build(lv_obj_t *scr);  void setup_refresh(void);
void voice_build(lv_obj_t *scr);  void voice_set(voice_state_t st, const char *text);

// Category helpers (shared by menu and lists)
bool cat_match(const char *key, const entity_t *e);
int  cat_summary(const char *key, char *buf, int len);   // returns number of visible entities
int  cat_summary_locked(const model_t *m, const settings_t *s, const char *key, char *buf, int len, bool *on);
bool ui_asleep_flag(void);
