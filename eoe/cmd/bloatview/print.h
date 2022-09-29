/*
 * print.h
 *
 * Header file for bloat printing
 */

extern void PrintAllBloat(FILE *fp, PROGRAM *bloat);
extern void PrintMapBloat(FILE *fp, PROGRAM *bloat);
extern void PrintProcBloat(FILE *fp, PROGRAM *bloat, char *procName,
			   pid_t pid);
