#ident "$Header: /proj/irix6.5.7m/isms/irix/cmd/icrash_old/include/RCS/mload.h,v 1.1 1999/05/25 19:19:20 tjm Exp $"

typedef struct i_ml_info {
	struct i_ml_info *next;      /* linked list forward pointer */
	ml_info_t 		 *m;	     /* kernel address of ml_info struct */
	ml_info_t 		  mp;        /* ml_info struct */
	cfg_desc_t  	  desc;      /* cfg_desc_t struct */
	SYMR        	 *symtab;	 /* pointer to copy of symbol table */
	char        	 *stringtab; /* pointer to copy of string table */
} i_ml_info_t;

