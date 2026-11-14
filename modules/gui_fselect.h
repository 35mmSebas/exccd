#ifndef GUI_FSELECT_H
#define GUI_FSELECT_H

#include "flt.h"

// Update version if changes are made to the module interface
#define GUI_FSELECT_VERSION     {2,1}

typedef struct
{
    base_interface_t    base;

    void (*file_select)(int title, const char* prev_dir, const char* default_dir, void (*on_select)(const char *fn));
    // EXCCD: RAW-Dateien auf der Karte loeschen (Rueckfrage mit Anzahl und Groesse).
    // purge = 1: nur RAW ohne JPEG gleicher Nummer, 0: alle RAW. JPEGs bleiben immer.
    void (*raw_clean)(int purge);
} libfselect_sym;

//-------------------------------------------------------------------
extern libfselect_sym* libfselect;

//-------------------------------------------------------------------
#endif
