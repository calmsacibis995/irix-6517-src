#include "stdio.h"

char *getnum(int *val, char *parmptr);

#define UNKNOWN 0
#define STARTFONT 1
#define COMMENT 2
#define FONT 3
#define SIZE 4
#define FONTBOUNDINGBOX 5
#define STARTPROPERTIES 6
#define _DEC_DEVICE_FONTNAMES 7
#define FONTNAME_REGISTRY 8
#define WEIGHT_NAME 9
#define SLANT 10
#define SETWIDTH_NAME 11
#define ADD_STYLE_NAME 12
#define PIXEL_SIZE 13
#define POINT_SIZE 14
#define RESOLUTION_X 15
#define RESOLUTION_Y 16
#define SPACING 17
#define AVERAGE_WIDTH 18
#define CHARSET_REGISTRY 19
#define CHARSET_ENCODING 20
#define CHARSET_COLLECTIONS 21
#define FULL_NAME 22
#define COPYRIGHT 23
#define FONT_ASCENT 24
#define FONT_DESCENT 25
#define CAP_HEIGHT 26
#define X_HEIGHT 27
#define ENDPROPERTIES 28
#define CHARS 29
#define STARTCHAR 30
#define ENCODING 31
#define SWIDTH 32
#define DWIDTH 33
#define BBX 34
#define BITMAP 35
#define ENDCHAR 36
#define ENDFONT 37
#define ENDOFFILE 99

char *linetypetab[]={
	"UNKNOWN",
	"STARTFONT",
	"COMMENT",
	"FONT",
	"SIZE",
	"FONTBOUNDINGBOX",
	"STARTPROPERTIES",
	"_DEC_DEVICE_FONTNAMES",
	"FONTNAME_REGISTRY",
	"WEIGHT_NAME",
	"SLANT",
	"SETWIDTH_NAME",
	"ADD_STYLE_NAME",
	"PIXEL_SIZE",
	"POINT_SIZE",
	"RESOLUTION_X",
	"RESOLUTION_Y",
	"SPACING",
	"AVERAGE_WIDTH",
	"CHARSET_REGISTRY",
	"CHARSET_ENCODING",
	"CHARSET_COLLECTIONS",
	"FULL_NAME",
	"COPYRIGHT",
	"FONT_ASCENT",
	"FONT_DESCENT",
	"CAP_HEIGHT",
	"X_HEIGHT",
	"ENDPROPERTIES",
	"CHARS",
	"STARTCHAR",
	"ENCODING",
	"SWIDTH",
	"DWIDTH",
	"BBX",
	"BITMAP",
	"ENDCHAR",
	"ENDFONT",
	0
	};

typedef struct charstruct_s {
	int encoding;
	int swidth1;
	int swidth2;
	int dwidth1;
	int dwidth2;
	int bbx1;
	int bbx2;
	int bbx3;
	int bbx4;
	int offset;
	unsigned short *bitmap;
} charstruct_t;

#ifdef non_iso8660
#define printable(X)	(((X) >= 32) && ((X) <= 127))
#else
#define printable(X)	( (((X) >= 32) && ((X) <= 127)) ||		\
			  (((X) >= (0x80|32) && ((X) <= (0x80|127)))) )
#endif

char line[1024];
int linestatus;

void
identifyline(int *linetype, char **firstparm)
{
	int i, tokenlen;
	if (linestatus == 1){
		*linetype = ENDOFFILE;
		*firstparm = 0;
		return;
	}
	for (i = 0; ; i++){
		if (linetypetab[i] == 0)
			return;
		tokenlen = strlen(linetypetab[i]);
		if (strncmp(line, linetypetab[i], tokenlen) == 0)
			break;
	}
	*linetype = i;
	*firstparm = line+tokenlen;
}

main(int argc, char **argv)
{
	int linetype, charstructlistp, i, j, nshorts;
	int offset, maxht = 0, maxds = 0;
	unsigned long v, len;
	charstruct_t *charstruct, *charstructlist[2048];
	char *p, *parmptr, *FontName;
	
	if (argc == 2)
		FontName = argv[1];
	else
		FontName = "Font";
	charstructlistp = 0;
	while (1) {		
		p = gets(line);
		if (p == 0)
			linestatus = 1;
		else
			linestatus = 0;
		
		identifyline(&linetype, &parmptr);
		switch(linetype) {
		case ENDOFFILE:
			goto output;
			break;
		case UNKNOWN:
			fprintf(stderr, "Unknown token\n");
			exit(1);
			break;
		case STARTFONT:
		case COMMENT:
		case FONT:
		case SIZE:
		case FONTBOUNDINGBOX:
		case STARTPROPERTIES:
		case _DEC_DEVICE_FONTNAMES:
		case FONTNAME_REGISTRY:
		case WEIGHT_NAME:
		case SLANT:
		case SETWIDTH_NAME:
		case ADD_STYLE_NAME:
		case PIXEL_SIZE:
		case POINT_SIZE:
		case RESOLUTION_X:
		case RESOLUTION_Y:
		case SPACING:
		case AVERAGE_WIDTH:
		case CHARSET_REGISTRY:
		case CHARSET_ENCODING:
		case CHARSET_COLLECTIONS:
		case FULL_NAME:
		case COPYRIGHT:
		case FONT_ASCENT:
		case FONT_DESCENT:
		case CAP_HEIGHT:
		case X_HEIGHT:
		case ENDPROPERTIES:
		case CHARS:
			break;
		case STARTCHAR:
			/* Start a new character by mallocing a struct for
			 * it and putting the struct on the charstructlist
			 */
			charstruct = (charstruct_t *)
				malloc(sizeof(charstruct_t));
			charstructlist[charstructlistp] = charstruct;
			charstructlistp++;
			break;
		case ENCODING:
			parmptr = getnum(&i, parmptr);
			charstruct->encoding = i;
			break;
		case SWIDTH:
			parmptr = getnum(&i, parmptr);
			charstruct->swidth1 = i;
			parmptr = getnum(&i, parmptr);
			charstruct->swidth2 = i;
			break;
		case DWIDTH:
			parmptr = getnum(&i, parmptr);
			charstruct->dwidth1 = i;
			parmptr = getnum(&i, parmptr);
			charstruct->dwidth2 = i;
			break;
		case BBX:
			parmptr = getnum(&i, parmptr);
			charstruct->bbx1 = i;
			parmptr = getnum(&i, parmptr);
			charstruct->bbx2 = i;
			parmptr = getnum(&i, parmptr);
			charstruct->bbx3 = i;
			parmptr = getnum(&i, parmptr);
			charstruct->bbx4 = i;
			charstruct->offset = 0;
			break;
		case BITMAP:
			nshorts = charstruct->bbx2*((charstruct->bbx1+15)/16);
			charstruct->bitmap = (unsigned short*)
				calloc(nshorts, sizeof(short));
			for (i = nshorts-1; i >= 0; i--) {
				gets(line);
				len = strlen(line);
				v = strtoul(line, NULL, 16);
				switch (len) {
				case 2:
					charstruct->bitmap[i] = v << 8;
					break;
				case 4:
					charstruct->bitmap[i] = v;
					break;
				case 6:
					charstruct->bitmap[i] = (v<<8)&0xffff;
					i--;
					charstruct->bitmap[i] = v >> 8;
					break;
				case 8:
					charstruct->bitmap[i] = v & 0xffff;
					i--;
					charstruct->bitmap[i] = v >> 16;
					break;
				}
			}
			break;
		case ENDCHAR:
			break;
		case ENDFONT:
			break;
		default:
			fprintf(stderr, "Unknown token\n");
			exit(1);
			break;
		}
	}
 output:
	printf("unsigned short %sData[] = {", FontName);
	for (i = 0, offset = 0; i < charstructlistp; i++) {
		charstruct = charstructlist[i];
		if (!printable(charstruct->encoding))
			continue;
		charstruct->offset = offset;
		nshorts = charstruct->bbx2*((charstruct->bbx1+15)/16);
		offset += nshorts;
		printf("\n\t\t\t\t\t\t\t\t/* char 0x%2.2x */\n",
		       charstruct->encoding);
		printf("\t");
		for (j = 0; j < nshorts; j++) {
			printf("0x%4.4x, ", charstruct->bitmap[j]);
			if (j % 7 == 6)
				printf("\n\t");
		}
	}
	printf("};\n\n");
	printf("struct fontinfo %sInfo[] = {\n", FontName);
	printf("#ifndef _STANDALONE\n{0,0,0,0,0,0},\n#endif\n");
	for (i = 0; i < charstructlistp; i++){
		charstruct = charstructlist[i];
		if (!printable(charstruct->encoding))
			continue; 
		if (charstruct->bbx2 > maxht)
			maxht = charstruct->bbx2;
		if (charstruct->bbx4 < maxds)
			maxds = charstruct->bbx4;
		printf("{%d, %d, %d, %d, %d, %d},    \t\t\t\t/* char 0x%2.2x */\n", 
		       charstruct->offset, 
		       charstruct->bbx1, 
		       charstruct->bbx2, 
		       charstruct->bbx3, 
		       charstruct->bbx4, 
		       charstruct->dwidth1,
		       charstruct->encoding);
	}
	printf("};\n\n");

	printf("#define %s_ht\t%d\n",FontName,maxht);
	printf("#define %s_ds\t%d\n",FontName,-maxds);
	printf("#define %s_nc\t(sizeof(%sInfo)/sizeof(struct fontinfo))\n",
	       FontName,FontName);
	printf("#define %s_nr\t(sizeof(%sData)/sizeof(short))\n",
	       FontName,FontName);
	printf("#define %s_iso\t%d\n",FontName,(charstructlistp > 128));
}

char*
getnum(int *val, char *parmptr)
{
	char *p;
	*val = strtol(parmptr, &p, 10);
	return p;
}
