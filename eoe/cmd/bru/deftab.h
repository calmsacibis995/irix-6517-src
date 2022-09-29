/*
 *	A default table to be used if no table can be loaded.
 */

char *deftab = 
#ifdef xenix		/* Can't handle big strings */
"/dev/mt0 \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=EIO zwerr=0 frerr=0 fwerr=0 wperr=0 \\\n\
   reopen tape advance \n\
 /dev/rmt0 \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=EIO zwerr=0 frerr=0 fwerr=0 wperr=0 \\\n\
   reopen rawtape tape advance \n\
 - \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=0 zwerr=0 frerr=0 fwerr=0 wperr=0 \n\
   ";
#else
# ifndef sgi
"/dev/mt0 \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=EIO zwerr=0 frerr=0 fwerr=0 wperr=0 \\\n\
   reopen tape advance \n\
 /dev/nmt0 \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=EIO zwerr=0 frerr=0 fwerr=0 wperr=0 \\\n\
   noreopen tape norewind advance \n\
 /dev/rmt0 \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=EIO zwerr=0 frerr=0 fwerr=0 wperr=0 \\\n\
   reopen rawtape tape advance \n\
 /dev/nrmt0 \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=EIO zwerr=0 frerr=0 fwerr=0 wperr=0 \\\n\
   noreopen tape rawtape norewind advance \n\
 - \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=0 zwerr=0 frerr=0 fwerr=0 wperr=0 \n\
   ";
# else
"/dev/tape* \\\n\
	size=0k seek=0 \\\n\
	prerr=EIO pwerr=EIO zrerr=ENOSPC zwerr=ENOSPC frerr=0 fwerr=0 \\\n\
	wperr=EROFS reopen tape rawtape advance\n\
 /dev/nrtape* \\\n\
	size=0k seek=0 \\\n\
	prerr=EIO pwerr=EIO zrerr=ENOSPC zwerr=ENOSPC frerr=0 fwerr=0 \\\n\
	wperr=EROFS norewind reopen tape rawtape advance\n\
 - \\\n\
   size=0 seek=0 \\\n\
   prerr=0 pwerr=0 zrerr=0 zwerr=0 frerr=0 fwerr=0 wperr=0 \n";
# endif
#endif
