#ifndef key_h
#define key_h

/*
	This file contains macros and data to be used in generation of a unique
	key by set_key().
*/

#define KEY_SIZE	50
#define	TABLE_SIZE	66
#define DB_ID_NTRACKS	5
static char table[TABLE_SIZE] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
  'U', 'V', 'W', 'X', 'Y', 'Z', '@', '_', '=', '+',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 
  'u', 'v', 'w', 'x', 'y', 'z',
};

/*
	map produces side effects on b and len.
	val is an int whose information contain must be rolled into b.
	b is a char*, and map appends to b information contained in val.
*/

#define	map(b, len, val)\
	if (val >= TABLE_SIZE) {\
		if ((len + 2) < KEY_SIZE) {\
			sprintf(b, "%02d", val);\
			b += 2;\
			len += 2;\
		}\
	}\
	else {\
		if (len + 1 < KEY_SIZE) {\
			*b++ = table[val];\
			len++;\
		}\
	}\

#endif
