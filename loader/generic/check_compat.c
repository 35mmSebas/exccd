/* This file should be used in loader for boot compatibility checks */

#ifndef OPT_DISABLE_COMPAT_CHECK

typedef struct {
    short *fw_pid;
    short pid;
} pid_sig_t;

typedef struct {
    const char *fw_str;
    const char *str;
} ver_sig_t;

/*
Any changes to the following struct have to be reflected in the tools/compatbuilder.sh script
*/
typedef struct {
    unsigned short pid;     // camera P-ID
    short method;           // LED control method (BLINK_LED_CONTROL= in platform/(cam)/makefile.inc)
    unsigned int led;       // LED MMIO address (BLINK_LED_GPIO= in platform/(cam)/makefile.inc)
    unsigned int addr;      // location of P-ID
} pid_led_t;

/*
LED control methods
*/
#define LEDCNTRL_UNK  0     // LED control unknown (no blink possible)
#define LEDCNTRL_OLD  1     // DIGIC II...4; 0x46 to on, 0x44 to off
#define LEDCNTRL_NEW1 2     // DIGIC 5; bit5, inverted
#define LEDCNTRL_NEW2 3     // DIGIC 4+, 5; 0x93d800 to on, 0x83dc00 to off
#define LEDCNTRL_NEW3 4     // DIGIC 6; 0x4d0002 to on, 0x4c0003 to off
#define LEDCNTRL_NEW4 5     // DIGIC 7; 0x024d0002 to on, 0x024c0003 to off

#ifndef NEED_ENCODED_DISKBOOT
#define NEED_ENCODED_DISKBOOT 0
#endif

// following must be ordered by P-ID, order to be maintained inside NEED_ENCODED_DISKBOOT blocks only
pid_led_t pid_leds[]={
// insert generated data
#include "compat_table.h"
};

// compatibility definitions from platform/sub
#include "bin_compat.h"

#ifndef DEBUG_DELAY
    #define DEBUG_DELAY 10000000
#endif

void set_led(int led, int state, int method) {
    volatile long *p = (void*)led;

    if (method == LEDCNTRL_NEW1) {
        // DIGIC 5
        *p = (*p & 0xFFFFFFCF) | ((state) ? 0x00 : 0x20);
    }
    else if (method == LEDCNTRL_NEW2) {
        // DIGIC 4+, 5
        *p = ((state) ? 0x93d800 : 0x83dc00);
    }
    else if (method == LEDCNTRL_NEW3) {
        // DIGIC 6
        *p = ((state) ? 0x4d0002 : 0x4c0003);
    }
    else if (method == LEDCNTRL_NEW4) {
        // DIGIC 7
        *p = ((state) ? 0x024d0002 : 0x024c0003);
    }
    else if (method == LEDCNTRL_OLD) {
        *p = ((state) ? 0x46 : 0x44);
    }
}

const int num_pid_sigs = sizeof(pid_sigs)/sizeof(pid_sig_t);
const int num_ver_sigs = sizeof(ver_sigs)/sizeof(ver_sig_t);
const int led_addresses = sizeof(pid_leds)/sizeof(pid_led_t);

unsigned short get_pid(void) {
    int i;
    for(i=0;i<led_addresses;i++) {
        if( (*(unsigned short*)pid_leds[i].addr >= pid_leds[0].pid) &&
            (*(unsigned short*)pid_leds[i].addr <= pid_leds[led_addresses-1].pid) ) {
            // return the first possible number which looks like a valid P-ID
            return *(unsigned short*)pid_leds[i].addr;
        }
    }
    return 0;
}

void blink(unsigned short cam_pid) {
    unsigned int led = 0;
    int method = LEDCNTRL_UNK;
    int i;
    // check if there's a known LED
    for(i=0;i<led_addresses;i++) {
        if(cam_pid == pid_leds[i].pid) {
            led = pid_leds[i].led;
            method = pid_leds[i].method;
            break;
        }
    }
    // no known LED, no blink
    if (!led)
        while(1);
    // LED found, blink (control method chosen according to the type of GPIO)
    while(1) {
        set_led(led, 1, method);
        for(i=0;i<DEBUG_DELAY;i++) {
            asm("nop\n nop\n");
        };
        set_led(led, 0, method);
        for(i=0;i<DEBUG_DELAY;i++) {
            asm("nop\n nop\n");
        };
    }
}

// my_ver assumed to be a null terminated string like 1.00A
int ver_cmp(const char *my_ver, const char *fw_ver) {
    int i=0;
    while(my_ver[i] == fw_ver[i] && my_ver[i] != 0)
        i++;
    return my_ver[i] == 0; // hit the null in our string
}

unsigned short check_pid(void) {
    int i;
    // skip check if pid information not available
    if (num_pid_sigs == 0)
        return 0;
    for(i=0;i<num_pid_sigs;i++) {
        if(*pid_sigs[i].fw_pid == pid_sigs[i].pid) {
            return pid_sigs[i].pid;
        }
    }
    i = get_pid();
    blink(i);
    return 0; // to make the compiler happy
}

#endif // OPT_DISABLE_COMPAT_CHECK

#ifdef EXCCD_FW_PROBE
/* EXCCD firmware probe (make fwprobe, written to the card by the installer).
 * Never starts CHDK: finds the "Firmware Ver GM1.02D" string in ROM and
 * blinks it in groups with a pause in between: one long blink per unit of
 * the middle number (1.00 = none, 1.01 = 1, 1.02 = 2), then one short blink
 * per letter (A = 1, B = 2, C = 3, D = 4 ...). After about 30 seconds and at
 * least three groups the camera switches itself off where we know how
 * (see PROBE_OFF_*), otherwise it blinks until the battery comes out.
 * String not found: fast flicker. Wrong camera: check_pid() blinks slowly,
 * as in a normal CHDK build. */

#ifdef OPT_DISABLE_COMPAT_CHECK
#error "EXCCD_FW_PROBE needs the compatibility check code"
#endif

// one unit; CHDK's own error blink is 4 units on, 4 off
#ifndef PROBE_UNIT
#define PROBE_UNIT (DEBUG_DELAY / 4)
#endif

// how long to blink before switching off, in units (~30 s, measured on the IXUS 132)
#ifndef PROBE_RUN_UNITS
#define PROBE_RUN_UNITS 150
#endif
#define PROBE_MIN_GROUPS 3

// LEDs missing from compat_table.h (address from platform/<cam>/lib.c)
#if defined(CAMERA_ixus132_elph115) && !defined(EXCCD_PROBE_LED)
#define EXCCD_PROBE_LED 0xC0220094
#endif

/* Switching off. PROBE_OFF_GPIO1/2: power-hold outputs, written 0x44 like
 * Canon's own boot code does when it gives up (hardware, so right for every
 * firmware version). PROBE_OFF_FUNC: Canon's TurnOffE1, a firmware address,
 * so only called when the camera runs the firmware this probe was built for. */
#if defined(CAMERA_ixus60_sd600)
#define PROBE_OFF_GPIO1 0xC02200A0      // shutdown() in loader/ixus60_sd600/main.c
#elif defined(CAMERA_sx110is)
#define PROBE_OFF_GPIO1 0xC0220054      // sub_FFC0CD..., platform/sx110is/sub/100b/boot.c
#define PROBE_OFF_GPIO2 0xC022004C
#elif defined(CAMERA_ixus132_elph115)
#define PROBE_OFF_FUNC 0xff037fe4       // TurnOffE1, sub/100b/stubs_entry_2.S
#elif defined(CAMERA_ixus145_elph135)
#define PROBE_OFF_FUNC 0xff8362b4       // TurnOffE1, sub/100c/stubs_entry_2.S
#endif

static void probe_wait(int units) {
    int i;
    for(i=0;i<units*PROBE_UNIT;i++) {
        asm("nop\n nop\n");
    }
}

static void probe_flash(unsigned int led, int method, int on, int off) {
    set_led(led, 1, method);
    probe_wait(on);
    set_led(led, 0, method);
    probe_wait(off);
}

// one group: long blinks for the middle number, short ones for the letter,
// then the pause. Returns its length in units.
static int probe_group(unsigned int led, int method, int middle, int letter) {
    int i;
    for(i=0;i<middle;i++)
        probe_flash(led, method, 7, 4);
    for(i=0;i<letter;i++)
        probe_flash(led, method, 2, 4);
    probe_wait(12);
    return middle * 11 + letter * 6 + 12;
}

static const char *probe_find_version(void) {
    static const char tag[] = "Firmware Ver GM";
    const unsigned int len = sizeof(tag) - 1;
    unsigned int a, i;
    for(a=ROMBASEADDR; a < 0xFFFFFF00; a++) {
        const char *s = (const char *)a;
        if (s[0] != 'F')
            continue;
        for(i=1;i<len && s[i] == tag[i];i++);
        if (i < len)
            continue;
        s += len;   // "1.00D"
        if (s[0] >= '0' && s[0] <= '9' && s[1] == '.' &&
            s[2] >= '0' && s[2] <= '9' && s[3] >= '0' && s[3] <= '9' &&
            s[4] >= 'A' && s[4] <= 'Z')
            return s;
    }
    return 0;
}

// "1.00D" is the PLATFORMSUB "100d" this probe was built for
static int probe_is_own_fw(const char *v) {
    const char *sub = PLATFORMSUB;
    return v[0] == sub[0] && v[2] == sub[1] && v[3] == sub[2] && v[4] + 32 == sub[3] && !sub[4];
}

static void probe_power_off(unsigned int led, int method, const char *v) {
    set_led(led, 0, method);
#if defined(PROBE_OFF_GPIO1)
    (void)v;
    *(volatile long *)PROBE_OFF_GPIO1 = 0x44;
#if defined(PROBE_OFF_GPIO2)
    *(volatile long *)PROBE_OFF_GPIO2 = 0x44;
#endif
#elif defined(PROBE_OFF_FUNC)
    if (probe_is_own_fw(v))
        ((void (*)(void))PROBE_OFF_FUNC)();
#else
    (void)v;
#endif
}

void __attribute__((noreturn)) fw_probe(unsigned short cam_pid) {
    unsigned int led = 0;
    int method = LEDCNTRL_UNK;
    int i, letter, middle, groups, units;
    const char *v;

    if (!cam_pid)
        cam_pid = PLATFORMID;

    for(i=0;i<led_addresses;i++) {
        if(cam_pid == pid_leds[i].pid) {
            led = pid_leds[i].led;
            method = pid_leds[i].method;
            break;
        }
    }
#ifdef EXCCD_PROBE_LED
    // camera without a known LED in compat_table.h
    if (!led) {
        led = EXCCD_PROBE_LED;
        method = LEDCNTRL_OLD;
    }
#endif
    if (!led)
        while(1);

    v = probe_find_version();
    if (!v) {
        while(1)
            probe_flash(led, method, 1, 1);     // fast flicker: not found
    }
    middle = (v[2] - '0') * 10 + (v[3] - '0');  // 1.02 -> 2
    letter = v[4] - 'A' + 1;

    probe_wait(8);
    units = 8;
    for(groups=0; groups < PROBE_MIN_GROUPS || units < PROBE_RUN_UNITS; groups++)
        units += probe_group(led, method, middle, letter);
    probe_power_off(led, method, v);
    // still running: no known way to switch off, keep blinking
    while(1)
        probe_group(led, method, middle, letter);
}
#endif // EXCCD_FW_PROBE

void check_compat(void) {
#ifdef EXCCD_FW_PROBE
    fw_probe(check_pid());
#endif
#ifndef OPT_DISABLE_COMPAT_CHECK
    int i;
    unsigned short cam_pid;
    cam_pid = check_pid();
    for(i=0;i<num_ver_sigs;i++) {
        if(ver_cmp(ver_sigs[i].str,ver_sigs[i].fw_str)) {
            return;
       }
    }
    blink(cam_pid);
#endif
}

void core_copy(const long *src, long *dst, long length) {
    if (src < dst && dst < src + length) {
        /* Have to copy backwards */
        src += length;
        dst += length;
        while (length--)  *--dst = *--src;
    }
    else
        while (length--)  *dst++ = *src++;
}

