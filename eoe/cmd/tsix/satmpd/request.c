#ident "$Revision: 1.3 $"

#include <sys/types.h>
#include <sys/t6satmp.h>
#include <sys/endian.h>
#include <stdlib.h>
#include <stdio.h>
#include "client.h"
#include "debug.h"
#include "lrep.h"
#include "request.h"
#include "server.h"

int
init_request_buffer (const char *buf, size_t buflen, init_request *req)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* hostid */
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", req->hostid);

	/* generation */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->generation = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);

	/* protocol version */
	memcpy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);
	req->version = ntohs (t16);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tversion: %hu\n", req->version);

	/* reserved field */
	bufptr++;

	/* server state */
	req->server_state = *bufptr++;
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tserver state: %u\n",
			     (unsigned) req->server_state);

	/* token server address */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->token_server = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\ttoken server: 0x%x\n", req->token_server);

	/* backup server address */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->backup_server = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tbackup server: 0x%x\n", req->backup_server);

	/* init misc fields */
	req->format_count = 0;
	req->format = NULL;

	while (bufptr - buf < buflen)
	{
		init_request_attr_format *iraf;
		u_short dcount;

		req->format = realloc (req->format, ++req->format_count * sizeof (*req->format));
		if (req->format == NULL)
			return (1);

		iraf = &req->format[req->format_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		iraf->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", iraf->attribute);

		/* DOT count */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		iraf->dot_count = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tDOT count: %hu\n", iraf->dot_count);

		iraf->dot = malloc (iraf->dot_count * sizeof (*iraf->dot));
		if (iraf->dot == NULL)
		{
			destroy_init_request (req);
			return (1);
		}

		for (dcount = 0; dcount < iraf->dot_count; dcount++)
		{
			init_request_dot_rep *irdr = &iraf->dot[dcount];

			/* DOT weight */
			irdr->weight = *bufptr++;
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT weight: %u\n",
					     (unsigned) irdr->weight);

			/* DOT flags */
			irdr->flags = *bufptr++;
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT flags: %u\n",
					     (unsigned) irdr->flags);

			/* DOT length */
			memcpy (&t16, bufptr, sizeof (t16));
			bufptr += sizeof (t16);
			irdr->length = ntohs (t16);
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT length: %hu\n",
					     irdr->length);

			/* DOT name */
			irdr->dot_name = malloc (irdr->length);
			if (irdr->dot_name == NULL)
			{
				destroy_init_request (req);
				return (1);
			}
			memcpy (irdr->dot_name, bufptr, irdr->length);
			irdr->dot_name[irdr->length - 1] = '\0';
			bufptr += irdr->length;
			if (debug_on (DEBUG_PROTOCOL))
				debug_print ("\t\t\tDOT name: %s\n",
					     irdr->dot_name);
		}
	}
	return (req->format == NULL);
}

int
init_reply_buffer (const char *buf, size_t buflen, init_reply *rep)
{
	return (init_request_buffer (buf, buflen, (init_request *) rep));
}

int
get_attr_request_buffer (const char *buf, size_t buflen, get_attr_request *req)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* generation */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->generation = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);

	/* init misc fields */
	req->token_spec_count = 0;
	req->token_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_attr_request_token_spec *garts;

		req->token_spec = realloc (req->token_spec, ++req->token_spec_count * sizeof (*req->token_spec));
		if (req->token_spec == NULL)
			return (1);

		garts = &req->token_spec[req->token_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		garts->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", garts->attribute);

		/* mapping identifier: not used */
		bufptr += sizeof (t32);

		/* token */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		garts->token = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", garts->token);

		garts->domain = attr_to_domain (req->hostid, garts->attribute);
		if (garts->domain == NULL)
		{
			destroy_get_attr_request (req);
			return (1);
		}
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tdomain: %s\n", garts->domain);
	}
	return (req->token_spec == NULL);
}

int
get_attr_reply_buffer (const char *buf, size_t buflen, get_attr_reply *rep)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* generation */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	rep->generation = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", rep->generation);

	/* init misc fields */
	rep->attr_spec_count = 0;
	rep->attr_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_attr_reply_attr_spec *garas;

		rep->attr_spec = realloc (rep->attr_spec, ++rep->attr_spec_count * sizeof (*rep->attr_spec));
		if (rep->attr_spec == NULL)
			return (SATMP_INTERNAL_ERROR);

		garas = &rep->attr_spec[rep->attr_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		garas->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", garas->attribute);

		/* length of network representation */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		garas->nr_length = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnr length: %hu\n", garas->nr_length);

		/* token */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		garas->token = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", garas->token);

		/* network representation itself */
		garas->nr = malloc (garas->nr_length);
		if (garas->nr == NULL)
		{
			destroy_get_attr_reply (rep);
			return (SATMP_INTERNAL_ERROR);
		}
		memcpy (garas->nr, bufptr, garas->nr_length);
		garas->nr[garas->nr_length - 1] = '\0';
		bufptr += garas->nr_length;
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnetwork rep: %s\n", garas->nr);
	}
	return (rep->attr_spec == NULL ? SATMP_INTERNAL_ERROR : SATMP_REPLY_OK);
}

int
get_attr_reply_failed (u_char status, get_attr_request *req,
		       get_attr_reply *rep)
{
	u_short i;

	rep->hostid = req->hostid;
	rep->generation = req->generation;
	rep->attr_spec_count = req->token_spec_count;

	rep->attr_spec = malloc (rep->attr_spec_count * sizeof (*rep->attr_spec));
	if (rep->attr_spec == NULL)
		return (1);

	for (i = 0; i < rep->attr_spec_count; i++)
	{
		get_attr_request_token_spec *garts;
		get_attr_reply_attr_spec *garas;

		garts = &req->token_spec[i];
		garas = &rep->attr_spec[i];

		garas->attribute = garts->attribute;
		garas->token = garts->token;
		garas->nr_length = 0;
		garas->nr = NULL;
		garas->status = status;
	}

	return (0);
}

int
get_tok_request_buffer (const char *buf, size_t buflen, get_tok_request *req)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* generation */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->generation = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);

	/* init misc fields */
	req->attr_spec_count = 0;
	req->attr_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_tok_request_attr_spec *gtras;

		req->attr_spec = realloc (req->attr_spec, ++req->attr_spec_count * sizeof (*req->attr_spec));
		if (req->attr_spec == NULL)
			return (1);

		gtras = &req->attr_spec[req->attr_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		gtras->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", gtras->attribute);

		/* length of network representation */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		gtras->nr_length = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnr length: %hu\n", gtras->nr_length);

		/* mapping identifier */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		gtras->mid = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     gtras->mid);

		gtras->nr = malloc (gtras->nr_length);
		gtras->domain = attr_to_domain (req->hostid, gtras->attribute);
		if (gtras->nr == NULL || gtras->domain == NULL)
		{
			destroy_get_tok_request (req);
			return (1);
		}
		memcpy (gtras->nr, bufptr, gtras->nr_length);
		gtras->nr[gtras->nr_length - 1] = '\0';
		bufptr += gtras->nr_length;
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tdomain: %s\n", gtras->domain);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnetwork rep: %s\n", gtras->nr);
	}
	return (req->attr_spec == NULL);
}

int
get_tok_reply_buffer (const char *buf, size_t buflen, get_tok_reply *rep)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* generation */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	rep->generation = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", rep->generation);

	/* init misc fields */
	rep->token_spec_count = 0;
	rep->token_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_tok_reply_token_spec *gtrts;

		rep->token_spec = realloc (rep->token_spec, ++rep->token_spec_count * sizeof (*rep->token_spec));
		if (rep->token_spec == NULL)
			return (1);

		gtrts = &rep->token_spec[rep->token_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		gtrts->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", gtrts->attribute);

		/* mapping identifier */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		gtrts->mid = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     gtrts->mid);

		/* token */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		gtrts->token = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", gtrts->token);
	}
	return (rep->token_spec == NULL);
}

int
get_lrtok_request_buffer (const char *buf, size_t buflen,
			  get_lrtok_request *req)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* hostid */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->hostid = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", req->hostid);

	/* generation */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	req->generation = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tgeneration: %u\n", req->generation);

	/* init misc fields */
	req->attr_spec_count = 0;
	req->attr_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_lrtok_request_attr_spec *glras;

		req->attr_spec = realloc (req->attr_spec, ++req->attr_spec_count * sizeof (*req->attr_spec));
		if (req->attr_spec == NULL)
			return (1);

		glras = &req->attr_spec[req->attr_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		glras->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", glras->attribute);

		/* length of network representation */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		glras->lr_length = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnr length: %hu\n", glras->lr_length);

		/* mapping identifier */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		glras->mid = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     glras->mid);

		/* domain */
		glras->domain = attr_to_domain (req->hostid, glras->attribute);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tdomain: %s\n", glras->domain != NULL ? glras->domain : "no domain");

		/* local representation */
		glras->lr = malloc (glras->lr_length);
		if (glras->lr == NULL)
		{
			destroy_get_lrtok_request (req);
			return (1);
		}
		memcpy (glras->lr, bufptr, glras->lr_length);
		bufptr += glras->lr_length;
		if (debug_on (DEBUG_PROTOCOL))
		{
			char *text;

			text = lrep_to_text (glras->lr, glras->attribute);
			debug_print ("\t\tlocal rep: %s\n", text);
			destroy_lrep (text, glras->attribute);
		}
	}
	return (req->attr_spec == NULL);
}

int
host_cmd_buffer (const char *buf, size_t buflen, host_cmd *cmd)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	if (buflen < sizeof (t32) + sizeof (t16) + sizeof (cmd->kernel))
		return (1);

	/* hostid */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	cmd->hostid = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", cmd->hostid);

	/* port */
	memcpy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);
	cmd->port = ntohs (t16);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tport: %hu\n", cmd->port);

	/* kernel? */
	cmd->kernel = *bufptr++;
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tkernel: %u\n", (unsigned) cmd->kernel);

	return (0);
}

int
get_attr_admin_buffer (const char *buf, size_t buflen, host_cmd *cmd,
		       get_attr_request *req)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* hostid */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	cmd->hostid = req->hostid = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", cmd->hostid);

	/* port */
	memcpy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);
	cmd->port = ntohs (t16);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tport: %hu\n", cmd->port);

	/* kernel? */
	cmd->kernel = *bufptr++;
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tkernel: %u\n", (unsigned) cmd->kernel);

	/* init misc fields */
	if (get_client_generation (req->hostid, &req->generation))
		return (1);
	req->token_spec_count = 0;
	req->token_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_attr_request_token_spec *garts;

		req->token_spec = realloc (req->token_spec, ++req->token_spec_count * sizeof (*req->token_spec));
		if (req->token_spec == NULL)
			return (1);

		garts = &req->token_spec[req->token_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		garts->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", garts->attribute);

		/* domain name */
		garts->domain = attr_to_domain (req->hostid, garts->attribute);
		if (debug_on (DEBUG_PROTOCOL) && garts->domain != NULL)
			debug_print ("\t\tdomain: %s\n", garts->domain);

		/* token */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		garts->token = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\ttoken: 0x%08.8x\n", garts->token);
	}
	return (req->token_spec == NULL);
}

int
get_tok_admin_buffer (const char *buf, size_t buflen, host_cmd *cmd,
		      get_tok_request *req)
{
	const char *bufptr = buf;
	u_int t32;
	u_short t16;

	/* hostid */
	memcpy (&t32, bufptr, sizeof (t32));
	bufptr += sizeof (t32);
	cmd->hostid = req->hostid = ntohl (t32);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\thostid: 0x%x\n", cmd->hostid);

	/* port */
	memcpy (&t16, bufptr, sizeof (t16));
	bufptr += sizeof (t16);
	cmd->port = ntohs (t16);
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tport: %hu\n", cmd->port);

	/* kernel? */
	cmd->kernel = *bufptr++;
	if (debug_on (DEBUG_PROTOCOL))
		debug_print ("\tkernel: %u\n", (unsigned) cmd->kernel);

	/* init misc fields */
	req->generation = 0;
	req->attr_spec_count = 0;
	req->attr_spec = NULL;

	while (bufptr - buf < buflen)
	{
		get_tok_request_attr_spec *gtras;

		req->attr_spec = realloc (req->attr_spec, ++req->attr_spec_count * sizeof (*req->attr_spec));
		if (req->attr_spec == NULL)
			return (1);

		gtras = &req->attr_spec[req->attr_spec_count - 1];

		/* attribute type */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		gtras->attribute = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattribute: %hu\n", gtras->attribute);

		/* length of network representation */
		memcpy (&t16, bufptr, sizeof (t16));
		bufptr += sizeof (t16);
		gtras->nr_length = ntohs (t16);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tattr length: %hu\n",
				     gtras->attribute);

		/* mapping identifier */
		memcpy (&t32, bufptr, sizeof (t32));
		bufptr += sizeof (t32);
		gtras->mid = ntohl (t32);
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tmapping identifier: %u\n",
				     gtras->mid);

		/* domain */
		gtras->domain = NULL;

		/* network representation */
		gtras->nr = malloc (gtras->nr_length);
		if (gtras->nr == NULL)
		{
			destroy_get_tok_request (req);
			return (1);
		}
		memcpy (gtras->nr, bufptr, gtras->nr_length);
		gtras->nr[gtras->nr_length - 1] = '\0';
		bufptr += gtras->nr_length;
		if (debug_on (DEBUG_PROTOCOL))
			debug_print ("\t\tnetwork rep: %s\n", gtras->nr);
	}
	return (req->attr_spec == NULL);
}

void
destroy_init_request (init_request *req)
{
	u_short i, j;

	for (i = 0; i < req->format_count; i++)
	{
		if (req->format[i].dot)
		{
			for (j = 0; j < req->format[i].dot_count; j++)
				free (req->format[i].dot[j].dot_name);
			free (req->format[i].dot);
		}
	}
	free (req->format);
}

void
destroy_init_reply (init_reply *rep)
{
	destroy_init_request ((init_request *) rep);
}

void
destroy_get_tok_request (get_tok_request *req)
{
	u_short i;

	for (i = 0; i < req->attr_spec_count; i++)
	{
		free (req->attr_spec[i].nr);
		free (req->attr_spec[i].domain);
	}
	free (req->attr_spec);
}

void
destroy_get_tok_reply (get_tok_reply *rep)
{
	free (rep->token_spec);
}

void
destroy_get_attr_request (get_attr_request *req)
{
	u_short i;

	for (i = 0; i < req->token_spec_count; i++)
		free (req->token_spec[i].domain);
	free (req->token_spec);
}

void
destroy_get_attr_reply (get_attr_reply *rep)
{
	u_short i;

	for (i = 0; i < rep->attr_spec_count; i++)
		free (rep->attr_spec[i].nr);
	free (rep->attr_spec);
}

void
destroy_get_lrtok_request (get_lrtok_request *req)
{
	u_short i;

	for (i = 0; i < req->attr_spec_count; i++)
	{
		destroy_lrep (req->attr_spec[i].lr,
			      req->attr_spec[i].attribute);
		free (req->attr_spec[i].domain);
	}
	free (req->attr_spec);
}

void
destroy_get_lrtok_reply (get_lrtok_reply *rep)
{
	destroy_get_tok_reply ((get_tok_reply  *) rep);
}
