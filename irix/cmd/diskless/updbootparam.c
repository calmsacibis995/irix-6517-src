/*
 *	@(#)updbootparam.c	8/12/88		initial release
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <sys/file.h>
#include <sys/time.h>
#include <syslog.h>

char	key_store[] = "/var/boot/keystore";
char	key_tmp[] = "/var/boot/keystore.tmp";

int	no_key = 0;
int	debug = 0;

char * opstring[] = {
	"",
	"CHANGE",
	"INSERT",
	"DELETE",
	"STORE",
};

void cexit(u_long err);

main(argc, argv)
	int argc;
	char *argv[];
{
	unsigned op;
	char name[MAXNETNAMELEN + 1];
	char key[256];
	char data[256], *data_p;
	char line[256];
	unsigned keylen;
	unsigned datalen;
	FILE *rf;
	FILE *wf;
	char *fname;
	char *tmpname, *outval, *domain;
	int  status, err, len, child;


	openlog("updbootprm", LOG_PID, LOG_USER);

	if (argc != 3) {
		if ((argc != 4) || (strcmp(argv[1], "-d"))) {
			syslog(LOG_ERR, "%s", yperr_string(YPERR_BADARGS));
			cexit(YPERR_BADARGS);
		}
		argc--;
		argv++;
		debug++;
	}

	if (yp_get_default_domain(&domain)) {
		syslog(LOG_ERR, "%s: yp_get_default_domain failed",
				yperr_string(YPERR_NODOM));
		cexit(YPERR_NODOM);
	}

	fname = argv[1];
	tmpname = malloc(strlen(fname) + 4);
	if (tmpname == NULL) {
		syslog(LOG_ERR, "%s: malloc failed",
			yperr_string(YPERR_RESRC));
		cexit(YPERR_RESRC);
	}
	sprintf(tmpname, "%s.tmp", fname);
	
	/*
	 * Get input
	 */
	if ((!scanf("%s\n", name)) ||
	   (!scanf("%u\n", &op) ||
	   (op != YPOP_INSERT && op != YPOP_DELETE)) ||
	   (!scanf("%u\n", &keylen)) ||
	   (!fread(key, keylen, 1, stdin)) ||
	   (!scanf("%u\n", &datalen)) ||
	   (!fread(data, datalen, 1, stdin))) {
		syslog(LOG_ERR, "%s: get input failed",
				yperr_string(YPERR_YPERR));
		cexit(YPERR_YPERR);
	}

	key[keylen] = 0;
	data[datalen] = 0;
	if (debug) {
		syslog(LOG_INFO, "%s %s request received",
				opstring[op], key);
	}

	/*
	 * Check permission
	 */
	data_p = data;
	if (strncmp(data_p, "nokey", 5) == 0) {
		no_key = 1;
		data_p += 6;
	}
	if (op == YPOP_DELETE) {
		if (check_permit(&data_p, key)) {
			cexit(YPERR_ACCESS);
		}
	}

	/*
	 * Open files 
	 */
	rf = fopen(fname, "r");
	if (rf == NULL) {
		syslog(LOG_ERR, "%s: fopen %s failed",
				fname, yperr_string(YPERR_BUSY));
		cexit(YPERR_BUSY);	
	}
	wf = fopen(tmpname, "w");
	if (wf == NULL) {
		syslog(LOG_ERR, "%s: fopen %s failed",
			tmpname, yperr_string(YPERR_BUSY));
		cexit(YPERR_BUSY);
	}

	err = -1;
	while (fgets(line, sizeof(line), rf)) {
		if (err < 0 && match(line, key)) {
			switch (op) {
			case YPOP_INSERT:
				err = YPERR_KEY;
				syslog(LOG_ERR, "%s: %s already exists",
					yperr_string(YPERR_KEY), key);
				break;
			case YPOP_DELETE:
				/* do nothing */
				err = 0;
				break;
			}	
		} else if (err > 0)
			break;
		else 
			fputs(line, wf);
	}

	if (err < 0) {
		switch (op) {
		case YPOP_DELETE:
			if (debug)
				syslog(LOG_INFO,
					"deleting non-exist entry(%s)", key);
			err = 0;
			break;
		case YPOP_INSERT:
			err = 0;	
			fprintf(wf, "%s %s\n", key, data_p);	
			break;
		}
	}
	fclose(wf);
	fclose(rf);

	if (err) {
		if (unlink(tmpname) < 0) {
			syslog(LOG_ERR, "%s: unlink %s failed",
				yperr_string(YPERR_YPERR), tmpname);
			cexit(YPERR_YPERR);
		}
		cexit(err);
	}

	if (rename(tmpname, fname) < 0) {
		syslog(LOG_ERR, "%s: rename %s to %s failed",
			yperr_string(YPERR_YPERR), tmpname, fname);
		cexit(YPERR_YPERR);
	}

	child = fork();
	if (child < 0) {
		syslog(LOG_ERR, "%s: fork failed", yperr_string(YPERR_RESRC));
		cexit(YPERR_RESRC);	
	}

	if (child == 0) {
		close(0); close(1); close(2);
		open("/dev/null", O_RDWR, 0);
		dup(0);
		dup(0);
		execl("/bin/sh", "sh", "-c", argv[2], NULL);
	}

	if (!no_key)
		err = change_key(op, key);

	if (wait(&status) == -1) {
		syslog(LOG_ERR, "%s: wait failed, status=%d",
			yperr_string(YPERR_YPERR), status);
		cexit(YPERR_YPERR);
	}

	if (err == 0) {
		syslog(LOG_INFO, "\"%s\" %sD successfully.",
			key, opstring[op]);
	}
	cexit(err);
}

/*
 *	print err to stdout so that ypupdated will receive the message
 */
void
cexit(u_long err)
{
	printf("%d", err);
	exit(0);
}

int
match(line, name)
	char *line;
	char *name;
{
	int len;

	len = strlen(name);
	return(strncmp(line, name, len) == 0 && 
		(line[len] == ' ' || line[len] == '\t'));
}


/*
 *	called for deletion only
 */
check_permit(ptr, key)
char	**ptr;
char	*key;
{
	FILE	*rf;
	char	name[50];
	int	hex1, hex2, hex3;

	if ( (rf = fopen(key_store, "r")) == NULL )
			return (0);

	if ( strncmp(*ptr, "key=", 4) == 0 )
		*ptr += 4;
	while( fscanf(rf, "%s	%x %x %x", name, &hex1, &hex2, &hex3) != EOF ) {
		if (strcmp(name, key) == 0 ) {
			if ( no_key ) {
				fclose(rf);
				return (1);
			}
			if ( hex1 == **ptr && hex2 == *(*ptr+1) &&
				hex3 == *(*ptr+2) ) {
				*ptr += 3;
				if ( *(*ptr)++ == ' ' ) {
					fclose(rf);
					return (0);
				}
			}
		}
	}

	fclose(rf);
	return (0);
}


change_key(op, key)
int	op;
char	*key;
{
	FILE	*rf, *wf;
	char	name[30], *string, new_string[7];
	int	rand_no, hex1, hex2, hex3;
	u_long	*ret;

	rf = fopen(key_store, "r");
	wf = fopen(key_tmp, "w");

	if (rf != NULL) {
		for (; fscanf(rf, "%s	%x %x %x\n", name, &hex1, &hex2, &hex3)
			!= EOF;) {
			if (strcmp(key, name) != 0)
				fprintf(wf, "%s	%x %x %x\n", name, hex1,
					hex2, hex3);
		}
		fclose(rf);
	}

	if (op == YPOP_INSERT) {
		time((time_t *)&rand_no);
		srand(rand_no);
		string = (char *)( ((int)new_string + 3) & ~3 );
		ret = (u_long *)string;
		*(string+3) = 0;
		*(string) = (rand() & 0xff);
		*(string+1) = (rand() & 0xff);
		*(string+2) = (rand() & 0xff);
		if ( *string == 0 )
			(*string)++;
		if ( *(string + 1) == 0 )
			(*(string + 1))++;
		if ( *(string + 2) == 0 )
			(*(string + 2))++;
		fprintf(wf, "%s	%x %x %x\n", key,
			*string, *(string+1), *(string+2));
	}

	fclose(wf);
	if (rename(key_tmp, key_store) < 0) {
		syslog(LOG_ERR, "%s: rename key_store failed",
			yperr_string(YPERR_YPERR));
		return(YPERR_YPERR);
	}
	if (chmod(key_store, 0600) == -1) {
		syslog(LOG_ERR, "%s: chmod key_store failed",
			yperr_string(YPERR_YPERR));
		return(YPERR_YPERR);
	}

	return(0);
}
