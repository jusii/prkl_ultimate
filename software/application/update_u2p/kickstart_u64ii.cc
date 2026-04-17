/*
 * kickstart.cc (for C64U)
 * (c) SpiffyCrew, GideonZ, ...
 */

#include "update_common.h"
#include "checksums.h"
#include "i2c_drv.h"
#include "wifi_cmd.h"
#include "product.h"
#include "usb_base.h"
#include "rpc_dispatch.h"

#define REQUIRED_FPGA_VERSION     0x22 // "122"

extern uint32_t _ultimate_app_start;
extern uint32_t _ultimate_app_end;

static void status_callback(void *user)
{
    UserInterface *ui = (UserInterface *)user;
    ui->update_progress(NULL, 1);
}

int window_print(Window *w, const char *fmt, ...)
{
    va_list ap;
    int ret = -1;

    va_start(ap, fmt);
    if(w) {
        ret = _my_vprintf(Window :: _put, (void **)w, fmt, ap);
    }
    va_end(ap);
    return (ret);
}

#ifndef KICKBURN
_Noreturn
void jump_n_run(uint32_t address)
{
    void (*jump_jump)() = (void (*)())address;

    jump_jump();

    // We never get here. Or we're doomed! :(
    while(1)
        ;
}

void launch(Window* window, uint32_t * start, uint32_t * end)
{
    uint32_t* dest        = (uint32_t*) *(start++);
    uint32_t  length      = *(start++);
    uint32_t  run_address = *(start++);
    uint32_t* dest_end    = dest + length;

#ifndef FAST_LAUNCH
    if(user_interface->popup(CONFIRM_MSG " " APPL_VERSION_ASCII "?", BUTTON_YES | BUTTON_NO) == BUTTON_YES)
#endif
    {
        window->move_cursor(13, 9);
        window_print(window, "\033\027" //yellow
                              "LOADING...");

        while (dest < dest_end) {
            *(dest++) = *(start++);
        }

        jump_n_run(run_address);
    }
}
#else // KICKBURN

Flash *flash2 = NULL;

static void _write_protect(Window* window, Flash *flash, int kilobytes)
{
    window_print(window, "Enabling flash write protection...\n");
    flash->protect_configure(kilobytes);
    flash->protect_enable();
}

bool burn(Window* window)
{
    if ((!flash2) ||
        (user_interface->popup(CONFIRM_MSG " " APPL_VERSION_ASCII "?", BUTTON_YES | BUTTON_NO) != BUTTON_YES)) {
        return false;
    }

    window->move_cursor(0, 7);
    window_print(window, "\033\021"); // WHITE

    flash2->protect_disable();
    bool Ok = flash_buffer_at(flash2, window, 0x220000, false, &_ultimate_app_start,  &_ultimate_app_end,  "V1.0", "\033\021" APPL_NAME);
    if (Ok)
        window_print(window, "Programming OK!  \n");
    else
        window_print(window, "\n");

    _write_protect(window, flash2, 4096);
    window_print(window, "\n");
    if (Ok)
        window_print(window, "\033\021Success! :)\n\n");
    else {
        window_print(window, "\033\022Flash programming has failed! :(\n\n");
        while (1);
    }
    return true;
}
#endif // KICKBURN

static void power_cycle(Window* window)
{
#if U64 == 1
    window_print(window, "\033\022Turning OFF machine in 5 seconds....\n");

    wait_ms(5000);
    U64_POWER_REG = 0x2B;
    U64_POWER_REG = 0xB2;
    U64_POWER_REG = 0x2B;
    U64_POWER_REG = 0xB2;
    window_print(window, "You shouldn't see this!\n");
#elif U64 == 2
    window_print(window, "\033\022Restarting in ");

    for (int i=5;i>=1;i--) {
       char msg[5] = "0 ";
       msg[0] = '0'+i;
       window_print(window, msg);
       wait_ms(1000);
    }
    window_print(window, "...");

    wifi_machine_reboot();
    wait_ms(1000);
    window_print(window, "You shouldn't see this!\n");
#else
    window_print(window, "\nPLEASE TURN OFF YOUR MACHINE.\n");
#endif

    while(1)
        ;
}

void init_esp32(void)
{
    static bool done=false;
    if (done)
        return;
    done = true;
    esp32.EnableRunMode();
    vTaskDelay(200);
    wifi_command_init();
}

bool check_esp32(Window* window)
{
    uint16_t major = 0, minor = 0;
    char moduleName[32];
    BaseType_t module_detected;

    init_esp32();
    module_detected = wifi_detect(&major, &minor, moduleName, 32);
    module_detected = wifi_detect(&major, &minor, moduleName, 32); // second time should pass
    if (module_detected == pdTRUE) {
        window_print(window, "ESP32:     %d.%d\n", major, minor);

        if ((major != IDENT_MAJOR) || (minor != IDENT_MINOR)) {
            window_print(window, "Incorrect ESP32 version: %d.%d required!\n", IDENT_MAJOR,IDENT_MINOR);
            return false;
        }
        return true;
    }

    window_print(window, "ESP32 not detected!\n");
    return false;
}

void do_kickstart(void)
{
    bool Ok=true;

    setup("\033\025** Ultimate Kickstart **\n\033\037");

    Window* window = new Window(screen, 0, 2, screen->get_size_x(), screen->get_size_y() - 3);
    window->draw_border();

    usb2.initHardware();

    window->move_cursor(9, 17);
    window_print(window, "\033\037" //gray
                         "Unofficial firmware!");
#ifdef KICKBURN
    window->move_cursor(11, 18);
    window_print(window, "\033\022" //red
                         "Programming FLASH!");
    window->move_cursor(2, 19);
    window_print(window, "(To revert, run original updater)");
#else
    window->move_cursor(9, 18);
    window_print(window, "Loading to RAM only.");
    window->move_cursor(2, 19);
    window_print(window, "(Power-cycling restores original)");
#endif

    window->move_cursor(0, 0);
    window_print(window,   "\033\021" // WHITE
                           "Hardware:  %s\n", /*(!U64II_BLACKBOARD) ? "Commodore 64 Ultimate" : */getProductString());
    //window->move_cursor(0, 1);
    window_print(window,   "Revision:  %s\n", getBoardRevision());
    //window->move_cursor(0, 2);

    unsigned int fpgav = getFpgaVersion();
    window_print(window,     "FPGA:      1%02X\n",fpgav);
    //window->move_cursor(0, 3);

    if (fpgav != REQUIRED_FPGA_VERSION) {
        window_print(window,     "\033\022Mismatching FPGA: 1%02X.\n", fpgav);
        window_print(window,     "FPGA 1%02X required.\n", REQUIRED_FPGA_VERSION);

        user_interface->popup("Reboot required", BUTTON_OK);
        //window->move_cursor(0, 6);
        Ok = false;
    }
    else
    {
#ifdef KICKBURN
        if (Ok) {
          flash2 = get_flash();
          window_print(window, "Flash:     %s\n", flash2->get_type_string());
        }
#endif
    }

    if (Ok) {
       Ok = check_esp32(window);
    }

    if (Ok) {
        /* Extra check on the loaded images */
        const char *check_error = "\033\022Bad checksum!\033\037\n";
        const char *check_ok = "OK\n";

        window_print(window, "\033\021" // WHITE
                             "Checksum:  ");
    
        Ok = (calc_checksum((uint8_t *)&_ultimate_app_start, (uint8_t *)&_ultimate_app_end) == CHK_ultimate_app);
        window_print(window, Ok ? check_ok : check_error);

        if (Ok) {
#ifdef KICKBURN
            if (!burn(window))
            {
                window->move_cursor(0, 7);
                window_print(window, "\033\022" //red
                                     "Aborted!\n\n");
            }
#else
            launch(window, (uint32_t *)&_ultimate_app_start, (uint32_t *)&_ultimate_app_end);
            window->move_cursor(0, 7);
            window_print(window, "\033\022" //red
                                 "Aborted!\n\n");
#endif
        }
    }

    init_esp32();
    power_cycle(window);
}

extern "C" int ultimate_main(int argc, char *argv[])
{
    i2c->enable_scan(true, false);
    do_kickstart();
    return 0;
}

extern "C" {
int _close(int file)
{
    return -1;
}

ssize_t _write(int file, const void *ptr, size_t len)
{
    return (size_t)0; // nothing sent
}

ssize_t _read(int file, void *ptr, size_t len)
{
    return 0;
}

off_t _lseek(int file, off_t ptr, int dir)
{
    return 0;
}

}
