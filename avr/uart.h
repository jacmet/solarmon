#ifndef _UART_H_
#define _UART_H_

#include <avr/pgmspace.h>
#include "types.h"

#define BAUD 9600

void uinit();

u8 ugetchar(void);

void uputchar(u8 data);

int upoll(); /* is char waiting? */

void uprintf_p(const PGM_P fmt, ...);

#define uprintf(fmt, ...) do { static const char __fmt__[] PROGMEM = fmt; \
		                  uprintf_p(__fmt__, ##__VA_ARGS__); } while (0)

#endif /* _UART_H_ */
