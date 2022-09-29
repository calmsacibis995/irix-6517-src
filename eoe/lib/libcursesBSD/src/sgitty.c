#include "curses.h"

#define BSDTIOCGETP 0x101
#define BSDTIOCSETP 0x102

#define BSDECHO ECHO
#define BSDXTABS XTABS

#undef ECHO
#undef XTABS
#undef TIOCGETP
#undef TIOCSETP

#include <termio.h>

int
ttioctl(int fd, long cmd, void *arg)
{
	struct termios t;
	struct sgttyb *sg;

	if (tcgetattr(fd, &t) < 0)
		return -1;

	switch (cmd) {
		case BSDTIOCGETP:
			sg = (struct sgttyb *)arg;	
			sg->sg_erase = t.c_cc[VERASE];
			sg->sg_kill  = t.c_cc[VKILL];
			sg->sg_flags = 0;
			sg->sg_flags =
				(t.c_iflag & IUCLC ? LCASE : 0) |
				(t.c_oflag & XTABS ? BSDXTABS : 0) |
				(t.c_lflag & ECHO  ? BSDECHO : 0) |
				(t.c_lflag & ICANON ? 0 : CBREAK) |
				(t.c_iflag & ICRNL ? CRMOD : 0) |
				(t.c_oflag & OPOST ? 0 : RAW);
			if (t.c_cc[VMIN] == 1)
				sg->sg_flags |= RAW;
			return 0;

		case BSDTIOCSETP:
			sg = (struct sgttyb *)arg;	
			t.c_cc[VERASE] = sg->sg_erase;
			t.c_cc[VKILL] = sg->sg_kill;

			if (sg->sg_flags & BSDXTABS)
				t.c_oflag |= XTABS;
			else
				t.c_oflag &= ~XTABS;
			if (sg->sg_flags & ECHO)
				t.c_lflag |= ECHO;
			else
				t.c_lflag &= ~ECHO;

			if (sg->sg_flags & CBREAK) {
				t.c_lflag &= ~ICANON;
				t.c_cc[VMIN] = 1;
				t.c_cc[VTIME] = 1;
			} else {
				t.c_lflag |= ICANON;
				t.c_cc[VEOL] = '\n';
				t.c_cc[VEOF] = '\004';
			}

			if (sg->sg_flags & CRMOD) {
				t.c_iflag |= ICRNL;
				t.c_oflag |= ONLCR;
			} else {
				t.c_oflag &= ~ONLCR;
				t.c_iflag &= ~ICRNL;
			}

			if (sg->sg_flags & RAW) {
				t.c_lflag &= ~ICANON;
				t.c_cc[VMIN] = 1;
				t.c_cc[VTIME] = 1;
			}
			else {
				t.c_lflag |= ICANON;
				t.c_cc[VEOL] = '\n';
				t.c_cc[VEOF] = '\004';
			}

			return tcsetattr(fd, TCSAFLUSH, &t) < 0 ? -1 : 0;

		default:
			abort();
	}
	/* NOTREACHED */
}
