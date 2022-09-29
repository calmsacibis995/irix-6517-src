#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include <rtmon.h>
#include <sys/rtmon.h>

#include "data.h"
#include "util.h"

void
data_init(data_state *s, const char *a_hostname, const char *proc_str,
	const char *event_str)
{
	s->evbuf_size = 0;
	s->evbuf_next = 0;
	hostname = s->hostname = a_hostname;

	s->rtmon = rtmond_open(s->hostname, 0);
	if (s->rtmon == NULL) {
		fprintf(stderr, "Unable to open connection to rtmond.\n");
		exit(EXIT_FAILURE);
	}

	numb_processors = s->rtmon->ncpus;
	if (s->rtmon->ncpus <= 0) {
		fprintf(stderr, "Invalid number of cpus returned by rtmond.\n");
		exit(EXIT_FAILURE);
	}

	parse_proc_str(proc_str, s->cpu_mask);
	s->event_mask = parse_event_str(event_str);

	if (rtmond_start_ncpu(s->rtmon, MAXCPU, s->cpu_mask,
		s->event_mask) < 0) {
		fprintf(stderr, "Unable to start rtmond data stream.\n");
		exit(EXIT_FAILURE);
	}
}

int data_read(data_state *s)
{
	ssize_t		cc;

	if (s->evbuf_size && s->evbuf_next > 0)
		memcpy(s->evbuf, &s->evbuf[s->evbuf_next], s->evbuf_size);
	s->evbuf_next = 0;

retry:
	cc = read(data_fd(s),
		  (void *)((char *)s->evbuf + s->evbuf_size),
		  EVBUF_SIZE - s->evbuf_size);
	if (cc < 0) {
		if (errno == EINTR)
			goto retry;
		else {
			fprintf(stderr, "data read from rtmond: %s\n",
				strerror(errno));
			return 0;
		}
	}
	else if (!cc) {
		fprintf(stderr, "zero-length data read: "
			"rtmond closed connection?\n");
		return 0;
	}
	s->evbuf_size += cc;
	return 1;
}

int data_next_event(data_state *s, tstamp_event_entry_t **evt)
{
	tstamp_event_entry_t *ev;
	int nevents;
	size_t esize;

	if (s->evbuf_size < sizeof(tstamp_event_entry_t))
		return 0;

	ev = &s->evbuf[s->evbuf_next];
	nevents = 1 + ev->jumbocnt;
	esize = nevents * sizeof(tstamp_event_entry_t);
	if (s->evbuf_size < esize)
		return 0;

	*evt = ev;
	s->evbuf_next += nevents;
	s->evbuf_size -= esize;

	return 1;
}
