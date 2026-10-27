#ifndef EMBER_DESKTOP_H
#define EMBER_DESKTOP_H

#include "gfx.h"

/* Theme: dark green with a bright mint accent. */
#define UI_WALL_TOP     0x0C2218
#define UI_WALL_BOT     0x040A07
#define UI_BODY         0x07110B
#define UI_TERM_BG      0x050C08
#define UI_TITLE        0x10261A
#define UI_TITLE_FOCUS  0x1A4A30
#define UI_BORDER       0x23523B
#define UI_STRIP        0x0A1810
#define UI_ACCENT       0x3DDC84
#define UI_TEXT         0xD4EBDD
#define UI_DIM          0x6FA588
#define UI_RED          0xFF5555

#define TITLE_H 26
#define STRIP_H 30

struct window {
    int x, y, w, h;
    char title[28];
    bool open;
    /* The content area starts below the title bar; cx/cy is its top-left. */
    void (*draw)(struct window *win, int cx, int cy, int cw, int ch);
    void (*key)(struct window *win, int key);
    void (*click)(struct window *win, int mx, int my);  /* content-relative */
    void (*closed)(struct window *win);
};

void wm_add(struct window *w);     /* opens the window, centered-ish, on top */
void wm_raise(struct window *w);
void wm_close(struct window *w);
void wm_invalidate(void);

/* apps.c */
void apps_install_console(void);   /* routes console output to the terminal window */
void app_terminal_open(void);
void app_files_open(void);
void app_editor_open(const char *file);  /* NULL for a blank untitled file */
void app_editor_new(void);
void app_clock_open(void);
void app_system_open(void);

#endif
