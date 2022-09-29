/*
 * Bug in Berkeley DB 1.85,  DB_HASH, Big data.
 * When trying to store data that is bigger than 4075 bytes
 * the 'put' silently fails (it returns success but somehow corrupts
 * the database) and a subsequent 'get' gives:	no such key
 *
 * The problem doesn't exist in DB_BTREE.
 * A possible workaround would be to use DB_BTREE
 */

#include <sys/param.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <db.h>

#include <stdarg.h>

static u_int	flags = 0;

static void err(const char *fmt, ...)
{
        va_list ap;
        va_start(ap, fmt);
        (void)vfprintf(stderr, fmt, ap);
        va_end(ap);
        (void)fprintf(stderr, "\n");
        exit(1);
}

#define NOOVERWRITE     "put failed, would overwrite key\n"

void put(DB *dbp, DBT *kp, DBT *dp)
{
        switch (dbp->put(dbp, kp, dp, flags)) {
        case 0:
                break;
        case -1:
                err("put: %s", strerror(errno));
                /* NOTREACHED */
        case 1:
                (void)write(2, NOOVERWRITE, sizeof(NOOVERWRITE) - 1);
                break;
        }
}


void getdata(DB *dbp, DBT *kp, DBT *dp)
{
        switch (dbp->get(dbp, kp, dp, flags)) {
        case 0:
                return;
        case -1:
                err("getdata: %s", strerror(errno));
                /* NOTREACHED */
        case 1:
                err("Compiler -O2 bug: getdata failed, no such key.");
                /* NOTREACHED */
        }
}


void main()
{
    const char * testdb = "test.db";
    size_t	size;
    void *	s;
    DB *	db;
    DBT		key, data1, data2;
    char	buf1[6000], buf2[6000], *mykey = "somekey";

    unlink(testdb);

    /* init, make them different */
    s = (void *) buf1; memset(s, 'X', 5000);
    s = (void *) buf2; memset(s, 'z', 5000);

    key.data = (void *) mykey;
    key.size = strlen(mykey);
    data1.data = buf1;
    data1.size = 5000;
    data2.data = buf2;
    data1.size = 5000;

    for (size = 4073; size <= 9075; size += 151) {
	if ((db = dbopen(testdb, O_CREAT|O_RDWR, S_IRUSR|S_IWUSR,
		 DB_HASH, NULL)) == NULL) {
	    err("dbopen(%s, ...): %s", testdb, strerror(errno));
	}
	data1.size = size;

	put(db, &key, &data1);

	if (db->close(db))
		err("db->close: %s", strerror(errno));

	if ((db = dbopen(testdb, O_RDONLY,
			S_IRUSR|S_IWUSR, DB_HASH, NULL)) == NULL) {
            err("dbopen(%s, ...): %s", testdb, strerror(errno));
        }
	getdata(db, &key, &data2);

	if (strncmp(data1.data, data2.data, size) != 0) {
	    fprintf(stderr, "BUG stored data of size=%lu is incorrect\n", size);
	} else {
	    fprintf(stderr, "size=%lu: OK\n", size);
	}
	if (db->close(db))
		err("db->close: %s", strerror(errno));

    }
    exit(0);
}

