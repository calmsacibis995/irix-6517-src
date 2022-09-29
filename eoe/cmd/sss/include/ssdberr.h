/* ----------------------------------------------------------------- */
/*                               SSDBERR.H                           */
/*                SSDB API Error code(s) definition file             */
/* ----------------------------------------------------------------- */
#ifndef H_SSDBERR_H
#define H_SSDBERR_H

/* SSDB Error code(s)                                                */
#define SSDBERR_SUCCESS          0  /* Success */
#define SSDBERR_INVPARAM       (-1) /* Invalid parameter(s) */
#define SSDBERR_NOTINITED      (-2) /* SSDB interface not initialized */
#define SSDBERR_ALREADYINIT1   (-3) /* Already in init phase */
#define SSDBERR_ALREADYINIT2   (-4) /* Already inited */
#define SSDBERR_INITERROR1     (-5) /* Init error (critical section/mutex initialization) */
#define SSDBERR_ALREADYDONE    (-6) /* Already Done */
#define SSDBERR_NOTENOGHRES    (-7) /* Not enough resources */
#define SSDBERR_RESOURCEINUSE  (-8) /* Resource already in use */
#define SSDBERR_DBCONERROR1    (-9) /* Database connection error N:1 */
#define SSDBERR_DBCONERROR2   (-10) /* Database connection error N:2 */
#define SSDBERR_CONNOTOPENED  (-11) /* Connection not opened */
#define SSDBERR_SQLQUERYERROR (-12) /* SQL Request error */
#define SSDBERR_PROCESSRESERR (-13) /* Process result Error */
#define SSDBERR_DBERROR	      (-14) /* Some error from the DB Server */
#define SSDBERR_ENDOFSET      (-15) /* Requested for more rows after reaching end of selected set */

#endif /* #ifndef H_SSDBERR_H */
