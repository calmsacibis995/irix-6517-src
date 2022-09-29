#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <klib/klib.h>

/*
 * init_string_table()
 */
string_table_t *
init_string_table(int flag)
{
	string_table_t *st;

	if (flag == B_PERM) {
		st = (string_table_t*)kl_alloc_block(sizeof(string_table_t), B_PERM);
		if (st) {
			st->block_list = kl_alloc_block(4096, B_PERM);
		}
	}
	else {
		st = (string_table_t*)kl_alloc_block(sizeof(string_table_t), B_TEMP);
		if (st) {
			st->block_list = kl_alloc_block(4096, B_TEMP);
		}
	}
	return(st);
}

/*
 * free_string_table()
 */
void
free_string_table(string_table_t *st)
{
	k_ptr_t block, next_block;

	block = st->block_list;

	while (block) {
		next_block = *(k_ptr_t*)block;
		kl_free_block(block);
		block = next_block;
	}
	kl_free_block((k_ptr_t)st);
}

/*
 * in_block()
 */
int
in_block(char *s, k_ptr_t block)
{
	if ((s >= (char*)block) && (s < ((char*)block + 4096))) {
		return(1);
	}
	return(0);
}

/*
 * get_string()
 */
char *
get_string(string_table_t *st, char *str, int flag)
{
	int len;
	k_ptr_t block, last_block;
	char *s;

	/* If, for some reason, st is NULL, then just allocate a block of
	 * type flag (this "feature" allows for string tables to be used
	 * or not used with the same code).
	 */
	if (!st) {
		s = (char *)kl_str_to_block(str, flag);
		return(s);
	}

	block = st->block_list;
	s = (char *)((uint)block + 4);

	/* Walk the strings in the table looking for str. If we find it
	 * return a pointer to the string.
	 */
	while (*s) {
		if (!strcmp(str, s)) {
			return(s);
		}
		s += strlen(s) +1;
		if (!in_block((s + strlen(str)), block)) {
			last_block = block;
			block = *(k_ptr_t*)last_block;
			if (!block) {
				break;
			}
			s = (char *)((uint)block + 4);
		}
	} 

	/* If we didn't find the string, we have to add it to the
	 * table and then return a pointer to it. If we are still
	 * in the middle of a block, make sure there is enough room
	 * for the new string. If there isn't, allocate a new block
	 * and put the string there (after linking in the new block).
	 */
	if (block) {
		if (in_block((s + strlen(str)), block)) {
			strcpy(s, str);
			st->num_strings++;
			return(s);
		}
		else {
			last_block = block;
			block = *(k_ptr_t*)block;
		}
	}

	/* If we get here, it means that there wasn't enough string in 
	 * an existing block for the new string. So, allocate a new block
	 * and put the string there.
	 */
	if (flag & B_PERM) {
		block = kl_alloc_block(4096, B_PERM);
	}
	else {
		block = kl_alloc_block(4096, B_TEMP);
	}
	*(k_ptr_t*)last_block = block;

	s = (char *)((uint)block + 4);
	strcpy(s, str);
	st->num_strings++;
	return(s);
}

