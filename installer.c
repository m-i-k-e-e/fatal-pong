// Home screen installer: a separate payload (fatal-pong-installer.elf) that puts a Fatal Pong tile on the PS5
// home screen. Sent once to the ELF loader, it writes the game, its icon and the tile's metadata to
// /user/app/<TITLE_ID>/ and registers that folder with the system's app installer. The tile's deeplink opens
// launch.html through websrv, which starts eboot.elf like websrv's Homebrew Launcher, so websrv must be running.
// The files are embedded at build time (EMBED_* paths come from the Makefile).
#include <errno.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <ps5/kernel.h>

#define TITLE_ID        "PONG00001"     // Must match assets/shortcut/param.json and launch.html
#define APP_ROOT        "/user/app/"
#define APP_DIR         APP_ROOT TITLE_ID
#define INSTALL_DIR_NID "Wudg3Xe3heE"   // sceAppInstUtilAppInstallTitleDir, not in the SDK's stubs

#define EMBED(name, file)                                               \
    __asm__(".section .rodata\n.balign 16\n.global " #name "\n"        \
            #name ":\n.incbin \"" file "\"\n"                           \
            ".global " #name "_end\n" #name "_end:\n.previous\n");     \
    extern const uint8_t name[], name##_end[]

EMBED(eboot_elf, EMBED_EBOOT);
EMBED(icon0_png, EMBED_ICON);
EMBED(param_json, EMBED_PARAM);
EMBED(launch_html, EMBED_LAUNCH);

int sceAppInstUtilInitialize(void);
int sceAppInstUtilAppInstallAll(void *unused);
int sceAppInstUtilAppUnInstall(const char *title_id);

// The system notification request: the message follows 45 bytes of header fields left zeroed
typedef struct {
    char header[45];
    char message[3075];
} NotifyRequest;

int sceKernelSendNotificationRequest(int device, NotifyRequest *req, size_t size, int blocking);

// Show a PS5 notification (printf-style) and echo it to the ELF loader's log
static void notify(const char *fmt, ...) {
    NotifyRequest req;
    memset(&req, 0, sizeof(req));
    va_list args;
    va_start(args, fmt);
    vsnprintf(req.message, sizeof(req.message), fmt, args);
    va_end(args);
    printf("[fatal-pong-installer] %s\n", req.message);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}

// Create a directory, fine if it's already there; false (errno set) otherwise
static bool make_dir(const char *path) {
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

// Write `size` bytes to `path`, replacing it; false on failure
static bool write_file(const char *path, const uint8_t *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

// Register APP_DIR with the system so its tile shows up: sceAppInstUtilAppInstallTitleDir, resolved by NID,
// or a rescan of every app folder if it can't be found. Returns the system's error code, 0 on success.
static int register_app(void) {
    int (*install_title_dir)(const char *, const char *, void *) = NULL;
    uint32_t handle;
    if (kernel_dynlib_handle(-1, "libSceAppInstUtil.sprx", &handle) == 0)
        install_title_dir = (void *)kernel_dynlib_resolve(-1, handle, INSTALL_DIR_NID);
    if (install_title_dir) return install_title_dir(TITLE_ID, APP_ROOT, NULL);
    return sceAppInstUtilAppInstallAll(NULL);
}

// Replace any earlier install with this build's files, register the tile and report how it went
int main(void) {
    int err;
    if ((err = sceAppInstUtilInitialize())) {
        notify("Fatal Pong: app installer unavailable (0x%08X)", err);
        return 1;
    }
    sceAppInstUtilAppUnInstall(TITLE_ID);      // An older version's tile; failing just means there was none

    static const struct { const char *path; const uint8_t *start, *end; } FILES[] = {
        { APP_DIR "/eboot.elf", eboot_elf, eboot_elf_end },
        { APP_DIR "/launch.html", launch_html, launch_html_end },
        { APP_DIR "/sce_sys/icon0.png", icon0_png, icon0_png_end },
        { APP_DIR "/sce_sys/param.json", param_json, param_json_end },
    };
    if (!make_dir(APP_DIR) || !make_dir(APP_DIR "/sce_sys")) {
        notify("Fatal Pong: can't create %s (%s)", APP_DIR, strerror(errno));
        return 1;
    }
    for (size_t i = 0; i < sizeof(FILES) / sizeof(FILES[0]); i++) {
        if (!write_file(FILES[i].path, FILES[i].start, (size_t)(FILES[i].end - FILES[i].start))) {
            notify("Fatal Pong: can't write %s (%s)", FILES[i].path, strerror(errno));
            return 1;
        }
    }
    if ((err = register_app())) {
        notify("Fatal Pong: home screen install failed (0x%08X)", err);
        return 1;
    }
    notify("Fatal Pong added to the home screen (needs websrv running to start)");
    return 0;
}
