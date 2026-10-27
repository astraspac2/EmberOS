#include "kernel.h"
#include "desktop.h"

#define LINE_MAX 256
#define HISTORY  16
#define MAX_ARGS 16

static char line[LINE_MAX];
static size_t len;
static char history[HISTORY][LINE_MAX];
static int hist_count;
static int browse;  /* index into history while pressing up/down */

/* ---- commands ---- */

typedef void (*cmd_fn)(int argc, char **argv);

struct command {
    const char *name;
    const char *help;
    cmd_fn run;
};

static const struct command commands[];

static void cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    for (const struct command *c = commands; c->name; c++) {
        console_set_fg(COLOR_ACCENT);
        kprintf("  %s", c->name);
        for (size_t i = strlen(c->name); i < 10; i++) {
            console_putc(' ');
        }
        console_set_fg(COLOR_FG);
        kprintf("%s\n", c->help);
    }
}

static void cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    console_clear();
}

static void cmd_echo(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s%s", i > 1 ? " " : "", argv[i]);
    }
    kprintf("\n");
}

static void cmd_mem(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("memory: %lu MiB usable, %lu MiB free\n",
            pmm_total_pages() * PAGE_SIZE / (1024 * 1024),
            pmm_free_pages() * PAGE_SIZE / (1024 * 1024));
    kprintf("heap:   %lu KiB free\n", (uint64_t)heap_free_bytes() / 1024);
}

static void cmd_uptime(int argc, char **argv) {
    (void)argc;
    (void)argv;
    uint64_t s = timer_ticks() / timer_hz();
    kprintf("up %luh %lum %lus\n", s / 3600, (s / 60) % 60, s % 60);
}

static void cmd_version(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("EmberOS 0.4 (x86-64)\n");
}

static void cmd_about(int argc, char **argv) {
    (void)argc;
    (void)argv;
    console_set_fg(COLOR_ACCENT);
    kprintf("      (\n     ) )\n    ( ( )\n   (  )  )\n    \\___/\n");
    console_set_fg(COLOR_FG);
    kprintf("EmberOS: a hobby OS written from scratch in C.\n");
}

void system_reboot(void) {
    __asm__ volatile ("cli");
    for (int i = 0; i < 100000 && (inb(0x64) & 0x02); i++) {
    }
    outb(0x64, 0xFE);  /* pulse the CPU reset line via the keyboard controller */

    /* Fallback: load an empty IDT and fault until the CPU resets. */
    struct { uint16_t limit; uint64_t base; } __attribute__((packed)) empty = { 0, 0 };
    __asm__ volatile ("lidt %0; int3" : : "m"(empty));
    cpu_halt_forever();
}

static void cmd_reboot(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("Rebooting...\n");
    system_reboot();
}

static void cmd_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (fs_name(i)) {
            kprintf("  %s", fs_name(i));
            for (size_t n = strlen(fs_name(i)); n < 26; n++) {
                console_putc(' ');
            }
            kprintf("%lu B\n", (uint64_t)fs_size(i));
        }
    }
}

static void cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: cat <file>\n");
        return;
    }
    int slot = fs_find(argv[1]);
    if (slot < 0) {
        kprintf("no such file: %s\n", argv[1]);
        return;
    }
    const char *d = fs_data(slot);
    for (size_t i = 0; i < fs_size(slot); i++) {
        console_putc(d[i]);
    }
    if (fs_size(slot) && d[fs_size(slot) - 1] != '\n') {
        console_putc('\n');
    }
}

static void cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: rm <file>\n");
        return;
    }
    if (fs_remove(argv[1]) < 0) {
        kprintf("no such file: %s\n", argv[1]);
    }
}

static void cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        kprintf("usage: touch <file>\n");
        return;
    }
    if (fs_find(argv[1]) < 0 && fs_write(argv[1], "", 0) < 0) {
        kprintf("could not create %s\n", argv[1]);
    }
}

static void cmd_write(int argc, char **argv) {
    if (argc < 3) {
        kprintf("usage: write <file> <text...>\n");
        return;
    }
    char text[LINE_MAX];
    size_t n = 0;
    for (int i = 2; i < argc; i++) {
        n += kstrlcpy(text + n, argv[i], sizeof(text) - n);
        if (i + 1 < argc && n + 1 < sizeof(text)) {
            text[n++] = ' ';
        }
    }
    text[n++] = '\n';
    if (fs_write(argv[1], text, n) < 0) {
        kprintf("could not write %s\n", argv[1]);
    }
}

static void cmd_desktop(int argc, char **argv);

static void cmd_halt(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("System halted. You can power off the VM now.\n");
    cpu_halt_forever();
}

static const struct command commands[] = {
    { "help",    "list commands",               cmd_help },
    { "clear",   "clear the screen",            cmd_clear },
    { "echo",    "print the arguments",         cmd_echo },
    { "mem",     "show memory usage",           cmd_mem },
    { "uptime",  "time since boot",             cmd_uptime },
    { "version", "show the OS version",         cmd_version },
    { "about",   "about EmberOS",               cmd_about },
    { "ls",      "list files",                  cmd_ls },
    { "cat",     "print a file",                cmd_cat },
    { "touch",   "create an empty file",        cmd_touch },
    { "write",   "write <file> <text>",         cmd_write },
    { "rm",      "delete a file",               cmd_rm },
    { "desktop", "start the graphical desktop", cmd_desktop },
    { "reboot",  "restart the machine",         cmd_reboot },
    { "halt",    "stop the CPU",                cmd_halt },
    { NULL, NULL, NULL },
};

static void execute(char *text) {
    char *argv[MAX_ARGS];
    int argc = 0;

    char *p = text;
    while (*p && argc < MAX_ARGS) {
        while (*p == ' ') {
            *p++ = '\0';
        }
        if (!*p) {
            break;
        }
        argv[argc++] = p;
        while (*p && *p != ' ') {
            p++;
        }
    }
    if (argc == 0) {
        return;
    }

    for (const struct command *c = commands; c->name; c++) {
        if (strcmp(argv[0], c->name) == 0) {
            c->run(argc, argv);
            return;
        }
    }
    console_set_fg(COLOR_ERR);
    kprintf("unknown command: %s (try 'help')\n", argv[0]);
    console_set_fg(COLOR_FG);
}

extern struct limine_framebuffer *boot_framebuffer;

static void cmd_desktop(int argc, char **argv) {
    (void)argc;
    (void)argv;
    kprintf("Starting desktop... (use Restart in the menu to leave)\n");
    desktop_run(boot_framebuffer);
    kprintf("could not start the desktop (graphics setup failed)\n");
}

/* ---- line editing ---- */

static void prompt(void) {
    console_set_fg(COLOR_ACCENT);
    kprintf("ember");
    console_set_fg(COLOR_DIM);
    kprintf("> ");
    console_set_fg(COLOR_FG);
}

static void replace_line(const char *text) {
    while (len > 0) {
        console_putc('\b');
        len--;
    }
    while (*text && len < LINE_MAX - 1) {
        line[len++] = *text;
        console_putc(*text++);
    }
}

static void remember(void) {
    if (len == 0 || (hist_count > 0 && strcmp(history[hist_count - 1], line) == 0)) {
        browse = hist_count;
        return;
    }
    if (hist_count == HISTORY) {
        for (int i = 1; i < HISTORY; i++) {
            memcpy(history[i - 1], history[i], LINE_MAX);
        }
        hist_count--;
    }
    memcpy(history[hist_count++], line, len + 1);
    browse = hist_count;
}

void shell_init(void) {
    len = 0;
    browse = hist_count;
    console_set_fg(COLOR_FG);
    kprintf("Welcome to EmberOS. Type 'help' for a list of commands.\n\n");
    prompt();
}

void shell_key(int key) {
    if (key == '\n') {
        console_putc('\n');
        line[len] = '\0';
        remember();
        execute(line);
        len = 0;
        prompt();
    } else if (key == '\b') {
        if (len > 0) {
            len--;
            console_putc('\b');
        }
    } else if (key == KEY_UP) {
        if (browse > 0) {
            browse--;
            replace_line(history[browse]);
        }
    } else if (key == KEY_DOWN) {
        if (browse < hist_count) {
            browse++;
            replace_line(browse == hist_count ? "" : history[browse]);
        }
    } else if (key >= 0x20 && key <= 0x7E) {
        if (len < LINE_MAX - 1) {
            line[len++] = (char)key;
            console_putc((char)key);
        }
    }
}
