/*
 *	Display image defines . . 
 *
 *  			Paul Haeberli - 1988
 *
 */
#ifndef DISPIMGDEF
#define DISPIMGDEF

typedef struct dispimage {
    struct dispimage *next;
    int 		type;
    int 		xsize;
    int 		ysize;
    unsigned char 	*data;
} DISPIMAGE;

#define DI_WRITEPIXELS 		0
#define DI_WRITERGB 		1
#define DI_RECTWRITE 		2
#define DI_LRECTWRITE		3

DISPIMAGE *makedisprgn();
DISPIMAGE *makedisp();
DISPIMAGE *readdispimage();

#endif
