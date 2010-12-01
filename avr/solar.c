/*
  (C) 2010 Peter Korsgaard <peter@korsgaard.com>

  1..8 channel pulse counter for solar PV installations through kWh meters
  with an isolated S0 output, like the Eltako Electronics WSZ12B:

  http://www.eltako.com/fileadmin/downloads/en/_bedienung/WSZ12B-65A_4853_gb.pdf

  This code:
  - Measures and debounce channel signals on port B every 2ms
  - For each rising edge detected, updates internal 32bit counter/channel,
    and blink LEDs on port D for debugging
  - Stores internal counters to EEPROM every ~30min if changed
  - Provides text-based serial protocol (9600 8N1) to read/reset/save counters
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>
#include "types.h"
#include "uart.h"

/* number of samples the signal has to be steady before a pulse is detected */
#define DEBOUNCE_SAMPLES 10


/* Number of channels to monitor on port B */
#define CHANNELS 2

/* Port D bit masks for LEDs for each channel */
#define LED0 0x10
#define LED1 0x20

/* EEPROM locations. A double buffer setup is used to guard against corruption
   if power is lost during writes */
#define EEPROM1 ((void*)0x20)
#define EEPROM2 ((void*)0x40)
#define ACTIVE 0x55

struct data {
	u8 active;
	u32 counts[CHANNELS];
};

volatile struct data data;
u8 active_eeprom;
volatile u8 save;

/* debounce input sample bitmask and return bitmask of debounced signals
   output bits = 1 on rising edge detection, 0 otherwise.
   Improved version of algorithm from http://www.ganssle.com/debouncing.pdf */
static u8 debounce(u8 sample)
{
	static u8 cur, pos, buf[DEBOUNCE_SAMPLES];
	u8 i, ones, zeros, pressed;

	buf[pos % sizeof(buf)/sizeof(buf[0])] = sample;
	pos++;

	for (i=1, ones = zeros = buf[0]; i < sizeof(buf)/sizeof(buf[0]); i++) {
		ones  &= buf[i];
		zeros |= buf[i];
	}

	/* we want rising edge */
	pressed = ones & ~cur;

	/* only detect falling edge if DEBOUNCE_SAMPLES zeros */
	cur = ones | (cur & zeros);

	return pressed;
}

ISR(TIMER0_OVF_vect)
{
	static u8 changed, ticks2;
	static u16 ticks;
	u8 pressed, i;

	pressed = debounce(PINB);

	PORTD &= ~(LED0 | LED1);

	for (i=0; i<CHANNELS; i++) {
		if (pressed & 1) {
			changed = 1;
			data.counts[i]++;
			PORTD |= i ? LED0 : LED1;
		}
		pressed >>= 1;
	}

	ticks++;

	if (ticks == 0) {
		ticks2++;
		if (((ticks2 & 0x15) == 0) && changed) {
			save = 1;
			changed = 0;
		}
	}
}

static void init(void)
{
	/* port B input */
	DDRB = 0;
	PORTB = 0;

	/* port D output */
	DDRD = 0xff;
	PORTD = 0;

	/* timer tick every ~2ms */
	TCCR0A = 0b00000000;
	TCCR0B = 0b00000011;
	TIMSK = 0x02;

	uinit();
	sei();
}

static void eeprom_load(void)
{
	eeprom_busy_wait();
	eeprom_read_block((void*)&data, EEPROM1, sizeof(data));

	if (data.active != ACTIVE) {
		eeprom_busy_wait();
		eeprom_read_block((void*)&data, EEPROM2, sizeof(data));
		active_eeprom = 1;
	}

	if (data.active != ACTIVE) {
		memset((void*)data.counts, 0, sizeof(data.counts));
		data.active = ACTIVE;
	}
}

static void eeprom_save(void)
{
	active_eeprom ^= 1;

	eeprom_busy_wait();
	cli();
	eeprom_write_block((void*)&data, active_eeprom ? EEPROM2 : EEPROM1,
			   sizeof(data));
	sei();
}

static u8 sum;

static void putsum(u8 c)
{
	sum += c;
	uputchar(c);
}

static void puthex(u8 c)
{
	static const char hex[] = "0123456789abcdef";

	putsum(hex[c>>4]);
	putsum(hex[c&0xf]);
}

/* print in hexidecimal as that's simpler
   also add a simple checksum to ensure data validity */
static void print_count(u8 ch)
{
	u8 *p = (u8*)&data.counts[ch];
	int i;

	uputchar('a' + ch);
	uputchar('=');

	/* avr is little endian */
	for (sum=0, i=3; i>=0; i--) {
		puthex(p[i]);
	}
	puthex(sum);

	uputchar('\n');
}

int main(void)
{
	char prev = 0;

	init();

	eeprom_load();

	while (1) {
		if (save) {
			save = 0;
			eeprom_save();
		}

		if (upoll()) {
			char c;

			/* simple text-based serial protocol. UART interface
			   is byte based, so use a 1-byte back buffer (prev)
			   to figure out the command on newline - E.G.:
			   a\n -> a=deadbeefxx
			*/
			c = ugetchar();

			if (c == '\n' || c == '\r') {
				switch (prev) {
				case 'a':
				case 'b':
					print_count(prev - 'a');
					break;

				case 'i':
					uprintf("active=%u\n", active_eeprom);
					break;

				case 'r':
					uprintf("resetting\n");
					memset((void*)data.counts, 0,
					       sizeof(data.counts));
					/* fall through */

				case 's':
					save = 1;
					break;

				default:
					uprintf("?\n");
					break;
				}
			}
			prev = c | 0x20; /* enforce lower case */
		}
	}

	return 0;
}
