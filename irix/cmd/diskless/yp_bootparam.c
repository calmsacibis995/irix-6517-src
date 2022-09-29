/*
 *	@(#)yp_bootparam.c	8/12/88		initial release
 */
#ident	"$Revision: 1.5 $"

#include <stdio.h>
#include <assert.h>
#include <getopt.h>
#include <string.h>
#include <rpc/rpc.h>
#include <netdb.h>
#include <sys/socket.h>
#include <rpcsvc/ypclnt.h>
#include <netinet/in.h>
#include <errno.h>
#include "hostreg.h"

char	bootparams[] = "bootparams";
char	key_file[50];
char	key_tmp[50];

void usage(void);

main(argc, argv)
int	argc;
char	**argv;
{
	int	err, c, op = 0, bare = 0;
	char	*domain, in_key[30], key[60], param_f[50], data[512], *r_key;
	char	*dot, *nxt_dot;
	FILE	*wf;
	struct hostent	*hp;

	in_key[0] = key[0] = param_f[0] = 0;
	while ((c = getopt(argc, argv, "adh:k:f:b")) != -1) {
		switch (c) {
		case 'a':
			op = YPOP_INSERT;
			break;
		case 'd':
			op = YPOP_DELETE;
			break;
		case 'b':
			bare = 1;
			break;
		case 'h':
			strcpy(in_key, optarg);
			break;
		case 'f':
			strcpy(param_f, optarg);
			break;
		case 'k':
			strcpy(key_file, optarg);
			strcat(key_tmp, key_file);
			strcat(key_tmp, ".tmp");
			break;
		case '?':
			usage();
			exit(1);
		}
	}

	if ( (optind >= argc && !strlen(param_f) && op == YPOP_INSERT) ||
		op == 0 || !strlen(in_key) ) {
		usage();
		exit(1);
	}

	if ( err = yp_get_default_domain(&domain) ) {
		fprintf(stderr, "get domain error.\n");
		exit(err);
	}
	strcpy(key, in_key);
	if ( !bare ) {
		if ( (hp = gethostbyname(key)) == NULL ) {
			herror(key);
			return 1;
		}
		strcpy(key, hp->h_name);
	}

	data[0] = 0;
	if ( !strlen(key_file) )
		strcat(data, "nokey ");
	else if ( op == YPOP_DELETE && strlen(key_file) )
		if ( err = get_key(key, data) ) {
			fprintf(stderr, "warning: key missing for %s\n", key);
			strcat(data, "nokey ");
		}


	if ( optind < argc ) {
		for ( ; optind < argc; optind++) {
			if ( valid_string(argv[optind]) ) {
				strcat(data, argv[optind]);
				strcat(data, " ");
			}
			else {
				fprintf(stderr, "invalid argument\n");
				exit(EINVAL);
			}
		}
		data[strlen(data) - 1] = 0;
	}
	else if ( strlen(param_f) )
		if ( err = get_data(param_f, data) ) {
			fprintf(stderr, "Get file error, %x\n", err);
			exit(err);
		}

	err = yp_update(domain, bootparams, op, key, strlen(key),
		data, strlen(data) );

	if ( err & 0xff ) 
		fprintf(stderr, "NIS update failed: %s\n",
			yperr_string(err&0xff));
	else if ( op == YPOP_INSERT && strlen(key_file) ) {
		wf = fopen(key_file, "a");
		r_key = (char *)&err;
		fprintf(wf, "%s	%x %x %x\n", key, r_key[0], r_key[1], r_key[2]);
		fclose(wf);
	} else if ( op == YPOP_DELETE && strlen(key_file) ) {
		err = remove_key(key);
	}

	if ( strlen(key_file) )
		chmod(key_file, 0600);
	exit(err&0xff);
}



get_data(pf, data)
char	*pf;
char	*data;
{
	FILE	*rf;
	int	len, error;
	char	buf[100];

	if ( (rf = fopen(pf, "r")) == NULL )
		return (ENOENT);
	while( fscanf(rf, "%s", buf) != EOF ) {
		if ( valid_string(buf)) {
			strcat(data, buf);
			strcat(data, " ");
		}
		else {
			fclose(rf);
			return (EBADF);
		}
	}
	fclose(rf);
	if ( len = strlen(data) ) {
		data[len-1] = 0;
		return (0);
	}
	return (EBADF);
}


get_key(name, buf)
char	*name;
char	*buf;
{
	int	err, hex1, hex2, hex3;
	char	key[40], data[7], *dat_p;
	FILE	*rf;

	if ( (rf = fopen(key_file, "r")) == NULL )
		return(ENOENT);
	data[3] = 0;
	err = ENOENT;
	while ( fscanf(rf, "%s	%x %x %x", key, &hex1, &hex2, &hex3) !=
		EOF )
		if ( strcmp(name, key) == 0 ) {
			data[0] = hex1;
			data[1] = hex2;
			data[2] = hex3;
			strcat(buf, "key=");
			strcat(buf, data);
			strcat(buf, " ");
			err = 0;
		}

	fclose(rf);
	return(err);
}


valid_string(str)
char	*str;
{
	if ( strncmp(str, "root=", 5) == 0 ||
		strncmp(str, "sbin=", 5) == 0 ||
		strncmp(str, "swap=", 5) == 0 ||
		strncmp(str, "dump=", 5) == 0 )
		return (1);
	else
		return (0);
}


remove_key(key)
char	*key;
{
	FILE	*rf, *wf;
	char	name[30];
	int	err, hex1, hex2, hex3;

	rf = fopen(key_file, "r");
	wf = fopen(key_tmp, "w");
	if (rf == NULL || wf == NULL)
		return (-1);

	for ( ; fscanf(rf, "%s	%x %x %x", name, &hex1, &hex2, &hex3) != EOF; )
		if ( strcmp(key, name) != 0 )
			fprintf(wf, "%s	%x %x %x\n", name, hex1, hex2, hex3);

	fclose(rf);
	if ( (err = rename(key_tmp, key_file)) < 0 ) {
		fprintf(stderr, "rename error");
		return (err);
	}

	return (0);
}

void
usage(void)
{
	fprintf(stderr, "yp_bootparam -h host -a [-b] [-k key_file] -f file\n");
	fprintf(stderr, "             -h host -a [-b] [-k key_file] root=... share=... swap=... dump=...\n");
	fprintf(stderr, "             -h host -d [-b] [-k key_file]\n");
}
