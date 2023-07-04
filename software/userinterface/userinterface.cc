#include "userinterface.h"
#include <stdio.h>
#include <malloc.h>

#ifndef NO_FILE_ACCESS
#include "FreeRTOS.h"
#include "task.h"
#include "tree_browser.h"
#include "tree_browser_state.h"
#include "path.h"
#include "keyboard_usb.h"
#include "json.h"
#ifndef UPDATER
#ifndef RECOVERYAPP
#include "c1541.h"
#endif // RECOVERYAPP
#endif // UPDATER
#endif // NO_FILE_ACCESS

extern "C" void u64_dispatch_usb_hid_status_refresh(void) __attribute__((weak));

/* Help */
static const char *helptext =
        "WASD:       Up/Left/Down/Right\n"
        "Cursor Keys:Up/Left/Down/Right\n"
        "  (Up/Down) Selection up/down\n"
        "  (Left)    Go one level up\n"
        "            leave directory or disk\n"
        "  (Right)   Go one level down\n"
        "            enter directory or disk\n"
        "RETURN:     Selection context menu\n"
        "RUN/STOP:   Leave menu / Back\n"
        "\n"
#if COMMERCIAL
        "F1:         Action Menu\n"
        "F3:         Page up\n"
        "F5:         Page down\n"
        "F7:         Help\n"
#else
        "F1:         Page up\n"
        "F3:         Help\n"
        "F5:         Action Menu\n"
        "F7:         Page down\n"
#endif
        "\n"
        "F2:         Enter Advanced Settings\n"
    #ifndef RECOVERYAPP
        "F4:         Show System Information\n"
      #if 0
        "F6:         CommoServe File Search\n"
      #else
        "F6:         Internet File Search\n"
      #endif
    #endif
        "\n"
        "SPACE:      Select file / directory\n"
        "C= A:       Select all\n"
        "C= N:       Deselect all\n"
        "C= C:       Copy current selection\n"
        "C= V:       Paste selection here\n"
        "C= J:       Swap joystick ports\n"
        "\n"
        "C= X:       C64 Reset (Spiffy!)\n"
        "C= Z:       C64 Reboot (Spiffy!)\n"
        "C= B:       Power Cycle (Spiffy!)\n"
        "C= O:       Power OFF (Spiffy!)\n"
        "\n"
        "HOME:       Enter home directory\n"
        "C= HOME:    Set current dir as home\n"
        "INST:       Delete selected files\n"
        "\n"  
		"Quick seek: Use the keyboard to type\n"
		"            the name to search for:\n"
		"            You can use ? as a\n"
		"            wildcard. When WASD\n"
        "            cursors are enabled,\n"
        "            type with SHIFT pressed.\n"
        "\n"  
    #ifndef RECOVERYAPP
    #endif
        "C= L:       Show Debug Log\n"
#if U64 == 2
        "\nOutside menus the machine can be\n"
        "reset by holding the switch up\n"
        "for 1 second.\n"
#endif
		"\nRUN/STOP to close this window.";

/* Configuration */
static       char *colors[] = { "Retro 64 Blue", "Ultimate Black", "Retro 1", "Retro 2", "Retro 3", "Retro 128 Gray" };
                          
static const char *filename_overflow_squeeze[] = { "None", "Beginning", "Middle", "End" };
static const char *itype[]      = { "Freeze", "Overlay on HDMI" };
static const char *cfg_save[]   = { "No", "Ask", "Yes" };
static const char *navstyles[] = { "Quick Search", "WASD Cursors" };
static const char *off_on[]    = { "Off", "On" };

struct t_cfg_definition user_if_config[] = {
#if U64
    { CFG_USERIF_ITYPE,      CFG_TYPE_ENUM,   "Interface Type",       "%s", itype,   0,  1, 0 },
#endif
#if COMMERCIAL && !RECOVERYAPP
    { CFG_USERIF_NAVIGATION, CFG_TYPE_ENUM,   "Navigation Style",     "%s", navstyles, 0,  1, 1 },
    { CFG_USERIF_COLORSCHEME,CFG_TYPE_ENUM,   "Color Scheme",         "%s", (const char**)colors,  0,  5, 0 },
#else
    { CFG_USERIF_NAVIGATION, CFG_TYPE_ENUM,   "Navigation Style",     "%s", navstyles, 0,  1, 0 },
    { CFG_USERIF_COLORSCHEME,CFG_TYPE_ENUM,   "Color Scheme",         "%s", (const char**)colors,  0,  5, 1 },
#endif
//    { CFG_USERIF_WORDWRAP,   CFG_TYPE_ENUM,   "Wordwrap text viewer", "%s", en_dis,  0,  1, 1 },

#ifndef COMMERCIAL
    { CFG_USERIF_START_HOME, CFG_TYPE_ENUM,   "Enter Home on Startup", "%s", en_dis, 0,  1, 0 },
#endif
    { CFG_USERIF_HOME_DIR,   CFG_TYPE_STRING, "Home Directory",        "%s", NULL, 0, 31, (int)"" },

    { CFG_USERIF_CFG_SAVE,   CFG_TYPE_ENUM,   "Auto Save Config",      "%s", cfg_save, 0, 2, 1 },
    { CFG_USERIF_ULTICOPY_NAME, CFG_TYPE_ENUM, "Ulticopy Uses disk name", "%s", en_dis, 0, 1, 1 },
    { CFG_USERIF_FILENAME_OVERFLOW_SQUEEZE, CFG_TYPE_ENUM, "Filename overflow squeeze", "%s", filename_overflow_squeeze, 0, 3, 0 },
#if COMMERCIAL
    { CFG_USERIF_PRKL_BANNER, CFG_TYPE_ENUM,   "Prkl Banner Hotkeys",   "%s", off_on, 0, 1, 1 },
#endif
    { CFG_TYPE_END,           CFG_TYPE_END,    "", "", NULL, 0, 0, 0 }
};

UserInterface :: UserInterface(const char *title, bool use_logo) : title(title)
{
    initialized = false;
    focus = -1;
    host = NULL;
    keyboard = NULL;
    screen = NULL;
    doBreak = false;
    available = false;
    color_sel_bg = 0;
    filename_overflow_squeeze = 0;
    menu_response_to_action = MENU_NOP;
    logo = use_logo;
    heap_info = false;
    prkl_banner_hotkeys = true;

    logo_title[0] = "\e2    SIXTY FOUR ";
    logo_title[1] = "  \eR\e1\x1a  ULTIMATE  \x1a\er ";
    logo_color[0] = 6;
    logo_color[1] = 2;
    customize();

    register_store(0x47454E2E, "User Interface Settings", user_if_config);
    effectuate_settings();
}

UserInterface :: ~UserInterface()
{
	printf("Destructing user interface..\n");
    do {
        if (ui_objects[focus]) {
            ui_objects[focus]->deinit();
            delete ui_objects[focus];
        }
        focus--;
    } while(focus>=0);
    printf(" bye UI!\n");
}

typedef struct {
    int border;
    int background;
    int foreground;
    int selected;
    int selected_bg;
    int selected_rev;
} t_scheme_colors;

t_scheme_colors schemes[] = {
    { 6, 14,  1, 1, 14, 1 },
    { 0,  0, 12, 1, 6,  0 },
    { 6, 14,  1, 0, 14, 0 },
    { 6, 14, 15, 1, 14, 0 },
    { 6, 14,  0, 1, 14, 0 },
    { 13,11, 15,13, 0,  1 },
    { 0,  0, 15,13, 0,  0 }, // telnet
};

void UserInterface :: effectuate_settings(void)
{
#ifdef KICKSTART
    const t_scheme_colors *scheme = &schemes[0];
#else
    const t_scheme_colors *scheme = logo ? &schemes[cfg->get_value(CFG_USERIF_COLORSCHEME)] : &schemes[6]; // for telnet always use something useful
#endif
    color_border = scheme->border;
    color_fg     = scheme->foreground;
    color_bg     = scheme->background;
    color_sel    = scheme->selected;
    reverse_sel  = scheme->selected_rev;

#if U64
    color_sel_bg = scheme->selected_bg;
#endif
    config_save  = cfg->get_value(CFG_USERIF_CFG_SAVE);
    filename_overflow_squeeze = cfg->get_value(CFG_USERIF_FILENAME_OVERFLOW_SQUEEZE);
    navmode      = cfg->get_value(CFG_USERIF_NAVIGATION);
#if COMMERCIAL
    prkl_banner_hotkeys = cfg->get_value(CFG_USERIF_PRKL_BANNER) != 0;
#else
    prkl_banner_hotkeys = false;
#endif

    if(host && host->is_accessible()) {
        host->set_colors(color_bg, color_border);
        if (screen) {
            set_screen_title();
        }
    }

    // push_event(e_refresh_browser); TODO
}
    
void UserInterface :: init(GenericHost *h)
{
    host = h;
    keyboard = h->getKeyboard();
	screen = h->getScreen();
    initialized = true;
    if (host->is_permanent()) {
    	appear();
    }
}

int UserInterface :: getPreferredType(void)
{
    return cfg->get_value(CFG_USERIF_ITYPE);
}

void UserInterface :: set_screen(Screen *s)
{
    screen = s;
}

void UserInterface :: run_remote(void)
{
    host->take_ownership(this);
    appear();
    available = true;
    while(host->exists()) {
        if (pollFocussed() == MENU_EXIT) {
            available = false;
            break;
        }
        vTaskDelay(3);
    }
    host->release_ownership();
}

void UserInterface :: run(void)
{
    while(1) {
        if (host->exists()) {
            host->checkButton();
            if (host->buttonPush()) {
                run_once();
                continue;
            }
            switch(system_usb_keyboard.getch()) {
            case KEY_SCRLOCK:
            case KEY_F10:
                run_once();
                continue;
            case 0x04: // CTRL-D
                swapDisk();
                break;
            }
        }
        vTaskDelay(3);
    }
}

void UserInterface :: run_once(void)
{
#ifndef NO_FILE_ACCESS

    if (!host->exists())
        return;

    if(buttonDownFor(1000)) {
        swapDisk();
        return;
    }

    host->take_ownership(this);
    if (!host->is_permanent()) {
        appear();
    }

    available = true;
    while(!doBreak) {
        host->checkButton();
        if (!host->exists()) {
            break;
        } else if (host->buttonPush()) {
            if (!host->is_permanent()) {
                available = false;
                release_host();
            }
            host->release_ownership();
            break;
        } else {
            int ret = pollFocussed();
            if (ret != 0)
                printf("Poll Focussed returned %d. DoBreak = %d.\n", ret, doBreak);
            switch(ret) {
            case MENU_NOP:
                break;
            case MENU_HIDE:
            case MENU_EXIT:
                available = false;
                doBreak = true;
                if (!host->is_permanent()) {
                    release_host();
                }
                host->release_ownership();
                break;
            default:
                break;
            }
        }
        vTaskDelay(3);
    }
    doBreak = false;
#endif
}


#ifndef RECOVERYAPP
bool UserInterface :: buttonDownFor(uint32_t ms)
{
    bool down = false;
#ifndef UPDATER
    uint8_t delay = 10;
    TickType_t ticks = delay / portTICK_PERIOD_MS;
    uint32_t elapsed = 0;
    
    while((ioRead8(ITU_BUTTON_REG) & ITU_BUTTONS) & ITU_BUTTON1) {
        vTaskDelay(ticks);
        elapsed+=delay;
    
        if(elapsed > ms) {
            down = true;
            break;
        }
    }
#endif
    return down;
}
#else
bool UserInterface :: buttonDownFor(uint32_t ms)
{
	return false;
}
#endif

void UserInterface :: swapDisk(void)
{
#ifndef RECOVERYAPP
#ifndef UPDATER                  
	C1541* drive = C1541::get_last_mounted_drive();

    if(drive != NULL) {
      SubsysCommand* swap =
        new SubsysCommand(this, drive->getID(), MENU_1541_SWAP, 0,
                          (const char*)NULL, (const char*)NULL);

      swap->execute();
    }
#endif
#endif                                                                
}

void UserInterface :: send_keystroke(int key)
{
    UIObject *obj = ui_objects[focus];
    if (obj) {
        obj->send_keystroke(key);
    }
}

int UserInterface :: pollInactive(void)
{
    return ui_objects[focus]->poll_inactive();
}

void UserInterface :: peel_off(void)
{
    // Peel off will always keep the first object
    if (!focus) {
        return;
    }
    // The underlying object gets focus. If the top object has the cleanup
    // flag set, we destoy it here and it can not be referenced in the
    // calling object.
    ui_objects[focus]->deinit();
    // Note that the object itself still exists, and needs to be cleaned up by the one who created
    // it, unless the auto cleanup flag is set
    if (ui_objects[focus]->needCleanup()) {
        delete ui_objects[focus];
    }
    ui_objects[focus] = NULL;
    focus--;
}

int UserInterface :: pollFocussed(void)
{
	int ret = 0;
    do {
        if (u64_dispatch_usb_hid_status_refresh) {
            u64_dispatch_usb_hid_status_refresh();
        }
        ret = ui_objects[focus]->poll(ret); // param pass chain

        // Stay in the current window configuration
        if(ret == 0)
            break;

        printf("Object level %d returned %d.\n", focus, (int)ret);

        // Pass non-root positive results to underlying object
        if ((ret > 0) && (focus)) {
            peel_off();
            continue; // pass result
        }

        switch (ret) {
        case MENU_CLOSE:
            // close single layer window and pass to caller (might be cancel code)
            peel_off();
            continue; //  pass to upper layer

        case MENU_HIDE:
        case MENU_EXIT:
            printf("MENU HIDE / EXIT.\n");
            // deinitialize everything, and roll back. Caller solves this, as what happens depends on type of UI.
            return ret;
            
        default:
            printf("ERROR: Return code %d not expected here.\n", ret); // could happen for root
            ret = 0;
            break;
        }

    } while(1);
    return ret;
}

void UserInterface :: appear(void)
{
	host->set_colors(color_bg, color_border);
	set_screen_title();
	for(int i=0;i<=focus;i++) {  // build up
		//printf("Going to (re)init objects %d.\n", i);
		ui_objects[i]->init();
		ui_objects[i]->redraw();
	}
}

void UserInterface :: release_host(void)
{
    for(int i=focus;i>=0;i--) {  // tear down
        ui_objects[i]->deinit();
    }
    doBreak = true;
}

bool UserInterface :: is_available(void)
{
    return available;
}

int UserInterface :: activate_uiobject(UIObject *obj)
{
    if(focus < (MAX_UI_OBJECTS-1)) {
        focus++;
        ui_objects[focus] = obj;
        return 0;
    }

    return -1;
}

bool UserInterface :: has_focus(UIObject *obj)
{
    return (ui_objects[focus] == obj);
}

void UserInterface :: set_screen_title()
{
    int width = screen->get_size_x();
    int height = screen->get_size_y();

    if (logo) {
        screen->clear();
        char color_code[3] = "\e6";
        color_code[1] = logo_color[0];
        if (prkl_banner_hotkeys) {
            screen->output("\e1");
            screen->output("X=RST Z=REB");
        } else {
            screen->output(color_code);
            screen->output("\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x1c");
        }
        screen->output(color_code);
        screen->output(logo_title[0]);
        screen->move_cursor(28, 0);
        if (prkl_banner_hotkeys) {
            screen->output("\e1");
            screen->output(" B=CYC O=OFF");
        } else {
            screen->output(color_code);
            screen->output("\er\x1e\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12\x12");
        }
        screen->move_cursor(0, 1);

        color_code[1] = logo_color[1];
        screen->output(color_code);
        if (prkl_banner_hotkeys) {
            screen->output("\x0b\x0b\x0b\x0b" "C=" "\x0b\x0b\x0b\x0b\x1d");
        } else {
            screen->output("\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x1d");
        }
        screen->output(logo_title[1]);
        screen->move_cursor(28, 1);
        screen->output(color_code);
        if (prkl_banner_hotkeys) {
            screen->output("\er\x1f\x0b\x0b\x0b\x0b" "C=" "\x0b\x0b\x0b\x0b\x0b");
        } else {
            screen->output("\er\x1f\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b");
        }
    } else {
        int len = title.length();
        int hpos = (width - len) / 2;

        screen->clear();
        screen->move_cursor(hpos, 0);
        screen->output("\eA");
        screen->output(title.c_str());
        screen->output("\eO");
        screen->move_cursor(0, 1);
        screen->repeat('\002', width);
        screen->move_cursor(0, height-1);
        screen->scroll_mode(false);
        screen->repeat('\002', width);
    }
}

/* Blocking variants of our simple objects follow: */
int  UserInterface :: popup(const char *msg, uint8_t flags)
{
    const char *c_button_names[] = { " Ok ", " Yes ", " No ", " All ", " Cancel " };
    const char c_button_keys[] = { 'o', 'y', 'n', 'a', 'c' };

    UIPopup *pop = new UIPopup(this, msg, flags, 5, c_button_names, c_button_keys);
    pop->init();
    int ret = 0;
    while(!ret && host->exists()) {
        ret = pop->poll(0);
    }
    pop->deinit();
    delete pop;
    return ret;
}
    
int  UserInterface :: popup(const char *msg, int count, const char **names, const char *keys)
{
    UIPopup *pop = new UIPopup(this, msg, (1 << (count + 1))-1, count, names, keys);
    pop->init();
    int ret = 0;
    while(!ret && host->exists()) {
        ret = pop->poll(0);
    }
    pop->deinit();
    delete pop;
    return ret;
}

int UserInterface :: string_box(const char *msg, char *buffer, int maxlen)
{
    UIStringBox *box = new UIStringBox(this, msg, buffer, maxlen);
    box->init();
    screen->cursor_visible(1);
    int ret = 0;
    while(!ret && host->exists()) {
        ret = box->poll(0);
    }
    screen->cursor_visible(0);
    box->deinit();
    delete box;
    return ret;
}

int UserInterface :: string_edit(char *buffer, int maxlen, Window *w, int x, int y)
{
    UIStringEdit *edit = new UIStringEdit(buffer, maxlen);
    edit->init(w, keyboard, x, y, maxlen); // maybe the max len should be limited by the window!
    screen->cursor_visible(1);
    int ret = 0;
    while(!ret && host->exists()) {
        ret = edit->poll(0);
    }
    screen->cursor_visible(0);
    delete edit;
    return ret;
}

int UserInterface :: choice(const char *msg, const char **choices, int count)
{
    UIChoiceBox *box = new UIChoiceBox(this, msg, choices, count);
    box->init();
    screen->cursor_visible(0);
    int ret = 0;
    while(!ret && host->exists()) {
        ret = box->poll(0);
    }
    box->deinit();
    delete box;
    // Return values are 1 based, unless it's an error
    if (!ret && !host->exists()) {
        return MENU_CLOSE;
    }
    return (ret > 0) ? (ret - 1) : ret;
}

void UserInterface :: show_progress(const char *msg, int steps)
{
    status_box = new UIStatusBox(this, msg, steps);
    status_box->init();
}

void UserInterface :: update_progress(const char *msg, int steps)
{
    status_box->update(msg, steps);
}

void UserInterface :: hide_progress(void)
{
    status_box->deinit();
    delete status_box;
}

void UserInterface :: run_editor(const char *text_buf, int max_len)
{
    Editor *edit = new Editor(this, text_buf, max_len);
    edit->init(screen, keyboard);
    int ret = 0;
    while(!ret && host->exists()) {
        ret = edit->poll(0);
    }
    edit->deinit();
    delete edit;
}

void add_hex_byte(char *buf, int offset, uint8_t byte)
{
    char hex_chars[] = "0123456789ABCDEF";
    buf[offset] = hex_chars[(byte >> 4) & 0x0F];
    buf[offset + 1] = hex_chars[byte & 0x0F];
}

void add_hex_word(char *buf, int offset, uint16_t word)
{
    add_hex_byte(buf, offset, (word >> 8) & 0xFF);
    add_hex_byte(buf, offset + 2, word & 0xFF);
}

void UserInterface :: run_hex_editor(const char *text_buf, int max_len)
{
    #define HEX_COL_START 5
    #define TXT_COL_START (HEX_COL_START + (3 * BYTES_PER_HEX_ROW))
    int hex_len = CHARS_PER_HEX_ROW * (max_len / BYTES_PER_HEX_ROW + 1);
    char hex_buf[hex_len + 1];
    for (int i = 0; i < hex_len; i++) {
        hex_buf[i] = ' ';
    }
    int row_offset = 0;
    for (int i = 0; i < max_len; i++) {
        int col = i % BYTES_PER_HEX_ROW;
        // offset and line break
        if (col == 0) {
            if (i > 0) {
                hex_buf[row_offset + CHARS_PER_HEX_ROW - 1] = '\n';
                row_offset += CHARS_PER_HEX_ROW;
            }
            add_hex_word(hex_buf, row_offset, i);
        }
        // data
        unsigned char c = text_buf[i];
        add_hex_byte(hex_buf, row_offset + HEX_COL_START + (3 * col), c);
        // represent all non-printable characters as '.' based on the character set used by firmware version 3.10j
        hex_buf[row_offset + TXT_COL_START + col] = (char) ((c == 0 || c == 8 || c == 10 || c == 13 || (c >=20 && c <= 31) || (c >= 144 && c <= 159)) ? '.' : c);
    }
    run_editor(hex_buf, hex_len);
}

int UserInterface :: enterSelection()
{
#ifndef NO_FILE_ACCESS
	// because we know that the command can only be caused by a TreeBrowser, we can safely cast
	TreeBrowser *browser = (TreeBrowser *)(get_root_object());
	if (browser) {
		if (browser->state) {
			if(browser->state->into2()) {
			    return -1;
			}
			return 0;
		}
	}
#endif
	return -1;
}

QueueHandle_t userMessageQueue = 0;

void UserInterface :: postMessage(const char *msg)
{
    if (!userMessageQueue) {
        userMessageQueue = xQueueCreate(4, sizeof(void *));
    }
    if (!userMessageQueue) {
        return;
    }
    // Message handler becomes owner of object
    mstring *message = new mstring(msg);

    xQueueSend(userMessageQueue, &message, 0);
}

mstring *UserInterface :: getMessage(void)
{
    mstring *msg = NULL;

    if (!userMessageQueue) {
        userMessageQueue = xQueueCreate(4, sizeof(void *));
    }
    if (userMessageQueue) {
        xQueueReceive(userMessageQueue, &msg, 0);
    }
    return msg;
}

int UserInterface :: keymapper(int c, keymap_options_t map)
{
    if (navmode == 1) { // WASD cursors enabled
        if (c >= 'A' && c <= 'Z') {
            c |= 0x20; // make uppercase lowercase
        } else {
            switch(c) {
            case 'w': c = KEY_UP; break;
            case 'a': c = KEY_LEFT; break;
            case 's': c = KEY_DOWN; break;
            case 'd': c = KEY_RIGHT; break;
            }
        }
    }
#if COMMERCIAL
    switch(c) {
    case KEY_F1: c = KEY_TASKS; break;
    case KEY_F3: c = KEY_PAGEUP; break;
    case KEY_F5: c = KEY_PAGEDOWN; break;
    case KEY_F7: c = KEY_HELP; break;
    case KEY_F2: c = KEY_CONFIG; break;
    case KEY_F4: c = KEY_SYSINFO; break;
    case KEY_F6: c = KEY_SEARCH; break;
    }
#else
    switch(c) {
    case KEY_F1: c = KEY_PAGEUP; break;
    case KEY_F3: c = KEY_HELP; break;
    case KEY_F5: c = KEY_TASKS; break;
    case KEY_F7: c = KEY_PAGEDOWN; break;
    case KEY_F2: c = KEY_CONFIG; break;
    case KEY_F4: c = KEY_SYSINFO; break;
    case KEY_F6: c = KEY_SEARCH; break;
    }
#endif
    return c;
}

void UserInterface :: help()
{
    run_editor(helptext, strlen(helptext));
}

#define JSON_GET(j, name) \
    ((j && (j->type() == eObject)) ? ((JSON_Object*) j)->get(name) : NULL)

#define JSON_GET_STRING(obj, name, s) \
    j = JSON_GET(obj, name); \
    if (j && (j->type() == eString)) \
        s = strdup(((JSON_String*) j)->get_string());

#define JSON_GET_INTEGER(obj, name, v, mask) \
    j = JSON_GET(obj, name); \
    if (j && (j->type() == eInteger)) \
        v = ((JSON_Integer*) j)->get_value() & mask;

char* unescape_unicode(char *str) {
    char *src = str, *dst = str;

    while (*src) {
        if (*src == '\\' && *(src + 1) == 'u') {
            char *p = src + 2;
            unsigned int val = 0, digits = 0;

            // Read exactly 4 hex digits
            while (digits < 4) {
                char c = *p++;
                if (c >= '0' && c <= '9') val = (val << 4) + (c - '0');
                else if (c >= 'a' && c <= 'f') val = (val << 4) + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') val = (val << 4) + (c - 'A' + 10);
                else break;
                digits++;
            }

            if ((digits == 4)&&(val <= 0xFF)) {
                *dst++ = (char)(val & 0xFF);
                src = p;
                continue;
            }
        }

        // Normal character
        *dst++ = *src++;
    }

    *dst = '\0';
    return str;
}

void UserInterface :: customize()
{
#ifndef RECOVERYAPP
    const char* branding_directory = "/Flash/config";

    ConfigManager *cm = ConfigManager :: getConfigManager();
    if (cm->get_safe_mode())
        return;

    printf("Checking device branding config...\n");
    FileManager *fm = FileManager::getFileManager();
    size_t bufferSize = 4096;
    char* jsonText = (char*) malloc(bufferSize);
    if (!jsonText)
        return;

    uint32_t jsonTextSize=0;
    JSON *obj = NULL;
    FRESULT fres = fm->load_file(branding_directory, "branding.json", (uint8_t*) jsonText, bufferSize, &jsonTextSize);

    if ((fres != 0)||(jsonTextSize == 0)) {
        free(jsonText);
        return;
    }

    convert_text_to_json_objects(jsonText, (size_t) jsonTextSize, 1024, &obj);
    if (!obj) {
        free(jsonText);
        return ;
    }

    JSON* j = JSON_GET(obj, "logo");
    JSON* t = JSON_GET(j, "title");
    JSON_List* l = (t && (t->type() == eList)) ? (JSON_List*) t : NULL;
    for (int i=0;(l && (i < l->get_num_elements()));i++) {
        JSON* e = (*l)[i];
        if (e && (e->type() == eString)) {
            logo_title[i] = strdup(((JSON_String*) e)->get_string());
            unescape_unicode(logo_title[i]);
        }
    }

    t = JSON_GET(j, "color");
    l = (t && (t->type() == eList)) ? (JSON_List*) t : NULL;
    for (int i=0;(l && (i < l->get_num_elements()));i++) {
        JSON* e = (*l)[i];
        if (e && (e->type() == eInteger))
            logo_color[i] = ((JSON_Integer*) e)->get_value();
    }

    JSON* colorJson = JSON_GET(obj, "color-schemes");
    l = (colorJson && (colorJson->type() == eList)) ? (JSON_List*) colorJson : NULL;
    if (l) {
        for (int i=0;i<l->get_num_elements();i++) {
            JSON* e = (*l)[i];
            if (e && (e->type()==eObject)) {
                JSON_GET_STRING( e, "name",         colors[i]);
                JSON_GET_INTEGER(e, "border",       schemes[i].border,       0xf);
                JSON_GET_INTEGER(e, "background",   schemes[i].background,   0xf);
                JSON_GET_INTEGER(e, "foreground",   schemes[i].foreground,   0xf);
                JSON_GET_INTEGER(e, "selected",     schemes[i].selected,     0xf);
                JSON_GET_INTEGER(e, "selected_bg",  schemes[i].selected_bg,  0xf);
                JSON_GET_INTEGER(e, "selected_rev", schemes[i].selected_rev, 0xf);

                // fg/bg must not be identical, otherwise use defaults
                if (schemes[i].background == schemes[i].foreground) {
                    schemes[i].background = 14;
                    schemes[i].foreground = 1;
                }
                if (schemes[i].selected == schemes[i].selected_bg) {
                    schemes[i].background = 1;
                    schemes[i].foreground = 14;
                }
                // selected must not match inverse of normal colors
                if ((schemes[i].selected == schemes[i].background)&&
                    (schemes[i].selected_bg == schemes[i].foreground)) {
                    schemes[i].selected    = schemes[i].foreground;
                    schemes[i].selected_bg = schemes[i].background;
                }
            }
        }
    }

    delete obj;
    free(jsonText);
#endif
}

void UserInterface :: show_heap_info()
{
    if (!heap_info)
        return;
    // Show heap information, just overwrite the status line.
    // Simple solution to help with leak hunting...
    screen->move_cursor(0, screen->get_size_y()-1);
    struct mallinfo mi = mallinfo();
    char buffer[80];
    sprintf(buffer, "\er\033\035heap: alloc=%-7u, avail=%-7u\e1 ",
            (uint32_t) mi.uordblks, (uint32_t) mi.fordblks);
    screen->output(buffer);
}
