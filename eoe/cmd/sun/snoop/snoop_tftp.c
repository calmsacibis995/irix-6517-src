/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_tftp.c,v 1.1 1996/06/19 20:34:28 nn Exp $"

#include <fcntl.h>
#include <arpa/tftp.h>
#include "snoop.h"

extern char *dlc_header;
char *tftperror();
char *show_type();

interpret_tftp(flags, tftp, fraglen)
	int flags;
	struct tftphdr *tftp;
	int fraglen;
{
	char *name, *mode;
	extern int src_port, dst_port;
	int blocksize = fraglen - 4;

	switch (ntohs(tftp->th_opcode)) {
	case RRQ:
	case WRQ:
		add_transient(src_port, interpret_tftp);
		break;
	case ERROR:
		del_transient(src_port);
		break;
	}

	if (flags & F_SUM) {
		switch (ntohs(tftp->th_opcode)) {
		case RRQ:
			name = (char *) &tftp->th_stuff;
			mode = name + (strlen(name) + 1);
			(void) sprintf(get_sum_line(),
				"TFTP Read \"%s\" (%s)", name, mode);
			break;
		case WRQ:
			name = (char *) &tftp->th_stuff;
			mode = name + (strlen(name) + 1);
			(void) sprintf(get_sum_line(),
				"TFTP Write \"%s\" (%s)", name, mode);
			break;
		case DATA:
			(void) sprintf(get_sum_line(),
				"TFTP Data block %d (%d bytes)%s",
				ntohs(tftp->th_block),
				blocksize,
				blocksize < 512 ? " (last block)":"");
			break;
		case ACK:
			(void) sprintf(get_sum_line(),
				"TFTP Ack  block %d",
				ntohs(tftp->th_block));
			break;
		case ERROR:
			(void) sprintf(get_sum_line(),
				"TFTP Error: %s",
				tftperror(ntohs(tftp->th_code)));
			break;
		}
	}

	if (flags & F_DTAIL) {

	show_header("TFTP:  ", "Trivial File Transfer Protocol", fraglen);
	show_space();
	(void) sprintf(get_line((char *)tftp->th_opcode - dlc_header, 2),
		"Opcode = %d (%s)",
		ntohs(tftp->th_opcode),
		show_type(ntohs(tftp->th_opcode)));

	switch (ntohs(tftp->th_opcode)) {
	case RRQ:
	case WRQ:
		name = (char *) &tftp->th_stuff;
		mode = name + (strlen(name) + 1);
		(void) sprintf(
			get_line(name - dlc_header, strlen(name) + 1),
			"File name = \"%s\"",
			name);
		(void) sprintf(
			get_line(mode - dlc_header, strlen(mode) + 1),
			"Transfer mode = %s",
			mode);
		break;

	case DATA:
		(void) sprintf(
			get_line((char *)tftp->th_block - dlc_header, 2),
			"Data block = %d%s",
			ntohs(tftp->th_block),
			blocksize < 512 ? " (last block)":"");
		(void) sprintf(
			get_line((char *)tftp->th_data - dlc_header, blocksize),
			"[ %d bytes of data ]",
			blocksize);
		break;

	case ACK:
		(void) sprintf(
			get_line((char *)tftp->th_block - dlc_header, 2),
			"Acknowledge block = %d",
			ntohs(tftp->th_block));
		break;

	case ERROR:
		(void) sprintf(
			get_line((char *)tftp->th_code - dlc_header, 2),
			"Error = %d (%s)",
			ntohs(tftp->th_code),
			tftperror(ntohs(tftp->th_code)));
		(void) sprintf(
			get_line((char *)tftp->th_data - dlc_header,
				strlen(tftp->th_data) + 1),
			"Error string = \"%s\"",
				tftp->th_data);
	}
	}

	return (fraglen);
}

char *
show_type(t)
	int t;
{
	switch (t) {
	case RRQ:	return ("read request");
	case WRQ:	return ("write request");
	case DATA:	return ("data packet");
	case ACK:	return ("acknowledgement");
	case ERROR:	return ("error");
	}
	return ("?");
}

char *
tftperror(code)
    unsigned short code;
{
	static char buf[128];

	switch (code) {
	case EUNDEF:	return ("not defined");
	case ENOTFOUND:	return ("file not found");
	case EACCESS:	return ("access violation");
	case ENOSPACE:	return ("disk full or allocation exceeded");
	case EBADOP:	return ("illegal TFTP operation");
	case EBADID:	return ("unknown transfer ID");
	case EEXISTS:	return ("file already exists");
	case ENOUSER:	return ("no such user");
	}
	(void) sprintf(buf, "%d", code);

	return (buf);
}
