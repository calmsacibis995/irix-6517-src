#ifndef _HASH_
#define _HASH_

#define SUCCESS 0
#define FAILURE 1

#include <sys/types.h>

typedef struct _entry {
	char *str;
	struct _entry *pNext;
} entry;

typedef struct bucket {
	uint h;						/* Used for numeric indexing */
	char *arKey;
	uint nKeyLength;
	uint count;
	entry *pData;
	struct bucket *pNext;
} Bucket;

typedef struct hashtable {
	uint nTableSize;
	uint initialized;
	uint(*pHashFunction) (char *arKey, uint nKeyLength);
	Bucket **arBuckets;
} HashTable;

extern int completion_hash_init(HashTable *ht, uint nSize);
extern int completion_hash_update(HashTable *ht, char *arKey, uint nKeyLength, char *str);
extern int hash_exists(HashTable *ht, char *arKey);
extern Bucket *find_all_matches(HashTable *ht, char *str, uint length, uint *res_length);
extern Bucket *find_longest_match(HashTable *ht, char *str, uint length, uint *res_length);
extern void add_word(HashTable *ht,char *str);
extern void completion_hash_clean(HashTable *ht);
extern int completion_hash_exists(HashTable *ht, char *arKey, uint nKeyLength);
extern void completion_hash_free(HashTable *ht);

#endif							/* _HASH_ */
