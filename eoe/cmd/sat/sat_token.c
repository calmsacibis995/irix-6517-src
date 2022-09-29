/*
 *
 * Copyright 1997, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.1 $"

#include <sys/sat.h>
#include "sat_token.h"
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>

sat_token_t
sat_read_token(int fd)
{
	sat_token_header_t header;
	sat_token_t token;
	int data_size;

	if (read(fd, &header, sizeof(header)) != sizeof(header))
		return NULL;

	token = (sat_token_t)malloc(header.sat_token_size);
	token->token_header = header;
	data_size = header.sat_token_size - sizeof(header);

	if (read(fd, &token->token_data, data_size) != data_size) {
		free(token);
		return NULL;
	}

	return token;
}

sat_token_t
sat_fread_token(FILE *fp)
{
	sat_token_header_t header;
	sat_token_t token;
	int data_size;

	if (fread(&header, 1, sizeof(header), fp) != sizeof(header))
		return NULL;

	token = (sat_token_t)malloc(header.sat_token_size);
	token->token_header = header;
	data_size = header.sat_token_size - sizeof(header);

	if (fread(&token->token_data, 1, data_size, fp) != data_size) {
		free(token);
		return NULL;
	}

	return token;
}

int
sat_fetch_token(sat_token_t token, void *data, int size, int cursor)
{
	bcopy(&(token->token_data[cursor]), data, size);

	return cursor + size;
}

void
sat_fetch_one_token(sat_token_t token, void *data, int size)
{
	bcopy(&token->token_data, data, size);
}
