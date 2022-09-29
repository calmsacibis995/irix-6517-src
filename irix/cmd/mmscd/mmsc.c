#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/serialio.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/capability.h>
#include <unistd.h>

#include "oobmsg.h"
#include "mmsc.h"
#include "error.h"
#include "double_time.h"

#define BENCH		0
#define SHOW_DATA	0
#define USE_FIONREAD	0

#define CTL_NAME_OLD	"/hw/machdep/ffsc_control"
#define CTL_NAME_NEW	"/hw/machdep/mmsc_control"
#define CON_NAME_DFL	"/dev/console"

#ifdef SIOC_MMSC_GRAB
#define IOCTL_GRAB	SIOC_MMSC_GRAB
#define IOCTL_RLS	SIOC_MMSC_RLS
#else
#define IOCTL_GRAB	SIOC_FFSC_GRAB
#define IOCTL_RLS	SIOC_FFSC_RLS
#endif

/*
 * MMSC structure (opaque)
 */

struct mmsc_s {
    int			console;
    int			tty_fd, ctl_fd;
    struct termios	ti_old, ti_new;
    int			baud;
};

mmsc_t *mmsc_open(char *name, int baud, int console)
{
    mmsc_t	       *m;
    cap_t		ocap;
    cap_value_t		cap_device_mgt = CAP_DEVICE_MGT;

    debug("mmsc_open: name=%s\n", name);

    m = malloc(sizeof (mmsc_t));

    m->console = console;

    if (name == 0)
	name = CON_NAME_DFL;

    m->tty_fd = -1;
    m->ctl_fd = -1;

    if (m->console) {
	if ((m->ctl_fd = open(CTL_NAME_NEW, O_RDWR)) < 0 &&
	    (m->ctl_fd = open(CTL_NAME_OLD, O_RDWR)) < 0) {
	    warning("Could not open %s: %s\n", CTL_NAME_NEW, STRERR);
	    goto fail;
	}
    }

    if ((m->tty_fd = open(name, O_RDWR)) < 0) {
	warning("Could not open %s: %s\n", name, STRERR);
	goto fail;
    }

    if (tcgetattr(m->tty_fd, &m->ti_old) < 0) {
	warning("Could not get TTY attributes: %s\n", STRERR);
	goto fail;
    }

    m->baud = (int) cfgetispeed(&m->ti_old);

    debug("mmsc_open: tty_fd=%d baud=%d ctl_fd=%d\n",
	  m->tty_fd, baud, m->ctl_fd);

    if (! m->console) {
	m->ti_new			= m->ti_old;

	m->ti_new.c_iflag		= 0;
	m->ti_new.c_oflag		= 0;
	m->ti_new.c_cflag		= CREAD | CLOCAL | CS8;
	m->ti_new.c_lflag		= 0;

	m->ti_new.c_cc[VMIN]	= 0;
	m->ti_new.c_cc[VTIME]	= 1;

	if (baud == 0)
	    baud = m->baud;
	else
	    m->baud = baud;

	if (cfsetispeed(&m->ti_new, (speed_t) baud) < 0 ||
	    cfsetospeed(&m->ti_new, (speed_t) baud) < 0) {
	    warning("Could not set TTY baud rate to %d: %s\n",
		    baud, STRERR);
	    goto fail;
	}

	ocap = cap_acquire(1, &cap_device_mgt);
	if (tcsetattr(m->tty_fd, TCSAFLUSH, &m->ti_new) < 0) {
	    cap_surrender(ocap);
	    warning("Could not set TTY attributes: %s\n", STRERR);
	    goto fail;
	}
	cap_surrender(ocap);
    }

    debug("mmsc_open: done, m=0x%lx\n", m);

    return m;

fail:

    debug("mmsc_open: failed\n");

    if (m->tty_fd >= 0)
	close(m->tty_fd);

    if (m->ctl_fd >= 0)
	close(m->ctl_fd);

    free(m);

    return 0;
}

int mmsc_baud_get(mmsc_t *m)
{
    return m->baud;
}

int mmsc_close(mmsc_t *m)
{
    int			r = 0;

    debug("mmsc_close: m=0x%lx\n", m);

    if (! m->console) {
	if (tcsetattr(m->tty_fd, TCSADRAIN, &m->ti_old) < 0) {
	    warning("Could not set TTY attributes: %s\n", STRERR);
	    r = -1;
	}
    }

    if (m->tty_fd >= 0 && close(m->tty_fd) < 0) {
	warning("Could not close TTY device: %s\n", STRERR);
	r = -1;
    }

    if (m->ctl_fd >= 0 && close(m->ctl_fd) < 0) {
	warning("Could not close MMSC control device: %s\n", STRERR);
	r = -1;
    }

    free(m);

    debug("mmsc_close: r=%d\n", r);

    return r;
}

static char warn_msg[] =
"mmscd: Could not contact system controller on this port.  If there\r\n"
"mmscd: is no MMSC, please turn off mmscd (chkconfig mmscd off).\r\n";

void mmsc_warn(mmsc_t *m)
{
    (void) write(m->tty_fd, warn_msg, strlen(warn_msg));
}

int mmsc_write(mmsc_t *m, u_char *data, int data_len)
{
    int			n, i, r;

    debug("mmsc_write: data=0x%lx len=%d\n", data, data_len);

    for (i = 0; i < data_len; i += n) {
	debug("mmsc_write: length %d\n", data_len - i);

	n = write(m->console ? m->ctl_fd : m->tty_fd,
		  data + i, data_len - i);

	debug("mmsc_write: result n=%d\n", n);

	if (n < 0 && errno == EINTR) {
	    n = 0;
	    continue;
	}

#if SHOW_DATA
	for (r = 0; r < n; r++)
	    debug("mmsc_write: data[%d] = 0x%02x\n", r, data[i + r]);
#endif

	if (n < 0) {
	    warning("Error writing command packet: %s\n", STRERR);
	    r = -1;
	    goto done;
	}
    }

    r = 0;

done:

    debug("mmsc_write: r=%d\n", r);
    return r;
}

/*
 * read_expire
 *
 *   A version of read with an absolute expiration time.  It will
 *   never block.
 */

static int read_expire(int fd, void *buf, int len, double expire)
{
    fd_set		rfs;
    int			r;
#if USE_FIONREAD
    int			nr;
#endif

    FD_ZERO(&rfs);
    FD_SET(fd, &rfs);

    if ((r = double_select(fd + 1, &rfs, 0, 0, expire)) < 0)
	return r;

    if (r == 0) {
	debug("read_expire: timeout\n");
	errno = ETIME;
	return -1;			/* Timed out */
    }

#if USE_FIONREAD
    if ((r = ioctl(fd, FIONREAD, &nr)) < 0)
	return r;

    debug("read_expire: fionread len=%d\n", nr);

    if (len > nr)
	len = nr;
#endif

    return read(fd, buf, len);
}

int mmsc_read(mmsc_t *m, u_char *data, int data_max, int *data_len,
	      double expire)
{
    int			n, i, r;

    debug("mmsc_read: data=0x%lx max=%d\n", data, data_max);

    i = 0;

    while (i < data_max) {
	debug("mmsc_read: length %d\n", data_max - i);

	n = read_expire(m->console ? m->ctl_fd : m->tty_fd,
			data + i, data_max - i, expire);

	debug("mmsc_read: result n=%d\n", n);

	if (n < 0 && errno == EINTR) {
	    n = 0;
	    continue;
	}

#if SHOW_DATA
	for (r = 0; r < n; r++)
	    debug("mmsc_read: data[%d] = 0x%02x\n", r, data[i + r]);
#endif

	if (n < 0) {
	    warning("Error reading response packet: %s\n", STRERR);
	    r = -1;
	    goto done;
	}

	if (n == 0)
	    break;

	i += n;
    }

    if (data_len)
	*data_len = i;

    r = 0;

done:

    debug("mmsc_read: r=%d, len=%d\n", r, i);
    return r;
}

int mmsc_command(mmsc_t *m, int opcode,
		 u_char *data, int data_len)
{
    int			i, r;
    u_char		sum;
    u_char		buf[1024];

    debug("mmsc_command: op=%d data=0x%lx len=%d\n",
	   opcode, data, data_len);

    buf[0] = MMSC_ESCAPE;
    buf[1] = opcode;
    sum = buf[1];
    buf[2] = data_len / 256;
    sum += buf[2];
    buf[3] = data_len % 256;
    sum += buf[3];

    for (i = 0; i < data_len; i++) {
	buf[4 + i] = (u_char) data[i];
	sum += buf[4 + i];
    }

    buf[4 + data_len] = sum;

    if ((r = mmsc_write(m, buf, data_len + 5)) < 0)
	goto done;

    r = 0;

done:

    debug("mmsc_command: r=%d\n", r);
    return r;
}

int mmsc_response(mmsc_t *m, int *status,
		  u_char *data, int data_max, int *data_len,
		  double expire)
{
    int			i, r;
    int			len;
    u_char		sum;
    u_char		buf[256];

    debug("mmsc_response: data=0x%lx max=%d\n", data, data_max);

    /*
     * Discard input up to and including the escape character.
     * If using mmsc_control, that has already been done.
     */

    if (! m->console)
	do {
	    if ((r = mmsc_read(m, buf, 1, 0, expire)) < 0)
		goto done;
	} while (buf[0] != MMSC_ESCAPE);

    /*
     * Read the status and length bytes.
     */

    if ((r = mmsc_read(m, buf, 3, 0, expire)) < 0)
	goto done;

    sum = buf[0] + buf[1] + buf[2];
    len = buf[1] * 256 + buf[2];

    if (len > data_max) {
	r = MMSC_ERROR_RESPLEN;
	goto done;
    }

    if (status)
	*status = buf[0];

    if (data_len)
	*data_len = len;

    if (len > 0 && (r = mmsc_read(m, data, len, 0, expire)) < 0)
	goto done;

    for (i = 0; i < len; i++)
	sum += data[i];

    if ((r = mmsc_read(m, buf, 1, 0, expire)) < 0)
	goto done;

    if (buf[0] != sum) {
	r = MMSC_ERROR_CHECKSUM;
	goto done;
    }

    r = 0;

done:

    debug("mmsc_response: r=%d len=%d\n", r, len);
    return r;
}

static int do_grab(mmsc_t *m)
{
    if (m->console) {
	debug("mmsc_transact: grab\n");
	if (ioctl(m->ctl_fd, IOCTL_GRAB) < 0)
	    return MMSC_ERROR_GRAB;
    }

    return 0;
}

static int do_release(mmsc_t *m)
{
    if (m->console) {
	debug("mmsc_transact: release\n");
	if (ioctl(m->ctl_fd, IOCTL_RLS) < 0)
	    return MMSC_ERROR_RELEASE;
    }

    return 0;
}

int mmsc_transact(mmsc_t *m,
		  int opcode, u_char *cmd_data, int cmd_data_len,
		  u_char *resp_data, int resp_data_max, int *resp_data_len)
{
    int			r;
    char	       *errmsg;
    int			status;
    double		expire;
#if BENCH
    double		t1, t2, t3;
#endif

    debug("mmsc_transact: op=%d cmd_data=0x%lx cmd_len=%d\n",
	   opcode, cmd_data, cmd_data_len);

#if BENCH
    t1 = double_time();
#endif

    if ((r = do_grab(m)) < 0)
	goto done;

    if ((r = mmsc_command(m, opcode,
			  cmd_data, cmd_data_len)) < 0) {
	(void) do_release(m);
	goto done;
    }

    if ((r = do_release(m)) < 0)
	goto done;

#if BENCH
    t2 = double_time();
#endif

    expire = double_time() + MMSC_TIMEOUT;

    if ((r = mmsc_response(m, &status,
			   resp_data, resp_data_max, resp_data_len,
			   expire)) < 0)
	goto done;

#if BENCH
    t3 = double_time();
#endif

    errmsg = 0;

    switch (status) {
    case STATUS_NONE:
	break;
    case STATUS_FORMAT:
	errmsg = "Illegal command format";
	break;
    case STATUS_COMMAND:
	errmsg = "Unknown command opcode";
	break;
    case STATUS_ARGUMENT:
	errmsg = "Illegal argument(s)";
	break;
    case STATUS_CHECKSUM:
	errmsg = "Command checksum error";
	break;
    case STATUS_INTERNAL:
	errmsg = "Internal error";
	break;
    default:
	errmsg = "Unknown error";
	break;
    }

    if (errmsg) {
	warning("Error message from MMSC: %s\n", errmsg);
	r = MMSC_ERROR_MMSC;
	goto done;
    }

    r = 0;

done:

#if BENCH
    debug("mmsc_transact: timing: send(%f) recv(%f)\n",
	  (t2 - t1) * 1000.0, (t3 - t2) * 1000.0);
#endif

    debug("mmsc_transact: r=%d\n", r);
    return r;
}

char *mmsc_errmsg(int code)
{
    switch (code) {
    case MMSC_ERROR_NONE:
	return "No error";
    case MMSC_ERROR_RESPLEN:
	return "Response too long";
    case MMSC_ERROR_TIMEOUT:
	return "Timed out receiving from MMSC";
    case MMSC_ERROR_CHECKSUM:
	return "Response checksum incorrect";
    case MMSC_ERROR_GRAB:
	return "Could not grab system controller device";
    case MMSC_ERROR_RELEASE:
	return "Could not release system controller device";
    case MMSC_ERROR_MMSC:
	return "MMSC generated an error (see SYSLOG)";
    }

    return "Unknown error";
}
