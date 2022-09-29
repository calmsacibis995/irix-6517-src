#ident "$Header: "

/** 
 ** Basic types 
 **/
typedef int                 int32;
typedef unsigned int        uint32;
typedef int                 bool;
#if (_MIPS_SZLONG == 64)
typedef long                int64;
typedef unsigned long       uint64;
#else /* 32-bit */
typedef long long           int64;
typedef unsigned long long  uint64;
#endif

/** 
 ** Special KLIB types
 **/

/* The following typedef should be used for variables or return values
 * that contain kernel virtual or physical addresses. This is true for
 * both 32-bit and 64-bit kernels (one size fits all). With 32-bit
 * values, the upper order 32 bits will always contain zeros.
 */
typedef uint64              kaddr_t;

/* The following typedef should be used for variables or return values
 * that contain unsigned integer values in a kernel structs or unions.
 * The functions that obtain the values determine their size (one byte, 
 * two bytes, four bytes, or eight bytes) and shift the value accordingly 
 * to fit in the 64-bit space. Using this approach allows KLIB-based 
 * applications to be much more flexible. They can use a single set of 
 * functions to handle both 32-bit and 64-bit dumps. And, they are less 
 * likely to be effected by minor changes to kernel structures (e.g. 
 * changing a short to an int). typedef unsigned long long k_uint_t;
 */
typedef unsigned long long	k_uint_t;  

/* The following typedef is similar to the previous one except it applys
 * to signed integer values.
 */

typedef long long 		 	k_int_t;  

/* The following typedef should be used for variables that will point
 * to a block of memory containing data read in from the vmcore image 
 * (or live system). Many KLIB functions have k_ptr_t parameters and 
 * return k_ptr_t values.
 */
typedef void 		       *k_ptr_t;  
