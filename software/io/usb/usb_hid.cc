#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "itu.h"
#include "dump_hex.h"
#include "integer.h"
#include "usb_hid.h"
#include "profiler.h"
#include "FreeRTOS.h"
#include "task.h"
#include "keyboard_usb.h"
#include "u64.h"

// USB mouse → 1351 scaling. Written by U64Config::effectuate_settings().
// Divisor is 1/2/4/8; auto-scale may raise the effective divisor when the
// USB report deltas are too large for the 1351's 7-bit quadrature to track.
uint8_t g_usb_mouse_divisor  = 1;
uint8_t g_usb_mouse_autoscale = 1;

// Entry point for call-backs.
void UsbHidDriver_interrupt_callback(void *object) {
	((UsbHidDriver *)object)->interrupt_handler();
}

/*********************************************************************
/ The driver is the "bridge" between the system and the device
/ and necessary system objects
/*********************************************************************/
// tester instance
FactoryRegistrator<UsbInterface *, UsbDriver *> hid_tester(UsbInterface :: getUsbDriverFactory(), UsbHidDriver :: test_driver);

UsbHidDriver :: UsbHidDriver(UsbInterface *intf) : UsbDriver(intf)
{
    device = NULL;
    host = NULL;
    interface = intf;
    irq_in = 0;
    irq_transaction = 0;
    keyboard = false;
    mouse = false;
    mouse_x = mouse_y = 0;
    mouse_rem_x = mouse_rem_y = 0;
    mouse_auto_div = 1;
    mouse_calm_count = 0;
}

UsbHidDriver :: ~UsbHidDriver()
{
}

UsbDriver * UsbHidDriver :: test_driver(UsbInterface *intf)
{
	if (intf->getInterfaceDescriptor()->interface_class != 3) {
		return NULL;
	}
	printf("** USB HID Device found!\n");
	return new UsbHidDriver(intf);
}

void UsbHidDriver :: install(UsbInterface *intf)
{
	UsbDevice *dev = intf->getParentDevice();
	printf("Installing '%s %s'\n", dev->manufacturer, dev->product);
    host = dev->host;
    interface = intf;
    device = intf->getParentDevice();
    
	dev->set_configuration(dev->get_device_config()->config_value); // not supposed to be here!

	dev->set_interface(interface->getInterfaceDescriptor()->interface_number,
			interface->getInterfaceDescriptor()->alternate_setting);

	int len = 0;
	uint8_t *reportDescriptor = intf->getHidReportDescriptor(&len);
	if (reportDescriptor) {
	    dump_hex_relative(reportDescriptor, len);
	}

	struct t_endpoint_descriptor *iin = interface->find_endpoint(0x83);

    host->initialize_pipe(&ipipe, device, iin);
	ipipe.Interval = 160; // 50 Hz
	ipipe.Length = 16; // just read 8 bytes max
	ipipe.MaxTrans = 16; // iin->max_packet_size;
	ipipe.buffer = this->irq_data;

	irq_transaction = host->allocate_input_pipe(&ipipe, UsbHidDriver_interrupt_callback, this);

    uint8_t c_set_idle[]     = { 0x21, 0x0A, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 };
    uint8_t c_set_protocol[] = { 0x21, 0x0B, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00 };
    c_set_idle[4] = interface->getInterfaceDescriptor()->interface_number;
    c_set_protocol[4] = interface->getInterfaceDescriptor()->interface_number;

    if (interface->getInterfaceDescriptor()->sub_class == 1) {
		if (interface->getInterfaceDescriptor()->protocol == 1) {
			printf("Boot Keyboard found!\n");
			keyboard = true;

            host->control_exchange(&dev->control_pipe, c_set_protocol, 8, NULL, 0);
            host->control_exchange(&dev->control_pipe, c_set_idle, 8, NULL, 0);
			host->resume_input_pipe(irq_transaction); // start polling
		} else if (interface->getInterfaceDescriptor()->protocol == 2) {
			printf("Boot Mouse found (maxtrans = %d)!\n", iin->max_packet_size);
            host->control_exchange(&dev->control_pipe, c_set_protocol, 8, NULL, 0);
            host->control_exchange(&dev->control_pipe, c_set_idle, 8, NULL, 0);
            mouse = true;

#if USE_HID_REPORT
            HidReportParser parser;
            HidItemList result;
            parser.decode(&result, reportDescriptor, len);
            mouse = true;
            mouse &= result.getInputItem(0x10002, 0x90001, rep_button1); // left button
            mouse &= result.getInputItem(0x10002, 0x90002, rep_button2); // right button
            mouse &= result.getInputItem(0x10002, 0x90003, rep_button3); // middle button
            mouse &= result.getInputItem(0x10002, 0x10030, rep_mouse_x); // X
            mouse &= result.getInputItem(0x10002, 0x10031, rep_mouse_y); // Y
#endif

#if U64
            C64_MOUSE_EN_1 = mouse ? 1 : 0;
#endif
            host->resume_input_pipe(irq_transaction); // start polling
		}
    } else {
            // host->resume_input_pipe(irq_transaction); // start polling
			// Another HID device which we cannot decode just yet
			// Note: Polling is not started!
	}
}

void UsbHidDriver :: disable()
{
	host->free_input_pipe(irq_transaction);
#if U64
    C64_MOUSE_EN_1 = 0;
    C64_JOY1_SWOUT = 0x1F;
#endif
}

void UsbHidDriver :: deinstall(UsbInterface *intf)
{
    printf("HID deinstalled.\n");
}

void UsbHidDriver :: poll(void)
{
}

void UsbHidDriver :: interrupt_handler()
{
    int data_len = host->getReceivedLength(irq_transaction);

    // printf("I%d: ", interface->getNumber());
    // for(int i=0;i<data_len;i++) {
    //     printf("%b ", irq_data[i]);
    // } printf("\n");

    if (keyboard) { // keyboard
		system_usb_keyboard.process_data(irq_data);
	} else if (mouse) { // mouse
        uint8_t mouse_joy = 0x1F;
        int xx, yy;

#if USE_HID_REPORT
        if (HidReport::getValueFromData(irq_data, rep_button1)) mouse_joy &= ~0x10;
        if (HidReport::getValueFromData(irq_data, rep_button2)) mouse_joy &= ~0x01;
        if (HidReport::getValueFromData(irq_data, rep_button3)) mouse_joy &= ~0x02;
        xx = HidReport::getValueFromData(irq_data, rep_mouse_x);
        yy = HidReport::getValueFromData(irq_data, rep_mouse_y);
#else
        if (irq_data[0] & 1) mouse_joy &= ~0x10;
        if (irq_data[0] & 2) mouse_joy &= ~0x01;
        if (irq_data[0] & 4) mouse_joy &= ~0x02;
        xx = (int8_t)irq_data[1];
        yy = (int8_t)irq_data[2];
#endif

        // 1351 carries 7-bit signed quadrature, so the C64 driver misreads
        // any inter-poll delta > 63 as reverse motion. Pick the smallest
        // power-of-2 divisor that keeps |delta|/div under ~50 (safety margin
        // below 63). Step up immediately on the offending report; step down
        // gradually after a calm period so we don't oscillate.
        int mag_x = (xx < 0) ? -xx : xx;
        int mag_y = (yy < 0) ? -yy : yy;
        int mag   = (mag_x > mag_y) ? mag_x : mag_y;

        int div;
        if (g_usb_mouse_autoscale) {
            uint8_t needed = 1;
            if      (mag > 200) needed = 8;
            else if (mag > 100) needed = 4;
            else if (mag >  50) needed = 2;

            if (needed > mouse_auto_div) {
                mouse_auto_div = needed;
                mouse_calm_count = 0;
            } else if (mag < 25 && mouse_auto_div > 1) {
                if (++mouse_calm_count >= 500) {
                    mouse_auto_div >>= 1;
                    mouse_calm_count = 0;
                }
            } else {
                mouse_calm_count = 0;
            }
            div = mouse_auto_div;
        } else {
            mouse_auto_div = 1;
            mouse_calm_count = 0;
            div = g_usb_mouse_divisor;
        }
        if (div < 1) div = 1;

        mouse_rem_x += xx;
        mouse_rem_y -= yy;
        mouse_x += mouse_rem_x / div;
        mouse_y += mouse_rem_y / div;
        mouse_rem_x %= div;
        mouse_rem_y %= div;

#if U64
        C64_JOY1_SWOUT = mouse_joy;
        C64_PADDLE_1_X = mouse_x & 0x7F;
        C64_PADDLE_1_Y = mouse_y & 0x7F;
#else
        printf("Mouse: %4x,%4x %b\n", mouse_x, mouse_y, mouse_joy);
#endif

    } else {
	    printf("HID (ADDR=%d) IRQ data: ", device->current_address);
	    for(int i=0;i<data_len;i++) {
	        printf("%b ", irq_data[i]);
	    } printf("\n");
	}
    host->resume_input_pipe(this->irq_transaction);
}

void UsbHidDriver :: pipe_error(int pipe) // called from IRQ!
{
	printf("UsbHid ERROR on IRQ pipe.\n");
}

