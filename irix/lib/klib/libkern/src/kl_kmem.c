#ident  "$Header: "

#include <stdio.h>
#include <sys/types.h>
#include <klib/klib.h>

#define IS_MAPPED_MEM(a) \
	((is_kseg2(a) && !(is_mapped_kern_ro(a) || is_mapped_kern_rw(a))) || \
		IS_KERNELSTACK(a))

/*
 * kl_read_block() -- Read in objects from memory/vmcore image
 *
 *   The block can contain a kernel data structure, an application
 *   data structure, raw memory, a stack, or whatever. An error code 
 *   will be returned in the event that an error occurred. Otherwise, 
 *   NULL will be returned.
 */
k_uint_t
kl_read_block(kaddr_t addr, unsigned size, k_ptr_t bp, k_ptr_t ktp, char *name)
{
	k_uint_t result;

	/* Before making the call to read in the block, determine what 
	 * type of address we have (physical, mapped_kern, or virtual).
	 */
	if (IS_PHYSICAL(addr)) {
		if (result = kl_readmem(addr, size, bp)) {
			if (DEBUG(KLDC_MEM, 1)) {
				fprintf(KL_ERRORFP, "kl_read_block: error (%lld) reading %s at "
						"0x%llx\n", result, name, addr);
			}
			goto read_mem_error;
		}
	}
	else if (ACTIVE && (is_mapped_kern_ro(addr) || is_mapped_kern_rw(addr))) {
		if (result = kl_readmem(addr, size, bp)) {
			if (DEBUG(KLDC_MEM, 1)) {
				fprintf(KL_ERRORFP, "kl_read_block: error (%lld) reading %s at "
					"0x%llx\n", result, name, addr);
			}
			goto read_mem_error;
		}
	}
	else {
		/* We have a virtual address. We need to convert the virtual 
		 * address into a physical address. We also need to check and 
		 * see if the virtual address we have is mapped. If it's a mapped
		 * address and the block of memroy we want to read spans a page
		 * boundry, we have to read the data into our buffer one page (or 
		 * partial page) at a time. That is because there is no guarantee 
		 * that the pages will be contiguous in physical memory.
		 */
		int s, ts, offset;
		kaddr_t a, p, paddr;
		k_ptr_t b;

		paddr = kl_virtop(addr, ktp);
		if (result = KL_ERROR) {
			goto read_mem_error;
		}

		offset = (K_PAGESZ - (addr % K_PAGESZ));
		if (IS_MAPPED_MEM(addr) && (size > offset)) {

			/* We need to read in from more than one mapped page of memory.
			 * We have to read in each page of memory one-at-a-time.
			 */
			
			/* Read in the memory from the first page
			 */
			if (result = kl_readmem(paddr, offset, bp)) {
				if (DEBUG(KLDC_MEM, 1)) {
					fprintf(KL_ERRORFP, "kl_read_block: error (%lld) reading "
						"%s at 0x%llx\n", result, name, addr);
				}
				goto read_mem_error;
			}

			s = size - offset;
			a = addr + offset;
			b = (k_ptr_t)((unsigned)bp + offset);
			p = kl_virtop(a, ktp);
			if (result = KL_ERROR) {
				goto read_mem_error;
			}
			while (s) {

				/* Read the next chunk of memory 
				 */
				if (s > K_PAGESZ) {
					ts = K_PAGESZ;
				}
				else {
					ts = s;
				}
				result = kl_readmem(p, ts, b);
				if (result) {
					if (DEBUG(KLDC_MEM, 1)) {
						fprintf(KL_ERRORFP, "kl_read_block: error (%lld) "
							"reading %s at 0x%llx\n", result, name, a);
					}
					goto read_mem_error;
				}
				if (s > K_PAGESZ) {
					s -= K_PAGESZ;
					b = (k_ptr_t)((unsigned)b + K_PAGESZ);
					a += K_PAGESZ;
					p = kl_virtop(a, ktp);
					if (result = KL_ERROR) {
						goto read_mem_error;
					}
				}
				else {
					s = 0;
				}
			}

		}
		else {
			/* We can read the block of memory in one operation. The
			 * page(es) we want are all contiguous.
			 */
			if (result = kl_readmem(paddr, size, bp)) {
				if (DEBUG(KLDC_MEM, 1)) {
					fprintf(KL_ERRORFP, "kl_read_block: error (%lld) reading "
						"%s at 0x%llx\n", result, name, addr);
				}
				goto read_mem_error;
			}
		}
	}

read_mem_error:

	return(result);
}

/*
 * kl_get_block()
 *
 *  This is a wrapper for the kl_read_block() function. It does not take a
 *  kthread parameter (if the memory is mapped to a particular kthread, e.g.,
 *  the kernel stack, then defkthread must be set to the kthread upon entry). 
 */
k_uint_t
kl_get_block(kaddr_t addr, unsigned size, k_ptr_t bp, char *name) 
{
	k_uint_t result = 0;

	if (result = kl_read_block(addr, size, bp, 0, name)) {
		KL_SET_ERROR_NVAL(result, addr, 2);	
	}
	return(result);
}

/*
 * kl_get_kaddr() -- Get the kernel address pointed to by 'addr'
 *
 *   If this is a 64-bit kernel, get an eight byte address. Otherwise, 
 *   get a four byte address and right shift them to the low-order word. 
 *   Note that it is not necessary to call kl_reset_error() since that 
 *   will be done in the kl_get_block() function (as will the setting of 
 *   the global error information).
 */
k_uint_t
kl_get_kaddr(kaddr_t addr, k_ptr_t buffp, char *label)
{
	k_uint_t result = 0;

	if (DEBUG(KLDC_FUNCTRACE, 4)) {
		fprintf(KL_ERRORFP, "kl_get_kaddr: addr=0x%llx, buffp=0x%x, "
			"label=\"%s\\n", addr, buffp, label);
	}

	if (!buffp) {
		KL_SET_ERROR(KLE_NULL_BUFF);
		return(KLE_NULL_BUFF);
	}
	if (PTRSZ64) {
		result = kl_get_block(addr, sizeof(kaddr_t), buffp, label);
	}
	else if (!(result = kl_get_block(addr, 4, buffp, label))) {
		*(kaddr_t*)buffp = (*(kaddr_t*)buffp >> 32);
	}
	return(result);
}

/*
 * kl_kaddr_to_ptr() -- Return the pointer stored at kernel address 'k'
 *
 */
kaddr_t
kl_kaddr_to_ptr(kaddr_t k)
{
	kaddr_t k1;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_kaddr_to_ptr: k=0x%llx\n", k);
	}

	kl_get_kaddr(k, &k1, "kaddr");
	if (KL_ERROR) {
		return((kaddr_t)0);
	}
	else {
		return(k1);
	}
}

/*
 * kl_uint() -- Return an unsigned intiger value stored in a structure
 *
 *    Pointer 'p' points to a buffer that contains a kernel structure 
 *    of type 's.' If the size of member 'm' is less than eight bytes
 *    then right shift the value the appropriate number of bytes so that 
 *    it lines up properly in an k_uint_t space. Return the resulting 
 *    value. 
 */
k_uint_t
kl_uint(k_ptr_t p, char *s, char *m, unsigned offset)
{
	int nbytes, shft;
	k_uint_t v;
	k_ptr_t source;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "k_uint: p=0x%x, s=\"%s\", m=\"%s\", offset=%u\n", 
			p, s, m, offset);
	}

	if (!(nbytes = kl_member_size(s, m))) {
		KL_SET_ERROR_CVAL(KLE_BAD_FIELD, m);
		return((k_uint_t)0);
	}
	source = (k_ptr_t)(ADDR(p, s, m) + offset);
	bcopy(source, &v, nbytes);
	if (shft = (8 - nbytes)) {
		v = v >> (shft * 8);
	}
	return(v);
}

/*
 * kl_int() -- Return a signed intiger value stored in a structure
 *
 *    Pointer 'p' points to a buffer that contains a kernel structure 
 *    of type 's.' If the size of member 'm' is less than eight bytes
 *    then right shift the value the appropriate number of bytes so that 
 *    it lines up properly in an k_uint_t space. Return the resulting 
 *    value. 
 */
k_int_t
kl_int(k_ptr_t p, char *s, char *m, unsigned offset)
{
	int nbytes, shft;
	k_int_t v;
	k_ptr_t source;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_int: p=0x%x, s=\"%s\", m=\"%s\", "
			"offset=%u\n", p, s, m, offset);
	}

	if (!(nbytes = kl_member_size(s, m))) {
		KL_SET_ERROR_CVAL(KLE_BAD_FIELD, m);
		return((k_uint_t)0);
	}
	source = (k_ptr_t)(ADDR(p, s, m) + offset);
	bcopy(source, &v, nbytes);
	if (shft = (8 - nbytes)) {
		v = v >> (shft * 8);
	}
	return(v);
}

/*
 * kl_kaddr() -- Return a kernel virtual address stored in a structure
 *
 *   Pointer 'p' points to a buffer that contains a kernel structure 
 *   of type 's.' Get the kernel address located in member 'm.' If the 
 *   system is running a 32-bit kernel, then right shift the address 
 *   four bytes. If the system is running a 64-bit kernel, then the 
 *   address is OK just the way it is.
 */
kaddr_t
kl_kaddr(k_ptr_t p, char *s, char *m)
{
	kaddr_t k;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kaddr: p=0x%x, s=\"%s\", m=\"%s\"\n", p, s, m);
	}

	bcopy(K_PTR(p, s, m), &k, 8);

	if (PTRSZ64) {
		return(k);
	}
	else {
		return(k >> 32);
	}
}

/*
 * kl_reg() -- Return a register value
 *
 *   Pointer 'p' points to a buffer that contains a kernel structure 
 *   of type 's.' Get the register value located in member 'm.' Unlike
 *   with the kl_kaddr() function, the full 64-bit value is returned
 *   even if the system is running a 32-bit kernel (all CPU registers
 *   are 64-bit).
 */
kaddr_t
kl_reg(k_ptr_t p, char *s, char *m)
{
	kaddr_t k;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_reg: p=0x%x, s=\"%s\", m=\"%s\"\n", p, s, m);
	}

	bcopy(K_PTR(p, s, m), &k, 8);
	return(k);
}

/*
 * kl_kaddr_val() -- Return the kernel virtual address located at pointer 'p'
 *
 *   If the system is running a 32-bit kernel, then right shift the 
 *   kernel address four bytes. If the system is running a 64-bit 
 *   kernel, then the address is OK just the way it is.
 */
kaddr_t
kl_kaddr_val(k_ptr_t p)
{
	kaddr_t k;

	if (DEBUG(KLDC_FUNCTRACE, 6)) {
		fprintf(KL_ERRORFP, "kl_kaddr_val: p=0x%x\n", p);
	}
	bcopy(p, &k, 8);

	if (PTRSZ64) {
		return(k);
	}
	else {
		return(k >> 32);
	}
}
