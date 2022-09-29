#ifndef __DSTRING_H__
#define __DSTRING_H__

typedef struct {
	char		*data;
	int		size;
	int		used;
} dstring_t;

#pragma mips_frequency_hint INIT
int ds_init(dstring_t *, int);
void ds_clear(dstring_t *);
void ds_reset(dstring_t *);
int ds_grow(dstring_t *, int);
int ds_append(dstring_t *, char *, int);
int ds_prepend(dstring_t *, char *, int);
int ds_set(dstring_t *, char *, int);
int ds_cmp(dstring_t *, dstring_t *);
int ds_scmp(dstring_t *, char *, int);
int ds_casecmp(dstring_t *, char *, int);
int ds_cpy(dstring_t *a, dstring_t *b);

#define DS_STRING(ds)	(ds)->data
#define DS_LEN(ds)	(ds)->used
#define	DS_SIZE(ds)	(ds)->size

#endif /* not __DSTRING_H__ */
