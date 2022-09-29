/* 
 * s3.h
 *
 */

#define REC_SIZE 32

extern int s3_init(size_t);
extern void s3_setloadaddr(__psunsigned_t);
extern void s3_convert(char*, size_t);
extern int s3_write(FILE*);
extern int s3_terminate(FILE*, unsigned int);
