/*
 *	authenticate_clnt()
 *	This routine uses simple UNIX style authentication to verify
 *	that the client is root. 
 *	Return Value : 	0 (if root)
 *			1 (otherwise)
 */

int
authenticate_clnt(struct svc_req *rqstp)
{
	int returnValue = 1;
	struct authunix_parms *aup=NULL;
	char *lname;
	char *rname;

	if (rqstp != NULL)
	{
		/* Get the authentication information */
		aup = (struct authuinx_parms *) (rqstp->rq_clntcred);
	
		/* Does client have root permissions ? */
		rname = cuserid(aup->aup_uid);
		lname = cuserid(0);
		if ((aup != NULL) && !ruserok(aup->aup_machname, 1, rname, lname))
		{
			returnValue = 0;
		}
	}

	return(returnValue);
}

