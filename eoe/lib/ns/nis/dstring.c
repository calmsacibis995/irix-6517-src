#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "dstring.h"

/*
** This routine just sets all the items in a dstring_t structure to zero.
** It should be called before first use to insure that the structure is in
** a known state.
*/
int
ds_init(dstring_t *ds, int len)
{
	if (! ds) {
		return 0;
	}

	if (len) {
		ds->data = (char *)nsd_malloc(len);
		if (! ds->data) {
			return 0;
		}
		*ds->data = 0;
	}

	ds->size = len;
	ds->used = 0;

	return 1;
}

/*
** This routine frees any dynamically allocated memory associated with
** a dynamic string.
*/
void
ds_clear(dstring_t *ds)
{
	if (ds && ds->data) {
		free(ds->data);
		ds->size = ds->used = 0;
	}
}

/*
** This routine simply resets the data to null without releasing the
** memory.  It is useful when the string is going to be reused.
*/
void
ds_reset(dstring_t *ds)
{
	if (ds && ds->data) {
		*ds->data = 0;
	}
	ds->used = 0;
}

/*
** This routine is used to allocate memory for the dynamic string.  It
** simply allocates a new chunk of memory, copies in the old data, and
** frees the old memory.  We don't use realloc so that we are not
** destructive if we could not allocate the memory.
*/
int
ds_grow(dstring_t *ds, int len)
{
	char *n;

	if (! ds) {
		return 0;
	}

	if (ds->size < len) {
		n = (char *)nsd_malloc(len);
		if (! n) {
			return 0;
		}
		ds->size = len;

		if (ds->data) {
			if (ds->used > 0) {
				memcpy(n, ds->data, ds->used);
				n[ds->used] = 0;
			}
			free(ds->data);
		}

		ds->data = n;
	}

	return 1;
}

/*
** This just appends the given string onto the end of the dynamic string,
** reallocating memory if needed.
*/
int
ds_append(dstring_t *ds, char *s, int len)
{
	int needed;

	if (! ds || ! s) {
		return 0;
	}

	needed = (len) ? len : strlen(s);
	needed += ds->used + 1;

	if (ds->size > needed || ds_grow(ds, needed)) {
		memcpy(ds->data + ds->used, s, len);
		ds->used += len;
		ds->data[ds->used] = 0;
		return 1;
	}

	return 0;
}

/*
** This routine inserts the given string at the beginning of a dynamic
** string, reallocating memory if needed.
*/
int
ds_prepend(dstring_t *ds, char *s, int len)
{
	int needed;

	if (! ds || ! s) {
		return 0;
	}

	needed = (len) ? len : strlen(s);
	needed += ds->used + 1;

	if (ds->size < needed || ds_grow(ds, needed)) {
		memmove(ds->data + len, ds->data, ds->used);
		memcpy(ds->data, s, len);
		ds->used += len;
		ds->data[ds->used] = 0;
		return 1;
	}

	return 0;
}

/*
** This routine resets the dynamic string to null then appends the given
** string reallocating space if needed.
*/
int
ds_set(dstring_t *ds, char *s, int len)
{
	ds_reset(ds);
	return ds_append(ds, s, len);
}

/*
** This routine does a blind memory compare on two dynamic strings.
*/
int
ds_cmp(dstring_t *a, dstring_t *b)
{
	int result;

	if (! a || ! a->used) {
		return -1;
	}
	if (! b || ! b->used) {
		return 1;
	}
	result = memcmp(a->data, b->data,
	    (a->used < b->used) ? a->used : b->used);
	if (result == 0 && a->used != b->used) {
		if (a->used > b->used) {
			return 1;
		}
		return -1;
	}
	
	return result;
}

/*
** This routine compares a dynamic string to a passed string, if the len is
** non-zero then only go that far.
*/
int
ds_scmp(dstring_t *ds, char *s, int len)
{
	if (! ds || ! ds->used) {
		return -1;
	}
	if (! s) {
		return 1;
	}
	if (len) {
		return strcmp(ds->data, s);
	}
	return strncmp(ds->data, s, len);
}

/*
** Just like the above, but case insensitive.
*/
int
ds_casecmp(dstring_t *ds, char *s, int len)
{
	if (! ds || ! ds->used) {
		return -1;
	}
	if (! s) {
		return 1;
	}
	if (len) {
		return strcasecmp(ds->data, s);
	}
	return strncasecmp(ds->data, s, len);
}

/*
** This function will copy b into a, allocating space if needed.
*/
int
ds_cpy(dstring_t *a, dstring_t *b)
{
	if (! a || ! b || ! b->used) {
		return 0;
	}
	return ds_append(a, b->data, b->used);
}
