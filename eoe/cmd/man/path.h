#ifndef __PATH_H__
#define __PATH_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
** Pathname convenience package
*/

extern char *	pathbasename(const char* path);
extern char *	pathdirname(const char* path);
extern char *	pathexpandtilde(const char* path);


#ifdef __cplusplus
}
#endif
#endif /* __PATH_H__ */
