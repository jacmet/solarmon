/*
  (C) 2010 Peter Korsgaard <peter@korsgaard.com>

  PC helper program for access to serial protocol or AVR running pulse counter
  firmware.

  Used with something like:
  solarget <serial-device> <channel>

  Where channels are numbered from a..z, E.G.
  solarget /dev/ttyUSB0 a
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define SPEED B9600
#define TEXTLEN 12 /* length in bytes of reply msg */
#define MAXTRIES 10

static void writel(int fd, const char *s)
{
	int len = strlen(s);

	while (len) {
		int ret;

		ret = write(fd, s, len);
		switch (ret) {
		case -1:
			perror("write");
			/* fallthrough */
		case 0:
			return;

		default:
			len -= ret;
			s += ret;
		}
	}
}

static void readl(int fd, char *s, int len)
{
	while (len > 1) {
		char *p;
		int ret;

		ret = read(fd, s, len);
		switch (ret) {
		case -1:
			perror("read");
			/* fallthrough */
		case 0:
			*s = 0;
			return;

		default:
			p = memchr(s, '\n', ret);
			if (p) {
				*p = 0;
				return;
			}

			len -= ret;
			s += ret;
		}
	}
	*s = 0;
}

int main(int argc, char **argv)
{
	struct termios oldts, ts;
	char buf[256];
	int fd, tries = MAXTRIES;

	if (argc < 3) {
		fprintf(stderr, "argument missing - %s <device>\n",
			argv[0]);
		return 1;
	}

	fd = open(argv[1], O_RDWR);
	if (fd == -1) {
		perror(argv[1]);
		return 2;
	}

	tcgetattr(fd, &oldts);
	tcgetattr(fd, &ts);
	ts.c_lflag = ICANON;
	ts.c_iflag = IGNCR;
	ts.c_oflag = ONLRET;
	cfsetspeed(&ts, SPEED);
	tcsetattr(fd, TCSAFLUSH, &ts);

	while (tries--) {
		buf[0] = argv[2][0];
		buf[1] = '\n';
		buf[2] = 0;
		writel(fd, buf);
		readl(fd, buf, sizeof(buf));

		if (strlen(buf) == TEXTLEN) {
			char *endp;
			long val;
			int i, sum;

			/* skip (a|b)= */
			val = strtol(&buf[2], &endp, 16);
			if (*endp) {
				fprintf(stderr, "invalid number '%s'\n", buf);
				continue;
			}

			/* cksum only over the number part (not a|b= or crc) */
			for (sum=0, i=2; i<(TEXTLEN-2); i++)
				sum += buf[i];

			if ((sum & 0xff) != (val & 0xff)) {
				fprintf(stderr,
					"invalid crc (0x%02x vs 0x%02x)\n",
					(sum & 0xff), (int)(val & 0xff));
				continue;
			}

			/* strip cksum */
			val >>= 8;

			/* 2 pulses / watt-hour */
			printf("%ld\n", val/2);

			return 0;
		}
	}

	return 0;
}
