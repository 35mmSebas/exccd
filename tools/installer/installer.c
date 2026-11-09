/* EXCCD SD card installer for Windows.
 *
 * One window, three steps: pick the download ZIP, pick the SD card, install.
 * Installing means: keep CHDK/FILMLOG.CSV, delete the old CHDK folder, unpack
 * the ZIP to the card, write the "BOOTDISK" mark into the card's boot sector
 * (like CHDK's "Make Card Bootable"), check it and eject the card.
 *
 * Only removable drives are offered. Writing the boot sector needs raw volume
 * access, so the program asks for administrator rights (see the manifest).
 */
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <winioctl.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <dbt.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "core.h"

#ifndef EXCCD_INSTALLER_VERSION
#define EXCCD_INSTALLER_VERSION "dev"
#endif

/* ---- strings (English / German) ----------------------------------------- */

enum {
    S_TITLE, S_STEP1, S_BROWSE, S_CAMERA, S_CAMERA_PICK, S_STEP2, S_REFRESH, S_INSTALL,
    S_ZIP_NONE, S_ZIP_FOUND, S_ZIP_READ_FAIL, S_ZIP_BAD, S_ZIP_NOT_EXCCD, S_ZIP_UNSAFE,
    S_CAM_UNKNOWN, S_NOCARD, S_CARD_OK, S_CARD_OK_AGAIN, S_CARD_FILES_ONLY, S_CARD_NEED_FAT16,
    S_CARD_EXFAT, S_CARD_BAD_FS, S_CARD_SPACE, S_CARD_READ_FAIL, S_CONFIRM, S_CONFIRM_TITLE,
    S_LOG_KEEP_LOG, S_LOG_DELETE, S_LOG_UNPACK, S_LOG_UNPACKED, S_LOG_BOOT, S_LOG_BOOT_OK,
    S_LOG_BOOT_ALREADY, S_LOG_EJECT, S_ERR_DELETE, S_ERR_WRITE, S_ERR_UNPACK, S_ERR_LOCK,
    S_ERR_BOOT, S_ERR_VERIFY, S_DONE, S_DONE_FILES_ONLY, S_FAILED, S_ZIP_FILTER,
    S_PROBE, S_PROBE_TITLE, S_PROBE_CONFIRM, S_PROBE_NEED_CAM, S_PROBE_BAD_CARD, S_LOG_PROBE,
    S_LOG_REQ, S_PROBE_DONE_BLINK, S_PROBE_DONE_REQ, S_PROBE_REQ_HOWTO, S_PROBE_SHORT,
    S_PROBE_LONG, S_PROBE_OK, S_PROBE_NO, S_COUNT
};

static const wchar_t *str_en[S_COUNT] = {
    [S_TITLE] = L"EXCCD SD card installer",
    [S_STEP1] = L"1   Your EXCCD download (ZIP)",
    [S_BROWSE] = L"Browse…",
    [S_CAMERA] = L"Camera:",
    [S_CAMERA_PICK] = L"(please choose)",
    [S_STEP2] = L"2   SD card (formatted in the camera first)",
    [S_REFRESH] = L"Refresh",
    [S_INSTALL] = L"3   Install EXCCD on the card",
    [S_ZIP_NONE] = L"Pick the ZIP you downloaded from the EXCCD website, or drop it on this window.",
    [S_ZIP_FOUND] = L"Download: %ls",
    [S_ZIP_READ_FAIL] = L"The file can't be read.",
    [S_ZIP_BAD] = L"This is not a valid ZIP file (%hs).",
    [S_ZIP_NOT_EXCCD] = L"This ZIP is not an EXCCD download (no DISKBOOT.BIN and CHDK folder).",
    [S_ZIP_UNSAFE] = L"This ZIP contains unsafe file names and is not used.",
    [S_CAM_UNKNOWN] = L"Please choose your camera.",
    [S_NOCARD] = L"No SD card found. Insert the card (formatted in the camera) and click Refresh.",
    [S_CARD_OK] = L"Ready: EXCCD will be unpacked and the card made bootable.",
    [S_CARD_OK_AGAIN] = L"Ready: the card is already bootable, EXCCD will be updated.",
    [S_CARD_FILES_ONLY] = L"The %ls only starts automatically from cards up to 2 GB (FAT16); this card is FAT32. "
                          L"EXCCD will be copied anyway: start it in playback mode with MENU → Firm Update.",
    [S_CARD_NEED_FAT16] = L"The %ls only starts EXCCD from cards up to 2 GB (FAT16); this card is FAT32. "
                          L"Please use a card of 2 GB or less and format it in the camera.",
    [S_CARD_EXFAT] = L"Cards over 32 GB (exFAT) are not supported yet. Please use a card of 32 GB or less "
                     L"and format it in the camera.",
    [S_CARD_BAD_FS] = L"The card is not FAT formatted. Please format it in the camera first.",
    [S_CARD_SPACE] = L"Not enough free space on the card.",
    [S_CARD_READ_FAIL] = L"The card can't be read.",
    [S_CONFIRM] = L"Install EXCCD on %ls?\n\nAn existing CHDK folder is replaced (FILMLOG.CSV is kept). "
                  L"Your photos stay untouched.",
    [S_CONFIRM_TITLE] = L"Install EXCCD",
    [S_LOG_KEEP_LOG] = L"Keeping CHDK\\FILMLOG.CSV",
    [S_LOG_DELETE] = L"Removing the old CHDK folder…",
    [S_LOG_UNPACK] = L"Unpacking %d files…",
    [S_LOG_UNPACKED] = L"%d files unpacked.",
    [S_LOG_BOOT] = L"Making the card bootable (%hs)…",
    [S_LOG_BOOT_OK] = L"Boot mark written and checked.",
    [S_LOG_BOOT_ALREADY] = L"The card was already bootable.",
    [S_LOG_EJECT] = L"Card ejected.",
    [S_ERR_DELETE] = L"Can't delete %ls (error %lu).",
    [S_ERR_WRITE] = L"Can't write %ls (error %lu).",
    [S_ERR_UNPACK] = L"%hs is damaged in the ZIP. Please download it again.",
    [S_ERR_LOCK] = L"The card is in use by another program (error %lu). Close Explorer windows showing the card and try again.",
    [S_ERR_BOOT] = L"Can't write the boot sector (error %lu).",
    [S_ERR_VERIFY] = L"The boot sector doesn't read back correctly. Try another card or card reader.",
    [S_DONE] = L"Done!\n\n1. Take the card out.\n2. Slide its write-protect switch to LOCK.\n"
               L"3. Put it in the camera and switch it on.\n\nThe camera still saves photos on a locked card.",
    [S_DONE_FILES_ONLY] = L"Done!\n\n1. Take the card out and put it in the camera.\n"
                          L"2. Switch on in playback mode.\n3. MENU → Firm Update → OK.\n\n"
                          L"Do this after every power-on.",
    [S_FAILED] = L"Installation failed.",
    [S_ZIP_FILTER] = L"ZIP files",
    [S_PROBE] = L"Check firmware…",
    [S_PROBE_TITLE] = L"Check firmware",
    [S_PROBE_CONFIRM] = L"Put a firmware test on %ls?\n\nThe camera will show its firmware version instead of "
                        L"starting EXCCD. Afterwards install EXCCD as usual, that replaces the test. "
                        L"Your photos stay untouched.",
    [S_PROBE_NEED_CAM] = L"Choose your camera first, then insert a card formatted in the camera.",
    [S_PROBE_BAD_CARD] = L"This card can't be used for the test. Format it in the camera first.",
    [S_LOG_PROBE] = L"Writing the firmware test (DISKBOOT.BIN)…",
    [S_LOG_REQ] = L"Writing VER.REQ and VERS.REQ…",
    [S_PROBE_DONE_BLINK] = L"Ready!\n\n1. Slide the card's write-protect switch to LOCK.\n"
                           L"2. Put it in the %ls and switch it on.\n"
                           L"3. The screen stays dark, a lamp blinks in groups with a pause in between. "
                           L"Count the blinks of one group:\n\n%ls\n\n"
                           L"Something else: a version we don't know yet, please tell us. "
                           L"Fast flickering: the version wasn't found.\n\n"
                           L"4. After about 30 seconds the camera switches itself off. "
                           L"If it keeps blinking, take the battery out.\n"
                           L"5. Unlock the card again.\n"
                           L"6. Is it the version marked ✓? Then put the card back in the card reader, "
                           L"choose your EXCCD download and click \"Install EXCCD on the card\". "
                           L"That replaces the test with EXCCD.\n\n"
                           L"If nothing blinks, unlock the card and check this way:\n%ls",
    [S_PROBE_SHORT] = L"%d× short",
    [S_PROBE_LONG] = L"%d× long + ",
    [S_PROBE_OK] = L"EXCCD works",
    [S_PROBE_NO] = L"no EXCCD for this version",
    [S_PROBE_DONE_REQ] = L"Ready!\n\nPut the card in the %ls (not locked).\n\n%ls\n\nEXCCD needs firmware %ls. "
                         L"If your camera shows it, put the card back in the card reader, choose your EXCCD "
                         L"download and click \"Install EXCCD on the card\".",
    [S_PROBE_REQ_HOWTO] = L"Switch the camera on in playback mode, hold FUNC/SET and press DISP. "
                          L"The camera shows a line like \"Firmware Ver GM1.00D\", the letter at the end counts.",
};

static const wchar_t *str_de[S_COUNT] = {
    [S_TITLE] = L"EXCCD SD-Karten-Installer",
    [S_STEP1] = L"1   Dein EXCCD-Download (ZIP)",
    [S_BROWSE] = L"Durchsuchen…",
    [S_CAMERA] = L"Kamera:",
    [S_CAMERA_PICK] = L"(bitte wählen)",
    [S_STEP2] = L"2   SD-Karte (vorher in der Kamera formatiert)",
    [S_REFRESH] = L"Aktualisieren",
    [S_INSTALL] = L"3   EXCCD auf die Karte installieren",
    [S_ZIP_NONE] = L"Wähle die ZIP, die du auf der EXCCD-Webseite geladen hast, oder zieh sie auf dieses Fenster.",
    [S_ZIP_FOUND] = L"Download: %ls",
    [S_ZIP_READ_FAIL] = L"Die Datei lässt sich nicht lesen.",
    [S_ZIP_BAD] = L"Das ist keine gültige ZIP-Datei (%hs).",
    [S_ZIP_NOT_EXCCD] = L"Diese ZIP ist kein EXCCD-Download (kein DISKBOOT.BIN und CHDK-Ordner).",
    [S_ZIP_UNSAFE] = L"Diese ZIP enthält unsichere Dateinamen und wird nicht verwendet.",
    [S_CAM_UNKNOWN] = L"Bitte wähle deine Kamera.",
    [S_NOCARD] = L"Keine SD-Karte gefunden. Karte (in der Kamera formatiert) einlegen und auf Aktualisieren klicken.",
    [S_CARD_OK] = L"Bereit: EXCCD wird entpackt und die Karte bootfähig gemacht.",
    [S_CARD_OK_AGAIN] = L"Bereit: Die Karte ist schon bootfähig, EXCCD wird aktualisiert.",
    [S_CARD_FILES_ONLY] = L"Die %ls startet nur von Karten bis 2 GB (FAT16) automatisch, diese Karte ist FAT32. "
                          L"EXCCD wird trotzdem kopiert: Start im Wiedergabemodus über MENU → Firm Update.",
    [S_CARD_NEED_FAT16] = L"Die %ls startet EXCCD nur von Karten bis 2 GB (FAT16), diese Karte ist FAT32. "
                          L"Bitte eine Karte bis 2 GB nehmen und in der Kamera formatieren.",
    [S_CARD_EXFAT] = L"Karten über 32 GB (exFAT) werden noch nicht unterstützt. Bitte eine Karte bis 32 GB "
                     L"nehmen und in der Kamera formatieren.",
    [S_CARD_BAD_FS] = L"Die Karte ist nicht FAT-formatiert. Bitte zuerst in der Kamera formatieren.",
    [S_CARD_SPACE] = L"Nicht genug Platz auf der Karte.",
    [S_CARD_READ_FAIL] = L"Die Karte lässt sich nicht lesen.",
    [S_CONFIRM] = L"EXCCD auf %ls installieren?\n\nEin vorhandener CHDK-Ordner wird ersetzt (FILMLOG.CSV bleibt). "
                  L"Deine Fotos bleiben unberührt.",
    [S_CONFIRM_TITLE] = L"EXCCD installieren",
    [S_LOG_KEEP_LOG] = L"CHDK\\FILMLOG.CSV wird behalten",
    [S_LOG_DELETE] = L"Alter CHDK-Ordner wird entfernt…",
    [S_LOG_UNPACK] = L"%d Dateien werden entpackt…",
    [S_LOG_UNPACKED] = L"%d Dateien entpackt.",
    [S_LOG_BOOT] = L"Karte wird bootfähig gemacht (%hs)…",
    [S_LOG_BOOT_OK] = L"Boot-Kennung geschrieben und geprüft.",
    [S_LOG_BOOT_ALREADY] = L"Die Karte war schon bootfähig.",
    [S_LOG_EJECT] = L"Karte ausgeworfen.",
    [S_ERR_DELETE] = L"%ls lässt sich nicht löschen (Fehler %lu).",
    [S_ERR_WRITE] = L"%ls lässt sich nicht schreiben (Fehler %lu).",
    [S_ERR_UNPACK] = L"%hs ist in der ZIP beschädigt. Bitte neu herunterladen.",
    [S_ERR_LOCK] = L"Die Karte wird von einem anderen Programm benutzt (Fehler %lu). Explorer-Fenster der Karte schließen und nochmal versuchen.",
    [S_ERR_BOOT] = L"Der Bootsektor lässt sich nicht schreiben (Fehler %lu).",
    [S_ERR_VERIFY] = L"Der Bootsektor liest sich nicht korrekt zurück. Andere Karte oder anderen Kartenleser versuchen.",
    [S_DONE] = L"Fertig!\n\n1. Karte herausnehmen.\n2. Schreibschutz-Schieber der Karte auf LOCK stellen.\n"
               L"3. Karte in die Kamera, Kamera einschalten.\n\nAuch mit gesperrter Karte speichert die Kamera Fotos.",
    [S_DONE_FILES_ONLY] = L"Fertig!\n\n1. Karte herausnehmen und in die Kamera stecken.\n"
                          L"2. Im Wiedergabemodus einschalten.\n3. MENU → Firm Update → OK.\n\n"
                          L"Das nach jedem Einschalten wiederholen.",
    [S_FAILED] = L"Installation fehlgeschlagen.",
    [S_ZIP_FILTER] = L"ZIP-Dateien",
    [S_PROBE] = L"Firmware prüfen…",
    [S_PROBE_TITLE] = L"Firmware prüfen",
    [S_PROBE_CONFIRM] = L"Firmware-Test auf %ls schreiben?\n\nDie Kamera zeigt dann ihre Firmware-Version, statt EXCCD "
                        L"zu starten. Danach EXCCD wie gewohnt installieren, das ersetzt den Test. "
                        L"Deine Fotos bleiben unberührt.",
    [S_PROBE_NEED_CAM] = L"Erst die Kamera wählen, dann eine in der Kamera formatierte Karte einlegen.",
    [S_PROBE_BAD_CARD] = L"Diese Karte geht für den Test nicht. Bitte zuerst in der Kamera formatieren.",
    [S_LOG_PROBE] = L"Firmware-Test wird geschrieben (DISKBOOT.BIN)…",
    [S_LOG_REQ] = L"VER.REQ und VERS.REQ werden geschrieben…",
    [S_PROBE_DONE_BLINK] = L"Fertig!\n\n1. Schreibschutz-Schieber der Karte auf LOCK stellen.\n"
                           L"2. Karte in die %ls, Kamera einschalten.\n"
                           L"3. Der Bildschirm bleibt dunkel, eine Lampe blinkt in Gruppen mit Pause dazwischen. "
                           L"Zähl die Blinker einer Gruppe:\n\n%ls\n\n"
                           L"Etwas anderes: eine Version, die wir noch nicht kennen, bitte melde dich. "
                           L"Schnelles Flackern: Version nicht gefunden.\n\n"
                           L"4. Nach etwa 30 Sekunden schaltet sich die Kamera selbst aus. "
                           L"Blinkt sie weiter, Akku herausnehmen.\n"
                           L"5. Karte wieder entsperren.\n"
                           L"6. Ist es die Version mit ✓? Dann die Karte wieder in den Kartenleser stecken, "
                           L"deinen EXCCD-Download wählen und auf \"EXCCD auf die Karte installieren\" klicken. "
                           L"Das ersetzt den Test durch EXCCD.\n\n"
                           L"Falls nichts blinkt, Karte entsperren und so prüfen:\n%ls",
    [S_PROBE_SHORT] = L"%d× kurz",
    [S_PROBE_LONG] = L"%d× lang + ",
    [S_PROBE_OK] = L"EXCCD läuft",
    [S_PROBE_NO] = L"kein EXCCD für diese Version",
    [S_PROBE_DONE_REQ] = L"Fertig!\n\nKarte in die %ls stecken (nicht gesperrt).\n\n%ls\n\nEXCCD braucht Firmware %ls. "
                         L"Zeigt deine Kamera diese Version, die Karte wieder in den Kartenleser stecken, deinen "
                         L"EXCCD-Download wählen und auf \"EXCCD auf die Karte installieren\" klicken.",
    [S_PROBE_REQ_HOWTO] = L"Kamera im Wiedergabemodus einschalten, FUNC/SET halten und DISP drücken. "
                          L"Die Kamera zeigt eine Zeile wie \"Firmware Ver GM1.00D\", es zählt der Buchstabe am Ende.",
};

static const wchar_t **str = str_en;
#define T(id) (str[id])

/* ---- state -------------------------------------------------------------- */

enum {
    ID_ZIP_EDIT = 100, ID_BROWSE, ID_CAMERA, ID_DRIVE, ID_REFRESH, ID_STATUS, ID_INSTALL,
    ID_PROGRESS, ID_LOG, ID_PROBE, ID_LANG
};
#define WM_APP_LOG        (WM_APP + 1)
#define WM_APP_PROGRESS   (WM_APP + 2)
#define WM_APP_DONE       (WM_APP + 3)
#define WM_APP_PROBE_DONE (WM_APP + 4)

typedef struct {
    wchar_t letter;
    wchar_t label[64];
    uint64_t total, free;
    int fs;                 /* from the boot sector */
    int bootable;
    int readable;
} drive_info;

static HWND g_wnd, g_zip_edit, g_browse, g_camera, g_drive, g_refresh, g_status, g_install,
            g_progress, g_log, g_zip_label, g_cam_label, g_card_label, g_probe, g_lang;
static HFONT g_font, g_bold;

static wchar_t g_zip_path[MAX_PATH];
static uint8_t *g_zip_data;
static zip_file g_zip;
static int g_zip_ok;

static drive_info g_drives[26];
static int g_drive_count;
static int g_busy;

/* ---- small helpers ------------------------------------------------------ */

static void fmt(wchar_t *buf, size_t n, const wchar_t *f, ...)
{
    va_list ap;
    va_start(ap, f);
    _vsnwprintf(buf, n - 1, f, ap);
    buf[n - 1] = 0;
    va_end(ap);
}

static void fmt_size(wchar_t *buf, size_t n, uint64_t bytes)
{
    wchar_t dec = str == str_de ? L',' : L'.';
    if (bytes >= 1000000000ull) {
        uint64_t t = (bytes + 50000000ull) / 100000000ull;
        fmt(buf, n, L"%llu%lc%llu GB", t / 10, dec, t % 10);
    } else {
        fmt(buf, n, L"%llu MB", (bytes + 500000ull) / 1000000ull);
    }
}

/* Thread-safe log: the worker posts a heap string to the window. */
static void post_log(const wchar_t *f, ...)
{
    wchar_t buf[512];
    va_list ap;
    va_start(ap, f);
    _vsnwprintf(buf, 511, f, ap);
    buf[511] = 0;
    va_end(ap);
    PostMessageW(g_wnd, WM_APP_LOG, 0, (LPARAM)_wcsdup(buf));
}

static void append_log(const wchar_t *s)
{
    int len = GetWindowTextLengthW(g_log);
    SendMessageW(g_log, EM_SETSEL, len, len);
    if (len)
        SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    SendMessageW(g_log, EM_REPLACESEL, FALSE, (LPARAM)s);
}

static void set_status(const wchar_t *s) { SetWindowTextW(g_status, s); }

/* ---- ZIP ---------------------------------------------------------------- */

static const camera *selected_camera(void)
{
    LRESULT i = SendMessageW(g_camera, CB_GETCURSEL, 0, 0);
    return i >= 1 && i <= camera_count ? &cameras[i - 1] : NULL;
}

static void update_state(void);

static void unload_zip(void)
{
    if (g_zip_ok)
        zip_close(&g_zip);
    free(g_zip_data);
    g_zip_data = NULL;
    g_zip_ok = 0;
}

static void load_zip(const wchar_t *path)
{
    HANDLE h;
    LARGE_INTEGER size;
    DWORD got = 0;
    char err[128], utf8[MAX_PATH * 3];
    wchar_t msg[512];

    unload_zip();
    lstrcpynW(g_zip_path, path, MAX_PATH);
    SetWindowTextW(g_zip_edit, path);

    h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE || !GetFileSizeEx(h, &size) || size.QuadPart > 64 * 1024 * 1024 ||
        !(g_zip_data = malloc((size_t)size.QuadPart + 1)) ||
        !ReadFile(h, g_zip_data, (DWORD)size.QuadPart, &got, NULL) || got != size.QuadPart) {
        if (h != INVALID_HANDLE_VALUE)
            CloseHandle(h);
        append_log(T(S_ZIP_READ_FAIL));
        unload_zip();
        update_state();
        return;
    }
    CloseHandle(h);

    if (zip_open(&g_zip, g_zip_data, got, err, sizeof(err)) != 0) {
        fmt(msg, 512, T(S_ZIP_BAD), err);
        append_log(msg);
        free(g_zip_data);
        g_zip_data = NULL;
        update_state();
        return;
    }
    g_zip_ok = 1;
    if (!zip_is_exccd(&g_zip)) {
        append_log(T(S_ZIP_NOT_EXCCD));
        unload_zip();
        update_state();
        return;
    }
    for (int i = 0; i < g_zip.count; i++)
        if (!zip_name_safe(g_zip.entries[i].name)) {
            append_log(T(S_ZIP_UNSAFE));
            unload_zip();
            update_state();
            return;
        }

    WideCharToMultiByte(CP_UTF8, 0, path, -1, utf8, sizeof(utf8), NULL, NULL);
    const camera *cam = camera_detect(&g_zip, utf8);
    SendMessageW(g_camera, CB_SETCURSEL, cam ? (WPARAM)(cam - cameras) + 1 : 0, 0);
    fmt(msg, 512, T(S_ZIP_FOUND), wcsrchr(path, L'\\') ? wcsrchr(path, L'\\') + 1 : path);
    append_log(msg);
    update_state();
}

/* Newest exccd-*.zip in the user's Downloads folder. */
static void find_download(void)
{
    PWSTR dl = NULL;
    wchar_t pattern[MAX_PATH], best[MAX_PATH] = L"";
    FILETIME best_time = {0, 0};
    WIN32_FIND_DATAW fd;
    HANDLE f;

    if (SHGetKnownFolderPath(&FOLDERID_Downloads, 0, NULL, &dl) != S_OK)
        return;
    fmt(pattern, MAX_PATH, L"%ls\\exccd-*.zip", dl);
    f = FindFirstFileW(pattern, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (CompareFileTime(&fd.ftLastWriteTime, &best_time) > 0) {
                best_time = fd.ftLastWriteTime;
                fmt(best, MAX_PATH, L"%ls\\%ls", dl, fd.cFileName);
            }
        } while (FindNextFileW(f, &fd));
        FindClose(f);
    }
    CoTaskMemFree(dl);
    if (best[0])
        load_zip(best);
}

static void browse_zip(void)
{
    wchar_t file[MAX_PATH] = L"", filter[128];
    OPENFILENAMEW ofn = {0};
    int n = _snwprintf(filter, 100, L"%ls (*.zip)", T(S_ZIP_FILTER));
    /* filter is "name\0*.zip\0\0" */
    wcscpy(filter + n + 1, L"*.zip");
    filter[n + 1 + 5] = 0;
    filter[n + 1 + 6] = 0;
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_wnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    if (GetOpenFileNameW(&ofn))
        load_zip(file);
}

/* ---- drives ------------------------------------------------------------- */

static HANDLE open_volume(wchar_t letter, DWORD access)
{
    wchar_t path[8];
    fmt(path, 8, L"\\\\.\\%lc:", letter);
    return CreateFileW(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                       FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
}

static DWORD sector_size(HANDLE h)
{
    DISK_GEOMETRY g;
    DWORD got;
    if (DeviceIoControl(h, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &g, sizeof(g), &got, NULL) &&
        g.BytesPerSector >= 512 && g.BytesPerSector <= 4096)
        return g.BytesPerSector;
    return 512;
}

/* Read the first sector of the volume into an aligned buffer (VirtualAlloc). */
static uint8_t *read_sector0(HANDLE h, DWORD *ssize)
{
    DWORD got;
    uint8_t *buf;
    *ssize = sector_size(h);
    buf = VirtualAlloc(NULL, *ssize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf)
        return NULL;
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    if (!ReadFile(h, buf, *ssize, &got, NULL) || got != *ssize) {
        VirtualFree(buf, 0, MEM_RELEASE);
        return NULL;
    }
    return buf;
}

static void probe_drive(drive_info *d)
{
    HANDLE h = open_volume(d->letter, GENERIC_READ);
    DWORD ss;
    uint8_t *s;
    d->readable = 0;
    if (h == INVALID_HANDLE_VALUE)
        return;
    s = read_sector0(h, &ss);
    if (s) {
        d->fs = bootsec_fs(s);
        d->bootable = bootsec_is_bootable(s);
        d->readable = 1;
        VirtualFree(s, 0, MEM_RELEASE);
    }
    CloseHandle(h);
}

static void refresh_drives(void)
{
    wchar_t sysdir[MAX_PATH], text[160], size[32];
    DWORD mask = GetLogicalDrives();
    wchar_t keep = 0;
    LRESULT sel = SendMessageW(g_drive, CB_GETCURSEL, 0, 0);
    int keep_index = 0;

    if (g_busy)
        return;
    if (sel >= 0 && sel < g_drive_count)
        keep = g_drives[sel].letter;
    GetWindowsDirectoryW(sysdir, MAX_PATH);
    SendMessageW(g_drive, CB_RESETCONTENT, 0, 0);
    g_drive_count = 0;

    for (int i = 2; i < 26; i++) {            /* skip A: and B: */
        wchar_t root[4] = { (wchar_t)(L'A' + i), L':', L'\\', 0 };
        ULARGE_INTEGER avail, total, fr;
        drive_info *d = &g_drives[g_drive_count];
        if (!(mask & (1u << i)) || towupper(sysdir[0]) == root[0])
            continue;
        if (GetDriveTypeW(root) != DRIVE_REMOVABLE)
            continue;
        memset(d, 0, sizeof(*d));
        d->letter = root[0];
        if (!GetVolumeInformationW(root, d->label, 64, NULL, NULL, NULL, NULL, 0))
            continue;                       /* reader without a card */
        if (GetDiskFreeSpaceExW(root, &avail, &total, &fr)) {
            d->total = total.QuadPart;
            d->free = avail.QuadPart;
        }
        probe_drive(d);
        fmt_size(size, 32, d->total);
        fmt(text, 160, L"%lc:   %ls   %ls   %hs", d->letter, d->label[0] ? d->label : L"SD", size,
            d->readable ? fs_name(d->fs) : "?");
        SendMessageW(g_drive, CB_ADDSTRING, 0, (LPARAM)text);
        if (d->letter == keep)
            keep_index = g_drive_count;
        g_drive_count++;
    }
    if (g_drive_count)
        SendMessageW(g_drive, CB_SETCURSEL, keep_index, 0);
    update_state();
}

static drive_info *selected_drive(void)
{
    LRESULT i = SendMessageW(g_drive, CB_GETCURSEL, 0, 0);
    return i >= 0 && i < g_drive_count ? &g_drives[i] : NULL;
}

/* ---- state -------------------------------------------------------------- */

static int g_check = -1;                    /* CHECK_* for the current selection */

static void update_state(void)
{
    const camera *cam = selected_camera();
    drive_info *d = selected_drive();
    wchar_t msg[512];
    int ok = 0;

    g_check = -1;
    if (!g_zip_ok)
        set_status(T(S_ZIP_NONE));
    else if (!cam)
        set_status(T(S_CAM_UNKNOWN));
    else if (!d)
        set_status(T(S_NOCARD));
    else if (!d->readable)
        set_status(T(S_CARD_READ_FAIL));
    else {
        g_check = check_card(cam, d->fs);
        switch (g_check) {
        case CHECK_OK:
            set_status(T(d->bootable ? S_CARD_OK_AGAIN : S_CARD_OK));
            ok = 1;
            break;
        case CHECK_FILES_ONLY:
            {
                wchar_t name[64];
                MultiByteToWideChar(CP_UTF8, 0, cam->name, -1, name, 64);
                fmt(msg, 512, T(S_CARD_FILES_ONLY), name);
            }
            set_status(msg);
            ok = 1;
            break;
        case CHECK_NEED_FAT16: {
            wchar_t name[64];
            MultiByteToWideChar(CP_UTF8, 0, cam->name, -1, name, 64);
            fmt(msg, 512, T(S_CARD_NEED_FAT16), name);
            set_status(msg);
            break;
        }
        case CHECK_EXFAT:
            set_status(T(S_CARD_EXFAT));
            break;
        default:
            set_status(T(S_CARD_BAD_FS));
            break;
        }
        /* The old CHDK folder is replaced, so its size is free space too;
         * a rough check against the whole download is good enough. */
        if (ok && d->free < zip_total_size(&g_zip) + 1024 * 1024) {
            wchar_t chdk[8] = { d->letter, L':', L'\\', L'C', L'H', L'D', L'K', 0 };
            if (GetFileAttributesW(chdk) == INVALID_FILE_ATTRIBUTES) {
                set_status(T(S_CARD_SPACE));
                ok = 0;
            }
        }
    }
    EnableWindow(g_install, ok && !g_busy);
    /* needs no ZIP; start_probe() says what's missing */
    EnableWindow(g_probe, !g_busy);
    EnableWindow(g_browse, !g_busy);
    EnableWindow(g_camera, !g_busy);
    EnableWindow(g_drive, !g_busy);
    EnableWindow(g_refresh, !g_busy);
}

/* ---- install (worker thread) -------------------------------------------- */

typedef struct {
    wchar_t letter;
    int make_bootable;
} job;

static int delete_tree(const wchar_t *dir)
{
    wchar_t pattern[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE f;
    int ok = 1;

    fmt(pattern, MAX_PATH, L"%ls\\*", dir);
    f = FindFirstFileW(pattern, &fd);
    if (f != INVALID_HANDLE_VALUE) {
        do {
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
                continue;
            fmt(path, MAX_PATH, L"%ls\\%ls", dir, fd.cFileName);
            SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                if (!delete_tree(path))
                    ok = 0;
            } else if (!DeleteFileW(path)) {
                post_log(T(S_ERR_DELETE), path, GetLastError());
                ok = 0;
            }
        } while (ok && FindNextFileW(f, &fd));
        FindClose(f);
    }
    if (ok && !RemoveDirectoryW(dir)) {
        post_log(T(S_ERR_DELETE), dir, GetLastError());
        ok = 0;
    }
    return ok;
}

static int write_file(const wchar_t *path, const uint8_t *data, DWORD size)
{
    HANDLE h;
    DWORD put = 0;
    SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        post_log(T(S_ERR_WRITE), path, GetLastError());
        return 0;
    }
    if (size && (!WriteFile(h, data, size, &put, NULL) || put != size)) {
        post_log(T(S_ERR_WRITE), path, GetLastError());
        CloseHandle(h);
        return 0;
    }
    CloseHandle(h);
    return 1;
}

/* Create every directory on the way to path (path itself if dir is set). */
static void make_dirs(wchar_t *path, int include_last)
{
    for (wchar_t *p = path + 3; *p; p++)
        if (*p == L'\\') {
            *p = 0;
            CreateDirectoryW(path, NULL);
            *p = L'\\';
        }
    if (include_last)
        CreateDirectoryW(path, NULL);
}

static int unpack(wchar_t letter)
{
    int files = 0, done = 0;
    for (int i = 0; i < g_zip.count; i++) {
        size_t len = strlen(g_zip.entries[i].name);
        if (len && g_zip.entries[i].name[len - 1] != '/')
            files++;
    }
    post_log(T(S_LOG_UNPACK), files);
    for (int i = 0; i < g_zip.count; i++) {
        const zip_entry *e = &g_zip.entries[i];
        wchar_t path[MAX_PATH], name[MAX_PATH];
        size_t len = strlen(e->name);
        int is_dir = len && e->name[len - 1] == '/';

        MultiByteToWideChar(CP_UTF8, 0, e->name, -1, name, MAX_PATH);
        for (wchar_t *p = name; *p; p++)
            if (*p == L'/')
                *p = L'\\';
        if (is_dir)
            name[wcslen(name) - 1] = 0;
        fmt(path, MAX_PATH, L"%lc:\\%ls", letter, name);
        if (is_dir) {
            make_dirs(path, 1);
            continue;
        }
        make_dirs(path, 0);
        uint8_t *buf = zip_read(&g_zip, i);
        if (!buf) {
            post_log(T(S_ERR_UNPACK), e->name);
            return 0;
        }
        int ok = write_file(path, buf, e->usize);
        free(buf);
        if (!ok)
            return 0;
        done++;
        PostMessageW(g_wnd, WM_APP_PROGRESS, done * 90 / (files ? files : 1), 0);
    }
    post_log(T(S_LOG_UNPACKED), done);
    return 1;
}

static int make_bootable(wchar_t letter)
{
    HANDLE h = open_volume(letter, GENERIC_READ | GENERIC_WRITE);
    DWORD ss, got, err = 0;
    uint8_t *s = NULL, *check = NULL;
    int ok = 0, locked = 0;

    if (h == INVALID_HANDLE_VALUE) {
        post_log(T(S_ERR_BOOT), GetLastError());
        return 0;
    }
    FlushFileBuffers(h);
    for (int tries = 0; tries < 20; tries++) {
        if (DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &got, NULL)) {
            locked = 1;
            break;
        }
        err = GetLastError();
        Sleep(250);
    }
    if (!locked) {
        post_log(T(S_ERR_LOCK), err);
        CloseHandle(h);
        return 0;
    }

    s = read_sector0(h, &ss);
    if (!s) {
        post_log(T(S_ERR_BOOT), GetLastError());
        goto out;
    }
    post_log(T(S_LOG_BOOT), fs_name(bootsec_fs(s)));
    if (bootsec_is_bootable(s)) {
        post_log(T(S_LOG_BOOT_ALREADY));
        ok = 1;
    } else {
        if (bootsec_mark(s) != 0) {
            post_log(T(S_ERR_VERIFY));
            goto out;
        }
        SetFilePointer(h, 0, NULL, FILE_BEGIN);
        if (!WriteFile(h, s, ss, &got, NULL) || got != ss) {
            post_log(T(S_ERR_BOOT), GetLastError());
            goto out;
        }
        FlushFileBuffers(h);
        check = read_sector0(h, &ss);
        if (!check || memcmp(check, s, ss) != 0 || !bootsec_is_bootable(check)) {
            post_log(T(S_ERR_VERIFY));
            goto out;
        }
        post_log(T(S_LOG_BOOT_OK));
        ok = 1;
    }
out:
    PostMessageW(g_wnd, WM_APP_PROGRESS, 100, 0);
    DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &got, NULL);
    if (ok && DeviceIoControl(h, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &got, NULL))
        post_log(T(S_LOG_EJECT));
    DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &got, NULL);
    CloseHandle(h);
    if (s)
        VirtualFree(s, 0, MEM_RELEASE);
    if (check)
        VirtualFree(check, 0, MEM_RELEASE);
    return ok;
}

/* Files-only install: just flush and eject so the card can be pulled. */
static void eject(wchar_t letter)
{
    HANDLE h = open_volume(letter, GENERIC_READ | GENERIC_WRITE);
    DWORD got;
    if (h == INVALID_HANDLE_VALUE)
        return;
    FlushFileBuffers(h);
    if (DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &got, NULL)) {
        DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &got, NULL);
        if (DeviceIoControl(h, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &got, NULL))
            post_log(T(S_LOG_EJECT));
        DeviceIoControl(h, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &got, NULL);
    }
    CloseHandle(h);
    PostMessageW(g_wnd, WM_APP_PROGRESS, 100, 0);
}

static DWORD WINAPI install_thread(void *arg)
{
    job *j = arg;
    wchar_t chdk[16], logpath[32];
    uint8_t *log = NULL;
    DWORD loglen = 0;
    int ok = 0, zip_has_log = 0;

    fmt(chdk, 16, L"%lc:\\CHDK", j->letter);
    fmt(logpath, 32, L"%lc:\\CHDK\\FILMLOG.CSV", j->letter);

    for (int i = 0; i < g_zip.count; i++)
        if (_stricmp(g_zip.entries[i].name, "CHDK/FILMLOG.CSV") == 0)
            zip_has_log = 1;

    /* keep the shot log */
    if (!zip_has_log) {
        HANDLE h = CreateFileW(logpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        LARGE_INTEGER size;
        if (h != INVALID_HANDLE_VALUE) {
            if (GetFileSizeEx(h, &size) && size.QuadPart < 64 * 1024 * 1024 &&
                (log = malloc((size_t)size.QuadPart + 1)) &&
                ReadFile(h, log, (DWORD)size.QuadPart, &loglen, NULL) && loglen == size.QuadPart) {
                post_log(T(S_LOG_KEEP_LOG));
            } else {
                free(log);
                log = NULL;
            }
            CloseHandle(h);
        }
    }

    if (GetFileAttributesW(chdk) != INVALID_FILE_ATTRIBUTES) {
        post_log(T(S_LOG_DELETE));
        if (!delete_tree(chdk))
            goto out;
    }
    if (!unpack(j->letter))
        goto out;
    if (log && !write_file(logpath, log, loglen))
        goto out;
    /* left over from "Check firmware"; DISKBOOT.BIN was just replaced by unpack() */
    for (int i = 0; i < 2; i++) {
        wchar_t req[32];
        fmt(req, 32, L"%lc:\\%ls", j->letter, i ? L"VERS.REQ" : L"VER.REQ");
        SetFileAttributesW(req, FILE_ATTRIBUTE_NORMAL);
        DeleteFileW(req);
    }

    if (j->make_bootable) {
        if (!make_bootable(j->letter))
            goto out;
    } else {
        eject(j->letter);
    }
    ok = 1;
out:
    free(log);
    PostMessageW(g_wnd, WM_APP_DONE, ok, j->make_bootable);
    free(j);
    return 0;
}

static void start_install(void)
{
    drive_info *d = selected_drive();
    wchar_t msg[512], what[128], size[32];
    job *j;
    HANDLE t;

    if (!d || !g_zip_ok || (g_check != CHECK_OK && g_check != CHECK_FILES_ONLY))
        return;
    fmt_size(size, 32, d->total);
    fmt(what, 128, L"%lc: (%ls, %ls)", d->letter, d->label[0] ? d->label : L"SD", size);
    fmt(msg, 512, T(S_CONFIRM), what);
    if (MessageBoxW(g_wnd, msg, T(S_CONFIRM_TITLE), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    j = malloc(sizeof(*j));
    j->letter = d->letter;
    j->make_bootable = g_check == CHECK_OK;
    g_busy = 1;
    SetWindowTextW(g_log, L"");
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
    update_state();
    t = CreateThread(NULL, 0, install_thread, j, 0, NULL);
    if (t)
        CloseHandle(t);
    else {
        free(j);
        g_busy = 0;
        update_state();
    }
}

/* ---- firmware test (worker thread) -------------------------------------- */

/* The probe for this camera, embedded as RCDATA named after the platform
 * (see Makefile). NULL if the installer was built without it. */
static const uint8_t *probe_data(const camera *cam, DWORD *size)
{
    wchar_t name[64];
    HRSRC r;
    HGLOBAL g;
    MultiByteToWideChar(CP_UTF8, 0, cam->platform, -1, name, 64);
    CharUpperW(name);
    if (!(r = FindResourceW(NULL, name, (LPCWSTR)RT_RCDATA)) || !(g = LoadResource(NULL, r)))
        return NULL;
    *size = SizeofResource(NULL, r);
    return *size ? LockResource(g) : NULL;
}

typedef struct {
    wchar_t letter;
    const uint8_t *probe;   /* NULL: only VER.REQ / VERS.REQ */
    DWORD probe_size;
} probe_job;

static DWORD WINAPI probe_thread(void *arg)
{
    probe_job *j = arg;
    static const char req[] = "EXCCD: playback mode, hold FUNC/SET, press DISP.\r\n";
    wchar_t path[32];
    int ok = 0, boot = j->probe != NULL;

    if (boot) {
        post_log(T(S_LOG_PROBE));
        fmt(path, 32, L"%lc:\\DISKBOOT.BIN", j->letter);
        if (!write_file(path, j->probe, j->probe_size))
            goto out;
    }
    PostMessageW(g_wnd, WM_APP_PROGRESS, 40, 0);
    /* older cameras answer VER.REQ, newer ones VERS.REQ */
    post_log(T(S_LOG_REQ));
    fmt(path, 32, L"%lc:\\VER.REQ", j->letter);
    if (!write_file(path, (const uint8_t *)req, sizeof(req) - 1))
        goto out;
    fmt(path, 32, L"%lc:\\VERS.REQ", j->letter);
    if (!write_file(path, (const uint8_t *)req, sizeof(req) - 1))
        goto out;
    PostMessageW(g_wnd, WM_APP_PROGRESS, 80, 0);
    if (boot) {
        if (!make_bootable(j->letter))
            goto out;
    } else {
        eject(j->letter);
    }
    ok = 1;
out:
    PostMessageW(g_wnd, WM_APP_PROBE_DONE, ok, boot);
    free(j);
    return 0;
}

static const camera *g_probe_cam;           /* camera of the running test */

static void start_probe(void)
{
    const camera *cam = selected_camera();
    drive_info *d = selected_drive();
    wchar_t msg[512], what[128], size[32];
    probe_job *j;
    HANDLE t;
    int check;

    if (!cam || !d) {
        MessageBoxW(g_wnd, T(S_PROBE_NEED_CAM), T(S_PROBE_TITLE), MB_OK | MB_ICONINFORMATION);
        return;
    }
    check = d->readable ? check_card(cam, d->fs) : CHECK_BAD_FS;
    if (check != CHECK_OK && check != CHECK_FILES_ONLY && check != CHECK_NEED_FAT16) {
        MessageBoxW(g_wnd, T(S_PROBE_BAD_CARD), T(S_PROBE_TITLE), MB_OK | MB_ICONWARNING);
        return;
    }
    fmt_size(size, 32, d->total);
    fmt(what, 128, L"%lc: (%ls, %ls)", d->letter, d->label[0] ? d->label : L"SD", size);
    fmt(msg, 512, T(S_PROBE_CONFIRM), what);
    if (MessageBoxW(g_wnd, msg, T(S_PROBE_TITLE), MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;

    j = calloc(1, sizeof(*j));
    j->letter = d->letter;
    /* the blinking test needs a card the camera boots from */
    if (check == CHECK_OK)
        j->probe = probe_data(cam, &j->probe_size);
    g_probe_cam = cam;
    g_busy = 1;
    SetWindowTextW(g_log, L"");
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
    update_state();
    t = CreateThread(NULL, 0, probe_thread, j, 0, NULL);
    if (t)
        CloseHandle(t);
    else {
        free(j);
        g_busy = 0;
        update_state();
    }
}

/* "1× long + 2× short  =  1.01b" for a version like "1.01b" */
static void probe_blinks(wchar_t *buf, const char *v)
{
    wchar_t part[32];
    int middle = (v[2] - '0') * 10 + (v[3] - '0'), letter = v[4] - 'a' + 1;
    buf[0] = 0;
    if (middle) {
        fmt(part, 32, T(S_PROBE_LONG), middle);
        wcscat(buf, part);
    }
    fmt(part, 32, T(S_PROBE_SHORT), letter);
    wcscat(buf, part);
}

static void probe_done(int ok, int blink)
{
    const camera *cam = g_probe_cam;
    wchar_t msg[2048], name[64], fw[16], table[512] = L"", blinks[64], line[160];

    if (!ok || !cam) {
        append_log(T(S_FAILED));
        MessageBoxW(g_wnd, T(S_FAILED), T(S_PROBE_TITLE), MB_OK | MB_ICONERROR);
        return;
    }
    MultiByteToWideChar(CP_UTF8, 0, cam->name, -1, name, 64);
    MultiByteToWideChar(CP_UTF8, 0, cam->fw, -1, fw, 16);
    if (blink) {
        /* one row per version CHDK knows for this camera */
        for (const char *v = cam->known_fw; *v; v += v[5] ? 6 : 5) {
            int own = strncmp(v, cam->fw, 5) == 0;
            probe_blinks(blinks, v);
            fmt(line, 160, L"%ls%ls  =  %.5hs   %ls %ls", table[0] ? L"\n" : L"", blinks, v,
                own ? L"✓" : L"✗", T(own ? S_PROBE_OK : S_PROBE_NO));
            wcscat(table, line);
        }
        fmt(msg, 2048, T(S_PROBE_DONE_BLINK), name, table, T(S_PROBE_REQ_HOWTO));
    } else {
        fmt(msg, 2048, T(S_PROBE_DONE_REQ), name, T(S_PROBE_REQ_HOWTO), fw);
    }
    MessageBoxW(g_wnd, msg, T(S_PROBE_TITLE), MB_OK | MB_ICONINFORMATION);
}

/* ---- window ------------------------------------------------------------- */

static int g_dpi = 96;
#define S(x) MulDiv((x), g_dpi, 96)

static HWND make(const wchar_t *cls, const wchar_t *text, DWORD style, int id, HFONT font)
{
    HWND h = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, g_wnd,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(h, WM_SETFONT, (WPARAM)font, TRUE);
    return h;
}

static void layout(int w)
{
    int m = S(16), x = m, y = m, inner = w - 2 * m, bw = S(110), h = S(24), gap = S(8);

    MoveWindow(g_zip_label, x, y, inner - S(100) - gap, S(20), TRUE);
    MoveWindow(g_lang, x + inner - S(100), y - S(4), S(100), S(200), TRUE); y += S(24);
    MoveWindow(g_zip_edit, x, y, inner - bw - gap, h, TRUE);
    MoveWindow(g_browse, x + inner - bw, y, bw, h, TRUE);         y += h + gap;
    MoveWindow(g_cam_label, x, y + S(3), S(70), S(20), TRUE);
    MoveWindow(g_camera, x + S(70), y, inner - S(70) - S(130) - gap, S(200), TRUE);
    MoveWindow(g_probe, x + inner - S(130), y, S(130), h, TRUE); y += h + S(18);

    MoveWindow(g_card_label, x, y, inner, S(20), TRUE);          y += S(24);
    MoveWindow(g_drive, x, y, inner - bw - gap, S(200), TRUE);
    MoveWindow(g_refresh, x + inner - bw, y, bw, h, TRUE);        y += h + gap;
    MoveWindow(g_status, x, y, inner, S(52), TRUE);               y += S(52) + gap;

    MoveWindow(g_install, x, y, inner, S(34), TRUE);              y += S(34) + gap;
    MoveWindow(g_progress, x, y, inner, S(10), TRUE);             y += S(10) + gap;
    MoveWindow(g_log, x, y, inner, S(120), TRUE);
}

static void create_controls(void)
{
    NONCLIENTMETRICSW ncm = {0};
    LOGFONTW lf;

    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    lf = ncm.lfMessageFont;
    lf.lfHeight = -S(13);
    g_font = CreateFontIndirectW(&lf);
    lf.lfWeight = FW_BOLD;
    g_bold = CreateFontIndirectW(&lf);

    g_zip_label = make(L"STATIC", T(S_STEP1), 0, 0, g_bold);
    g_lang = make(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, ID_LANG, g_font);
    SendMessageW(g_lang, CB_ADDSTRING, 0, (LPARAM)L"English");
    SendMessageW(g_lang, CB_ADDSTRING, 0, (LPARAM)L"Deutsch");
    SendMessageW(g_lang, CB_SETCURSEL, str == str_de, 0);
    g_zip_edit = make(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, ID_ZIP_EDIT, g_font);
    g_browse = make(L"BUTTON", T(S_BROWSE), WS_TABSTOP | BS_PUSHBUTTON, ID_BROWSE, g_font);
    g_cam_label = make(L"STATIC", T(S_CAMERA), 0, 0, g_font);
    g_camera = make(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, ID_CAMERA, g_font);
    g_probe = make(L"BUTTON", T(S_PROBE), WS_TABSTOP | BS_PUSHBUTTON, ID_PROBE, g_font);
    g_card_label = make(L"STATIC", T(S_STEP2), 0, 0, g_bold);
    g_drive = make(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, ID_DRIVE, g_font);
    g_refresh = make(L"BUTTON", T(S_REFRESH), WS_TABSTOP | BS_PUSHBUTTON, ID_REFRESH, g_font);
    g_status = make(L"STATIC", L"", SS_LEFT, ID_STATUS, g_font);
    g_install = make(L"BUTTON", T(S_INSTALL), WS_TABSTOP | BS_DEFPUSHBUTTON, ID_INSTALL, g_bold);
    g_progress = make(PROGRESS_CLASSW, L"", 0, ID_PROGRESS, g_font);
    g_log = make(L"EDIT", L"", WS_BORDER | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                 ID_LOG, g_font);

    SendMessageW(g_camera, CB_ADDSTRING, 0, (LPARAM)T(S_CAMERA_PICK));
    for (int i = 0; i < camera_count; i++) {
        wchar_t name[64];
        MultiByteToWideChar(CP_UTF8, 0, cameras[i].name, -1, name, 64);
        SendMessageW(g_camera, CB_ADDSTRING, 0, (LPARAM)name);
    }
    SendMessageW(g_camera, CB_SETCURSEL, 0, 0);
}

static void set_title(void)
{
    wchar_t title[96];
    fmt(title, 96, L"%ls  %hs", T(S_TITLE), EXCCD_INSTALLER_VERSION);
    SetWindowTextW(g_wnd, title);
}

/* Language switch: put every fixed text in the other language. The log keeps
 * what it already says. */
static void set_language(int german)
{
    LRESULT cam = SendMessageW(g_camera, CB_GETCURSEL, 0, 0);
    str = german ? str_de : str_en;
    set_title();
    SetWindowTextW(g_zip_label, T(S_STEP1));
    SetWindowTextW(g_browse, T(S_BROWSE));
    SetWindowTextW(g_cam_label, T(S_CAMERA));
    SetWindowTextW(g_probe, T(S_PROBE));
    SetWindowTextW(g_card_label, T(S_STEP2));
    SetWindowTextW(g_refresh, T(S_REFRESH));
    SetWindowTextW(g_install, T(S_INSTALL));
    SendMessageW(g_camera, CB_DELETESTRING, 0, 0);
    SendMessageW(g_camera, CB_INSERTSTRING, 0, (LPARAM)T(S_CAMERA_PICK));
    SendMessageW(g_camera, CB_SETCURSEL, cam, 0);
    refresh_drives();                       /* sizes use , or . and update_state() */
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_CREATE:
        g_wnd = hwnd;
        create_controls();
        DragAcceptFiles(hwnd, TRUE);
        /* allow drag & drop from a non-elevated Explorer */
        ChangeWindowMessageFilterEx(hwnd, WM_DROPFILES, MSGFLT_ALLOW, NULL);
        ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, NULL);
        ChangeWindowMessageFilterEx(hwnd, 0x0049 /* WM_COPYGLOBALDATA */, MSGFLT_ALLOW, NULL);
        return 0;
    case WM_SIZE:
        layout(LOWORD(lp));
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BROWSE: browse_zip(); break;
        case ID_REFRESH: refresh_drives(); break;
        case ID_INSTALL: start_install(); break;
        case ID_PROBE: start_probe(); break;
        case ID_LANG:
            if (HIWORD(wp) == CBN_SELCHANGE)
                set_language(SendMessageW(g_lang, CB_GETCURSEL, 0, 0) == 1);
            break;
        case ID_CAMERA:
        case ID_DRIVE:
            if (HIWORD(wp) == CBN_SELCHANGE)
                update_state();
            break;
        }
        return 0;
    case WM_DROPFILES: {
        wchar_t file[MAX_PATH];
        if (!g_busy && DragQueryFileW((HDROP)wp, 0, file, MAX_PATH))
            load_zip(file);
        DragFinish((HDROP)wp);
        return 0;
    }
    case WM_DEVICECHANGE:
        if (wp == DBT_DEVICEARRIVAL || wp == DBT_DEVICEREMOVECOMPLETE)
            SetTimer(hwnd, 1, 700, NULL);   /* let the volume settle */
        return TRUE;
    case WM_TIMER:
        KillTimer(hwnd, 1);
        refresh_drives();
        return 0;
    case WM_APP_LOG:
        append_log((wchar_t *)lp);
        free((void *)lp);
        return 0;
    case WM_APP_PROGRESS:
        SendMessageW(g_progress, PBM_SETPOS, wp, 0);
        return 0;
    case WM_APP_DONE:
        g_busy = 0;
        if (wp) {
            MessageBoxW(hwnd, T(lp ? S_DONE : S_DONE_FILES_ONLY), T(S_TITLE), MB_OK | MB_ICONINFORMATION);
        } else {
            append_log(T(S_FAILED));
            MessageBoxW(hwnd, T(S_FAILED), T(S_TITLE), MB_OK | MB_ICONERROR);
        }
        refresh_drives();
        return 0;
    case WM_APP_PROBE_DONE:
        g_busy = 0;
        probe_done((int)wp, (int)lp);
        refresh_drives();
        return 0;
    case WM_CLOSE:
        if (g_busy)
            return 0;                       /* don't leave a half-written card */
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    WNDCLASSEXW wc = {0};
    wchar_t title[96];
    RECT r;
    HDC dc;
    MSG m;
    int argc;
    wchar_t **argv;
    (void)prev;
    (void)cmd;

    if (PRIMARYLANGID(GetUserDefaultUILanguage()) == LANG_GERMAN)
        str = str_de;
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    dc = GetDC(NULL);
    g_dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);

    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"EXCCDInstaller";
    RegisterClassExW(&wc);

    r.left = 0; r.top = 0; r.right = S(560); r.bottom = S(452);
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    fmt(title, 96, L"%ls  %hs", T(S_TITLE), EXCCD_INSTALLER_VERSION);
    CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                    CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    ShowWindow(g_wnd, show);

    /* a ZIP given on the command line (or dropped on the .exe) wins */
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1)
        load_zip(argv[1]);
    else
        find_download();
    LocalFree(argv);
    refresh_drives();

    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(g_wnd, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return 0;
}
