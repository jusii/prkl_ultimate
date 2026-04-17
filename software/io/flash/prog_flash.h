/*
 * prog_flash.h
 *
 *  Created on: May 19, 2016
 *      Author: gideon
 */

#ifndef PROG_FLASH_H_
#define PROG_FLASH_H_

#include "flash.h"
#include "screen.h"

bool flash_buffer(Flash *flash, Window *window, int id, void *buffer, void *buf_end, const char *version, const char *descr);
bool flash_buffer_at(Flash *flash, Window *window, int address, bool header, void *buffer, void *buf_end, const char *version, const char *descr);
bool flash_buffer_length(Flash *flash, Window *window, int address, bool header, void *buffer, uint32_t length, const char *version, const char *descr);

#endif /* PROG_FLASH_H_ */
