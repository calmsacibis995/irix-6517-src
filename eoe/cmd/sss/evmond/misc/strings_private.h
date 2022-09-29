#ifndef __STRINGS_PRIVATE_H
#define __STRINGS_PRIVATE_H

#define STRINGS_DEBUG 0

typedef struct string_node_s {
  struct string_node_s *Pnext_hash;
  char *str;
  int   refcnt;
} string_node_t;

extern __uint64_t fnv_hash(uchar_t *key, int len);
static string_node_t **PPstring_hash;
static int num_entries_hash=32;

#endif
