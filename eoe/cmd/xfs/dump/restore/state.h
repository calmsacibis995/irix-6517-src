#ifndef STATE_H
#define STATE_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/state.h,v 1.2 1994/10/17 23:00:47 cbullis Exp $"

/* state.[hc] - cumulative restore state
 */

struct statestream {
	uuid_t ss_sessionid;
	uuid_t ss_mediaid;
	size_t ss_mediafileix;
	bool_t ss_interrupted;
};

typedef struct statestream statestream_t;

struct state {
	size_t s_streamcnt;
	size_t s_interruptedstreamcnt; /* updated on open */
	bool_t s_cumulative;
	statestream_t s_stream[ STREAM_MAX ];
};

typedef struct state state_t;

/* creates a new state file, returns an mmap()ed ptr
 */
extern state_t *state_create( char *pathname,
			      size_t streamcnt,
			      bool_t cumulative );


/* opens an existing state file, returns an mmap()ed ptr
 */
extern state_t *state_open( char *pathname );


#endif /* STATE_H */
