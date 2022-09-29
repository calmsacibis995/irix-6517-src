#ident "$Revision: 1.8 $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/t6satmp.h>
#include <sys/t6samp.h>
#include <sys/time.h>
#include <sys/capability.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <rpcsvc/ypclnt.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include "atomic.h"
#include "defaults.h"
#include "debug.h"
#include "client.h"
#include "generic_list.h"
#include "lrep.h"
#include "name.h"
#include "ref.h"
#include "request.h"
#include "server.h"
#include "sem.h"
#include "timer.h"
#include "thread.h"

typedef int so_set[2];
typedef struct timer_client
{
	so_set so;
	struct sockaddr_in host;
	struct sockaddr_in token_server;
	satmp_header hdr;
	union
	{
		init_request *irq;
		get_tok_request *gtr;
		get_attr_request *gar;
		void *req;
		u_int hid;
	} __tc_request;
} timer_client;

#define tc_irq __tc_request.irq
#define tc_gtr __tc_request.gtr
#define tc_gar __tc_request.gar
#define tc_req __tc_request.req
#define tc_hid __tc_request.hid

#define CHECK_BUFLEN(len, max)	{ \
					if ((len) > (max)) { \
						return; \
					} \
				}
#define dgram_max		0x2000

static const int DEMONSO = 0;	/* socket for satmpd<->satmpd messages */
static const int KERNSO = 1;	/* socket for kernel<->satmpd messages */

static const cap_value_t cap_priv_port[] = {CAP_PRIV_PORT};
static const cap_value_t cap_network_mgt[] = {CAP_NETWORK_MGT};

static const time_t timeout = 3 * USECS_PER_SEC;
static const unsigned int retries = 3;

/*******************************************************************
 * PROTOCOL DEBUG ROUTINES
 ******************************************************************/

static char *
result_text (u_char result)
{
	static char *satmp_result_names[] =
	{
		"SATMP_REPLY_OK",
		"SATMP_INVALID_VERSION",
		"SATMP_INVALID_GENERATION",
		"SATMP_UNSUPPORTED_MESSAGE",
		"SATMP_FORMAT_ERROR",
		"SATMP_TRANSLATION_ERROR",
		"SATMP_NO_DOT",
		"SATMP_NO_MAPPING",
		"SATMP_INTERNAL_ERROR"
	};

	if (result <= 7)
		return (satmp_result_names[result]);
	if (result == 255)
		return (satmp_result_names[8]);
	return ("Unknown");
}

static char *
message_name (u_char message_number)
{
	static char *satmp_message_names[] =
	{
		"SATMP_FLUSH_REPLY",		/* 162 */
		"SATMP_GET_ATTR_REQUEST",	/* 163 */
		"SATMP_GET_ATTR_REPLY",		/* 164 */
		"SATMP_INIT_REQUEST",		/* 165 */
		"SATMP_INIT_REPLY",		/* 166 */
		"SATMP_FLUSH_REMOTE_CACHE",	/* 167 */
		"SATMP_GET_TOK_REQUEST",	/* 173 */
		"SATMP_GET_TOK_REPLY",		/* 174 */
		"SATMP_SEND_INIT_REQUEST",	/* 200 */
		"SATMP_SEND_GET_ATTR_REQUEST",	/* 201 */
		"SATMP_SEND_GET_TOK_REQUEST",	/* 202 */
		"SATMP_SEND_FLUSH_REMOTE_CACHE", /* 203 */
		"SATMP_EXIT",			/* 204 */
		"SATMP_GET_LRTOK_REQUEST",	/* 210 */
		"SATMP_GET_LRTOK_REPLY",	/* 211 */
	};

	if (message_number >= SATMP_FLUSH_REPLY &&
	    message_number <= SATMP_FLUSH_REMOTE_CACHE)
		return (satmp_message_names[message_number - 162]);
	if (message_number >= SATMP_GET_TOK_REQUEST &&
	    message_number <= SATMP_GET_TOK_REPLY)
		return (satmp_message_names[message_number - 173 + 6]);
	if (message_number >= SATMP_SEND_INIT_REQUEST &&
	    message_number <= SATMP_EXIT)
		return (satmp_message_names[message_number - 200 + 8]);
	if (message_number >= SATMP_GET_LRTOK_REQUEST &&
	    message_number <= SATMP_GET_LRTOK_REPLY)
		return (satmp_message_names[message_number - 210 + 13]);
	return ("UNSUPPORTED");
}

/*******************************************************************
 * PROTOCOL ROUTINES
 ******************************************************************/

/*
 * this must exactly track copy_from_header()
 */
static size_t
header_size (void)
{
	satmp_header *hdr;

	return (2 + sizeof (hdr->message_length) + sizeof (hdr->message_esi));
}

static void
copy_from_header (char *bufptr, satmp_header *hdr)
{
	u_short t16;

	/* message number */
	*bufptr++ = hdr->message_number;

	/* message status */
	*bufptr++ = hdr->message_status;

	/* message length */
	t16 = htons (hdr->message_length);
	memcpy (bufptr, &t16, sizeof (t16));
	bufptr += sizeof (t16);

	/* extended session identifier */
	memcpy (bufptr, &hdr->message_esi, sizeof (hdr->message_esi));
	bufptr += sizeof (hdr->message_esi);
}

static char *
copy_to_header (char *bufptr, satmp_header *hdr)
{
	u_short t16;

	/* message number */
	hdr->message_number = *bufptr++;

	/* message status */
	hdr->message_status = *bufptr++;

	/* message length */
	memcpy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);
	hdr->message_length = ntohs (t16);

	/* extended session identifier */
	memcpy (&hdr->message_esi, bufptr, sizeof (hdr->message_esi));
	bufptr += sizeof (hdr->message_esi);

	return (bufptr);
}

static void
send_buffer (int so, void *buf, size_t size, struct sockaddr_in *to)
{
	int mlen;

	do
	{
		mlen = sendto (so, buf, size, 0, to, sizeof (*to));
	} while (mlen == -1 && errno == EINTR);

	if (mlen == -1 && errno != EINTR)
		debug_print ("sendto: %s\n", strerror (errno));
}

static void
send_header (int so, struct sockaddr_in *to, satmp_header *hdr)
{
	char buf[sizeof (*hdr)], *bufptr = buf + header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent\n", message_name (hdr->message_number));

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	send_buffer (so, buf, bufptr - buf, to);
}

static void
send_init_request (int so, struct sockaddr_in *to, satmp_header *hdr,
		   init_request *req)
{
	char buf[dgram_max], *bufptr = buf;
	u_short raf_count, dot_count, t16;
	u_int t32;

	/* reserve space for header */
	bufptr += header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent\n", message_name (hdr->message_number));

	/* extended session identifier */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tesi: 0x%llx\n", hdr->message_esi);

	/* hostid */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", req->hostid);

	/* copy reply header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);
	t32 = htonl (req->generation);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	/* version */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tversion: %hu\n", req->version);
	t16 = htons (req->version);
	memcpy (bufptr, &t16, sizeof (t16));
	bufptr += sizeof (t16);

	/* reserved */
	*bufptr++ = 0;

	/* server state */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tserver state: %u\n",
			     (unsigned) req->server_state);
	*bufptr++ = req->server_state;

	/* token server address */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\ttoken server: %u\n", req->token_server);
	t32 = htonl (req->token_server);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	/* backup server address */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tbackup server: %u\n", req->backup_server);
	t32 = htonl (req->backup_server);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	for (raf_count = 0; raf_count < req->format_count; raf_count++)
	{
		init_request_attr_format *iraf;

		iraf = &req->format[raf_count];

		/* attribute type */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", iraf->attribute);
		t16 = htons (iraf->attribute);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* number of DOTS to follow */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tDOT count: %hu\n", iraf->dot_count);
		t16 = htons (iraf->dot_count);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		for (dot_count = 0; dot_count < iraf->dot_count; dot_count++)
		{
			init_request_dot_rep *irdr;

			irdr = &iraf->dot[dot_count];

			/* weight of this DOT */
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT weight: %u\n",
					(unsigned) irdr->weight);
			*bufptr++ = irdr->weight;
			CHECK_BUFLEN (bufptr - buf, dgram_max);

			/* network representation flags */
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT flags: %u\n",
					(unsigned) irdr->flags);
			*bufptr++ = irdr->flags;
			CHECK_BUFLEN (bufptr - buf, dgram_max);

			/* length of DOT name */
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT length: %hu\n",
					     irdr->length);
			t16 = htons (irdr->length);
			memcpy (bufptr, &t16, sizeof (t16));
			bufptr += sizeof (t16);
			CHECK_BUFLEN (bufptr - buf, dgram_max);

			/* the DOT name itself */
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT name: %s\n",
					irdr->dot_name);
			memcpy (bufptr, irdr->dot_name, irdr->length);
			bufptr += irdr->length;
			CHECK_BUFLEN (bufptr - buf, dgram_max);
		}
	}

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	send_buffer (so, buf, bufptr - buf, to);
}

static void
send_init_reply (int so, struct sockaddr_in *to, satmp_header *hdr,
		 init_reply *rep)
{
	send_init_request (so, to, hdr, (init_request *) rep);
}

static void
send_get_tok_request (int so, struct sockaddr_in *to, satmp_header *hdr,
		      get_tok_request *req)
{
	char buf[dgram_max], *bufptr = buf;
	u_short i;
	u_int t32;

	/* reserve space for header */
	bufptr += header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent\n", message_name (hdr->message_number));

	/* copy generation */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);
	t32 = htonl (req->generation);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	for (i = 0; i < req->attr_spec_count; i++)
	{
		get_tok_request_attr_spec *gtras;
		u_short t16;

		gtras = &req->attr_spec[i];

		/* attribute type */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", gtras->attribute);
		t16 = htons (gtras->attribute);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* length of network representation */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnr length: %hu\n", gtras->nr_length);
		t16 = htons (gtras->nr_length);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* mapping identifier */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     gtras->mid);
		t32 = htonl (gtras->mid);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* network representation itself */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnetwork rep: %s\n", gtras->nr);
		memcpy (bufptr, gtras->nr, gtras->nr_length);
		bufptr += gtras->nr_length;
		CHECK_BUFLEN (bufptr - buf, dgram_max);
	}

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	send_buffer (so, buf, bufptr - buf, to);
}

static void
send_get_tok_reply (int so, struct sockaddr_in *to, satmp_header *hdr,
		    get_tok_reply *rep)
{
	char buf[dgram_max], *bufptr = buf;
	u_short i;
	u_int t32;

	/* reserve space for header */
	bufptr += header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent\n", message_name (hdr->message_number));

	/* extended session identifier */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tesi: 0x%llx\n", hdr->message_esi);

	/* copy generation */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", rep->generation);
	t32 = htonl (rep->generation);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	for (i = 0; i < rep->token_spec_count; i++)
	{
		get_tok_reply_token_spec *gtrts;
		u_short t16;

		gtrts = &rep->token_spec[i];

		/* attribute type */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", gtrts->attribute);
		t16 = htons (gtrts->attribute);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* mapping identifier */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     gtrts->mid);
		t32 = htonl (gtrts->mid);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* token */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", gtrts->token);
		t32 = htonl (gtrts->token);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);
	}

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	send_buffer (so, buf, bufptr - buf, to);
}

static void
send_get_attr_request (int so, struct sockaddr_in *to, satmp_header *hdr,
		       get_attr_request *req)
{
	char buf[dgram_max], *bufptr = buf;
	u_short i;
	u_int t32;

	/* reserve space for header */
	bufptr += header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent\n", message_name (hdr->message_number));

	/* copy generation */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);
	t32 = htonl (req->generation);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	/* copy requests */
	for (i = 0; i < req->token_spec_count; i++)
	{
		get_attr_request_token_spec *garts;
		u_short t16;

		garts = &req->token_spec[i];

		/* attribute type */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", garts->attribute);
		t16 = htons (garts->attribute);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* mid */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     (unsigned) 0);
		t32 = 0;
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* token */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", garts->token);
		t32 = htonl (garts->token);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);
	}

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	send_buffer (so, buf, bufptr - buf, to);
}

static void
send_get_attr_reply (int so, struct sockaddr_in *to, satmp_header *hdr,
		     get_attr_reply *rep, int kernel)
{
	char buf[dgram_max], *bufptr = buf;
	u_short i;
	u_int t32;

	/* reserve space for header */
	bufptr += header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent %s\n",
			     message_name (hdr->message_number),
			     kernel ? "to kernel" : "to remote host");

	/* extended session identifier */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tesi: 0x%llx\n", hdr->message_esi);

	/* copy hostid */
	if (kernel)
	{
		/* copy hostid */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\thostid: 0x%x\n", rep->hostid);
		t32 = htonl (rep->hostid);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
	}

	/* copy generation */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", rep->generation);
	t32 = htonl (rep->generation);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	/* copy reply specs */
	for (i = 0; i < rep->attr_spec_count; i++)
	{
		get_attr_reply_attr_spec *garas;
		u_short t16;

		garas = &rep->attr_spec[i];

		/* attribute type */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", garas->attribute);
		t16 = htons (garas->attribute);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* length of network representation */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnr length: %hu\n", garas->nr_length);
		t16 = htons (garas->nr_length);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* token */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", garas->token);
		t32 = htonl (garas->token);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* status */
		if (kernel)
		{
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\tstatus: %u\n",
					     (unsigned) garas->status);
			*bufptr++ = garas->status;
			CHECK_BUFLEN (bufptr - buf, dgram_max);

			if (debug_on (DEBUG_PROTOCOL))
			{
				char *text;

				if (garas->nr != NULL)
				{
					text = lrep_to_text (garas->nr,
							     garas->attribute);
					debug_print ("\t\tlocal rep: %s\n",
						     text);
					destroy_lrep (text, garas->attribute);
				}
				else
					debug_print ("\t\tlocal rep: not available\n");
			}
			if (garas->nr != NULL)
			{
				memcpy (bufptr, garas->nr, garas->nr_length);
				bufptr += garas->nr_length;
				CHECK_BUFLEN (bufptr - buf, dgram_max);
			}
		}
		else
		{
			/* network representation itself */
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\tnetwork rep: %s\n",
					     garas->nr);
			memcpy (bufptr, garas->nr, garas->nr_length);
			bufptr += garas->nr_length;
			CHECK_BUFLEN (bufptr - buf, dgram_max);
		}
	}

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	if (kernel)
	{
		if (satmp_get_attr_reply (so, buf, bufptr - buf) == -1)
			debug_print ("\t\tsatmp_get_attr_reply: %s\n",
				     strerror (errno));
	}
	else
		send_buffer (so, buf, bufptr - buf, to);
}

/* ARGSUSED */
static void
send_get_lrtok_reply (int so, struct sockaddr_in *to, satmp_header *hdr,
		      get_lrtok_reply *rep)
{
	char buf[dgram_max], *bufptr = buf;
	u_short i;
	u_int t32;

	/* reserve space for header */
	bufptr += header_size ();

	/* request header */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s sent\n", message_name (hdr->message_number));

	/* extended session identifier */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tesi: 0x%llx\n", hdr->message_esi);

	/* copy hostid */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", rep->hostid);
	t32 = htonl (rep->hostid);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	/* copy generation */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", rep->generation);
	t32 = htonl (rep->generation);
	memcpy (bufptr, &t32, sizeof (t32));
	bufptr += sizeof (t32);

	for (i = 0; i < rep->token_spec_count; i++)
	{
		get_lrtok_reply_token_spec *glrts;
		u_short t16;

		glrts = &rep->token_spec[i];

		/* attribute type */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", glrts->attribute);
		t16 = htons (glrts->attribute);
		memcpy (bufptr, &t16, sizeof (t16));
		bufptr += sizeof (t16);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* mapping identifier */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     glrts->mid);
		t32 = htonl (glrts->mid);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* token */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", glrts->token);
		t32 = htonl (glrts->token);
		memcpy (bufptr, &t32, sizeof (t32));
		bufptr += sizeof (t32);
		CHECK_BUFLEN (bufptr - buf, dgram_max);

		/* status */
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tstatus: %u\n",
				     (unsigned) glrts->status);
		*bufptr++ = glrts->status;
		CHECK_BUFLEN (bufptr - buf, dgram_max);
	}

	hdr->message_length = bufptr - buf;
	copy_from_header (buf, hdr);
	if (satmp_get_lrtok_reply (so, buf, bufptr - buf) == -1)
		debug_print ("\t\tsatmp_get_lrtok_reply: %s\n",
			     strerror (errno));
}

/*******************************************************************
 * TIMEOUT CALLBACKS
 ******************************************************************/

static void
init_request_notify (void *arg)
{
	timer_client *client = (timer_client *) arg;

	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: timed out\n",
			     message_name (SATMP_INIT_REQUEST));
	if (satmp_init_reply (client->so[KERNSO], client->hdr.message_esi,
			      IRQ_FLAG_FAILED, 0) == -1)
		debug_print ("satmp_init_reply: %s\n", strerror (errno));
}

static void
init_request_destroy (void *arg)
{
	timer_client *client = (timer_client *) arg;

	destroy_init_request (client->tc_irq);
	free (client->tc_irq);
	free (client);
}

static void
init_request_retry (void *arg)
{
	timer_client *client = (timer_client *) arg;

	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: retry\n",
			     message_name (SATMP_INIT_REQUEST));
	send_init_request (client->so[DEMONSO], &client->token_server,
			   &client->hdr, client->tc_irq);
}

/* ARGSUSED */
static void
send_flush_notify (void *arg)
{
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: timed out\n",
			     message_name (SATMP_FLUSH_REMOTE_CACHE));
}

static void
send_flush_destroy (void *arg)
{
	timer_client *client = (timer_client *) arg;

	free (client);
}

static void
send_flush_retry (void *arg)
{
	timer_client *client = (timer_client *) arg;

	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: retry\n",
			     message_name (SATMP_FLUSH_REMOTE_CACHE));
	send_header (client->so[DEMONSO], &client->token_server, &client->hdr);
}

/* ARGSUSED */
static void
send_tok_notify (void *arg)
{
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: timed out\n",
			     message_name (SATMP_GET_TOK_REQUEST));
}

static void
send_tok_destroy (void *arg)
{
	timer_client *client = (timer_client *) arg;

	destroy_get_tok_request (client->tc_gtr);
	free (client->tc_gtr);
	free (client);
}

static void
send_tok_retry (void *arg)
{
	timer_client *client = (timer_client *) arg;

	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: retry\n",
			     message_name (SATMP_GET_TOK_REQUEST));
	send_get_tok_request (client->so[DEMONSO], &client->token_server,
			      &client->hdr, client->tc_gtr);
}

static void
send_attr_notify (void *arg)
{
	timer_client *client = (timer_client *) arg;
	get_attr_reply rep;

	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: timed out\n",
			     message_name (SATMP_GET_ATTR_REQUEST));
	(void) get_attr_reply_failed (SATMP_INTERNAL_ERROR, client->tc_gar,
				      &rep);
	send_get_attr_reply (client->so[KERNSO], &client->host, &client->hdr,
			     &rep, 1);
	destroy_get_attr_reply (&rep);
}

static void
send_attr_destroy (void *arg)
{
	timer_client *client = (timer_client *) arg;

	destroy_get_attr_request (client->tc_gar);
	free (client->tc_gar);
	free (client);
}

static void
send_attr_retry (void *arg)
{
	timer_client *client = (timer_client *) arg;

	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("%s: retry\n",
			     message_name (SATMP_GET_ATTR_REQUEST));
	send_get_attr_request (client->so[DEMONSO], &client->token_server,
			       &client->hdr, client->tc_gar);
}

/*******************************************************************
 * MAIN REQUEST LOOP
 ******************************************************************/

static void
timer_client_init (timer_client *client, int so[],
		   struct sockaddr_in *host, struct sockaddr_in *token_server,
		   satmp_header *hdr, void *request)
{
	memcpy (client->so, so, sizeof (client->so));
	memcpy (&client->host, host, sizeof (*host));
	memcpy (&client->token_server, token_server, sizeof (*token_server));
	memcpy (&client->hdr, hdr, sizeof (*hdr));
	client->tc_req = request;
}

typedef struct {
	satmp_header hdr;
	struct sockaddr_in from;
	char buf[dgram_max], *bufptr;
	size_t buflen;
	u_int hostid;
	so_set so;
} request_t;

static atomic esi_seq;

static thread_rtn
reqvec (void *arg)
{
	request_t *request = (request_t *) arg;
	int err;

	satmp_header hdr = request->hdr;
	struct sockaddr_in from = request->from, to;
	size_t buflen = request->buflen;
	u_int hostid = request->hostid;
	char *bufptr = request->bufptr;
	int demonso = request->so[DEMONSO];
	int kernso = request->so[KERNSO];

	switch (hdr.message_number)
	{
		case SATMP_FLUSH_REMOTE_CACHE:
		{
			init_request req, *reqp;
			timer_client *client;

			/* flush cache */
			hdr.message_number = SATMP_FLUSH_REPLY;
			hdr.message_status = receive_flush_remote_cache (hostid);
			send_header (demonso, &from, &hdr);

			/* create request */
			req.hostid = hostid;
			hdr.message_status = create_init_request (&req);
			if (hdr.message_status != SATMP_REPLY_OK)
				break;

			/* allocate request & client */
			reqp = (init_request *) malloc (sizeof (*reqp));
			client = (timer_client *) malloc (sizeof (*client));
			if (reqp == NULL || client == NULL)
			{
				destroy_init_request (&req);
				free (reqp);
				free (client);
				break;
			}
			hdr.message_number = SATMP_INIT_REQUEST;
			hdr.message_esi = (satmp_esi_t) incatomic (&esi_seq);
			memcpy (reqp, &req, sizeof (req));
			timer_client_init (client, request->so, &from, &from,
					   &hdr, reqp);

			/* send SATMP_INIT_REQUEST message to sender */
			send_init_request (demonso, &from, &hdr, reqp);
			if (timer_set (timeout, retries, hdr.message_esi,
				       client, init_request_notify,
				       init_request_retry,
				       init_request_destroy))
				init_request_destroy (client);
			break;
		}
		case SATMP_FLUSH_REPLY:
		{
			timer_client *client;

			client = timer_cancel (hdr.message_esi);
			if (client != NULL)
				send_flush_destroy (client);
			break;
		}
		case SATMP_INIT_REQUEST:
		{
			init_request req;
			init_reply rep;

			rep.hostid = req.hostid = hostid;
			hdr.message_number = SATMP_INIT_REPLY;
			if (init_request_buffer (bufptr, buflen, &req))
			{
				hdr.message_status = SATMP_INTERNAL_ERROR;
				send_header (demonso, &from, &hdr);
				break;
			}
			hdr.message_status = receive_init_request (&req, &rep);
			send_init_reply (demonso, &from, &hdr, &rep);
			destroy_init_request (&req);
			destroy_init_reply (&rep);
			break;
		}
		case SATMP_INIT_REPLY:
		{
			init_reply rep;
			timer_client *client;

			client = timer_cancel (hdr.message_esi);
			if (client == NULL)
				break;

			rep.hostid = client->host.sin_addr.s_addr;
			if (init_reply_buffer (bufptr, buflen, &rep))
			{
				satmp_init_reply (kernso, hdr.message_esi,
						  IRQ_FLAG_FAILED, 0);
				init_request_destroy (client);
				break;
			}
			if (receive_init_reply (&rep) == SATMP_REPLY_OK)
				satmp_init_reply (kernso, client->hdr.message_esi, IRQ_FLAG_OK, rep.generation);
			else
				satmp_init_reply (kernso, client->hdr.message_esi, IRQ_FLAG_FAILED, 0);
			init_request_destroy (client);
			destroy_init_reply (&rep);
			break;
		}
		case SATMP_GET_TOK_REQUEST:
		{
			get_tok_request req;
			get_tok_reply rep;

			req.hostid = hostid;
			hdr.message_number = SATMP_GET_TOK_REPLY;
			if (get_tok_request_buffer (bufptr, buflen, &req))
			{
				hdr.message_status = SATMP_INTERNAL_ERROR;
				send_header (demonso, &from, &hdr);
				break;
			}
			hdr.message_status = receive_get_tok_request (&req,
								      &rep);
			send_get_tok_reply (demonso, &from, &hdr, &rep);
			destroy_get_tok_request (&req);
			destroy_get_tok_reply (&rep);
			break;
		}
		case SATMP_GET_TOK_REPLY:
		{
			get_tok_reply rep;
			get_tok_request *req;
			timer_client *client;

			client = timer_cancel (hdr.message_esi);
			if (client == NULL)
				break;
			req = client->tc_gtr;

			rep.hostid = client->host.sin_addr.s_addr;
			if (get_tok_reply_buffer (bufptr, buflen, &rep) == 0)
			{
				hdr.message_status =
					receive_get_tok_reply (req, &rep);
				destroy_get_tok_reply (&rep);
			}
			send_tok_destroy (client);
			break;
		}
		case SATMP_GET_ATTR_REQUEST:
		{
			get_attr_request req;
			get_attr_reply rep;

			req.hostid = hostid;
			hdr.message_number = SATMP_GET_ATTR_REPLY;
			if (get_attr_request_buffer (bufptr, buflen, &req))
			{
				hdr.message_status = SATMP_INTERNAL_ERROR;
				send_header (demonso, &from, &hdr);
				break;
			}
			hdr.message_status = receive_get_attr_request (&req,
								       &rep);
			send_get_attr_reply (demonso, &from, &hdr, &rep, 0);
			destroy_get_attr_request (&req);
			destroy_get_attr_reply (&rep);
			break;
		}
		case SATMP_GET_ATTR_REPLY:
		{
			get_attr_reply rep;
			timer_client *client;

			client = timer_cancel (hdr.message_esi);
			if (client == NULL)
				break;
			rep.hostid = client->host.sin_addr.s_addr;

			switch (hdr.message_status)
			{
				case SATMP_INVALID_GENERATION:
					(void) receive_flush_remote_cache (client->host.sin_addr.s_addr);
				case SATMP_UNSUPPORTED_MESSAGE:
				case SATMP_FORMAT_ERROR:
				case SATMP_INTERNAL_ERROR:
					if (get_attr_reply_failed (hdr.message_status, client->tc_gar, &rep) == 0)
					{
						send_get_attr_reply (kernso, &client->host, &hdr, &rep, 1);
						destroy_get_attr_reply (&rep);
					}
					send_attr_destroy (client);
					goto get_attr_reply_out;
			}

			if (err = get_attr_reply_buffer (bufptr, buflen, &rep))
			{
				if (get_attr_reply_failed (err, client->tc_gar, &rep) == 0)
				{
					send_get_attr_reply (kernso, &client->host, &hdr, &rep, 1);
					destroy_get_attr_reply (&rep);
				}
				send_attr_destroy (client);
				break;
			}
			hdr.message_status = receive_get_attr_reply (&rep);
			destroy_get_attr_reply (&rep);
			xlate_get_attr_reply (client->tc_gar, &rep);
			send_get_attr_reply (kernso, &client->host, &hdr,
					     &rep, 1);
			destroy_get_attr_reply (&rep);
			send_attr_destroy (client);
get_attr_reply_out:
			break;
		}
		case SATMP_GET_LRTOK_REQUEST:
		{
			get_lrtok_request req;
			get_lrtok_reply rep;

			hdr.message_number = SATMP_GET_LRTOK_REPLY;
			if (get_lrtok_request_buffer (bufptr, buflen, &req))
			{
				hdr.message_status = SATMP_INTERNAL_ERROR;
				send_header (demonso, &from, &hdr);
				break;
			}
			hdr.message_status = receive_get_lrtok_request (&req,
									&rep);
			send_get_lrtok_reply (kernso, &from, &hdr, &rep);
			destroy_get_lrtok_request (&req);
			destroy_get_lrtok_reply (&rep);
			break;
		}
		case SATMP_SEND_INIT_REQUEST:
		{
			init_request req, *reqp;
			host_cmd cmd;
			timer_client *client;

			if (host_cmd_buffer (bufptr, buflen, &cmd))
			{
				if (debug_on (DEBUG_PROTOCOL))
					debug_print ("SATMP_SEND_INIT_REQUEST: bad buffer\n");
				satmp_init_reply (kernso, hdr.message_esi,
						  IRQ_FLAG_FAILED, 0);
				break;
			}

			/* send ack if command didn't come from the kernel */
			if (!cmd.kernel)
				send_header (demonso, &from, &hdr);

			/* create request */
			req.hostid = cmd.hostid;
			hdr.message_status = create_init_request (&req);
			if (hdr.message_status != SATMP_REPLY_OK)
			{
				if (debug_on (DEBUG_PROTOCOL))
					debug_print ("SATMP_SEND_INIT_REQUEST: request creation failed\n");
				satmp_init_reply (kernso, hdr.message_esi,
						  IRQ_FLAG_FAILED, 0);
				break;
			}

			/* allocate request & client */
			reqp = (init_request *) malloc (sizeof (*reqp));
			client = (timer_client *) malloc (sizeof (*client));
			if (reqp == NULL || client == NULL)
			{
				if (debug_on (DEBUG_PROTOCOL))
					debug_print ("SATMP_SEND_INIT_REQUEST: cannot allocate memory\n");
				destroy_init_request (&req);
				free (reqp);
				free (client);
				satmp_init_reply (kernso, hdr.message_esi,
						  IRQ_FLAG_FAILED, 0);
				break;
			}
			hdr.message_number = SATMP_INIT_REQUEST;
			from.sin_addr.s_addr = cmd.hostid;
			from.sin_port = cmd.port;
			memcpy (reqp, &req, sizeof (req));
			timer_client_init (client, request->so, &from, &from,
					   &hdr, reqp);

			/* send SATMP_INIT_REQUEST message to specified host */
			send_init_request (demonso, &from, &hdr, reqp);
			if (timer_set (timeout, retries, hdr.message_esi,
				       client, init_request_notify,
				       init_request_retry,
				       init_request_destroy))
				init_request_destroy (client);
			break;
		}
		case SATMP_SEND_FLUSH_REMOTE_CACHE:
		{
			host_cmd cmd;
			timer_client *client;

			if (host_cmd_buffer (bufptr, buflen, &cmd))
				break;

			/* send ack if command didn't come from the kernel */
			if (!cmd.kernel)
				send_header (demonso, &from, &hdr);

			client = (timer_client *) malloc (sizeof (*client));
			if (client == NULL)
				break;

			hdr.message_number = SATMP_FLUSH_REMOTE_CACHE;
			hdr.message_status = SATMP_REPLY_OK;
			from.sin_addr.s_addr = cmd.hostid;
			from.sin_port = cmd.port;
			send_header (demonso, &from, &hdr);
			timer_client_init (client, request->so, &from, &from,
					   &hdr, (void *) cmd.hostid);
			if (timer_set (timeout, retries, hdr.message_esi,
				       client, send_flush_notify,
				       send_flush_retry, send_flush_destroy))
				send_flush_destroy (client);
			break;
		}
		case SATMP_SEND_GET_ATTR_REQUEST:
		{
			get_attr_request req, *reqp;
			get_attr_reply rep;
			host_cmd cmd;
			timer_client *client;

			if (get_attr_admin_buffer (bufptr, buflen, &cmd, &req))
				break;

			/* send ack if command didn't come from the kernel */
			if (!cmd.kernel)
				send_header (demonso, &from, &hdr);

			hdr.message_number = SATMP_GET_ATTR_REQUEST;
			hdr.message_status = SATMP_REPLY_OK;
			from.sin_addr.s_addr = rep.hostid = cmd.hostid;
			from.sin_port = cmd.port;
			to = from;
			if (get_client_server (cmd.hostid, &to))
			{
				if (get_attr_reply_failed (SATMP_INTERNAL_ERROR, &req, &rep) == 0)
				{
					send_get_attr_reply (kernso, &from,
							     &hdr, &rep, 1);
					destroy_get_attr_reply (&rep);
				}
				destroy_get_attr_request (&req);
				break;
			}

			reqp = (get_attr_request *) malloc (sizeof (*reqp));
			client = (timer_client *) malloc (sizeof (*client));
			if (reqp == NULL || client == NULL)
			{
				if (get_attr_reply_failed (SATMP_INTERNAL_ERROR, &req, &rep) == 0)
				{
					send_get_attr_reply (kernso, &from,
							     &hdr, &rep, 1);
					destroy_get_attr_reply (&rep);
				}
				destroy_get_attr_request (&req);
				free (reqp);
				free (client);
				break;
			}
			memcpy (reqp, &req, sizeof (req));
			timer_client_init (client, request->so, &from, &to, &hdr, reqp);

			send_get_attr_request (demonso, &to, &hdr, reqp);
			if (timer_set (timeout, retries, hdr.message_esi,
				       client, send_attr_notify,
				       send_attr_retry, send_attr_destroy))
				send_attr_destroy (client);
			break;
		}
		case SATMP_SEND_GET_TOK_REQUEST:
		{
			get_tok_request req, *reqp;
			host_cmd cmd;
			timer_client *client;

			if (get_tok_admin_buffer (bufptr, buflen, &cmd, &req))
				break;

			/* send ack if command didn't come from the kernel */
			if (!cmd.kernel)
				send_header (demonso, &from, &hdr);

			hdr.message_number = SATMP_GET_TOK_REQUEST;
			hdr.message_status = SATMP_REPLY_OK;
			from.sin_addr.s_addr = cmd.hostid;
			from.sin_port = cmd.port;
			to = from;
			if (get_client_server (cmd.hostid, &to))
			{
				destroy_get_tok_request (&req);
				break;
			}

			reqp = (get_tok_request *) malloc (sizeof (*reqp));
			client = (timer_client *) malloc (sizeof (*client));
			if (reqp == NULL || client == NULL)
			{
				destroy_get_tok_request (&req);
				free (reqp);
				free (client);
				break;
			}
			memcpy (reqp, &req, sizeof (req));
			timer_client_init (client, request->so, &from, &to, &hdr, reqp);

			send_get_tok_request (demonso, &to, &hdr, reqp);
			if (timer_set (timeout, retries, hdr.message_esi,
				       client, send_tok_notify,
				       send_tok_retry, send_tok_destroy))
				send_tok_destroy (client);
			break;
		}
		case SATMP_EXIT:
			exit (0);
			break;
		default:
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("%s message: %d\n",
					     message_name (hdr.message_number),
					     (unsigned) hdr.message_number);
					
			hdr.message_status = SATMP_UNSUPPORTED_MESSAGE;
			send_header (demonso, &from, &hdr);
			break;
	}

	free (request);
	thread_return (0);
}

static void
handle_requests (so_set so)
{
	request_t *req;
	int fromlen, mlen, sockfd, nfds;
	thread_id tid;
	fd_set rfds, ofds;

	nfds = (so[DEMONSO] > so[KERNSO] ? so[DEMONSO] : so[KERNSO]);
	FD_ZERO (&ofds);
	FD_SET (so[DEMONSO], &ofds);
	FD_SET (so[KERNSO], &ofds);
reselect:
	rfds = ofds;

	if (select (nfds + 1, &rfds, (fd_set *) NULL, (fd_set *) NULL,
		    (struct timeval *) NULL) == -1)
		goto reselect;
again:
	if (FD_ISSET (so[KERNSO], &rfds))
		sockfd = so[KERNSO];
	else if (FD_ISSET (so[DEMONSO], &rfds))
		sockfd = so[DEMONSO];
	else
		goto reselect;
	FD_CLR (sockfd, &rfds);

	req = (request_t *) malloc (sizeof (*req));
	if (req == NULL)
		goto reselect;
	memcpy (req->so, so, sizeof (req->so));

	/* get message header */
	fromlen = sizeof (req->from);
	mlen = recvfrom (sockfd, req->buf, sizeof (req->buf), 0, &req->from,
			 &fromlen);
	if (mlen < header_size ())
	{
		if (debug_on (DEBUG_PROTOCOL))
			if (mlen == -1)
				debug_print ("recvfrom: %s\n", strerror (errno));
			else
				debug_print ("short datagram from host 0x%x\n", req->from.sin_addr.s_addr);
		free (req);
		goto reselect;
	}

	/* ignore untrusted clients */
	if (req->from.sin_port > IPPORT_RESERVED)
	{
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("request from untrusted host 0x%x\n",
				     req->from.sin_addr.s_addr);
		free (req);
		goto reselect;
	}
	req->hostid = req->from.sin_addr.s_addr;

	/* get header info */
	req->bufptr = copy_to_header (req->buf, &req->hdr);
	req->buflen = (req->buf + req->hdr.message_length) - req->bufptr;

	/* verify header info */
	if (req->hdr.message_length != mlen)
	{
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("invalid message length:"
				     "got 0x%hx bytes, expected 0x%x bytes\n",
				     req->hdr.message_length,
				     (unsigned int) mlen);
		free (req);
		goto reselect;
	}

	/* do stuff according to message number */
	if (debug_on (DEBUG_PROTOCOL))
	{
		debug_print ("%s received\n",
			     message_name (req->hdr.message_number));
		debug_print ("\tesi: 0x%llx\n", req->hdr.message_esi);
	}

	/* create thread to handle request */
	if (thread_create (reqvec, &tid, (void *) req) == -1)
		free (req);

	/*
	 * If requests are pending on the other socket, handle them before
	 * waiting in select again. This ensures that handling requests from
	 * one socket doesn't starve the other.
	 */
	if (FD_ISSET (so[DEMONSO], &rfds) || FD_ISSET (so[KERNSO], &rfds))
		goto again;
	else
		goto reselect;
}

static void
usage (const char *program)
{
	fprintf (stderr, "Usage: %s [-c [configdir] -d [%s] -l [logfile] -p [port]]\n", program, name_debug_opts ());
	exit (EXIT_FAILURE);
}

static void
deconfigure_databases (void)
{
	deconfigure_lrep_mappings ();
	deconfigure_client ();
	deconfigure_server ();
}

static int
configure_databases (const char *configdir)
{
	if (configure_server (configdir) ||
	    configure_client (configdir) ||
	    configure_lrep_mappings (configdir))
		return (1);
	if (atexit (deconfigure_databases) == 0)
		return (0);
	deconfigure_databases ();
	return (1);
}

static void
kill_daemon (void)
{
	cap_t ocap = cap_acquire (1, cap_network_mgt);
	if (satmp_done () == -1)
		debug_print ("satmp_done: %s\n", strerror (errno));
	cap_surrender (ocap);
}

static void
sighup (int sig)
{
	/*
	 * In the future, this will reconfigure
	 * satmpd internal databases, rather than
	 * exit and die.
	 */
	kill_daemon ();
	deconfigure_databases ();
	_exit (sig);
}

static void
sigint (int sig)
{
	kill_daemon ();
	deconfigure_databases ();
	_exit (sig);
}

/*
 * Enabled privileged listen, so TCB communication
 * from other daemons can be received.
 * 
 * Set session ID to 1, so SAMP attributes are
 * not attached to satmpd communication.
 */
static int
socket_priv_session (int so)
{
	t6attr_t t6attr;
	t6mask_t t6mask;
	cap_t ocap;
	uid_t sid = T6SAMP_PRIV_SID;
	int failed;
	int r = 0;

	ocap = cap_acquire (1, cap_network_mgt);
	if (t6mls_socket (so, T6_ON) == -1)
	{
		cap_surrender (ocap);
		return (-1);
	}
	cap_surrender (ocap);

	/* allocate control block */
	t6attr = t6alloc_blk (T6M_SESSION_ID);
	if (t6attr == (t6attr_t) NULL)
		return (-1);

	ocap = cap_acquire (1, cap_network_mgt);
	failed = (t6set_attr (T6_SESSION_ID, (void *) &sid, t6attr) == -1 ||
		  t6set_endpt_default (so, T6M_SESSION_ID, t6attr) == -1);
	cap_surrender (ocap);

	/* free memory */
	t6free_blk (t6attr);

	/* return error if setting the session id failed */
	if (failed)
		return (-1);

	/* set the connection mask to enable these attributes */
	if ((r = t6get_endpt_mask (so, &t6mask)) == 0)
	{
	    ocap = cap_acquire (1, cap_network_mgt);
	    r = t6set_endpt_mask (so, t6mask | T6M_SESSION_ID);
	    cap_surrender (ocap);
	}
	return (r);
}

static int
create_socket (int port)
{
	struct sockaddr_in addr;	/* address of daemon's socket */
	int so, on = 1;
	cap_t ocap;

	memset ((void *) &addr, '\0', sizeof (addr));
	addr.sin_port = port;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;

	so = socket (AF_INET, SOCK_DGRAM, 0);
	if (so == -1)
		return (so);
	
	/* permit immediate reuse of this socket address */
	if (setsockopt (so, SOL_SOCKET, SO_REUSEADDR, (void *) &on,
			sizeof (on)) == -1)
	{
		close (so);
		return (-1);
	}

	/* bind socket */
	ocap = cap_acquire (1, cap_priv_port);
	if (bind (so, (struct sockaddr *) &addr, sizeof (addr)) == -1)
	{
		cap_surrender (ocap);
		close (so);
		return (-1);
	}
	cap_surrender (ocap);

	/* set session id */
	if (socket_priv_session (so) == -1)
	{
		close (so);
		return (-1);
	}

	return (so);
}

int
main (int argc, char *argv[])
{
	int port = DEFAULT_SATMPD_PORT;
	int error = 0, c;
	so_set so;
	char *program;
	char *configdir = DEFAULT_CONFIG_DIR;
	struct sigaction act;
	cap_t ocap;
	init_reply rep;

	/* continue only if T6 is enabled */
	if (sysconf (_SC_IP_SECOPTS) <= 0)
	{
		fprintf (stderr, "satmpd: IP Security not enabled - exiting\n");
		exit (0);
	}

	/* turn off yp */
	_yp_disabled = 1;

	/* get program name */
	program = strrchr (argv[0], '/');
	if (program == NULL)
		program = argv[0];
	else
		program++;

	/* parse option arguments */
	while (!error && (c = getopt (argc, argv, "c:d:l:p:")) != -1)
	{
		switch (c)
		{
			char *end;

			case 'c':
				configdir = optarg;
				break;
			case 'd':
				if (parse_debug_opts (optarg))
					error++;
				break;
			case 'l':
				debug_set_log (optarg);
				break;
			case 'p':
				port = (int) strtol (optarg, &end, 10);
				if ((port == 0 && end == optarg) ||
				    *end != '\0')
					error++;
				break;
			case '?':
				error++;
				break;
		}
	}

	/* exit on errors or extraneous arguments */
	if (error || optind < argc)
		usage (program);

	/* turn ourselves into a daemon, preserving debug_fp */
	if (is_debug_state (DEBUG_ALL_OFF) &&
	    _daemonize (_DF_NOCHDIR, fileno (debug_fp), -1, -1) == -1)
	{
		fprintf (stderr, "satmpd: _daemonize failed\n");
		exit (1);
		
	}

	/* create socket */
	if ((so[DEMONSO] = create_socket (port)) == -1 ||
	    (so[KERNSO] = create_socket (0)) == -1)
	{
		debug_print ("socket: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	/* register daemon death */
	if (atexit (kill_daemon))
	{
		debug_print ("cannot register kill_daemon\n");
		kill_daemon ();
		exit (EXIT_FAILURE);
	}

	/* set signal handlers */
	act.sa_flags = 0;
	if (sigemptyset (&act.sa_mask) == -1)
	{
		debug_print ("sigemptyset: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	/* handle SIGHUP */
	act.sa_handler = sighup;
	if (sigaction (SIGHUP, &act, (struct sigaction *) NULL) == -1)
	{
		debug_print ("sigaction: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

	/* handle SIGINT & SIGTERM */
	act.sa_handler = sigint;
	if (sigaction (SIGINT, &act, (struct sigaction *) NULL) == -1 ||
	    sigaction (SIGTERM, &act, (struct sigaction *) NULL) == -1)
	{
		debug_print ("sigaction: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}

#ifndef PTHREADS
	/* ignore SIGCHLD */
	act.sa_handler = SIG_IGN;
#ifdef SA_NOCLDWAIT
	/* prevent creation of zombies */
	act.sa_flags |= SA_NOCLDWAIT;
#endif
	if (sigaction (SIGCHLD, &act, (struct sigaction *) NULL) == -1)
	{
		debug_print ("sigaction: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}
#endif

	/* initialize global semaphore area */
	if (sem_initialize () == -1)
	{
		debug_print ("cannot initialize mutex subsystem\n");
		exit (EXIT_FAILURE);
	}

	/* initialize timer queues */
	if (timer_initialize () == -1)
	{
		debug_print ("cannot initialize timer subsystem\n");
		exit (EXIT_FAILURE);
	}

	/* initialize atomic operations */
	if (atomic_initialize () == -1)
	{
		debug_print ("cannot initialize atomic op subsystem\n");
		exit (EXIT_FAILURE);
	}

	/* initialize databases */
	if (configure_databases (configdir))
	{
		debug_print ("cannot configure databases\n");
		exit (EXIT_FAILURE);
	}

	/* install synthetic reply for localhost */
	if ((error = create_loopback_reply (&rep)) ||
	    (error = receive_init_reply (&rep)))
	{
		debug_print ("synthetic loopback reply: %s\n",
			     result_text (error));
		exit (EXIT_FAILURE);
	}
	destroy_init_reply (&rep);

	if (name_initialize () == -1)
	{
		debug_print ("cannot start name subsystem\n");
		exit (EXIT_FAILURE);
	}

	/* start timer subsystem */
	if (timer_threads_start () == -1)
	{
		debug_print ("cannot start timer threads\n");
		exit (EXIT_FAILURE);
	}

	/* initialize kernel<->satmpd interface */
	ocap = cap_acquire (1, cap_network_mgt);
	if (satmp_init (so[KERNSO], server_generation) == -1)
	{
		cap_surrender (ocap);
		debug_print ("satmp_init: %s\n", strerror (errno));
		exit (EXIT_FAILURE);
	}
	cap_surrender (ocap);

#if 0
	/* re-enable yp */
	_yp_disabled = 0;
#endif

	/* create atomic variables */
	iniatomic (&esi_seq, 0);

	/* Request Loop */
	handle_requests (so);

	return (0);
}
