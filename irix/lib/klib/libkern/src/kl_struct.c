#ident "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

/*
 * kl_get_struct() -- Get any type of struct and load into bufp
 */
k_uint_t
kl_get_struct(kaddr_t addr, int size, k_ptr_t bufp, char *s)
{
	k_uint_t result = 0;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "kl_get_struct: addr=0x%llx, size=%d, "
			"bufp=0x%x, s=\"%s\"\n", addr, size, bufp, s);
	}

	kl_reset_error();

	if (!bufp) {
		KL_SET_ERROR(KLE_NULL_BUFF);
		return(KLE_NULL_BUFF);
	}

	/* Make sure that the kernel virtual address is valid (physical 
	 * addresses are not allowed here). Also make sure the (potentially
	 * valid) virtual address maps to a valid physical address on the 
	 * system (an error will be set if it's not).
	 */
	kl_is_valid_kaddr(addr, (k_ptr_t)NULL, WORD_ALIGN_FLAG);
	if (KL_ERROR) {
		return(KL_ERROR);
	}

	/* In most cases, the size of the struct will be passed in.
	 * It's much faster than calling kl_struct_len() all the time. However,
	 * in case the size isn't provided, get it here (this should really 
	 * never fail unless the struct name is bogus).
	 */
	if ((size == 0) && !(size = kl_struct_len(s))) {
		KL_SET_ERROR_CVAL(KLE_INVALID_STRUCT_SIZE, s);
		return(KL_ERROR);
	}
	result = kl_get_block(addr, size, bufp, s);
	return(result);
}

/* 
 * kl_get_ml_info() -- Get a particular ml_info struct (depending on mode). 
 *
 *  Load the contents of structure into buffer pointed to by mp
 *  and return kernel address of the ml_info struct. The value of
 *  mode determines how value is treated. If mode is equal to
 *  two, then value is assumed to be a virtual address of ml_info
 *  struct. If mode is equal to three, then it is assumed to be a
 *  virtual address to match in the symbol table. And, if the
 *  mode is equal to four, then value is a pointer to a string to
 *  be matched in the string table.
 */
k_ptr_t
kl_get_ml_info(kaddr_t value, int mode, k_ptr_t mp)
{
	k_int_t id;
	kaddr_t ml, cfg;
	k_ptr_t mlp, cfgp;

	/* There has to be a valid buffer coming in...
	 */
	if (!mp) {
		KL_SET_ERROR(KLE_NULL_BUFF);
		return((k_ptr_t)NULL);
	}

	if (mode == 2) {  
		/* value is virtual address of ml_info struct 
		 */
		kl_get_struct(value, ML_INFO_SIZE, mp, "ml_info");
        if (KL_ERROR) {
            return((k_ptr_t)NULL);
        }
		return(mp);
	}

	ml = K_MLINFOLIST;
	while (ml) {
		kl_get_struct(ml, ML_INFO_SIZE, mp, "ml_info");
		if (KL_ERROR) {
			return((k_ptr_t)NULL);
		}
		else if (mode == 3) {
			/* value is an addr to be matched in symbol table
			 */
			if (KL_INT(mp, "ml_info", "ml_nsyms") &&
					(value >= kl_kaddr(mp, "ml_info", "ml_text")) &&
					(value < kl_kaddr(mp, "ml_info", "ml_end"))) {
				return(mp);
			}
		}
#ifdef NOTYET
		else if (mode == 4) {
			/* value is a char pointer containing the name of a symbol
			 * to be matched in symbol table
			 */
			if (ml->mp.ml_nsyms && (find_symr((char*)value, ml))) {
				return(ml);
			}
		}
#endif
		ml = kl_kaddr(mp, "ml_info", "ml_next");
	}
	return(mp);
}
