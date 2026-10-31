#include "desktop.h"

#define MAX_WINDOWS 8
#define ORB_CORE    14
#define MENU_ITEM_H 38
#define MENU_W      230
#define MENU_X      12

static struct window *z[MAX_WINDOWS];  /* back to front */
static int nz;
static struct window *dragging;
static int drag_dx, drag_dy;

static int W, H;
static int mx, my;
static bool prev_left;
static bool menu_open;
static bool dirty = true;

static struct rtc_time now_time;
static uint64_t time_sec = (uint64_t)-1;

struct menu_item {
    const char *label;
    void (*action)(void);
};

static void menu_reboot(void) {
    system_reboot();
}

static const struct menu_item menu[] = {
    { "Terminal", app_terminal_open },
    { "Files",    app_files_open },
    { "Editor",   app_editor_new },
    { "Clock",    app_clock_open },
    { "System",   app_system_open },
    { "Restart",  menu_reboot },
};
#define MENU_COUNT ((int)(sizeof(menu) / sizeof(menu[0])))

/* ---- window list ---- */

void wm_invalidate(void) {
    dirty = true;
}

static int z_index(struct window *w) {
    for (int i = 0; i < nz; i++) {
        if (z[i] == w) {
            return i;
        }
    }
    return -1;
}

void wm_add(struct window *w) {
    if (nz >= MAX_WINDOWS) {
        return;
    }
    if (w->w > W) w->w = W - 20;
    if (w->h > H - STRIP_H - 10) w->h = H - STRIP_H - 10;
    w->x = 60 + 36 * nz;
    w->y = STRIP_H + 24 + 30 * nz;
    if (w->x + w->w > W) w->x = W - w->w;
    if (w->y + w->h > H) w->y = H - w->h;
    if (w->x < 0) w->x = 0;
    w->open = true;
    z[nz++] = w;
    dirty = true;
}

void wm_raise(struct window *w) {
    int i = z_index(w);
    if (i < 0) {
        return;
    }
    for (; i + 1 < nz; i++) {
        z[i] = z[i + 1];
    }
    z[nz - 1] = w;
    dirty = true;
}

void wm_close(struct window *w) {
    int i = z_index(w);
    if (i < 0) {
        return;
    }
    for (; i + 1 < nz; i++) {
        z[i] = z[i + 1];
    }
    nz--;
    w->open = false;
    if (dragging == w) {
        dragging = NULL;
    }
    if (w->closed) {
        w->closed(w);
    }
    dirty = true;
}

/* ---- rendering ---- */

static const char *const cursor_sprite[17] = {
    "X           ",
    "XX          ",
    "XoX         ",
    "XooX        ",
    "XoooX       ",
    "XooooX      ",
    "XoooooX     ",
    "XooooooX    ",
    "XoooooooX   ",
    "XooooooooX  ",
    "XoooooXXXXX ",
    "XooXooX     ",
    "XoX XooX    ",
    "XX  XooX    ",
    "X    XooX   ",
    "     XooX   ",
    "      XX    ",
};

static void draw_cursor(void) {
    gfx_clear_clip();
    for (int row = 0; row < 17; row++) {
        for (int col = 0; col < 12; col++) {
            char c = cursor_sprite[row][col];
            if (c == 'X') {
                gfx_pixel(mx + col, my + row, 0x000000);
            } else if (c == 'o') {
                gfx_pixel(mx + col, my + row, 0xE8FFF0);
            }
        }
    }
}

static void draw_wallpaper(void) {
    gfx_vgradient(0, 0, W, H, UI_WALL_TOP, UI_WALL_BOT);
    int scale = W >= 1000 ? 10 : 6;
    const char *name = "EmberOS";
    gfx_text((W - gfx_text_width(name, scale)) / 2, H / 2 - 4 * scale, name, scale, 0x0E2A1D);
}

static bool in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool close_button_hit(struct window *w) {
    return in_rect(mx, my, w->x + w->w - 30, w->y + 2, 28, TITLE_H - 4);
}

static void draw_window(struct window *w, bool focused) {
    int x = w->x, y = w->y, ww = w->w, hh = w->h;

    gfx_blend(x + 6, y + 8, ww, hh, 0x000000, 110);
    if (focused) {
        gfx_blend(x - 4, y - 4, ww + 8, hh + 8, UI_ACCENT, 26);
        gfx_blend(x - 2, y - 2, ww + 4, hh + 4, UI_ACCENT, 40);
    }
    gfx_fill(x, y, ww, hh, UI_BODY);
    gfx_fill(x, y, ww, TITLE_H, focused ? UI_TITLE_FOCUS : UI_TITLE);
    gfx_rect(x, y, ww, hh, focused ? UI_ACCENT : UI_BORDER);
    gfx_text(x + 10, y + 5, w->title, 2, focused ? UI_TEXT : UI_DIM);
    gfx_text(x + ww - 24, y + 5, "x", 2, close_button_hit(w) ? UI_RED : UI_TEXT);

    gfx_set_clip(x + 1, y + TITLE_H, ww - 2, hh - TITLE_H - 1);
    if (w->draw) {
        w->draw(w, x + 1, y + TITLE_H, ww - 2, hh - TITLE_H - 1);
    }
    gfx_clear_clip();
}

static void chip_rect(int i, int *x, int *w) {
    int cx = 130;
    for (int k = 0; k < i; k++) {
        cx += gfx_text_width(z[k]->title, 2) + 28;
    }
    *x = cx;
    *w = gfx_text_width(z[i]->title, 2) + 20;
}

static void draw_strip(void) {
    gfx_fill(0, 0, W, STRIP_H, UI_STRIP);
    gfx_fill(0, STRIP_H - 1, W, 1, UI_BORDER);
    gfx_text(12, 7, "EmberOS", 2, UI_ACCENT);

    for (int i = 0; i < nz; i++) {
        int x, w;
        chip_rect(i, &x, &w);
        bool top = (i == nz - 1);
        gfx_fill(x, 4, w, STRIP_H - 9, top ? UI_TITLE_FOCUS : UI_TITLE);
        gfx_text(x + 10, 7, z[i]->title, 2, top ? UI_TEXT : UI_DIM);
    }

    uint64_t sec = timer_ticks() / timer_hz();
    if (sec != time_sec) {
        time_sec = sec;
        rtc_read(&now_time);
    }
    char clock[16];
    ksnprintf(clock, sizeof(clock), "%02d:%02d", now_time.hour, now_time.minute);
    gfx_text(W - gfx_text_width(clock, 2) - 14, 7, clock, 2, UI_TEXT);
}

static void orb_center(int *cx, int *cy) {
    *cx = 36;
    *cy = H - 36;
}

static void draw_orb(void) {
    int cx, cy;
    orb_center(&cx, &cy);
    int phase = (int)((timer_ticks() / 5) % 16);
    int tri = phase < 8 ? phase : 16 - phase;
    int glow = (menu_open ? 36 : 26) + tri;
    gfx_glow(cx, cy, ORB_CORE, glow, UI_ACCENT, 200);
    gfx_glow(cx, cy, 5, ORB_CORE - 3, 0xC8FFE0, 255);
}

static int menu_top(void) {
    return H - 72 - MENU_COUNT * MENU_ITEM_H;
}

static void draw_menu(void) {
    int top = menu_top();
    gfx_blend(MENU_X + 5, top + 7, MENU_W, MENU_COUNT * MENU_ITEM_H, 0x000000, 120);
    gfx_fill(MENU_X, top, MENU_W, MENU_COUNT * MENU_ITEM_H, UI_TITLE);
    gfx_rect(MENU_X, top, MENU_W, MENU_COUNT * MENU_ITEM_H, UI_ACCENT);
    for (int i = 0; i < MENU_COUNT; i++) {
        int y = top + i * MENU_ITEM_H;
        bool hover = in_rect(mx, my, MENU_X, y, MENU_W, MENU_ITEM_H);
        if (hover) {
            gfx_blend(MENU_X + 1, y + 1, MENU_W - 2, MENU_ITEM_H - 2, UI_ACCENT, 70);
        }
        gfx_text(MENU_X + 18, y + 11, menu[i].label, 2, hover ? 0xFFFFFF : UI_TEXT);
    }
}

static void render(void) {
    draw_wallpaper();
    for (int i = 0; i < nz; i++) {
        draw_window(z[i], i == nz - 1);
    }
    draw_strip();
    draw_orb();
    if (menu_open) {
        draw_menu();
    }
    draw_cursor();
    gfx_present();
}

/* ---- input ---- */

static void activate_menu(int i) {
    menu_open = false;
    menu[i].action();
    dirty = true;
}

static void handle_press(void) {
    int ocx, ocy;
    orb_center(&ocx, &ocy);
    int odx = mx - ocx, ody = my - ocy;
    bool on_orb = odx * odx + ody * ody <= 30 * 30;

    if (menu_open) {
        int top = menu_top();
        if (in_rect(mx, my, MENU_X, top, MENU_W, MENU_COUNT * MENU_ITEM_H)) {
            activate_menu((my - top) / MENU_ITEM_H);
            return;
        }
        menu_open = false;
        return;
    }
    if (on_orb) {
        menu_open = true;
        return;
    }

    if (my < STRIP_H) {
        for (int i = 0; i < nz; i++) {
            int x, w;
            chip_rect(i, &x, &w);
            if (in_rect(mx, my, x, 4, w, STRIP_H - 9)) {
                wm_raise(z[i]);
                return;
            }
        }
        return;
    }

    for (int i = nz - 1; i >= 0; i--) {
        struct window *w = z[i];
        if (!in_rect(mx, my, w->x, w->y, w->w, w->h)) {
            continue;
        }
        wm_raise(w);
        if (close_button_hit(w)) {
            wm_close(w);
        } else if (my < w->y + TITLE_H) {
            dragging = w;
            drag_dx = mx - w->x;
            drag_dy = my - w->y;
        } else if (w->click) {
            w->click(w, mx - (w->x + 1), my - (w->y + TITLE_H));
        }
        return;
    }
}

static void on_mouse(const struct mouse_event *e) {
    mx = e->x;
    my = e->y;
    bool left = e->buttons & 1;

    if (dragging && left) {
        int nx = mx - drag_dx;
        int ny = my - drag_dy;
        if (nx < 60 - dragging->w) nx = 60 - dragging->w;
        if (nx > W - 60) nx = W - 60;
        if (ny < STRIP_H) ny = STRIP_H;
        if (ny > H - TITLE_H) ny = H - TITLE_H;
        dragging->x = nx;
        dragging->y = ny;
    }
    if (left && !prev_left) {
        handle_press();
    }
    if (!left) {
        dragging = NULL;
    }
    prev_left = left;
    dirty = true;
}

static void on_key(int key) {
    dirty = true;
    if (menu_open) {
        if (key == 27) {
            menu_open = false;
        }
        return;
    }
    if (nz > 0 && z[nz - 1]->key) {
        z[nz - 1]->key(z[nz - 1], key);
    }
}

void desktop_run(struct limine_framebuffer *fb) {
    if (!gfx_init(fb)) {
        return;
    }
    W = gfx_width();
    H = gfx_height();
    mx = W / 2;
    my = H / 2;

    __asm__ volatile ("cli");
    mouse_init(W, H);
    __asm__ volatile ("sti");

    apps_install_console();
    app_terminal_open();

    uint64_t last_frame = (uint64_t)-1;
    for (;;) {
        int k;
        while ((k = keyboard_getchar()) >= 0) {
            on_key(k);
        }
        struct mouse_event e;
        while (mouse_poll(&e)) {
            on_mouse(&e);
        }

        uint64_t frame = timer_ticks() / 5;  /* 20 fps cap, also drives animation */
        if (dirty || frame != last_frame) {
            last_frame = frame;
            dirty = false;
            render();
        }
        __asm__ volatile ("hlt");
    }
}
