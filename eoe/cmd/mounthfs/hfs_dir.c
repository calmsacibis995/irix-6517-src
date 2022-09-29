/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "$Revision: 1.10 $"



#include <errno.h>
#include <rpc/rpc.h>
#include <string.h>
#include "hfs.h"


/* Forward routines */

static void to_unix_name(char *mname, char *uname, int hidden);
static int to_hfs_name(char *uname, char *mname, int *hidden);


/*
#define MIN(a,b) ((a)<(b)?(a):(b))
 */

char *dot=".";
char *dotdot="..";
char *dotresource=DOTRESOURCE;
char *dotancillary=DOTANCILLARY;


/************
 * dir_lookup	Lookup a file in a directory given an IRIX name.
 */
int dir_lookup(hnode_t *dhp, char *name, frec_t **frp){
  char mname[33];
  int sts;
  if (sts = to_hfs_name(name,mname,NULL))
    return sts;
  return dir_lookup_m(dhp,mname,frp);
}


/**************
 * dir_lookup_m Lookup a file in a directory given a mac name.
 */
int dir_lookup_m(hnode_t *dhp, char *mname, frec_t **frp){
  int     error;
  catk_t  key;
  rdesc_t rd;
  frec_t  *fp;

  fr_cons_key(FID(dhp->h_hno),&key,mname+1);
  if (error=bt_rget(&dhp->h_hfs->fs_cbt,(btk_t*)&key,&rd))
    return error;
  if (error=fr_construct(dhp->h_hfs,&key,&rd,&fp))
    return error;
  *frp = fp;
  return 0;
}


/*************
 * dir_iterate	Iterate over all directory and file records in a directory.
 *============
 *
 * This routine first fetches the directory thread record and then fetches
 * sucessive records until there are no more or a record is found with
 * a different parent.  The active scan begins at the n'th dir/file record
 * and n is incremented for each dir/file record processed.
 */
int dir_iterate(hnode_t *dhp, u_int *n, dir_callback_t cbf, void *arg){
  int     error,count=0;
  catk_t  key;
  rdesc_t rd;
  frec_t  *fp;
  bt_t    *bt=&_h_cbt(dhp);
  int     ctr=5000;

  fr_cons_key(FID(dhp->h_hno),&key,"");
  if (error=bt_rget(bt,(btk_t*)&key,&rd))
    return error;

  while ((error=bt_rnext(bt,(btk_t*)&key,&rd))==0){

    /* Indicate fs corruption if the iteration takes too long. */
    if (ctr-- <=0)
      return __chk(dhp->h_hfs,EIO,dir_iterate);

    if (CAT_PARENT(&key)!=FID(dhp->h_hno))
      return 0;
    if (IS_FILE_OR_DIR(&rd)){
      if (count++ < *n)
	continue;
      if (error=fr_construct(dhp->h_hfs,&key,&rd,&fp))
	break;
      error = cbf(dhp,fp,arg);
      fr_put(fp);
      if (error)
	break;
      *n += 1;
    }
  }
  if (error==ENOENT)
    return 0;
  return error;
}


/*************
 * dir_init_fr	Initialize an frec.
 */
int dir_init_fr(frec_t *fp, char *name, u_int hfstime, u_char type){
  int   sts;
  int   hidden=0;
  char  mname[33];
  cdr_t *cdr = &fp->f_cdr;

  /*
   * Clear the entire cdr.
   * Convert the unix name to an hfs name.
   * Construct the key for this frec.
   */
  bzero(cdr,sizeof *cdr);
  if (sts = to_hfs_name(name,mname,&hidden))
    return sts;
  fr_cons_key(fp->f_pfid,&fp->f_key,mname+1);

  /*
   * Initialize the cdr entry.
   */
  if (type==CDR_DIRREC){
    cdr->c_Type        = type;
    cdr->c.c_Dir.Flags = 0;
    cdr->c.c_Dir.Val   = 0;
    cdr->c.c_Dir.DirID = fp->f_fid;
    cdr->c.c_Dir.CrDat = hfstime;
    cdr->c.c_Dir.MdDat = hfstime;
    cdr->c.c_Dir.BkDat = 0;
    if (hidden)
      cdr->c.c_Dir.DInfo.isInvisible=1;
  }
  else{
    cdr->c_Type        = type;
    cdr->c.c_Fil.Flags = 0;
    cdr->c.c_Fil.FlNum = fp->f_fid;
    cdr->c.c_Fil.CrDat = hfstime;
    cdr->c.c_Fil.MdDat = hfstime;
    cdr->c.c_Fil.BkDat = 0;
    bcopy("TEXT",cdr->c.c_Fil.UsrWds.Type,4);
    bcopy("IRIX",cdr->c.c_Fil.UsrWds.Creator,4);
    if (hidden)
      cdr->c.c_Fil.UsrWds.isInvisible=1;
  }

  return 0;
}


/***************
 * dir_rename_fr	Changes the name of an frec.
 */
int dir_rename_fr(frec_t *fp, u_int pfid, char *uname){
  char mname[33];
  int  hidden=0;
  int  sts;
  if (sts = to_hfs_name(uname,mname,&hidden))
    return sts;
  fr_cons_key(pfid,&fp->f_key,mname+1);
  if (_f_cdr(fp).c_Type==CDR_DIRREC)
    _f_dir(fp).DInfo.isInvisible = hidden;
  else
    _f_file(fp).UsrWds.isInvisible = hidden;
  return 0;
}


/*****************
 * dir_fr_to_entry	Generates an nfs directory entry from a frec.
 */
int dir_fr_to_entry(frec_t *fp, dirs_t* dsp, char *defname, int tag){
  entry   *e = dsp->cur;
  char    name[100];
  int     entlen;
  char    *nm;

  /*
   * If the directory is a resource directory, ignore directory entries.
   */
  if ((TAG(dsp->hp->h_hno)==nt_dotresource) &&
      (_f_cdr(fp).c_Type==CDR_DIRREC)){
    dsp->offset++;
    return 0;
  }

  /*
   * If a default name is supplied, it overrides the name in the key.  This is
   * so we can print "." and ".." etc.
   */
  nm = defname;
  if (nm==NULL)
    to_unix_name(fp->f_key.k_key.k_CName,name,HIDDEN(fp));
  else
    strcpy(name,nm);

  /*
   * Figure out how big this entry is going to be and if it will fit in
   * the space remaining.
   */
  entlen = sizeof(entry)+ RNDUP(strlen(name)+4);
  if (entlen > dsp->free)
    return 1;

  /*
   * Fill in the entry.
   */
  e->fileid =HNO(fp->f_fid,tag);
  *(u_int*)e->cookie = ++dsp->offset;
  dsp->last = e;
  dsp->cur  = (entry*)((char*)e+entlen);
  dsp->free -= entlen;

  e->nextentry=dsp->cur;
  e->name = strcpy((char*)e + sizeof(entry), name);

  TRACE(("fr_to_entry: hno: %4d, name: \"%-16.16s\"  cookie: %2d\n",
	e->fileid, e->name, *(u_int*)e->cookie));

  return 0;
}


#define ROW(p0,p1,p2,p3,p4,p5,p6,p7,p8,p9,pa,pb,pc,pd,pe,pf)\
  0x##p0,0x##p1,0x##p2,0x##p3,0x##p4,0x##p5,0x##p6,0x##p7,\
  0x##p8,0x##p9,0x##pa,0x##pb,0x##pc,0x##pd,0x##pe,0x##pf
  


/*
 * Translation table map Macintosh 8bit characters to ISO Latin 1.
 *
 * NOTE:  Characters that have no isolatin1 representation are mapped to
 *        UNMAPABLE_MAC.  Note that '/' is unmappable.
 *
 * NOTE:  Characters above 0xd8 are not really defined for HFS. It's
 *        not well defined what should happen with these characters.
 */
#define UNMAPABLE_MAC 0xb7

static char mactoiso[256]={
  /*   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
  ROW(00,01,02,03,04,05,06,07,08,09,0d,0b,0c,0a,0e,0f),	/* 0 */
  ROW(10,11,12,13,14,15,16,17,18,19,1a,1b,1c,1d,1e,1f),	/* 1 */
  ROW(20,21,22,23,24,25,26,27,28,29,2a,2b,2c,2d,2e,b7),	/* 2 */
  ROW(30,31,32,33,34,35,36,37,38,39,3a,3b,3c,3d,3e,3f),	/* 3 */
  ROW(40,41,42,43,44,45,46,47,48,49,4a,4b,4c,4d,4e,4f),	/* 4 */
  ROW(50,51,52,53,54,55,56,57,58,59,5a,5b,5c,5d,5e,5f),	/* 5 */
  ROW(60,61,62,63,64,65,66,67,68,69,6a,6b,6c,6d,6e,6f),	/* 6 */
  ROW(70,71,72,73,74,75,76,77,78,79,7a,7b,7c,7d,7e,7f),	/* 7 */
  ROW(c4,c5,c7,c9,d1,d6,dc,e1,e0,e2,e4,e3,e5,e7,e9,e8),	/* 8 */
  ROW(ea,eb,ed,ec,ee,ef,f1,f3,f2,f4,f6,f5,fa,f9,fb,fc),	/* 9 */
  ROW(b7,b0,a2,a3,a7,b7,b6,df,ae,a9,b7,b4,a8,b7,c6,d8),	/* A */
  ROW(b7,b1,b7,b7,a5,b5,b7,b7,b7,b7,b7,aa,ba,b7,e6,f8),	/* B */
  ROW(bf,a1,ac,b7,b7,b7,b7,ab,bb,b7,a0,c0,c3,d5,b7,b7),	/* C */
  ROW(ad,b7,b7,b7,b7,b7,f7,b7,ff,b7,b7,a4,b7,b7,b7,b7),	/* D */
  ROW(b7,b7,b7,b7,b7,c2,ca,c1,cb,c8,cd,ce,cf,cc,d3,d4),	/* E */
  ROW(b7,d2,da,db,d9,b7,b7,b7,af,b7,b7,b7,b8,b7,b7,b7)  /* F */
};


/*
 * Translation table to map ISO Latin 1 characters to macintosh characters.
 *
 * NOTE:  Characters that have no Macintosh representation are mapped to
 *        UNMAPABLE_ISO.  Note that ':' is unmappable.
 &
 * NOTE:  Characters above 0xd8 are not really defined for HFS. It's
 *        not well defined what should happen with these characters.
 */
#define UNMAPABLE_ISO 0xf0

static char isotomac[256]={
  /*   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
  ROW(00,01,02,03,04,05,06,07,08,09,0d,0b,0c,0a,0e,0f),	/* 0 */
  ROW(10,11,12,13,14,15,16,17,18,19,1a,1b,1c,1d,1e,1f),	/* 1 */
  ROW(20,21,22,23,24,25,26,27,28,29,2a,2b,2c,2d,2e,2f),	/* 2 */
  ROW(30,31,32,33,34,35,36,37,38,39,f0,3b,3c,3d,3e,3f),	/* 3 */
  ROW(40,41,42,43,44,45,46,47,48,49,4a,4b,4c,4d,4e,4f),	/* 4 */
  ROW(50,51,52,53,54,55,56,57,58,59,5a,5b,5c,5d,5e,5f),	/* 5 */
  ROW(60,61,62,63,64,65,66,67,68,69,6a,6b,6c,6d,6e,6f),	/* 6 */
  ROW(70,71,72,73,74,75,76,77,78,79,7a,7b,7c,7d,7e,7f),	/* 7 */
  ROW(f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0),	/* 8 */
  ROW(f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0,f0),	/* 9 */
  ROW(ca,c1,a2,a3,db,b4,f0,a4,ac,a9,bb,c7,c2,d0,a8,f8),	/* A */
  ROW(a1,b1,f0,f0,ab,b5,a6,a5,fc,f0,bc,c8,f0,f0,f0,c0),	/* B */
  ROW(cb,e7,e5,cc,80,81,ae,82,e9,83,e6,e8,ed,ea,eb,ec),	/* C */
  ROW(f0,84,f1,ee,ef,cd,85,f0,af,f4,f2,f3,86,f0,f0,a7),	/* D */
  ROW(88,87,89,8b,8a,8c,be,8d,8f,8e,90,91,93,92,94,95),	/* E */
  ROW(f0,96,98,97,99,9b,9a,d6,bf,9d,9c,9e,9f,f0,f0,d8)  /* F */
};

static char HEXBITS(char c){
  if (c>='a')
    c -= 'a'-'A';
  return c>='A'? (c-'A')+10 : c & 0xF;
}
static char HEXDIGIT(char c){
  return c>=10 ? (c-10)+'A' : c+'0';
}


/********************
 * dir_validate_uname  Verifies that a unix file name can be converted to a
 *		      hfs file name.
 */
int dir_validate_uname(char *uname){
  char mname[33];
  return to_hfs_name(uname,mname,NULL);
}


/**************
 * to_unix_name	Convert HFS name to unix name.
 *=============
 *
 * NOTE: Converts unmappable characters to :XX.
 */
static void to_unix_name(char *mname, char *uname, int hidden){
  int  i;
  char c,cm;

  if (hidden)
    *uname++ = '.';

  for (i=0;i<mname[0];i++){
    c = mname[i+1];
    if (c=='\\' && isxdigit(mname[i+2]) && isxdigit(mname[i+3])){
      *uname    = HEXBITS(mname[++i+1])<<4;
      *uname++ |= HEXBITS(mname[++i+1]);
    }
    else
      if ((cm=mactoiso[c])==UNMAPABLE_MAC){
	*uname++ = ':';
	*uname++ = HEXDIGIT(c>>4);
	*uname++ = HEXDIGIT(c & 0xf);
      }
      else
	if (c=='.' && i==0){
	  *uname++ = '.';
	  *uname++ = '.';
	}
	else
	  *uname++ = cm;
  }
  *uname = 0;
}


/*************
 * to_hfs_name	Convert a unix name to an HFS name.
 *============
 *
 * NOTE: Converts unmappable characters to \XX.
 *       Returns error if length is greather than 31.
 */
static int to_hfs_name(char *uname, char *mname,int *hidden){
  char  c,cm;
  char *ms=mname+1;
  int  mnlength=0;

  /* Check for hidden file */

  if (*uname=='.'){
    uname++;
    if (hidden)
      *hidden=1;
  }

  /*
   * Scan the input string, converting :xx to mac characters, unmapable 
   * characters to \xx, and otherwise passing the characters through.
   * Limit the result to 31 characters.
   */
  while((c = *uname++)!=0)
    if (mnlength++>=31)
      return ENAMETOOLONG;
    else
      if (c==':' && isxdigit(uname[0]) && isxdigit(uname[1])){
	*ms    = HEXBITS(*uname++)<<4;
	*ms++ |= HEXBITS(*uname++);
      }
      else{
	cm = isotomac[c];
	if (cm==UNMAPABLE_ISO)
	  if (mnlength>=29)
	    return ENAMETOOLONG;
	  else{
	    *ms++ = '\\';
	    *ms++ = HEXDIGIT(c>>4);
	    *ms++ = HEXDIGIT(c);
	    mnlength +=2;	/* One more gets added above */
	  }
	else
	  *ms++ = cm;
      }
  *ms = 0;
  *mname = mnlength;
  return 0;
}


/*
 * Primary and secondary sorting tables.
 *
 * HFS file names are 8 bit characters and are sorted in diacritical sensitive,
 * case insensitive order.  This is accomplished by comparing each character's
 * primary code (diacritical and case insensitive) and if necessary, by
 * comparing the character's secondary code (diacritical and case sensitive).
 * In the tables below, the primary/secondary code pairs for characters
 * differing only in their case are identical.  The result is a case
 * insensitive comparison.
 */

static char primary[]={
  /*   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
  ROW(00,01,02,03,04,05,06,07,08,09,0a,0b,0c,0d,0e,0f),	/* 0 */
  ROW(10,11,12,13,14,15,16,17,18,19,1a,1b,1c,1d,1e,1f),	/* 1 */
  ROW(20,21,22,23,24,25,26,27,28,29,2a,2b,2c,2d,2e,2f),	/* 2 */
  ROW(30,31,32,33,34,35,36,37,38,39,3a,3b,3c,3d,3e,3f),	/* 3 */
  ROW(40,41,42,43,44,45,46,47,48,49,4a,4b,4c,4d,4e,4f),	/* 4 */
  ROW(50,51,52,53,54,55,56,57,58,59,5a,5b,5c,5d,5e,5f),	/* 5 */
  ROW(41,41,42,43,44,45,46,47,48,49,4a,4b,4c,4d,4e,4f),	/* 6 */
  ROW(50,51,52,53,54,55,56,57,58,59,5a,7b,7c,7d,7e,7f),	/* 7 */
  ROW(41,41,43,45,4e,4f,55,41,41,41,41,41,41,43,45,45),	/* 8 */
  ROW(45,45,49,49,49,49,4e,4f,4f,4f,4f,4f,55,55,55,55),	/* 9 */
  ROW(a0,a1,a2,a3,a4,a5,a6,53,a8,a9,aa,ab,ac,ad,41,4f),	/* A */
  ROW(b0,b1,b2,b3,b4,b5,b6,b7,b8,b9,ba,41,4f,bd,41,4f),	/* B */
  ROW(c0,c1,c2,c3,c4,c5,c6,22,22,c9,20,41,41,4f,4f,4f),	/* C */
  ROW(d0,d1,22,22,27,27,d6,d7,59,d9,da,db,dc,dd,de,df),	/* D */
  ROW(e0,e1,e2,e3,e4,e5,e6,e7,e8,e9,ea,eb,ec,ed,ee,ef),	/* E */
  ROW(f0,f0,f2,f3,f4,f5,f6,f7,f8,f9,fa,fb,fc,fd,fe,ff)  /* F */
};

static char secondary[]={
  /*   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F  */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 0 */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 1 */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 2 */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 3 */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 4 */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 5 */
  ROW(06,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 6 */
  ROW(00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00),	/* 7 */
  ROW(02,04,01,01,01,01,01,07,01,08,02,03,04,01,01,02),	/* 8 */
  ROW(03,04,01,02,03,04,01,05,06,07,01,02,02,03,04,01),	/* 9 */
  ROW(00,00,00,00,00,00,00,01,00,00,00,00,00,00,05,03),	/* A */
  ROW(00,00,00,00,00,00,00,00,00,00,00,09,08,00,05,03),	/* B */
  ROW(00,00,00,00,00,00,00,03,04,00,01,01,03,02,04,04),	/* C */
  ROW(00,00,01,02,01,02,00,00,01,02,00,00,00,00,00,00),	/* D */
  ROW(00,00,00,00,00,03,03,01,04,02,01,03,04,02,01,03),	/* E */
  ROW(00,02,01,03,02,00,00,00,00,00,00,00,00,00,00,00)  /* F */
};


/**************
 * dir_mstr_cmp	Compare to mac strings, handling mac 8 bit char's.
 *=============
 *
 * Returns:    =0	Strings match.
 *	       <0	String s0 is less than string s1.
 *	       >0	String s0 is greater than string s1.
 */
int dir_mstr_cmp(char *s0, char *s1){
  char *g=s0, *t=s1;
  int  r,l;

  l=MIN(*g,*t); g++; t++;

  for (r=0; l-- && r==0; g++,t++){
    if (r=primary[*g] - primary[*t])
      break;
    if (r=secondary[*g] - secondary[*t])
      break;
  }
  if (r==0)
    r = *s0 - *s1;
  return r;
}
