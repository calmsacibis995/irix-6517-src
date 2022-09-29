/*
 * In order to allow using minimally modified cmd/find/find.c source
 * for the bulk of the server side rfindd code, we need to do
 * something about capturing its stdio.  We want to leave the calls
 * to printf, puts, ... in place in the code, and sneak in underneath
 * the stdio package to redirect this I/O into XDR packets that
 * return to the client via RPC svc_sendreply calls.
 *
 * To do this, we use setvbuf (in rfindd.c) to ask that both
 * stdout and stderr calls go into buffers that we have set up,
 * inside the following struct io's.  These structs also keep
 * the real file descriptor (1 for stdout and 2 for stderr),
 * and a pointer to stdio's FILE structures for these two streams.
 *
 * We check frequently for output in either of these two buffers,
 * and when we find any, package it up and send it out.
 * To do this, we cheat like crazy - directly accessing the fields
 * inside stdio's FILE structures in order to steal its buffered
 * output and leave nothing behind.
 */

#include "rfind.h"

struct io {
	FILE  *io_ptr;
	u_int io_minlen;
	struct rfindhdr io_rfh;
};

void rpcexit(int);
void rpcsetbuf(void);
void rpcflushall(void);
void rpcflush (struct io *);
