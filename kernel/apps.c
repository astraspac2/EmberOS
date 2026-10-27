#include "desktop.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static void set_title(struct window *w, const char *title) {
    kstrlcpy(w->title, title, sizeof(w->title));
}

/* ======================================================================
 * Terminal: a character grid fed by the console hooks, driven by the shell
 * ====================================================================== */

#define TERM_SCALE 2
#define TERM_CELL  (8 * TERM_SCALE)
#define TERM_PAD   4

struct term_cell {
    char c;
    uint32_t fg;
};

static struct {
    int cols, rows, x, y;
    uint32_t fg;
    struct term_cell *cells;
} term;

static struct window win_term;

static void term_clear_grid(void) {
    if (!term.cells) {
        return;
    }
    for (int i = 0; i < term.cols * term.rows; i++) {
        term.cells[i].c = ' ';
        term.cells[i].fg = UI_TEXT;
    }
    term.x = 0;
    term.y = 0;
}

static void term_newline(void) {
    term.x = 0;
    if (term.y + 1 < term.rows) {
        term.y++;
        return;
    }
    memmove(term.cells, term.cells + term.cols,
            (size_t)(term.rows - 1) * term.cols * sizeof(struct term_cell));
    struct term_cell *last = term.cells + (term.rows - 1) * term.cols;
    for (int i = 0; i < term.cols; i++) {
        last[i].c = ' ';
        last[i].fg = UI_TEXT;
    }
}

static void term_put(char c) {
    if (!term.cells) {
        return;  /* terminal window is closed: drop output */
    }
    wm_invalidate();
    if (c == '\n') {
        term_newline();
    } else if (c == '\r') {
        term.x = 0;
    } else if (c == '\b') {
        if (term.x > 0) {
            term.x--;
        } else if (term.y > 0) {
            term.y--;
            term.x = term.cols - 1;
        }
        term.cells[term.y * term.cols + term.x].c = ' ';
    } else if (c == '\t') {
        int next = (term.x + 4) & ~3;
        while (term.x < next && term.x < term.cols) {
            term.cells[term.y * term.cols + term.x++].c = ' ';
        }
        if (term.x >= term.cols) {
            term_newline();
        }
    } else {
        struct term_cell *cell = &term.cells[term.y * term.cols + term.x];
        cell->c = c;
        cell->fg = term.fg;
        if (++term.x >= term.cols) {
            term_newline();
        }
    }
}

static void term_set_fg(uint32_t rgb) {
    term.fg = rgb;
}

static const struct console_hooks term_hooks = {
    .putc = term_put,
    .set_fg = term_set_fg,
    .clear = term_clear_grid,
};

void apps_install_console(void) {
    console_set_hooks(&term_hooks);
}

static void term_draw(struct window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gfx_fill(cx, cy, cw, ch, UI_TERM_BG);
    if (!term.cells) {
        return;
    }
    for (int r = 0; r < term.rows; r++) {
        for (int c = 0; c < term.cols; c++) {
            struct term_cell *cell = &term.cells[r * term.cols + c];
            if (cell->c > ' ') {
                gfx_char(cx + TERM_PAD + c * TERM_CELL, cy + TERM_PAD + r * TERM_CELL,
                         cell->c, TERM_SCALE, cell->fg);
            }
        }
    }
    if ((timer_ticks() / 50) & 1) {
        gfx_fill(cx + TERM_PAD + term.x * TERM_CELL, cy + TERM_PAD + term.y * TERM_CELL + TERM_CELL - 3,
                 TERM_CELL, 3, UI_ACCENT);
    }
}

static void term_key(struct window *w, int key) {
    (void)w;
    shell_key(key);
}

static void term_closed(struct window *w) {
    (void)w;
    kfree(term.cells);
    term.cells = NULL;
}

void app_terminal_open(void) {
    if (win_term.open) {
        wm_raise(&win_term);
        return;
    }
    memset(&win_term, 0, sizeof(win_term));
    set_title(&win_term, "Terminal");
    win_term.w = 720;
    win_term.h = 440;
    win_term.draw = term_draw;
    win_term.key = term_key;
    win_term.closed = term_closed;

    term.cols = (win_term.w - 2 - 2 * TERM_PAD) / TERM_CELL;
    term.rows = (win_term.h - TITLE_H - 1 - 2 * TERM_PAD) / TERM_CELL;
    term.cells = kmalloc((size_t)term.cols * term.rows * sizeof(struct term_cell));
    if (!term.cells) {
        return;
    }
    term.fg = UI_TEXT;
    term_clear_grid();
    wm_add(&win_term);

    console_set_fg(UI_DIM);
    kprintf("Click the green orb (bottom-left) for apps.\n");
    shell_init();
}

/* ======================================================================
 * Editor
 * ====================================================================== */

#define ED_CAP 16384

static struct window win_ed;
static struct {
    char name[FS_NAME_MAX];
    char *buf;
    size_t len, cursor;
    bool dirty;
    int top;
    char msg[40];
} ed;

static void ed_update_title(void) {
    ksnprintf(win_ed.title, sizeof(win_ed.title), "Editor - %s%s", ed.name, ed.dirty ? "*" : "");
}

static void ed_save(void) {
    if (fs_write(ed.name, ed.buf, ed.len) >= 0) {
        ed.dirty = false;
        kstrlcpy(ed.msg, "saved", sizeof(ed.msg));
    } else {
        kstrlcpy(ed.msg, "save failed (disk full?)", sizeof(ed.msg));
    }
    ed_update_title();
}

static void ed_key(struct window *w, int k) {
    (void)w;
    ed.msg[0] = '\0';

    if (k == 0x13) {  /* Ctrl+S */
        ed_save();
    } else if (k == KEY_LEFT) {
        if (ed.cursor > 0) ed.cursor--;
    } else if (k == KEY_RIGHT) {
        if (ed.cursor < ed.len) ed.cursor++;
    } else if (k == KEY_UP || k == KEY_DOWN) {
        size_t ls = ed.cursor;
        while (ls > 0 && ed.buf[ls - 1] != '\n') ls--;
        size_t col = ed.cursor - ls;

        if (k == KEY_UP) {
            if (ls == 0) return;
            size_t prev_end = ls - 1;
            size_t pls = prev_end;
            while (pls > 0 && ed.buf[pls - 1] != '\n') pls--;
            ed.cursor = pls + MIN(col, prev_end - pls);
        } else {
            size_t le = ed.cursor;
            while (le < ed.len && ed.buf[le] != '\n') le++;
            if (le >= ed.len) return;
            size_t nls = le + 1;
            size_t nle = nls;
            while (nle < ed.len && ed.buf[nle] != '\n') nle++;
            ed.cursor = nls + MIN(col, nle - nls);
        }
    } else if (k == '\b') {
        if (ed.cursor > 0) {
            memmove(ed.buf + ed.cursor - 1, ed.buf + ed.cursor, ed.len - ed.cursor);
            ed.cursor--;
            ed.len--;
            ed.dirty = true;
        }
    } else if ((k >= 0x20 && k <= 0x7E) || k == '\n') {
        if (ed.len < ED_CAP - 1) {
            memmove(ed.buf + ed.cursor + 1, ed.buf + ed.cursor, ed.len - ed.cursor);
            ed.buf[ed.cursor++] = (char)k;
            ed.len++;
            ed.dirty = true;
        }
    }
    ed_update_title();
}

static void ed_draw(struct window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gfx_fill(cx, cy, cw, ch, UI_TERM_BG);

    int cols = (cw - 16) / 16;
    int rows = (ch - 12 - 22) / 16;

    int cursor_line = 0;
    for (size_t i = 0; i < ed.cursor; i++) {
        if (ed.buf[i] == '\n') cursor_line++;
    }
    if (cursor_line < ed.top) ed.top = cursor_line;
    if (cursor_line >= ed.top + rows) ed.top = cursor_line - rows + 1;

    int line = 0, col = 0, cur_line = 0, cur_col = 0;
    for (size_t i = 0; i <= ed.len; i++) {
        if (i == ed.cursor) {
            cur_line = line;
            cur_col = col;
        }
        if (i == ed.len) break;
        char c = ed.buf[i];
        if (c == '\n') {
            line++;
            col = 0;
            continue;
        }
        if (line >= ed.top && line < ed.top + rows && col < cols) {
            gfx_char(cx + 8 + col * 16, cy + 6 + (line - ed.top) * 16, c, 2, UI_TEXT);
        }
        col++;
    }
    gfx_fill(cx + 8 + cur_col * 16, cy + 6 + (cur_line - ed.top) * 16, 2, 16, UI_ACCENT);

    int by = cy + ch - 22;
    gfx_fill(cx, by, cw, 22, 0x0F2A1A);
    char status[96];
    ksnprintf(status, sizeof(status), "Ctrl+S save   Ln %d Col %d   %s",
              cur_line + 1, cur_col + 1, ed.msg);
    gfx_text(cx + 8, by + 7, status, 1, UI_DIM);
}

void app_editor_open(const char *file) {
    if (!ed.buf) {
        ed.buf = kmalloc(ED_CAP);
        if (!ed.buf) {
            return;
        }
    }
    if (win_ed.open && ed.dirty) {
        fs_write(ed.name, ed.buf, ed.len);  /* don't lose edits when switching files */
    }

    kstrlcpy(ed.name, file ? file : "untitled.txt", sizeof(ed.name));
    ed.len = 0;
    int slot = fs_find(ed.name);
    if (slot >= 0) {
        ed.len = MIN(fs_size(slot), (size_t)ED_CAP - 1);
        memcpy(ed.buf, fs_data(slot), ed.len);
    }
    ed.cursor = 0;
    ed.dirty = false;
    ed.top = 0;
    ed.msg[0] = '\0';

    if (win_ed.open) {
        ed_update_title();
        wm_raise(&win_ed);
        return;
    }
    memset(&win_ed, 0, sizeof(win_ed));
    win_ed.w = 640;
    win_ed.h = 420;
    win_ed.draw = ed_draw;
    win_ed.key = ed_key;
    ed_update_title();
    wm_add(&win_ed);
}

void app_editor_new(void) {
    app_editor_open(NULL);
}

/* ======================================================================
 * Files
 * ====================================================================== */

#define FILES_ROW_H   30
#define FILES_TOP     48

static struct window win_files;

static void files_draw(struct window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    (void)ch;
    gfx_fill(cx, cy, cw, ch, UI_BODY);
    gfx_text(cx + 12, cy + 16, "Files (kept in RAM)", 2, UI_DIM);
    gfx_fill(cx + cw - 110, cy + 8, 98, 30, UI_TITLE_FOCUS);
    gfx_text(cx + cw - 98, cy + 15, "+ New", 2, UI_TEXT);

    int row = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        const char *name = fs_name(i);
        if (!name) {
            continue;
        }
        int y = cy + FILES_TOP + row * FILES_ROW_H;
        gfx_fill(cx + 8, y, cw - 16, FILES_ROW_H - 4, row & 1 ? 0x0B1A12 : 0x0E2218);
        gfx_text(cx + 18, y + 6, name, 2, UI_TEXT);
        char size[24];
        ksnprintf(size, sizeof(size), "%lu B", (uint64_t)fs_size(i));
        gfx_text(cx + cw - 60 - gfx_text_width(size, 1) - 90, y + 11, size, 1, UI_DIM);
        gfx_text(cx + cw - 40, y + 6, "x", 2, UI_RED);
        row++;
    }
}

static void files_click(struct window *w, int mx, int my) {
    (void)w;
    int cw = win_files.w - 2;

    if (my >= 8 && my < 38 && mx >= cw - 110 && mx < cw - 12) {
        char name[FS_NAME_MAX];
        for (int n = 1; n < 1000; n++) {
            ksnprintf(name, sizeof(name), "new%d.txt", n);
            if (fs_find(name) < 0) {
                break;
            }
        }
        if (fs_write(name, "", 0) >= 0) {
            app_editor_open(name);
        }
        return;
    }
    if (my < FILES_TOP) {
        return;
    }
    int want = (my - FILES_TOP) / FILES_ROW_H;
    int row = 0;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        const char *name = fs_name(i);
        if (!name) {
            continue;
        }
        if (row++ == want) {
            if (mx >= cw - 50) {
                char copy[FS_NAME_MAX];
                kstrlcpy(copy, name, sizeof(copy));
                fs_remove(copy);
            } else {
                app_editor_open(name);
            }
            return;
        }
    }
}

void app_files_open(void) {
    if (win_files.open) {
        wm_raise(&win_files);
        return;
    }
    memset(&win_files, 0, sizeof(win_files));
    set_title(&win_files, "Files");
    win_files.w = 460;
    win_files.h = 400;
    win_files.draw = files_draw;
    win_files.click = files_click;
    wm_add(&win_files);
}

/* ======================================================================
 * Clock
 * ====================================================================== */

static struct window win_clock;

static void clock_draw(struct window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gfx_fill(cx, cy, cw, ch, UI_BODY);

    struct rtc_time t;
    rtc_read(&t);
    char text[24];

    ksnprintf(text, sizeof(text), "%02d:%02d:%02d", t.hour, t.minute, t.second);
    gfx_text(cx + (cw - gfx_text_width(text, 6)) / 2, cy + 36, text, 6, UI_ACCENT);

    ksnprintf(text, sizeof(text), "%04d-%02d-%02d", t.year, t.month, t.day);
    gfx_text(cx + (cw - gfx_text_width(text, 3)) / 2, cy + 110, text, 3, UI_TEXT);

    uint64_t s = timer_ticks() / timer_hz();
    ksnprintf(text, sizeof(text), "up %luh %lum %lus", s / 3600, (s / 60) % 60, s % 60);
    gfx_text(cx + (cw - gfx_text_width(text, 2)) / 2, cy + 160, text, 2, UI_DIM);
}

void app_clock_open(void) {
    if (win_clock.open) {
        wm_raise(&win_clock);
        return;
    }
    memset(&win_clock, 0, sizeof(win_clock));
    set_title(&win_clock, "Clock");
    win_clock.w = 440;
    win_clock.h = 240;
    win_clock.draw = clock_draw;
    wm_add(&win_clock);
}

/* ======================================================================
 * System
 * ====================================================================== */

static struct window win_sys;

static void sys_draw(struct window *w, int cx, int cy, int cw, int ch) {
    (void)w;
    gfx_fill(cx, cy, cw, ch, UI_BODY);

    char line[64];
    gfx_text(cx + 16, cy + 16, "EmberOS 0.4", 3, UI_ACCENT);

    ksnprintf(line, sizeof(line), "Screen  %dx%d", gfx_width(), gfx_height());
    gfx_text(cx + 16, cy + 62, line, 2, UI_TEXT);

    uint64_t total = pmm_total_pages() * PAGE_SIZE / (1024 * 1024);
    uint64_t freem = pmm_free_pages() * PAGE_SIZE / (1024 * 1024);
    ksnprintf(line, sizeof(line), "Memory  %lu of %lu MiB used", total - freem, total);
    gfx_text(cx + 16, cy + 96, line, 2, UI_TEXT);

    int bar_w = cw - 32;
    gfx_fill(cx + 16, cy + 128, bar_w, 18, 0x0B1A12);
    if (total) {
        gfx_fill(cx + 16, cy + 128, (int)((total - freem) * bar_w / total), 18, UI_ACCENT);
    }
    gfx_rect(cx + 16, cy + 128, bar_w, 18, UI_BORDER);

    ksnprintf(line, sizeof(line), "Heap    %lu KiB free", (uint64_t)heap_free_bytes() / 1024);
    gfx_text(cx + 16, cy + 164, line, 2, UI_TEXT);

    uint64_t s = timer_ticks() / timer_hz();
    ksnprintf(line, sizeof(line), "Uptime  %luh %lum %lus", s / 3600, (s / 60) % 60, s % 60);
    gfx_text(cx + 16, cy + 198, line, 2, UI_TEXT);
}

void app_system_open(void) {
    if (win_sys.open) {
        wm_raise(&win_sys);
        return;
    }
    memset(&win_sys, 0, sizeof(win_sys));
    set_title(&win_sys, "System");
    win_sys.w = 480;
    win_sys.h = 290;
    win_sys.draw = sys_draw;
    wm_add(&win_sys);
}
