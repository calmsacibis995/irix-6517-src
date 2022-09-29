#if (!defined(_COMPILER_VERSION) || (_COMPILER_VERSION<700)) /* !Mongoose -- XXX 710  */
int __compare_and_swap(long *, long, long);
#endif
