#include <dirent.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <citro2d.h>
#include "drc_core.h"
#include "vb_dsp.h"
#include "vb_gui.h"
#include "vb_set.h"
#include "vb_sound.h"
#include "vb_types.h"
#include "main.h"
#include "sprites_t3x.h"
#include "sprites.h"

#define TINT_R ( (tVBOpt.TINT & 0x000000FF) )
#define TINT_G ( (tVBOpt.TINT & 0x0000FF00) >> 8 )
#define TINT_B ( (tVBOpt.TINT & 0x00FF0000) >> 16 )

static bool buttons_on_screen = false;
void setTouchControls(bool button);
bool guiShouldSwitch();
void drawTouchControls(int inputs);

static C3D_RenderTarget *screen;

static C2D_TextBuf static_textbuf;
static C2D_TextBuf dynamic_textbuf;

static C2D_Text text_A, text_B, text_switch, text_saving, text_on, text_off,
                text_toggle, text_hold, text_3ds, text_vbipd, text_left, text_right,
                text_sound_error, text_anykeyexit, text_about;

static C2D_SpriteSheet sprite_sheet;
static C2D_Sprite colour_wheel_sprite, logo_sprite;

// helpers
static inline int sqr(int i) {
    return i * i;
}

static inline int u16len(const u16 *s) {
    const u16 *e = s;
    while (*e) e++;
    return e - s;
}

typedef struct {
    char *str;
    float x, y, w, h;
    bool show_toggle, toggle;
    C2D_Text *toggle_text_on;
    C2D_Text *toggle_text_off;
    C2D_Text text;
} Button;

static inline int handle_buttons(Button buttons[], int count);
#define HANDLE_BUTTONS(buttons) handle_buttons(buttons, sizeof(buttons) / sizeof(buttons[0]))

#define SETUP_BUTTONS(arr) \
    for (int i = 0; i < sizeof(arr) / sizeof(arr[0]); i++) { \
        C2D_TextParse(&arr[i].text, static_textbuf, arr[i].str); \
        C2D_TextOptimize(&arr[i].text); \
    }

#define LOOP_BEGIN(buttons) \
    int button = -1; \
    bool loop = true; \
    while (loop && aptMainLoop()) { \
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW); \
        C2D_TargetClear(screen, 0); \
        video_flush(eye_count); \
        C2D_SceneBegin(screen); \
        C2D_Prepare(); \
        hidScanInput(); \

#define LOOP_END(buttons) \
        button = HANDLE_BUTTONS(buttons); \
        if (button >= 0) loop = false; \
        C2D_Flush(); \
        C3D_FrameEnd(0); \
    } \
    if (loop) { \
        /* home menu exit */ \
        guiop = GUIEXIT; \
        return; \
    }

static void first_menu();
static Button first_menu_buttons[] = {
    {"Load ROM", 16, 16, 288, 144},
    {"Controls", 0, 176, 80, 64},
    {"Options", 240, 176, 80, 64},
    {"Quit", 112, 192, 96, 48},
};

static void game_menu();
static Button game_menu_buttons[] = {
    {"Load ROM", 224 - 48, 64, 80 + 48, 80},
    {"Controls", 0, 176, 80, 64},
    {"Options", 240, 176, 80, 64},
    {"Quit", 112, 192, 96, 48},
    {"Resume", 0, 0, 320, 48},
    {"Reset", 16, 64, 80 + 48, 80},
};

static void rom_loader();
static Button rom_loader_buttons[] = {
    {"Up", 0, 0, 32, 32},
    {"Back", 0, 208, 48, 32},
};

static void controls();
static Button controls_buttons[] = {
    {"Touchscreen\nsettings", 96, 144, 128, 64},
    {"Back", 0, 208, 48, 32},
};

static void touchscreen_settings();
static Button touchscreen_settings_buttons[] = {
    {"Back", 0, 208, 48, 32},
    {"Reset", 0, 0, 48, 32},
};

static void options();
static Button options_buttons[] = {
    {"Video settings", 16, 16, 288, 48},
    {"Fast forward", 16, 80, 128, 48, true, false, &text_toggle, &text_hold},
    {"Sound", 176, 80, 128, 48, true, false, &text_on, &text_off},
    {"Perf. info", 16, 144, 128, 48, true, false, &text_on, &text_off},
    {"About", 176, 144, 128, 48},
    {"Back", 0, 208, 48, 32},
};

static void video_settings();
static Button video_settings_buttons[] = {
    {"Color mode", 16, 16, 288, 48},
    {"Slider mode", 16, 80, 288, 48, true, false, &text_vbipd, &text_3ds},
    {"Default eye", 16, 144, 288, 48, true, false, &text_right, &text_left},
    {"Back", 0, 208, 48, 32},
};

static void colour_filter();
static Button colour_filter_buttons[] = {
    {"Back", 0, 208, 48, 32},
    {"Red", 16, 64, 48, 32},
    {"Gray", 16, 128, 48, 32},
};

static void sound_error();
static Button sound_error_buttons[] = {
    {"Continue without sound", 48, 130, 320-48*2, 32},
};

static void about();
static Button about_buttons[] = {
    {"Back", 160-48, 180, 48*2, 48},
};

#define SETUP_ALL_BUTTONS \
    SETUP_BUTTONS(first_menu_buttons); \
    SETUP_BUTTONS(game_menu_buttons); \
    SETUP_BUTTONS(rom_loader_buttons); \
    SETUP_BUTTONS(controls_buttons); \
    SETUP_BUTTONS(options_buttons); \
    SETUP_BUTTONS(video_settings_buttons); \
    SETUP_BUTTONS(colour_filter_buttons); \
    SETUP_BUTTONS(sound_error_buttons); \
    SETUP_BUTTONS(touchscreen_settings_buttons); \
    SETUP_BUTTONS(about_buttons);

static void first_menu() {
    LOOP_BEGIN(first_menu_buttons);
        // draw initial "logo"
        C2D_SceneBegin(screenTarget);
        C2D_ViewScale(1, -1);
        C2D_ViewTranslate(0, -512);
        C2D_SpriteSetPos(&logo_sprite, 384 / 2 - 2, 224 / 2);
        C2D_DrawSprite(&logo_sprite);
        C2D_SpriteSetPos(&logo_sprite, 384 / 2 + 2, 224 / 2 + 256);
        C2D_DrawSprite(&logo_sprite);
        C2D_ViewReset();
        C2D_SceneBegin(screen);
    LOOP_END(first_menu_buttons);
    guiop = 0;
    switch (button) {
        case 0:
            return rom_loader();
        case 1:
            return controls();
        case 2:
            return options();
        case 3: // Quit
            guiop = GUIEXIT;
            return;
    }
}

static void game_menu() {
    LOOP_BEGIN(game_menu_buttons);
    LOOP_END(game_menu_buttons);
    switch (button) {
        case 0: // Load ROM
            guiop = AKILL | VBRESET;
            return rom_loader();
        case 1: // Controls
            return controls();
        case 2: // Options
            return options();
        case 3: // Quit
            guiop = AKILL | GUIEXIT;
            return;
        case 4: // Resume
            guiop = 0;
            return;
        case 5: // Reset
            guiop = AKILL | VBRESET;
            return;
    }
}

static void main_menu() {
    if (game_running) game_menu();
    else first_menu();
}

int strptrcmp(const void *s1, const void *s2) {
    return strcasecmp(*(const char**)s1, *(const char**)s2);
}

static void rom_loader() {
    static char *path = NULL;
    static int path_cap = 128;
    if (!path) {
        if (tVBOpt.ROM_PATH) {
            int path_len = strlen(tVBOpt.ROM_PATH);
            while (path_cap <= path_len) path_cap *= 2;
            path = malloc(path_cap);
            strcpy(path, tVBOpt.ROM_PATH);
        } else {
            path = calloc(path_cap, 1);
        }
    }

    // in case we broke it somehow
    if (strlen(path) < 6) {
        strcpy(path, "sdmc:/");
    }

    DIR *dirHandle;
    // cut filename from path if we reload mid-game
    // also go up a directory if we can't load this directory
    do {
        strrchr(path, '/')[1] = 0;
        dirHandle = opendir(path);
        if (!dirHandle) path[strlen(path) - 1] = 0;
    } while (!dirHandle && strlen(path) > 6);

    if (!dirHandle) {
        // TODO error
        guiop = 0;
        return;
    }

    char **dirs = malloc(8 * sizeof(char*));
    char **files = malloc(8 * sizeof(char*));
    int dirCount = 0, fileCount = 0;
    int dirCap = 8, fileCap = 8;

    struct dirent *dp;

    // read .vb files and directories
    while ((dp = readdir(dirHandle))) {
        archive_dir_t* dirSt = (archive_dir_t*)dirHandle->dirData->dirStruct;
        FS_DirectoryEntry *thisEntry = &dirSt->entry_data[dirSt->index];
        if ((thisEntry->attributes & FS_ATTRIBUTE_HIDDEN) || dp->d_name[0] == '.')
            continue;
        if (thisEntry->attributes & FS_ATTRIBUTE_DIRECTORY) {
            if (dirCount == dirCap) {
                dirCap *= 2;
                dirs = realloc(dirs, dirCap * sizeof(char*));
            }
            int len = strlen(dp->d_name) + 1;
            dirs[dirCount] = malloc(len);
            memcpy(dirs[dirCount++], dp->d_name, len);
        } else {
            // check the file extension
            char *dot = strrchr(dp->d_name, '.');
            if (dot && strcasecmp(dot, ".vb") == 0) {
                if (fileCount == fileCap) {
                    fileCap *= 2;
                    files = realloc(files, fileCap * sizeof(char*));
                }
                int len = strlen(dp->d_name) + 1;
                files[fileCount] = malloc(len);
                memcpy(files[fileCount++], dp->d_name, len);
            }
        }
    }

    closedir(dirHandle);

    qsort(files, fileCount, sizeof(char*), strptrcmp);
    qsort(dirs, dirCount, sizeof(char*), strptrcmp);

    int entry_count = dirCount + fileCount;
    const float entry_height = 32;
    float scroll_top = -entry_height;
    float scroll_bottom = entry_count * entry_height - 240;
    if (scroll_bottom < scroll_top) scroll_bottom = scroll_top;
    float scroll_pos = scroll_top;
    float scroll_speed = 0;

    if (tVBOpt.ROM_PATH && strstr(tVBOpt.ROM_PATH, path) == tVBOpt.ROM_PATH) {
        char *filename = strrchr(tVBOpt.ROM_PATH, '/');
        // null check but also skip the slash
        if (filename++) {
            for (int i = 0; i < fileCount; i++) {
                if (strcmp(files[i], filename) == 0) {
                    int button_y = i * entry_height;
                    scroll_pos = C2D_Clamp(button_y - (240 / 2), scroll_top, scroll_bottom);
                }
            }
        }
    }

    int last_py = 0;
    int clicked_entry = -1;
    bool dragging = false;

    LOOP_BEGIN(rom_loader_buttons);
        // process rom list
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);
        if ((hidKeysDown() & KEY_TOUCH) && touch_pos.px >= 48) {
            last_py = touch_pos.py;
            clicked_entry = floorf((touch_pos.py + scroll_pos) / entry_height);
            if (clicked_entry < 0 || clicked_entry >= entry_count)
                clicked_entry = -1;
            dragging = false;
            scroll_speed = 0;
        } else if (clicked_entry >= 0 && (hidKeysHeld() & KEY_TOUCH)) {
            if (!dragging && abs(touch_pos.py - last_py) >= 3) {
                clicked_entry = -1;
                dragging = true;
            }
        }
        if (dragging) {
            if (!(hidKeysHeld() & KEY_TOUCH)) {
                dragging = false;
            } else {
                // negated
                scroll_speed = -(touch_pos.py - last_py);
                last_py = touch_pos.py;
            }
        } else if (clicked_entry >= 0) {
            if (!(hidKeysHeld() & KEY_TOUCH)) {
                bool clicked_dir = clicked_entry < dirCount;
                const char *new_entry = clicked_dir ? dirs[clicked_entry] : files[clicked_entry - dirCount];
                int new_path_len = strlen(path) + strlen(new_entry) + clicked_dir;
                if (new_path_len + 2 > path_cap) {
                    while (new_path_len + 2 > path_cap)
                        path_cap *= 2;
                    path = realloc(path, path_cap);
                }
                strcat(path, new_entry);
                if (clicked_entry < dirCount) {
                    // clicked on directory, so add a slash
                    path[new_path_len - 1] = '/';
                    path[new_path_len] = 0;
                }
                loop = false;
            }
        } else if (scroll_speed != 0) {
            scroll_speed += scroll_speed > 0 ? -1 : 1;
            if (abs(scroll_speed) < 1) scroll_speed = 0;
        }
        // inertia
        int top_pos = scroll_top;
        if (dragging) {
            top_pos -= 8;
        } else if (scroll_pos > scroll_top) {
            top_pos -= 8;
        } else {
            if (scroll_speed < 0) scroll_speed = 0;
            top_pos = scroll_pos < scroll_top ? scroll_pos + 0.25 : scroll_top;
        }
        int bottom_pos = scroll_bottom;
        if (dragging) {
            bottom_pos += 8;
        } else if (scroll_pos < scroll_bottom) {
            bottom_pos += 8;
        } else {
            if (scroll_speed > 0) scroll_speed = 0;
            bottom_pos = scroll_pos > scroll_bottom ? scroll_pos - 0.25 : scroll_bottom;
        }
        scroll_pos += scroll_speed;
        scroll_pos = C2D_Clamp(scroll_pos, top_pos, bottom_pos);
        // draw
        C2D_TextBufClear(dynamic_textbuf);
        float y = -scroll_pos;
        C2D_Text text;
        // entries
        for (int i = 0; i < entry_count; i++) {
            if (y + entry_height >= 0 && y < 240) {
                C2D_TextParse(&text, dynamic_textbuf, i < dirCount ? dirs[i] : files[i - dirCount]);
                C2D_TextOptimize(&text);
                C2D_DrawRectSolid(56, y, 0, 264, entry_height, (clicked_entry == i)? C2D_Color32(TINT_R*0.5, TINT_G*0.5, TINT_B*0.5, 255): C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
                C2D_DrawText(&text, C2D_AlignLeft, 64, y + 8, 0, 0.5, 0.5);
            }
            y += entry_height;
        }
        // scrollbar
        if (scroll_top != scroll_bottom) {
            C2D_DrawRectSolid(320-2, 32, 0, 2, 240-32, C2D_Color32(0, 0, 0, 255));
            C2D_DrawRectSolid(320-2, 32 + (240-32-8) * (scroll_pos - scroll_top) / (scroll_bottom - scroll_top), 0, 2, 8, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        }
        // path
        C2D_TextParse(&text, dynamic_textbuf, path);
        C2D_TextOptimize(&text);
        C2D_DrawRectSolid(0, 0, 0, 320, 32, C2D_Color32(0, 0, 0, 255));
        C2D_DrawText(&text, C2D_AlignLeft | C2D_WithColor, 48, 8, 0, 0.5, 0.5, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
    LOOP_END(rom_loader_buttons);

    for (int i = 0; i < dirCount; i++) free(dirs[i]);
    for (int i = 0; i < fileCount; i++) free(files[i]);
    free(dirs);
    free(files);

    if (clicked_entry < 0) {
        switch (button) {
            case 0: // Up
                int len = strlen(path);
                // don't get shorter than sdmc:/
                if (len > 6) {
                    // the stuff at the start of the file will get rid of everything after the last slash
                    // so we can just get rid of the slash at the end
                    path[len - 1] = 0;
                }
                return rom_loader();
            case 1: return main_menu();
        }
    } else if (clicked_entry < dirCount) {
        return rom_loader();
    } else {
        tVBOpt.ROM_PATH = realloc(tVBOpt.ROM_PATH, path_cap);
        memcpy(tVBOpt.ROM_PATH, path, path_cap);
        tVBOpt.RAM_PATH = realloc(tVBOpt.RAM_PATH, path_cap);
        memcpy(tVBOpt.RAM_PATH, path, path_cap);
        strcpy(strrchr(tVBOpt.RAM_PATH, '.'), ".ram");
        saveFileOptions();
        return;
    }
}

static void controls() {
    bool pressed = false;
    const int FACEX = 160;
    const int FACEY = 80;
    const int FACEW = 128;
    const int FACEH = 80;
    const int OFFSET = 22;
    bool changed = false;
    LOOP_BEGIN(controls_buttons);
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);
        if (hidKeysHeld() & KEY_TOUCH) {
            if (touch_pos.px >= FACEX - FACEW/2 && touch_pos.px < FACEX + FACEW/2 && touch_pos.py >= FACEY - FACEH/2 && touch_pos.py < FACEY + FACEH/2) {
                if (hidKeysDown() & KEY_TOUCH) {
                    pressed = true;
                }
            } else {
                pressed = false;
            }
        } else if (pressed) {
            changed = true;
            tVBOpt.ABXY_MODE = (tVBOpt.ABXY_MODE + 1) % 6;
            pressed = false;
            setTouchControls(buttons_on_screen);
        }
        C2D_DrawRectSolid(FACEX - FACEW/2, FACEY - FACEH/2, 0, FACEW, FACEH, pressed ? C2D_Color32(TINT_R*0.5, TINT_G*0.5, TINT_B*0.5, 255) : C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
        C2D_DrawCircleSolid(FACEX + OFFSET, FACEY, 0, 12, C2D_Color32(0, 0, 0, 192));
        C2D_DrawCircleSolid(FACEX - OFFSET, FACEY, 0, 12, C2D_Color32(0, 0, 0, 192));
        C2D_DrawCircleSolid(FACEX, FACEY + OFFSET, 0, 12, C2D_Color32(0, 0, 0, 192));
        C2D_DrawCircleSolid(FACEX, FACEY - OFFSET, 0, 12, C2D_Color32(0, 0, 0, 192));
        C2D_DrawText(tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 5 ? &text_A : &text_B, C2D_AlignCenter | C2D_WithColor, FACEX, FACEY - OFFSET - 8, 0, 0.5, 0.5, C2D_Color32(255, 255, 255, 255));
        C2D_DrawText(tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 4 ? &text_B : &text_A, C2D_AlignCenter | C2D_WithColor, FACEX, FACEY + OFFSET - 8, 0, 0.5, 0.5, C2D_Color32(255, 255, 255, 255));
        C2D_DrawText(tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 5 ? &text_B : &text_A, C2D_AlignCenter | C2D_WithColor, FACEX - OFFSET, FACEY - 8, 0, 0.5, 0.5, C2D_Color32(255, 255, 255, 255));
        C2D_DrawText(tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 4 ? &text_A : &text_B, C2D_AlignCenter | C2D_WithColor, FACEX + OFFSET, FACEY - 8, 0, 0.5, 0.5, C2D_Color32(255, 255, 255, 255));
    LOOP_END(controls_buttons);
    if (changed) saveFileOptions();
    switch (button) {
        case 0: return touchscreen_settings();
        case 1: return main_menu();
    }
}

static void touchscreen_settings() {
    // 1: pause, 2: dpad/a, 3: b
    int dragging = 0;
    int xoff = 0, yoff = 0;
    int BUTTON_RAD = 24;
    int PAD_RAD = 48;
    int PAUSE_RAD = 4;
    LOOP_BEGIN(touchscreen_settings_buttons);
        // handle dragging
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);
        if (hidKeysDown() & KEY_TOUCH) {
            if (guiShouldSwitch()) {
                buttons_on_screen = !buttons_on_screen;
            } else if (abs(tVBOpt.PAUSE_RIGHT - touch_pos.px) < PAUSE_RAD) {
                // pause slider
                dragging = 1;
                xoff = tVBOpt.PAUSE_RIGHT - touch_pos.px;
            } else if (buttons_on_screen) {
                int adx = tVBOpt.TOUCH_AX - touch_pos.px;
                int ady = tVBOpt.TOUCH_AY - touch_pos.py;
                int bdx = tVBOpt.TOUCH_BX - touch_pos.px;
                int bdy = tVBOpt.TOUCH_BY - touch_pos.py;
                if (adx*adx + ady*ady < BUTTON_RAD*BUTTON_RAD) {
                    // a button
                    dragging = 2;
                    xoff = adx;
                    yoff = ady;
                } else if (bdx*bdx + bdy*bdy < BUTTON_RAD*BUTTON_RAD) {
                    // b button
                    dragging = 3;
                    xoff = bdx;
                    yoff = bdy;
                }
            } else {
                int dx = tVBOpt.TOUCH_PADX - touch_pos.px;
                int dy = tVBOpt.TOUCH_PADY - touch_pos.py;
                if (dx*dx + dy*dy < PAD_RAD*PAD_RAD) {
                    // dpad
                    dragging = 2;
                    xoff = dx;
                    yoff = dy;
                }
            }
        }
        if (hidKeysHeld() & KEY_TOUCH) {
            if (dragging == 1) {
                // pause slider
                int dest_x = touch_pos.px + xoff;
                if (dest_x > tVBOpt.TOUCH_AX - BUTTON_RAD - PAUSE_RAD)
                    dest_x = tVBOpt.TOUCH_AX - BUTTON_RAD - PAUSE_RAD;
                if (dest_x > tVBOpt.TOUCH_BX - BUTTON_RAD - PAUSE_RAD)
                    dest_x = tVBOpt.TOUCH_BX - BUTTON_RAD - PAUSE_RAD;
                if (dest_x > tVBOpt.TOUCH_PADX - PAD_RAD - PAUSE_RAD)
                    dest_x = tVBOpt.TOUCH_PADX - PAD_RAD - PAUSE_RAD;
                if (dest_x > 192)
                    dest_x = 192;
                if (dest_x < 80 + PAUSE_RAD) 
                    dest_x = 80 + PAUSE_RAD;
                tVBOpt.PAUSE_RIGHT = dest_x;
            } else if (dragging != 0) {
                // button or dpad
                int dest_x = touch_pos.px + xoff;
                int dest_y = touch_pos.py + yoff;
                int rad = buttons_on_screen ? BUTTON_RAD : PAD_RAD;
                if (dest_x > 320 - rad)
                    dest_x = 320 - rad;
                if (dest_y > 240 - rad)
                    dest_y = 240 - rad;
                if (dest_x < tVBOpt.PAUSE_RIGHT + PAUSE_RAD + rad)
                    dest_x = tVBOpt.PAUSE_RIGHT + PAUSE_RAD + rad;
                // lower top range to account for switch button
                if (dest_y < 32 + rad)
                    dest_y = 32 + rad;
                if (buttons_on_screen) {
                    if (dragging == 2) {
                        // a button
                        int dx = dest_x - tVBOpt.TOUCH_BX;
                        int dy = dest_y - tVBOpt.TOUCH_BY;
                        int dist_sqr = dx*dx + dy*dy;
                        if (dist_sqr < 4*BUTTON_RAD*BUTTON_RAD && dist_sqr != 0) {
                            float dist = sqrt(dist_sqr);
                            dest_x = tVBOpt.TOUCH_BX + dx * BUTTON_RAD * 2 / dist;
                            dest_y = tVBOpt.TOUCH_BY + dy * BUTTON_RAD * 2 / dist;
                        }
                    } else {
                        // b button
                        int dx = dest_x - tVBOpt.TOUCH_AX;
                        int dy = dest_y - tVBOpt.TOUCH_AY;
                        int dist_sqr = dx*dx + dy*dy;
                        if (dist_sqr < 4*BUTTON_RAD*BUTTON_RAD && dist_sqr != 0) {
                            float dist = sqrt(dist_sqr);
                            dest_x = tVBOpt.TOUCH_AX + dx * BUTTON_RAD * 2 / dist;
                            dest_y = tVBOpt.TOUCH_AY + dy * BUTTON_RAD * 2 / dist;
                        }
                    }
                    // additional check in case the button check shunted us OOB
                    if (dest_x <= 320 - BUTTON_RAD && dest_x >= tVBOpt.PAUSE_RIGHT + PAUSE_RAD + BUTTON_RAD
                        && dest_y <= 240 - BUTTON_RAD && dest_y >= 32 + BUTTON_RAD
                    ) {
                        if (dragging == 2) {
                            tVBOpt.TOUCH_AX = dest_x;
                            tVBOpt.TOUCH_AY = dest_y;
                        } else {
                            tVBOpt.TOUCH_BX = dest_x;
                            tVBOpt.TOUCH_BY = dest_y;
                        }
                    }
                } else {
                    tVBOpt.TOUCH_PADX = dest_x;
                    tVBOpt.TOUCH_PADY = dest_y;
                }
            }
        } else {
            dragging = 0;
        }
        // draw
        drawTouchControls(
            dragging == 2 ? VB_KEY_A :
            dragging == 3 ? VB_KEY_B :
            dragging ? VB_KEY_START : 0);
        C2D_DrawRectSolid(
            tVBOpt.PAUSE_RIGHT - PAUSE_RAD, 240/2 - 8, 0,
            PAUSE_RAD * 2, 8*2, dragging == 1 ? C2D_Color32(TINT_R*0.9, TINT_G*0.9, TINT_B*0.9, 255) : C2D_Color32(TINT_R*0.5, TINT_G*0.5, TINT_B*0.5, 255)
        );
    LOOP_END(touchscreen_settings_buttons);
    switch (button) {
        case 0: // Back
            saveFileOptions();
            return controls();
        case 1: // Reset
            tVBOpt.PAUSE_RIGHT = 160;
            tVBOpt.TOUCH_AX = 250;
            tVBOpt.TOUCH_AY = 64;
            tVBOpt.TOUCH_BX = 250;
            tVBOpt.TOUCH_BY = 160;
            tVBOpt.TOUCH_PADX = 240;
            tVBOpt.TOUCH_PADY = 128;
            return touchscreen_settings();
    }
}

static void colour_filter() {
    bool dragging = false;
    const float circle_x = colour_wheel_sprite.params.pos.x;
    const float circle_y = colour_wheel_sprite.params.pos.y;
    const float circle_w = colour_wheel_sprite.params.pos.w;
    const float circle_h = colour_wheel_sprite.params.pos.h;

    LOOP_BEGIN(colour_filter_buttons);
        touchPosition touch_pos;
        hidTouchRead(&touch_pos);
        float touch_dx = (touch_pos.px - circle_x) / (circle_w / 2);
        float touch_dy = (touch_pos.py - circle_y) / (circle_h / 2);
        float sat = sqrt(touch_dx * touch_dx + touch_dy * touch_dy);
        if ((hidKeysDown() & KEY_TOUCH) && sat <= 1)
            dragging = true;
        if (hidKeysUp() & KEY_TOUCH)
            dragging = false;
        if (dragging) {
            if (sat > 1) {
                touch_dx /= sat;
                touch_dy /= sat;
                sat = 1;
            }
            // touch position to hue saturation, then to rgb
            float hue = atan2(touch_dy, touch_dx);
            float hprime = fmod(hue / (M_PI / 3) + 6, 6);
            float sub = sat * (1 - fabs(fmod(hprime, 2) - 1));
            float col[3] = {0};
            if (hprime < 1) {
                col[0] = sat;
                col[1] = sub;
            } else if (hprime < 2) {
                col[0] = sub;
                col[1] = sat;
            } else if (hprime < 3) {
                col[1] = sat;
                col[2] = sub;
            } else if (hprime < 4) {
                col[1] = sub;
                col[2] = sat;
            } else if (hprime < 5) {
                col[0] = sub;
                col[2] = sat;
            } else {
                col[0] = sat;
                col[2] = sub;
            }
            tVBOpt.TINT = 0xff000000 |
                ((int)((col[0] + 1 - sat) * 255)) |
                ((int)((col[1] + 1 - sat) * 255) << 8) |
                ((int)((col[2] + 1 - sat) * 255) << 16);
        }
        C2D_DrawSprite(&colour_wheel_sprite);
        if (!dragging) {
            // tint to hue saturation
            float col[3] = {
                (tVBOpt.TINT & 0xff) / 255.0,
                ((tVBOpt.TINT >> 8) & 0xff) / 255.0,
                ((tVBOpt.TINT >> 16) & 0xff) / 255.0,
            };
            float max = col[0] > col[1] ? col[0] : col[1];
            max = max > col[2] ? max : col[2];
            float min = col[0] < col[1] ? col[0] : col[1];
            min = min < col[2] ? min : col[2];
            if (max == min) {
                // white
                touch_dx = touch_dy = 0;
            } else {
                float chroma = max - min;
                float hprime;
                if (max == col[0]) {
                    hprime = (col[1] - col[2]) / chroma;
                } else if (max == col[1]) {
                    hprime = (col[2] - col[0]) / chroma + 2;
                } else {
                    hprime = (col[0] - col[1]) / chroma + 4;
                }
                float hue = hprime * (M_PI / 3);
                float sat = chroma / max;
                touch_dx = sat * cos(hue);
                touch_dy = sat * sin(hue);
            }
        }
        C2D_DrawCircleSolid(
            circle_x + touch_dx * (circle_w / 2),
            circle_y + touch_dy * (circle_h / 2),
            0, 4, 0xff000000);
        C2D_DrawCircleSolid(
            circle_x + touch_dx * (circle_w / 2),
            circle_y + touch_dy * (circle_h / 2),
            0, 2, tVBOpt.TINT);
    LOOP_END(colour_filter_buttons);
    switch (button) {
        case 0: // Back
            saveFileOptions();
            return video_settings();
        case 1: // Red
            tVBOpt.TINT = 0xff0000ff;
            return colour_filter();
        case 2: // Gray
            tVBOpt.TINT = 0xffffffff;
            return colour_filter();
    }
}

static void options() {
    options_buttons[1].toggle = tVBOpt.FF_TOGGLE;
    options_buttons[2].toggle = tVBOpt.SOUND;
    options_buttons[3].toggle = tVBOpt.PERF_INFO;
    LOOP_BEGIN(options_buttons);
    LOOP_END(options_buttons);
    switch (button) {
        case 0: // Video settings
            return video_settings();
        case 1: // Fast forward
            tVBOpt.FF_TOGGLE = !tVBOpt.FF_TOGGLE;
            saveFileOptions();
            return options();
        case 2: // Sound
            tVBOpt.SOUND = !tVBOpt.SOUND;
            if (tVBOpt.SOUND) sound_enable();
            else sound_disable();
            return options();
        case 3: // Performance info
            tVBOpt.PERF_INFO = !tVBOpt.PERF_INFO;
            saveFileOptions();
            return options();
        case 4: // About
            return about();
        case 5: // Back
            return main_menu();
    }
}

static void video_settings() {
    video_settings_buttons[1].toggle = tVBOpt.SLIDERMODE;
    video_settings_buttons[2].toggle = tVBOpt.DEFAULT_EYE;
    LOOP_BEGIN(video_settings_buttons);
    LOOP_END(video_settings_buttons);
    switch (button) {
        case 0: // Colour filter
            return colour_filter();
        case 1: // Slider mode
            tVBOpt.SLIDERMODE = !tVBOpt.SLIDERMODE;
            saveFileOptions();
            return video_settings();
        case 2: // Default eye
            tVBOpt.DEFAULT_EYE = !tVBOpt.DEFAULT_EYE;
            saveFileOptions();
            return video_settings();
        case 3: // Back
            return options();
    }
}

static void sound_error() {
    LOOP_BEGIN(sound_error_buttons);
        C2D_DrawText(&text_sound_error, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.7, 0.7, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
    LOOP_END(sound_error_buttons);
    return;
}

static void about() {
    C2D_SpriteSetPos(&logo_sprite, 320 / 2, 36);
    C2D_SetTintMode(C2D_TintMult);
    C2D_ImageTint tint;
    C2D_PlainImageTint(&tint, C2D_Color32(255, 0, 0, 255), 1);
    LOOP_BEGIN(about_buttons);
        C2D_DrawSpriteTinted(&logo_sprite, &tint);
        C2D_DrawText(&text_about, C2D_AlignCenter | C2D_WithColor, 320 / 2, 80, 0, 0.5, 0.5, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
    LOOP_END(about_buttons);
    return options();
}

static inline int handle_buttons(Button buttons[], int count) {
    static int pressed = -1;
    int ret = -1;
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    // check buttons for input
    for (int i = 0; i < count; i++) {
        if (
            touch_pos.px >= buttons[i].x && touch_pos.py >= buttons[i].y &&
            touch_pos.px < buttons[i].x + buttons[i].w && touch_pos.py < buttons[i].y + buttons[i].h
        ) {
            // touching the button
            if (hidKeysDown() & KEY_TOUCH) {
                pressed = i;
            }
        } else {
            // not touching the button
            if (pressed == i && (hidKeysHeld() & KEY_TOUCH)) {
                pressed = -1;
            }
        }
        if ((hidKeysUp() & KEY_TOUCH) && pressed == i) {
            ret = i;
        }
    }
    // draw buttons
    for (int i = 0; i < count; i++) {
        u32 normal_colour = C2D_Color32(TINT_R, TINT_G, TINT_B, 255);
        u32 pressed_colour = C2D_Color32(TINT_R*0.5, TINT_G*0.5, TINT_B*0.5, 255);
        C2D_DrawRectSolid(buttons[i].x, buttons[i].y, 0, buttons[i].w, buttons[i].h, pressed == i ? pressed_colour : normal_colour);
        int yoff = -10;
        char *strptr = buttons[i].str;
        while ((strptr = strchr(strptr, '\n'))) {
            yoff -= 10;
            strptr++;
        }
        C2D_DrawText(&buttons[i].text, C2D_AlignCenter, buttons[i].x + buttons[i].w / 2, buttons[i].y + buttons[i].h / 2 + yoff, 0, 0.7, 0.7);
        if (buttons[i].show_toggle) C2D_DrawText(buttons[i].toggle ? buttons[i].toggle_text_on : buttons[i].toggle_text_off, C2D_AlignLeft, buttons[i].x, buttons[i].y, 0, 0.5, 0.5);
    }
    if (save_thread) C2D_DrawText(&text_saving, C2D_AlignLeft, 0, 224, 0, 0.5, 0.5);
    if (ret >= 0) pressed = -1;
    return ret;
}

static void draw_status_bar(float total_time, float drc_time) {
    if (!tVBOpt.PERF_INFO) return;
    C2D_DrawRectSolid(0, 240 - 12, 0, 320, 12, C2D_Color32(128, 128, 128, 255));
    C2D_TextBufClear(dynamic_textbuf);
    C2D_Text text;
    char buf[32];

    // All
    sprintf(buf, "A:%5.2fms", total_time);
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 310-60*5, 240 - 12, 0, 0.35, 0.35);

    // DRC
    sprintf(buf, "D:%5.2fms", drc_time);
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 310-60*4, 240 - 12, 0, 0.35, 0.35);

    // C3D
    sprintf(buf, "C:%5.2fms", C3D_GetProcessingTime());
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 310-60*3, 240 - 12, 0, 0.35, 0.35);

    // PICA
    sprintf(buf, "P:%5.2fms", C3D_GetDrawingTime());
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 310-60*2, 240 - 12, 0, 0.35, 0.35);

    // Memory
    sprintf(buf, "M:%5.2f%%", (cache_pos-cache_start)*4*100./CACHE_SIZE);
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_AlignLeft, 310-60, 240 - 12, 0, 0.35, 0.35);
}

void guiInit() {
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    screen = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

    sprite_sheet = C2D_SpriteSheetLoadFromMem(sprites_t3x, sprites_t3x_size);
    C2D_SpriteFromSheet(&colour_wheel_sprite, sprite_sheet, sprites_colour_wheel_idx);
    C2D_SpriteSetCenter(&colour_wheel_sprite, 0.5, 0.5);
    C2D_SpriteSetPos(&colour_wheel_sprite, 176, 112);
    C2D_SpriteFromSheet(&logo_sprite, sprite_sheet, sprites_logo_idx);
    C2D_SpriteSetCenter(&logo_sprite, 0.5, 0.5);

    setTouchControls(buttons_on_screen);

    static_textbuf = C2D_TextBufNew(1024);
    dynamic_textbuf = C2D_TextBufNew(4096);
    SETUP_ALL_BUTTONS;
    C2D_TextParse(&text_A, static_textbuf, "A");
    C2D_TextOptimize(&text_A);
    C2D_TextParse(&text_B, static_textbuf, "B");
    C2D_TextOptimize(&text_B);
    C2D_TextParse(&text_switch, static_textbuf, "Switch");
    C2D_TextOptimize(&text_switch);
    C2D_TextParse(&text_saving, static_textbuf, "Saving...");
    C2D_TextOptimize(&text_saving);
    C2D_TextParse(&text_on, static_textbuf, "On");
    C2D_TextOptimize(&text_on);
    C2D_TextParse(&text_off, static_textbuf, "Off");
    C2D_TextOptimize(&text_off);
    C2D_TextParse(&text_toggle, static_textbuf, "Toggle");
    C2D_TextOptimize(&text_toggle);
    C2D_TextParse(&text_hold, static_textbuf, "Hold");
    C2D_TextOptimize(&text_hold);
    C2D_TextParse(&text_3ds, static_textbuf, "Nintendo 3DS");
    C2D_TextOptimize(&text_3ds);
    C2D_TextParse(&text_vbipd, static_textbuf, "Virtual Boy IPD");
    C2D_TextOptimize(&text_vbipd);
    C2D_TextParse(&text_left, static_textbuf, "Left");
    C2D_TextOptimize(&text_left);
    C2D_TextParse(&text_right, static_textbuf, "Right");
    C2D_TextOptimize(&text_right);
    C2D_TextParse(&text_sound_error, static_textbuf, "Error: couldn't initialize audio.\nDid you dump your DSP firmware?");
    C2D_TextOptimize(&text_sound_error);
    C2D_TextParse(&text_anykeyexit, static_textbuf, "Press any key to exit");
    C2D_TextOptimize(&text_anykeyexit);
    C2D_TextParse(&text_about, static_textbuf, VERSION "\nBy Floogle, danielps, & others\nHeavily based on Reality Boy by David Tucker\nMore info at:\ngithub.com/skyfloogle/red-viper");
    C2D_TextOptimize(&text_about);
}

static bool shouldRedrawMenu = true;

void openMenu() {
    shouldRedrawMenu = true;
    if (game_running) {
        save_sram();
        if (tVBOpt.SOUND)
            ndspSetMasterVol(0.0);
    }
    C2D_Prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    main_menu();
    if (game_running && tVBOpt.SOUND)
        ndspSetMasterVol(1.0);
}

void showSoundError() {
    C2D_Prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    sound_error();
}

void showError(int code) {
    gfxSetDoubleBuffering(GFX_BOTTOM, false);
    C2D_Prepare();
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
    C2D_TextBufClear(dynamic_textbuf);
    char buf[100];
    sprintf(buf, "DRC error #%d\nPC=0x%08lx\nDumping debug info...", code, v810_state->PC);
    C2D_Text text;
    C2D_TextParse(&text, dynamic_textbuf, buf);
    C2D_TextOptimize(&text);
    C3D_FrameBegin(0);
    C2D_TargetClear(screen, 0);
    C2D_SceneBegin(screen);
    C2D_DrawText(&text, C2D_AlignCenter | C2D_WithColor, 320 / 2, 60, 0, 0.7, 0.7, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
    C2D_Flush();
    C3D_FrameEnd(0);

    drc_dumpDebugInfo();

    C3D_FrameBegin(0);
    C3D_FrameDrawOn(screen);
    C2D_DrawText(&text_anykeyexit, C2D_AlignCenter | C2D_WithColor, 320 / 2, 140, 0, 0.7, 0.7, C2D_Color32(TINT_R, TINT_G, TINT_B, 255));
    C2D_Flush();
    C3D_FrameEnd(0);
}

void setTouchControls(bool buttons) {
    shouldRedrawMenu = true;
    buttons_on_screen = buttons;
    if (buttons) {
        vbkey[__builtin_ctz(KEY_A)] = VB_RPAD_R;
        vbkey[__builtin_ctz(KEY_X)] = VB_RPAD_U;
        vbkey[__builtin_ctz(KEY_B)] = VB_RPAD_D;
        vbkey[__builtin_ctz(KEY_Y)] = VB_RPAD_L;
    } else {
        vbkey[__builtin_ctz(KEY_A)] = tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 4 ? VB_KEY_A : VB_KEY_B;
        vbkey[__builtin_ctz(KEY_Y)] = tVBOpt.ABXY_MODE < 2 || tVBOpt.ABXY_MODE == 5 ? VB_KEY_B : VB_KEY_A;
        vbkey[__builtin_ctz(KEY_B)] = tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 4 ? VB_KEY_B : VB_KEY_A;
        vbkey[__builtin_ctz(KEY_X)] = tVBOpt.ABXY_MODE == 0 || tVBOpt.ABXY_MODE == 3 || tVBOpt.ABXY_MODE == 5 ? VB_KEY_A : VB_KEY_B;
    }
}

bool guiShouldSwitch() {
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    return touch_pos.px >= 320 - 64 && touch_pos.py < 32;
}

void drawTouchControls(int inputs) {
    int col_up = C2D_Color32(TINT_R*0.5, TINT_G*0.5, TINT_B*0.5, 255);
    int col_down = C2D_Color32(TINT_R*0.75, TINT_G*0.75, TINT_B*0.75, 255);
    int col_drag = C2D_Color32(TINT_R*0.9, TINT_G*0.9, TINT_B*0.9, 255);
    int col_line = C2D_Color32(32, 32, 32, 255);
    if (buttons_on_screen) {
        float mx = (float)(tVBOpt.TOUCH_AX + tVBOpt.TOUCH_BX) / 2;
        float my = (float)(tVBOpt.TOUCH_AY + tVBOpt.TOUCH_BY) / 2;
        if (tVBOpt.TOUCH_AY == tVBOpt.TOUCH_BY) {
            // edge case so we don't div0
            C2D_DrawLine(mx, 0, col_line, mx, 240, col_line, 1, 0);
        } else {
            float rico = -(tVBOpt.TOUCH_BX - tVBOpt.TOUCH_AX) / (float)(tVBOpt.TOUCH_BY - tVBOpt.TOUCH_AY);
            int oy = -rico * mx + my;
            int ly = oy + rico * tVBOpt.PAUSE_RIGHT;
            int ry = oy + rico * 320;
            C2D_DrawLine(tVBOpt.PAUSE_RIGHT, ly, col_line, 320, ry, col_line, 1, 0);
        }
    } else {
        C2D_DrawLine(
            tVBOpt.PAUSE_RIGHT, tVBOpt.TOUCH_PADY - tVBOpt.TOUCH_PADX + tVBOpt.PAUSE_RIGHT, col_line,
            320, tVBOpt.TOUCH_PADY - tVBOpt.TOUCH_PADX + 320, col_line,
            1, 0);
        C2D_DrawLine(
            tVBOpt.PAUSE_RIGHT, tVBOpt.TOUCH_PADX + tVBOpt.TOUCH_PADY - tVBOpt.PAUSE_RIGHT, col_line,
            320, tVBOpt.TOUCH_PADX + tVBOpt.TOUCH_PADY - 320, col_line,
            1, 0);
    }

    C2D_DrawLine(
        tVBOpt.PAUSE_RIGHT, 0, C2D_Color32(64, 64, 64, 255),
        tVBOpt.PAUSE_RIGHT, 240, C2D_Color32(64, 64, 64, 255),
        1, 0);

    bool dragging = inputs != 0;
    if (inputs == 0) inputs = guiGetInput(false);

    int pause_square_height = 70;
    C2D_DrawRectSolid(tVBOpt.PAUSE_RIGHT / 2 - pause_square_height / 2, 240 / 2 - pause_square_height / 2, 0,
        pause_square_height * 0.4, pause_square_height, C2D_Color32(64, 64, 64, 255));
    C2D_DrawRectSolid(tVBOpt.PAUSE_RIGHT / 2 - pause_square_height / 2 + pause_square_height * 0.6, 240 / 2 - pause_square_height / 2, 0,
        pause_square_height * 0.4, pause_square_height, C2D_Color32(64, 64, 64, 255));

    if (buttons_on_screen) {
        C2D_DrawCircleSolid(tVBOpt.TOUCH_AX, tVBOpt.TOUCH_AY, 0, 24, inputs & VB_KEY_A ? (dragging ? col_drag : col_down) : col_up);
        C2D_DrawCircleSolid(tVBOpt.TOUCH_BX, tVBOpt.TOUCH_BY, 0, 24, inputs & VB_KEY_B ? (dragging ? col_drag : col_down) : col_up);
        C2D_DrawText(&text_A, C2D_AlignCenter, tVBOpt.TOUCH_AX, tVBOpt.TOUCH_AY - 12, 0, 0.7, 0.7);
        C2D_DrawText(&text_B, C2D_AlignCenter, tVBOpt.TOUCH_BX, tVBOpt.TOUCH_BY - 12, 0, 0.7, 0.7);
    } else {
        C2D_DrawRectSolid(tVBOpt.TOUCH_PADX - 16, tVBOpt.TOUCH_PADY - 48, 0, 16*2, 48*2, inputs & VB_KEY_A ? col_drag : col_up);
        C2D_DrawRectSolid(tVBOpt.TOUCH_PADX - 48, tVBOpt.TOUCH_PADY - 16, 0, 48*2, 16*2, inputs & VB_KEY_A ? col_drag : col_up);
        if (!dragging) {
            if (inputs & VB_RPAD_L)
                C2D_DrawRectSolid(tVBOpt.TOUCH_PADX - 48, tVBOpt.TOUCH_PADY - 16, 0, 16*2, 16*2, col_down);
            if (inputs & VB_RPAD_R)
                C2D_DrawRectSolid(tVBOpt.TOUCH_PADX + 16, tVBOpt.TOUCH_PADY - 16, 0, 16*2, 16*2, col_down);
            if (inputs & VB_RPAD_U)
                C2D_DrawRectSolid(tVBOpt.TOUCH_PADX - 16, tVBOpt.TOUCH_PADY - 48, 0, 16*2, 16*2, col_down);
            if (inputs & VB_RPAD_D)
                C2D_DrawRectSolid(tVBOpt.TOUCH_PADX - 16, tVBOpt.TOUCH_PADY + 16, 0, 16*2, 16*2, col_down);
        }
    }

    C2D_DrawRectSolid(320 - 64, 0, 0, 64, 32, C2D_Color32(TINT_R*0.5, TINT_G*0.5, TINT_B*0.5, 255));
    C2D_DrawText(&text_switch, C2D_AlignCenter, 320 - 32, 6, 0, 0.7, 0.7);
}

void guiUpdate(float total_time, float drc_time) {
    static int last_inputs = 0;
    static bool last_fastforward = false;
    static bool last_saving = false;
    int new_inputs = guiGetInput(false);
    if (new_inputs != last_inputs) shouldRedrawMenu = true;
    last_inputs = new_inputs;
    if (last_fastforward != tVBOpt.FASTFORWARD) shouldRedrawMenu = true;
    last_fastforward = tVBOpt.FASTFORWARD;
    if (!!save_thread != last_saving) shouldRedrawMenu = true;
    last_saving = !!save_thread;

    if (!shouldRedrawMenu && !tVBOpt.PERF_INFO)
        return;

    C2D_Prepare();
    C2D_SceneBegin(screen);

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    if (shouldRedrawMenu) {
        shouldRedrawMenu = false;
        C2D_TargetClear(screen, 0);
        drawTouchControls(0);
        // fastforward icon
        if (tVBOpt.FASTFORWARD) C2D_DrawRectSolid(0, 240-32, 0, 32, 32, C2D_Color32(32, 32, 32, 255));
        int col_line = C2D_Color32(64, 64, 64, 255);
        C2D_DrawLine(0, 240-32, col_line, 31.5, 240-32, col_line, 1, 0);
        C2D_DrawLine(31.5, 240-32, col_line, 31.5, 240, col_line, 1, 0);
        int t1l = 8 - 5 * !tVBOpt.PERF_INFO;
        int t1r = 15 - !tVBOpt.PERF_INFO;
        int t2l = 18 - !tVBOpt.PERF_INFO;
        int t2r = 25 + 3 * !tVBOpt.PERF_INFO;
        int tu = 240-28;
        int tm = 240-22 + 6 * !tVBOpt.PERF_INFO;
        int tb = 240-16 + 12 * !tVBOpt.PERF_INFO;
        C2D_DrawTriangle(t1l, tu, col_line, t1l, tb, col_line, t1r, tm, col_line, 0);
        C2D_DrawTriangle(t2l, tu, col_line, t2l, tb, col_line, t2r, tm, col_line, 0);
    }
    
    draw_status_bar(total_time, drc_time);

    if (save_thread) C2D_DrawText(&text_saving, C2D_AlignLeft, 0, 224, 0, 0.5, 0.5);

    C2D_Flush();

	C3D_ColorLogicOp(GPU_LOGICOP_COPY);
}

bool guiShouldPause() {
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    return touch_pos.px < tVBOpt.PAUSE_RIGHT && (touch_pos.px >= 32 || touch_pos.py < 240-32);
}

int guiGetInput(bool do_switching) {
    if ((hidKeysHeld() & KEY_TOUCH) && guiShouldSwitch()) {
        if (do_switching && (hidKeysDown() & KEY_TOUCH)) setTouchControls(!buttons_on_screen);
        return 0;
    }
    touchPosition touch_pos;
    hidTouchRead(&touch_pos);
    if (do_switching) {
        if (touch_pos.px < 32 && touch_pos.py >= 240-32) {
            if ((tVBOpt.FF_TOGGLE ? hidKeysDown() : hidKeysHeld()) & KEY_TOUCH) {
                tVBOpt.FASTFORWARD = !tVBOpt.FASTFORWARD;
            }
            return 0;
        }
    }
    if (touch_pos.px < tVBOpt.PAUSE_RIGHT) {
        return 0;
    }
    if (buttons_on_screen) {
        int axdist = touch_pos.px - tVBOpt.TOUCH_AX;
        int aydist = touch_pos.py - tVBOpt.TOUCH_AY;
        int bxdist = touch_pos.px - tVBOpt.TOUCH_BX;
        int bydist = touch_pos.py - tVBOpt.TOUCH_BY;
        if (axdist*axdist + aydist*aydist < bxdist*bxdist + bydist*bydist)
            return VB_KEY_A;
        else
            return VB_KEY_B;
    } else {
        int xdist = touch_pos.px - tVBOpt.TOUCH_PADX;
        int ydist = touch_pos.py - tVBOpt.TOUCH_PADY;
        if (abs(xdist) >= abs(ydist)) {
            return xdist >= 0 ? VB_RPAD_R : VB_RPAD_L;
        } else {
            return ydist <= 0 ? VB_RPAD_U : VB_RPAD_D;
        }
    }
}