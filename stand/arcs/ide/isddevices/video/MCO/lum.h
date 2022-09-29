#ifndef LUMDEF
#define LUMDEF

#define RLUM	(0.3086)
#define GLUM	(0.6094)
#define BLUM	(0.0820)

#define RINTLUM	(79)
#define GINTLUM	(156)
#define BINTLUM	(21)

#ifdef OLDWAY
#define RLUM	(0.299)
#define GLUM	(0.587)
#define BLUM	(0.114)

#define RINTLUM	(77)
#define GINTLUM	(150)
#define BINTLUM	(29)
#endif

#define ILUM(r,g,b)	((RINTLUM*(r)+GINTLUM*(g)+BINTLUM*(b))>>8)
#define LUM(r,g,b)	(RLUM*(r)+GLUM*(g)+BLUM*(b))

#endif
