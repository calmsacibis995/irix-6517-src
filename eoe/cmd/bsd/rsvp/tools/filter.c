/*
 *  @(#) $Id: filter.c,v 1.6 1997/11/14 07:35:57 mwang Exp $
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "rapi_lib.h"
#include "rsvp_api.h"	/* To define management interface */

char *usage = "Usage:\t%s on\n\t%s off\n\t%s [[+/-]address] ...\n";

int
main(int argc, char *argv[]) {
	if (argc == 1) {
		printf(usage, *argv, *argv, *argv);
		return(0);
	} else if (argc == 2) {
		if (strcmp(argv[1], "on") == 0) {
			filteron();
			return(0);
		} else if (strcmp(argv[1], "off") == 0) {
			filteroff();
			return(0);
		}
	}
	while (--argc)
		(void) modify_filters(*++argv);
}

int
modify_filters(char *ap) {
	int		num_filters;
	rapi_cmd_t	cmd = {2, RAPI_CMD_ADD_FILTER, 0};

	if (*ap == '-') {
		cmd.rapi_cmd_type = RAPI_CMD_DEL_FILTER;
		ap++;
	} else if (*ap == '+')
		ap++;
	if ((cmd.rapi_data[0] = inet_addr(ap)) == -1) {
		printf("Malformed address: %s\n", ap);
		return(-1);
	}
	num_filters = rapi_rsvp_cmd(&cmd);
	printf("Adding debug filter %s\n", ap);
	if (num_filters == -1)
		printf("Error!\n");
	else
		printf("There is now %d filter(s)\n", num_filters);
	return(0);
}

int
filteron() {
	printf("Changing debug filter to: ON\n");
	set_filter_status(RAPI_CMD_FILTER_ON);
	return(1);
}

int
filteroff() {
	printf("Changing debug filter to: OFF\n");
	set_filter_status(RAPI_CMD_FILTER_OFF);
	return(1);
}

int set_filter_status(int code) {
	int	old_level;
#ifdef sgi
	/* SGI compiler doesn't like a variable in the array - mwang */
	rapi_cmd_t	cmd = {1, 0, 0, 0};
	cmd.rapi_cmd_type = code;
#else
	rapi_cmd_t	cmd = {1, code, 0, 0};
#endif

	old_level = rapi_rsvp_cmd(&cmd);
	if (old_level == 0)
		printf("Debug filter was: OFF\n");
	else if (old_level == 1)
		printf("Debug filter was: ON\n");
	else {
		printf("Something is wrong!\n");
		return(-1);
	}
	return(1);
}
