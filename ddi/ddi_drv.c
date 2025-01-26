/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * Function:
 * drv_getparm()
 *
 * Description:
 * Store value of kernel parameter associated with parm in valuep.
 *
 * Arguments:
 * parm: parameter
 * valuep: pointer to value
 *
 * Return value:
 * 0 if parm is UPROCP, UCRED, PPGRP, LBOLT, PPID, PSID, TIME.
 * -1 on error.
 */
int
drv_getparm(unsigned long parm, unsigned long *valuep)
{
	switch (parm) {
		case UPROCP:
			*valuep= (unsigned long) u.u_procp;
			break;
		case UCRED:
			*valuep= (unsigned long) u.u_procp->p_cred;
			break;
		case PPGRP:
			*valuep= (unsigned long) u.u_procp->p_pgrp;
			break;
		case LBOLT:
			*valuep= (unsigned long) lbolt;
			break;
		case TIME:
			*valuep= (unsigned long) hrestime.tv_sec;
			break;
		case PPID:
			*valuep= (unsigned long) u.u_procp->p_pid;
			break;
		case PSID:
			*valuep= (unsigned long) u.u_procp->p_sessp->s_sid;
			break;
		default:
			return(-1);
	}

	return (0);
}
