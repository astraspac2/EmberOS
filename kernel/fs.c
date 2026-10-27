#include "kernel.h"

/* A tiny RAM file system: a flat list of named files. */

struct file {
    bool used;
    char name[FS_NAME_MAX];
    char *data;
    size_t size;
};

static struct file files[FS_MAX_FILES];

void fs_init(void) {
    static const char readme[] =
        "Welcome to EmberOS!\n\n"
        "This is a text file stored in RAM.\n"
        "Open the Editor, change it, and press Ctrl+S to save.\n"
        "Files live in memory for now and disappear on reboot.\n";
    static const char todo[] =
        "ideas for EmberOS:\n"
        "- a filesystem on disk\n"
        "- more apps\n"
        "- sound\n";
    fs_write("readme.txt", readme, sizeof(readme) - 1);
    fs_write("ideas.txt", todo, sizeof(todo) - 1);
}

int fs_find(const char *name) {
    for (int i = 0; i < FS_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *fs_name(int slot) {
    return files[slot].used ? files[slot].name : NULL;
}

const char *fs_data(int slot) {
    return files[slot].data;
}

size_t fs_size(int slot) {
    return files[slot].size;
}

int fs_write(const char *name, const char *data, size_t len) {
    int slot = fs_find(name);
    if (slot < 0) {
        for (int i = 0; i < FS_MAX_FILES; i++) {
            if (!files[i].used) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            return -1;
        }
    }

    char *copy = kmalloc(len + 1);
    if (!copy) {
        return -1;
    }
    memcpy(copy, data, len);
    copy[len] = '\0';

    if (files[slot].used) {
        kfree(files[slot].data);
    }
    files[slot].used = true;
    kstrlcpy(files[slot].name, name, FS_NAME_MAX);
    files[slot].data = copy;
    files[slot].size = len;
    return slot;
}

int fs_remove(const char *name) {
    int slot = fs_find(name);
    if (slot < 0) {
        return -1;
    }
    kfree(files[slot].data);
    files[slot].data = NULL;
    files[slot].size = 0;
    files[slot].used = false;
    return 0;
}
