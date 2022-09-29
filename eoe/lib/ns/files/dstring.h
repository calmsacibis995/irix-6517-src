#pragma once

typedef struct {
	char		*data;
	int		size;
	int		used;
} dstring_t;

#pragma mips_frequency_hint INIT
int ds_init(dstring_t *, int, void *);
void ds_clear(dstring_t *, void *);
void ds_free(dstring_t *, void *);
void ds_reset(dstring_t *);
int ds_grow(dstring_t *, int, void *);
int ds_append(dstring_t *, char *, int, void *);
int ds_prepend(dstring_t *, char *, int, void *);
int ds_set(dstring_t *, char *, int, void *);
int ds_cmp(dstring_t *, dstring_t *);
int ds_casecmp(dstring_t *, dstring_t *);
int ds_scmp(dstring_t *, char *, int);
int ds_scasecmp(dstring_t *, char *, int);
int ds_cpy(dstring_t *, dstring_t *, void *);
dstring_t *ds_dup(dstring_t *, void *);
dstring_t *ds_new(char *, int, void *);
