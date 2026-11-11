#ifndef CURVES_H
#define CURVES_H

#include "flt.h"

#define CURVE_DIR "A/CHDK/CURVES"

// Update version if changes are made to the module interface
#define CURVES_VERSION          {2,1}

typedef struct 
{
    base_interface_t    base;

	void (*curve_init_mode)();
	void (*curve_apply)();
    void (*curve_set_mode)();
    void (*curve_set_file)();
    // EXCCD: Faktoren R, G, B (4096 = 1) fuer eigene Kurven, wirken ab dem naechsten Laden
    void (*curve_set_gains)(int r, int g, int b);
} libcurves_sym;

extern libcurves_sym* libcurves;

//-------------------------------------------------------------------

#endif
