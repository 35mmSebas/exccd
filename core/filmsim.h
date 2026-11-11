#ifndef FILMSIM_H
#define FILMSIM_H

// Filmsimulationen fuer CHDK (IXUS 60 / SD600)
// Kombiniert Canon "My Colors / Custom Color" (Propcases) mit
// RAW-Tonkurven (curves.flt) zu waehlbaren Looks.

#define FILMSIM_VERSION "v1.10.2"

extern const char* filmsim_menu_enum(int change, int arg);
extern const char* filmsim_name(void);
extern void filmsim_apply(void);
extern void filmsim_tick(void);
extern void filmsim_draw_osd(void);
extern void filmsim_set(int v);
extern void filmsim_open_menu(void);
extern void filmsim_raw_apply(char *raw);
extern int filmsim_photo_mode(void);
extern int filmsim_alt_allowed(void);
extern int filmsim_osd_hidden(void);
extern const char* filmsim_double_enum(int change, int arg);
extern const char* filmsim_bwfilter_enum(int change, int arg);
extern const char* filmsim_mist_enum(int change, int arg);
extern const char* filmsim_halation_enum(int change, int arg);
extern const char* filmsim_vignette_enum(int change, int arg);
extern const char* filmsim_leak_enum(int change, int arg);
extern const char* filmsim_datestamp_enum(int change, int arg);
extern const char* filmsim_grain_enum(int change, int arg);
extern const char* filmsim_frame_enum(int change, int arg);
extern int filmsim_develop_armed(void);
extern void filmsim_develop_select(const char *fn);
extern void filmsim_develop_cancel(void);
extern void filmsim_develop_begin(char *raw);
extern int filmsim_dng_mark(const char *fn);
extern const char* filmsim_dr_enum(int change, int arg);
extern void filmsim_reset_settings(void);
extern const char* filmsim_wb_enum(int change, int arg);
extern const char* filmsim_wb_r_enum(int change, int arg);
extern const char* filmsim_wb_b_enum(int change, int arg);
extern const char* filmsim_wb_shift_enum(int change, int arg);
extern const char* filmsim_recipe_enum(int change, int arg);
extern void filmsim_recipe_save_menu(int arg);
extern char filmsim_slot_text[4][32];
extern char filmsim_look_text[40];
extern void filmsim_recipe_save(int arg);
extern int filmsim_dr_tv96_shift(int automatic);
extern int filmsim_autoiso(void);
extern const char* filmsim_aiso_enum(int change, int arg);
extern const char* filmsim_aiso_max_enum(int change, int arg);
extern const char* filmsim_aiso_str(void);
extern void filmsim_dr_curve_hook(int end);
extern const char* filmsim_cnr_enum(int change, int arg);
extern const char* filmsim_zoom_enum(int change, int arg);
extern const char* filmsim_zoom_str(void);
extern int filmsim_zoom_kbd(void);
extern void filmsim_zoom_tick(void);

#endif
