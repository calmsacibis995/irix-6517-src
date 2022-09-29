/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Memory mapped file test.  Test correct functioning of memory mapped
 * files with ftruncate.
 *
 * mmap_ftrunc [-vVc] [-p processes] [iterations]
 *
 * -v		validate the file on each pass
 * -V		be verbose about operations in MDupdate_test
 * -c		catch SIGBUS and SIGSEGV and abort
 * -p processes	use the number of processes specified by processes (the
 *		default is 1)
 * iterations	test passes (default is infinite)
 */
#include <sys/types.h>
#include <bstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <assert.h>
#include <time.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <signal.h>
#include <assert.h>
#include <siginfo.h>
#include <ucontext.h>
#include <sys/wait.h>

#define MAX_LINE_LEN	4096
#define MAX_DATA_LEN	1024
#define MIN_DATA_LEN	32
#define MAX_SIGLEN		6
#define MIN_SIGLEN		2

#define PID_FIELD		4
#define DATALEN_FIELD	5
#define DATA_FIELD		6

#define FIELD_DELIMITER	'|'

#define OPTION_STRING	"cVvp:"
#define TESTFILE_NAME	"mmapt"

#define OUTBUFFERS		6
#define OUTBUFFER_LEN	BUFSIZ

static char Data[MAX_DATA_LEN];
static char Data_value = ' ';
static char *Keys[] = {
	"key0",
	"key1",
	"key2",
	"key3",
	"key4",
	"key5",
	"key6",
	"key7",
	"key8",
	"key9"
};
#define KEYS			10

static char *Names[] = {
	"nm0",
	"nm1",
	"nm2",
	"nm3"
};
#define NAMES			4

static int Sigsegv = 0;
static int Verbose = 0;
static int Outbuf_index = 0;
static int Wrapped = 0;
static char *Output_buffer[OUTBUFFERS];
static pid_t Parent = 0;
static int Recid = 0;

static char *
code_to_str(int sig, int code)
{
	static char str[32];
	char *codestr = str;

	switch (sig) {
		case SIGSEGV:
			switch (code) {
				case SEGV_MAPERR:
					codestr = "SEGV_MAPERR";
					break;
				case SEGV_ACCERR:
					codestr = "SEGV_ACCERR";
					break;
				default:
					sprintf(codestr, "code %d", code);
			}
			break;
		case SIGBUS:
			switch (code) {
				case BUS_ADRALN:
					codestr = "BUS_ADRALN";
					break;
				case BUS_ADRERR:
					codestr = "BUS_ADRERR";
					break;
				case BUS_OBJERR:
					codestr = "BUS_OBJERR";
					break;
				default:
					sprintf(codestr, "code %d", code);
			}
			break;
		default:
			sprintf(codestr, "code %d", code);
	}
	return(codestr);
}

static void
signal_handler( int sig, siginfo_t *sip, ucontext_t *up)
{
	switch (sig) {
		case SIGSEGV:
			fprintf(stderr,
				"SIGSEGV pid %d: %s (%d), %s at 0x%x, cause 0x%x\n",
				getpid(),
				sip->si_errno ? strerror(sip->si_errno) : "No error",
				sip->si_errno, code_to_str(sig, sip->si_code),
				(int)up->uc_mcontext.gregs[CTX_EPC],
				(int)up->uc_mcontext.gregs[CTX_CAUSE]);
			abort();
			break;
		case SIGBUS:
			fprintf(stderr,
				"SIGBUS pid %d: %s (%d), %s at 0x%x, cause 0x%x\n",
				getpid(),
				sip->si_errno ? strerror(sip->si_errno) : "No error",
				sip->si_errno, code_to_str(sig, sip->si_code),
				(int)up->uc_mcontext.gregs[CTX_EPC],
				(int)up->uc_mcontext.gregs[CTX_CAUSE]);
			abort();
			break;
		default:
			fprintf(stderr,
				"Pid %d caught signal %d, error %d, code %d\n",
				getpid(), sig, sip->si_errno, sip->si_code);
	}
	return;
}

static void
MDupdate_init(char c)
{
	int i;

	Data_value = c;
	for (i = 0; i < MAX_DATA_LEN; i++) {
		Data[i] = Data_value;
	}
}

static char *
locate_delimiter(char *entry, int delim)
{
	char *delp;

	if (delim < 1) {
		return(NULL);
	}
	/*
	 * locate the desired delimiter
	 */
	for (delp = entry;
		delim && (delp = strchr(delp + 1, FIELD_DELIMITER));
		delim--)
			;
	return(delp);
}

static int
get_integer_field(char *entry, int number)
{
	char *end;
	char *token;
	int value;

	if (!(end = locate_delimiter(entry, number))) {
		fprintf(stderr,
			"get_integer_field (line %d, pid %d): Missing delimiter %d\n",
			__LINE__, getpid(), number);
		return(-1);
	}
	/*
	 * put a NULL in place of the delimiter to isolate the pid string
	 */
	*end = '\0';
	token = strrchr(entry, FIELD_DELIMITER);
	if (!token) {
		token = entry;
	}
	/*
	 * translate the pid from ASCII to integer
	 */
	value = atoi(token + 1);
	*end = FIELD_DELIMITER;
	return(value);
}

static int
valid_pid(char *entry, int processes)
{
	pid_t entry_pid;
	pid_t pid;

	/*
	 * get the pid from the entry
	 */
	entry_pid = (pid_t)get_integer_field(entry, PID_FIELD);
	if (entry_pid == (pid_t)-1) {
		fprintf(stderr, "valid_pid (line %d, pid %d): invalid pid %d\n",
			__LINE__, getpid(), entry_pid);
		return(0);
	}
	/*
	 * if there is only one process, the pid should match what is returned
	 * by getpid
	 */
	if (processes == 1) {
		pid = getpid();
		if (pid != entry_pid) {
			fprintf(stderr,
				"valid_pid (line %d, pid %d): invalid pid, %d should be %d\n",
				__LINE__, getpid(), entry_pid, pid);
			return(0);
		}
	} else if (entry_pid <= 0) {
		fprintf(stderr,
			"valid_pid (line %d, pid %d): invalid pid, %d should be greater "
			"than 0\n", __LINE__, getpid(), entry_pid);
		return(0);
	}
	return(1);
}

static int
valid_data(char *entry, int processes)
{
	char data_char;
	pid_t entry_pid;
	int datalen;
	char *datap;
	char *end;

	entry_pid = (pid_t)get_integer_field(entry, PID_FIELD);
	if (entry_pid == (pid_t)-1) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Invalid data, cannot get entry "
			"pid\n", __LINE__, getpid());
		return(0);
	}
	datalen = get_integer_field(entry, DATALEN_FIELD);
	if (datalen == -1) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Invalid data, cannot get data "
			"length\n", __LINE__, getpid());
		return(0);
	}
	if (datalen > MAX_DATA_LEN) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Data length %d too large\n",
			__LINE__, getpid(), datalen);
		return(0);
	}
	if (datalen < MIN_DATA_LEN) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Data length %d too short\n",
			__LINE__, getpid(), datalen);
		return(0);
	}
	if (datalen > strlen(entry)) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Data length %d longer than entry\n",
			__LINE__, getpid(), datalen);
		return(0);
	}
	if (!(end = locate_delimiter(entry, DATA_FIELD))) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Missing delimiter %d\n", __LINE__,
			getpid(), DATA_FIELD);
		return(0);
	}
	*end = '\0';
	datap = strrchr(entry, FIELD_DELIMITER);
	assert(datap);
	datap++;
	if (datalen != strlen(datap)) {
		fprintf(stderr,
			"valid_data (line %d, pid %d): Data length %d incorrect "
			"(should be %d)\n", __LINE__, getpid(), datalen,
			strlen(datap));
		*end = FIELD_DELIMITER;
		return(0);
	}
	for (data_char = *datap; *datap; datap++) {
		if (data_char != *datap) {
			fprintf(stderr,
				"valid_data (line %d, pid %d): Invalid data (char %c)\n",
				__LINE__, getpid(), *datap);
			*end = FIELD_DELIMITER;
			return(0);
		}
	}
	*end = FIELD_DELIMITER;
	return(1);
}

static int
valid_signature(char *entry)
{
	char *signature;
	int siglen;

	signature = strrchr(entry, FIELD_DELIMITER);
	if (!signature) {
		fprintf(stderr,
			"valid_signature (line %d, pid %d): Missing signature\n",
			__LINE__, getpid());
		return(0);
	}
	signature++;
	siglen = strlen(signature);
	if (siglen > MAX_SIGLEN) {
		fprintf(stderr, "valid_signature (line %d, pid %d): Long signature "
			"(%d greater than %d)\n", __LINE__, getpid(), siglen, MAX_SIGLEN);
		return(0);
	}
	if (siglen < MIN_SIGLEN) {
		fprintf(stderr,
			"valid_signature (line %d, pid %d): Short signature (%d less "
			"than %d)\n", __LINE__, getpid(), siglen, MIN_SIGLEN);
		return(0);
	}
	if ((*signature != '#') || (*(signature + 1) != '!')) {
		fprintf(stderr,
			"valid_signature (line %d, pid %d): Invalid signature %s\n",
			__LINE__, getpid(), signature);
		return(0);
	}
	return(1);
}

/*
 * validate an entry
 * assumes the entry is terminated by a newline
 */
static int
valid_entry(char *entry, int processes)
{
	char *eol;
	int status = 0;

	eol = strchr(entry, '\n');
	if (eol) {
		if ((int)(eol - entry) < MAX_LINE_LEN) {
			*eol = '\0';
			status = valid_pid(entry, processes) &&
				valid_data(entry, processes) &&
				valid_signature(entry);
			*eol = '\n';
		} else {
			*eol = '\0';
			fprintf(stderr, "valid_entry (line %d, pid %d): Entry too long "
				"(%d larger than %d)\n", __LINE__, getpid(),
				(int)(eol - entry), MAX_LINE_LEN);
			*eol = '\n';
		}
	} else {
		fprintf(stderr, "valid_entry (line %d, pid %d): Missing newline\n",
			__LINE__, getpid());
	}
	return(status);
}

static void
dump_output_buffer(void)
{
	int i;

	if (Wrapped) {
		i = Outbuf_index;
		do {
			fprintf(stderr, "%s\n", Output_buffer[i]);
			i = (i + 1) % OUTBUFFERS;
		} while (i != Outbuf_index);
	} else {
		for (i = 0; i < Outbuf_index; i++) {
			fprintf(stderr, "%s\n", Output_buffer[i]);
		}
	}
}

/*
 * match the key
 * assumes the entry is terminated by a NULL
 */
static int
keymatch(char *key, char *entry)
{
	char *end;
	int match = 0;

	if (end = locate_delimiter(entry, 1)) {
		*end = '\0';
		match = (strcmp(entry, key) == 0);
		*end = FIELD_DELIMITER;
	}
	return(match);
}

/*
 * match the signature
 * assumes the entry is terminated by a NULL
 */
static int
sigmatch(char *sig, char *entry)
{
	char *signature;
	int match = 0;

	if (signature = strrchr(entry, FIELD_DELIMITER)) {
		match = (strcmp(sig, signature + 1) == 0);
	}
	return(match);
}

/*
 * access the specified file using the read/write interface
 */
int
MDupdate_access(char *filename, int processes)
{
	int c;
	int eof;
	int i;
	off_t offset;
	char entry[MAX_LINE_LEN];
	int entno;
	int status = 1;
	struct flock l;
	FILE *fp;
	int fd;

	/*
	 * Open read/write so we can do file locking
	 */
	fp = fopen(filename, "r+");
	if (fp == NULL) {
		fprintf(stderr,
			"MDupdate_access (line %d, pid %d): %s, fopen error %s\n",
			__LINE__, getpid(), filename, strerror(errno));
		return(0);
	}
	fd = fileno(fp);
	if (processes > 1) {
		l.l_type = F_RDLCK;
		l.l_whence = SEEK_SET;
		l.l_start = l.l_len = 0;
		if (fcntl(fd, F_SETLKW, &l) < 0) {
			fprintf(stderr, "MDupdate_access (line %d, pid %d): %s, "
				"fcntl(F_SETLKW) error %s\n", __LINE__, getpid(), filename,
				strerror(errno));
			if (close(fd) == -1)
				fprintf(stderr, "MDupdate_access (line %d, pid %d): %s, "
					"close error %s\n", __LINE__, getpid(), filename,
					strerror(errno));
			return(0);
		}
	}
	for (eof = 0, status = 1, entno = 1, i = 0, offset = 0; !eof && status; ) {
		switch (c = fgetc(fp)) {
			case EOF:
				/*
				 * If we hit an EOF with a non-empty buffer,
				 * the entry in the buffer is invalid
				 */
				if (i != 0) {
					dump_output_buffer();
					fprintf (stderr,
						"MDupdate_access (line %d, pid %d): %s, "
						"premature EOF at offset %d for entry %d.\n", __LINE__,
						getpid(), filename, offset, entno);
					/*
					 * kill other processes
					 */
					signal(SIGTERM, SIG_IGN);
					kill(-Parent, SIGTERM);
					signal(SIGTERM, SIG_DFL);
					status = 0;
				}
				eof = 1;
				break;
			case '\0':
				dump_output_buffer();
				fprintf (stderr,
					"MDupdate_access (line %d, pid %d): %s, "
					"NULL byte at offset %d for entry %d.\n", __LINE__,
					getpid(), filename, offset, entno);
				/*
				 * kill other processes
				 */
				signal(SIGTERM, SIG_IGN);
				kill(-Parent, SIGTERM);
				signal(SIGTERM, SIG_DFL);
				status = 0;
				break;
			case '\n':
				entry[i] = (char)c;
				/*
				 * validate the entry
				 */
				if (!valid_entry(entry, processes)) {
					dump_output_buffer();
					fprintf (stderr,
						"MDupdate_access (line %d, pid %d): %s, "
						"invalid entry number %d, offset %d.\n", __LINE__,
						getpid(), filename, entno, offset);
					/*
					 * kill other processes
					 */
					signal(SIGTERM, SIG_IGN);
					kill(-Parent, SIGTERM);
					signal(SIGTERM, SIG_DFL);
					status = 0;
				} else {
					/*
					 * reset for next entry
					 */
					entno++;
					i = 0;
				}
				break;
			default:
				entry[i] = (char)c;
				i++;
				if (i == MAX_LINE_LEN) {
					dump_output_buffer();
					fprintf (stderr,
						"MDupdate_access (line %d, pid %d): %s, entry "
						"number %d, offset %d longer than %d.\n", __LINE__,
						getpid(), filename, entno, offset, MAX_LINE_LEN);
					/*
					 * kill other processes
					 */
					signal(SIGTERM, SIG_IGN);
					kill(-Parent, SIGTERM);
					signal(SIGTERM, SIG_DFL);
					status = 0;
				}
				break;
		}
	}
	if (fclose(fp))
		fprintf(stderr,
			"MDupdate_access (line %d, pid %d): %s, close error %s\n",
			__LINE__, getpid(), filename, strerror(errno));
	return(status);
}

/*
 * Close h by locking its dependency file and replacing key's rule with
 * the one in h's table.  We update the locked file in-place to avoid inode
 * allocation and deallocation overhead.  We use mmap to page-flip.
 */
int
MDupdate_test(char *filename, char *key, char *name, int processes)
{
	long offset;
	int status = 1;
	int entno;
	char *timestr;
	int datalen;
	time_t timestamp;
	char *eol;
	struct flock l;
	struct stat stb;
	off_t size;
	char *base = NULL, *limit, *entry, *next;
	char *signature;
	FILE *file;
	pid_t pid;
	int f;

	pid = getpid();
	signature = malloc(strlen(name) + 1);
	/*
	 * Open, lock, and stat the dependency file.
	 */
	f = open(filename, O_CREAT|O_RDWR, 0666);
	if (f == -1) {
		fprintf(stderr, "MDupdate_test (line %d, pid %d): %s, open error %s\n",
			__LINE__, getpid(), filename, strerror(errno));
		status = 0;
		goto test_exit;
	}
	if (processes > 1) {
		l.l_type = F_WRLCK;
		l.l_whence = SEEK_SET;
		l.l_start = l.l_len = 0;
		if (fcntl(f, F_SETLKW, &l) < 0) {
			fprintf(stderr, "MDupdate_test (line %d, pid %d): %s, "
				"fcntl(F_SETLKW) error %s\n", __LINE__, getpid(), filename,
				strerror(errno));
			if (close(f))
				fprintf(stderr,
					"MDupdate_test (line %d, pid %d): %s, close error %s\n",
					__LINE__, getpid(), filename, strerror(errno));
			status = 0;
			goto test_exit;
		}
	}
	/*
	 * must fstat after locking to get correct size
	 */
	if (fstat(f, &stb) < 0) {
		fprintf(stderr, "MDupdate_test (line %d, pid %d): %s, fstat error %s\n",
			__LINE__, getpid(), filename, strerror(errno));
		if (close(f))
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, close error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		status = 0;
		goto test_exit;
	}

	/*
	 * Map the dependency file at base if it is non-empty.
	 */
	size = stb.st_size;
	if (size == 0)
		base = limit = 0;
	else {
		base = mmap(0, size, PROT_READ|PROT_WRITE, MAP_SHARED, f, 0);
		if (base == (char *)-1) {
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, mmap error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		 	(void) close(f);
			status = 0;
			goto test_exit;
		}

		/*
		 * Worry about lack of final newline.
		 */
		limit = base + size;
		if ((size > 0) && (limit[-1] != '\n'))
			limit = base;
	}

	/*
	 * For each line in the mapped file, look for a make rule key.
	 * If we find one and it matches key, slide the rest of the file
	 * down over it.
	 */
	if (sprintf(signature, "#!%.4s", name) < 0) {
		fprintf(stderr,
			"MDupdate_test (line %d, pid %d): %s, sprintf error %s\n",
			__LINE__, getpid(), filename, strerror(errno));
	}
	for (entno = 1, entry = base; entry < limit; entno++, entry = next) {
		assert(entry);
		if (!valid_entry(entry, processes)) {
			dump_output_buffer();
			fprintf (stderr, "MDupdate_test (line %d, pid %d): %s, %s, "
				"invalid entry number %d, offset %d.\n", __LINE__, getpid(),
				name, filename, entno, entry - base);
			/*
			 * kill other processes
			 */
			signal(SIGTERM, SIG_IGN);
			kill(-Parent, SIGTERM);
			signal(SIGTERM, SIG_DFL);
			(void)close(f);
			status = 0;
			goto test_exit;
		}
		/*
		 * NULL-terminate entry and point to the next entry
		 */
		eol = next = strchr(entry, '\n');
		*eol = '\0';
		next++;
		/*
		 * remove the entry if the key and the signature match those
		 * in the entry
		 */
		if (keymatch(key, entry) && sigmatch(signature, entry)) {
			if (Verbose) {
				assert((Outbuf_index >= 0) && (Outbuf_index < OUTBUFFERS));
				sprintf(Output_buffer[Outbuf_index],
					"MDupdate_test: pid %d remove entry, key %s "
					"signature %s offset %d length %d\n", getpid(), key,
					signature, entry - base, next - entry);
				Outbuf_index = (Outbuf_index + 1) % OUTBUFFERS;
				if (Outbuf_index == 0) {
					Wrapped++;
				}
			}
			bcopy(next, entry, limit - next);
			limit -= next - entry;
			next = entry;
		} else {
			/*
			 * leave the entry in place, so put the newline
			 * terminator back
			 */
			*eol = '\n';
		}
		assert(next);
	}

	/*
	 * Truncate the file if necessary.
	 */
	size = limit - base;
	if (size < stb.st_size) {
		if (Verbose) {
			assert((Outbuf_index >= 0) && (Outbuf_index < OUTBUFFERS));
			sprintf(Output_buffer[Outbuf_index],
				"MDupdate_test: pid %d truncate to size %d\n", getpid(),
				size);
			Outbuf_index = (Outbuf_index + 1) % OUTBUFFERS;
			if (Outbuf_index == 0) {
				Wrapped++;
			}
		}
		if (ftruncate(f, size)) {
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, ftruncate error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		}
	}

	/*
	 * Open a stdio append-mode stream, write our rule at end of file,
	 * and close the file (releasing the lock).
	 */
	file = fdopen(f, "a");
	if (file == 0) {
		fprintf(stderr,
			"MDupdate_test (line %d, pid %d): %s, fdopen error %s\n",
			 __LINE__, getpid(), filename, strerror(errno));
		close(f);
	} else {
		if (Verbose) {
			assert((Outbuf_index >= 0) && (Outbuf_index < OUTBUFFERS));
			sprintf(Output_buffer[Outbuf_index],
				"MDupdate_test: pid %d, append entry key %s signature %s "
				"offset %d\n", getpid(), key, signature, size);
			Outbuf_index = (Outbuf_index + 1) % OUTBUFFERS;
			if (Outbuf_index == 0) {
				Wrapped++;
			}
		}
		/*
		 * key
		 */
		if (fprintf(file, "%s%c", key, FIELD_DELIMITER) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		/*
		 * timestamp
		 */
		timestamp = time(NULL);
		timestr = ctime(&timestamp);
		eol = strchr(timestr, '\n');
		*eol = '\0';
		if (fprintf(file, "%s%c", timestr, FIELD_DELIMITER) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		/*
		 * record ID
		 */
		Recid++;
		if (fprintf(file, "%d%c", Recid, FIELD_DELIMITER) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		/*
		 * process ID
		 */
		if (fprintf(file, "%d%c", pid, FIELD_DELIMITER) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		/*
		 * data length
		 */
		datalen = (int)(drand48() * (double)(MAX_DATA_LEN - MIN_DATA_LEN)) +
			MIN_DATA_LEN;
		assert((datalen >= MIN_DATA_LEN) && (datalen < MAX_DATA_LEN));
		if (fprintf(file, "%d%c", datalen, FIELD_DELIMITER) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		/*
		 * data
		 */
		Data[datalen] = '\0';
		if (fprintf(file, "%s%c", Data, FIELD_DELIMITER) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		Data[datalen] = Data_value;
		/*
		 * signature
		 */
		if (fprintf(file, "%s\n", signature) < 0)
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fprintf error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
		if (fclose(file))
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, fclose error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
	}

test_exit:
	/*
	 * Unmap the file and free dynamic store.
	 */
	if (base)
		if (munmap(base, stb.st_size)) {
			fprintf(stderr,
				"MDupdate_test (line %d, pid %d): %s, munmap error %s\n",
				__LINE__, getpid(), filename, strerror(errno));
			status = 0;
		}
	free(signature);
	return(status);
}

static void
usage(char *progname)
{
	fprintf(stderr, "usage: %s [-vs] [-f path] [iterations]\n", progname);
}

int
main(int argc, char **argv)
{
	int catch = 0;
	int wait_stat;
	pid_t pid;
	int i;
	struct sigaction sigact;
	struct stat sb;
	int validate = 0;
	int iterations = -1;
	int opt;
	int status = 0;
	int processes = 1;
	char *testfile = NULL;
	char *namebuf;
	char *progname;

	progname = *argv;
	while ((opt = getopt(argc, argv, OPTION_STRING)) != EOF) {
		switch (opt) {
			case 'c':
				catch = 1;
				break;
			case 'V':
				Verbose++;
				break;
			case 'v':
				validate = 1;
				break;
			case 'p':
				processes = atoi(optarg);
				break;
			default:
				usage(progname);
				exit(-1);
		}
	}
	if (optind < argc) {
		iterations = atoi(argv[optind]);
	}
	for (i = 0; i < OUTBUFFERS; i++) {
		Output_buffer[i] = malloc(OUTBUFFER_LEN);
		if (Output_buffer[i] == NULL) {
			if (errno) {
				perror("malloc");
				exit(errno);
			} else {
				fprintf(stderr, "malloc failed for buffer %d\n", i + 1);
				exit(-1);
			}
		}
	}
	testfile = tempnam(NULL, TESTFILE_NAME);
	/*
	 * create processes - 1 processes
	 * put everything in the same process group so it can be killed
	 */
	Parent = getpid();
	if (setpgid(Parent, Parent) == -1) {
		perror("setpgid");
	}
	for (i = 0; i < processes - 1; i++) {
		if ((pid = fork()) == -1) {
			perror("fork");
			signal(SIGINT, SIG_IGN);
			if (kill(-Parent, SIGINT) == -1) {
				perror("kill");
			}
			exit(errno);
		} else if (pid == 0) {
			/*
			 * child processes do not continue the loop
			 */
			if (setpgid(getpid(), Parent) == -1) {
				perror("setpgid");
			}
			break;
		}
	}
	srand48(time(NULL) + getpid());
	if (catch) {
		sigact.sa_handler = signal_handler;
		bzero(&sigact.sa_mask, sizeof(sigact.sa_mask));
		sigact.sa_flags = SA_SIGINFO;
		if (sigaction(SIGSEGV, &sigact, NULL) == -1) {
			perror("sigaction");
			exit(errno);
		}
		if (sigaction(SIGBUS, &sigact, NULL) == -1) {
			perror("sigaction");
			exit(errno);
		}
	}
	for (i = 1; (i <= iterations) || (iterations < 0); i++) {
		MDupdate_init((char)((drand48() * 26) + 'a'));
		if (MDupdate_test(testfile, Keys[(int)(drand48() * KEYS)],
			Names[(int)(drand48() * NAMES)], processes)) {
			if (validate) {
				if (!MDupdate_access(testfile, processes)) {
					fprintf(stderr, "MDupdate_access failed (pid %d) in "
						"iteration number %d\n", getpid(), i);
					status = -1;
					break;
				}
			}
		} else {
			fprintf(stderr,
				"MDupdate_test failed (pid %d) in iteration number %d\n",
				getpid(), i);
			status = -1;
			break;
		}
	}
	if (status != 0) {
		signal(SIGTERM, SIG_IGN);
		kill(-Parent, SIGTERM);
	}
	/*
	 * parent collect up all the child status values
	 */
	if (getpid() == Parent) {
		wait(&wait_stat);
		while ((pid = wait(&wait_stat)) != -1) {
			if (WIFEXITED(wait_stat)) {
				if (!status) {
					status = WEXITSTATUS(wait_stat);
				}
			} else if (WIFSIGNALED(wait_stat)) {
				fprintf(stderr, "pid %d: terminated on signal %d\n", pid,
					WTERMSIG(wait_stat));
				status = -1;
			} else if (WIFSTOPPED(wait_stat)) {
				fprintf(stderr, "pid %d: stopped on signal %d\n", pid,
					WSTOPSIG(wait_stat));
				status = -1;
			} else {
				fprintf(stderr, "pid %d: unknown status 0x%x\n", pid,
					wait_stat);
				status = -1;
			}
		}
	}
	exit(status);
}
