#ifndef __RTMON_WRAPPER_H__
#define __RTMON_WRAPPER_H__

#include <sys/types.h>
#include <rtmon.h>
#include <sys/rtmon.h>

#define MAXCPU 128
#define NEVENTS_BUF 4096
#define EVBUF_SIZE (NEVENTS_BUF * sizeof(tstamp_event_entry_t))

typedef struct data_state_s {
	rtmond_t *rtmon;	/* rtmond state information */
	int data;		/* incoming data connection */
	const char *hostname;	/* target host */
	uint64_t cpu_mask[MAXCPU/64];	/* target processor mask */
	uint64_t event_mask;	/* target events */
	tstamp_event_entry_t evbuf[NEVENTS_BUF];	/* event buffer */
	int evbuf_size;		/* data in event buffer */
	int evbuf_next;		/* next event to be returned */
} data_state;

void data_init(data_state *s, const char *hostname, const char *proc_str,
	const char *event_str);

int data_read(data_state *s);

int data_next_event(data_state *s, tstamp_event_entry_t **evt);

#define data_fd(s) ((s)->rtmon->socket)

#endif /* __RTMON_WRAPPER_H__ */
