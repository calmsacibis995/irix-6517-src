#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>

#include "promlog.h"

#define FFILE		"prom"

__uint64_t prom_space[FPROM_SIZE / 8];

void makeprom(void)
{
    int                 fd, i;
    char                buf[256];

    if ((fd = open(FFILE, O_WRONLY | O_TRUNC | O_CREAT, 0666)) < 0) {
        printf("couldn't create %s\n", FFILE);
        exit(1);
    }

    memset(buf, 0xff, sizeof (buf));

    for (i = 0; i < FPROM_SIZE / sizeof (buf); i++)
        write(fd, buf, sizeof (buf));

    close(fd);
}

void load(void)
{
    int			fd;

    if ((fd = open(FFILE, O_RDONLY)) < 0) {
        makeprom();
        if ((fd = open(FFILE, O_RDONLY)) < 0) {
            printf("couldn't open %s\n", FFILE);
            exit(1);
        }
    }

    read(fd, prom_space, sizeof (prom_space));

    close(fd);
}

void save(void)
{
    int			fd;

    if ((fd = open(FFILE, O_WRONLY | O_CREAT, 0666)) < 0) {
	printf("couldn't open %s\n", FFILE);
	exit(1);
    }

    write(fd, prom_space, sizeof (prom_space));

    close(fd);
}

static void chkerr(int code)
{
    if (code < 0)
	printf("ERROR %d: %s\n", code, promlog_errmsg(code));
    else
	printf("Return %d\n", code);
}

static void doshow(promlog_t *l, int type)
{
    int			r;
    char		key[PROMLOG_KEY_MAX];
    char		value[PROMLOG_VALUE_MAX];

    if ((r = promlog_first(l, type)) < 0) {
	if (r != PROMLOG_ERROR_EOL)
	    chkerr(r);
	return;
    }

    while (1) {
	r = promlog_get(l, key, value);
	if (r) {
	    chkerr(r);
	    break;
	}

	printf("KEY(%s) VALUE(%s)\n", key, value);

	if ((r = promlog_next(l, type)) < 0) {
	    if (r != PROMLOG_ERROR_EOL)
		chkerr(r);
	    break;
	}
    }
}

main(int argc, char **argv)
{
    promlog_t		l;
    fprom_t		f;
    char		buf[256], *s;
    int			i, r, type;
    char		key[PROMLOG_KEY_MAX];
    char		value[PROMLOG_VALUE_MAX];

    load();

    f.base	= (void *) prom_space;
    f.dev	= FPROM_DEV_HUB;
    f.afn	= 0;

    chkerr(promlog_init(&l, &f, 14, 0));

    while (1) {
	promlog_pos_t	spos;

	save();

	promlog_tell(&l, &spos);
	promlog_dump(&l);
	promlog_seek(&l, spos);

	printf("Pos=0x%x\n", l.pos);

	printf("> ");
	fflush(stdout);

	if (gets(buf) == 0 || buf[0] == 'q')
	    break;

	switch (buf[0]) {
	case 'c':
	    chkerr(promlog_compact(&l));
	    break;
	case 'C':
	    chkerr(promlog_clear(&l));
	    break;
	case 't':
	    chkerr(promlog_type(&l));
	    break;
	case 'f':
	    chkerr(promlog_first(&l, atoi(buf + 1)));
	    break;
	case 'p':
	    chkerr(promlog_prev(&l, atoi(buf + 1)));
	    break;
	case 'n':
	    chkerr(promlog_next(&l, atoi(buf + 1)));
	    break;
	case 'l':
	    chkerr(promlog_last(&l));
	    break;
	case 'g':
	    chkerr(promlog_get(&l, key, value));
	    printf("Key(%s) Value(%s)\n", key, value);
	    break;
	case 'F':
	    type = atoi(buf + 1);
	    s = strchr(buf, ',') + 1;
	    chkerr(promlog_find(&l, s, type));
	    break;
	case 'v':
	    r = promlog_lookup(&l, buf + 1, value, "DEFAULT");
	    chkerr(r);
	    if (r == 0)
		printf("Value(%s)\n", value);
	    break;
	case 'R':
	    type = atoi(buf + 1);
	    s = strchr(buf, ',') + 1;
	    memcpy(key, s, strchr(s, ',') - s);
	    key[strchr(s, ',') - s] = 0;
	    strcpy(value, strchr(s, ',') + 1);
	    chkerr(promlog_replace(&l, key, value, type));
	    break;
	case 'P':
	    memcpy(key, buf + 1, strchr(buf, ',') - (buf + 1));
	    key[strchr(buf, ',') - (buf + 1)] = 0;
	    strcpy(value, strchr(buf, ',') + 1);
	    chkerr(promlog_put_var(&l, key, value));
	    break;
	case 'L':
	    memcpy(key, buf + 1, strchr(buf, ',') - (buf + 1));
	    key[strchr(buf, ',') - (buf + 1)] = 0;
	    strcpy(value, strchr(buf, ',') + 1);
	    chkerr(promlog_put_list(&l, key, value));
	    break;
	case 'M':
	    memcpy(key, buf + 1, strchr(buf, ',') - (buf + 1));
	    key[strchr(buf, ',') - (buf + 1)] = 0;
	    strcpy(value, strchr(buf, ',') + 1);
	    chkerr(promlog_put_log(&l, key, value));
	    break;
	case 'd':
	    chkerr(promlog_delete(&l));
	    break;
	case 's':
	    doshow(&l, atoi(buf + 1));
	    break;
	case '?':
	    printf("c           Compact\n");
	    printf("C           Clear\n");
	    printf("t           Type\n");
	    printf("fN          First type N\n");
	    printf("pN          Previous type N\n");
	    printf("nN          Next type N\n");
	    printf("l           Last\n");
	    printf("g           Get\n");
	    printf("FN,key      Find key type N\n");
	    printf("vkey        Lookup var\n");
	    printf("RN,key,val  Replace with type/key/value\n");
	    printf("Pkey,val    Put var\n");
	    printf("Lkey,val    Put list\n");
	    printf("Mkey,val    Put log\n");
	    printf("d           Delete\n");
	    printf("sN          Show all entries type N\n");
	    break;
	default:
	    printf("?\n");
	    break;
	}
    }

    exit(0);
}
