#ifndef FILMSIM_H
#define FILMSIM_H

// Filmsimulationen fuer CHDK (IXUS 60 / SD600)
// Kombiniert Canon "My Colors / Custom Color" (Propcases) mit
// RAW-Tonkurven (curves.flt) zu waehlbaren Looks.

#define FILMSIM_VERSION "v1.9.23"

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
extern const char* filmsim_dr_enum(int change, int arg);
extern int filmsim_dr_tv96_shift(void);
extern const char* filmsim_cnr_enum(int change, int arg);

#endif
