/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/ddi.h>

/*
 * Function:
 * itoemajor()
 *
 * Description:
 * Pass a lastemaj val of -1 to start the search initially.
 * Typical use of this function is of the form:
 *
 *	     lastemaj=-1;
 *	     while ( (lastemaj = itoemajor(imag,lastemaj)) != -1)
 *	        { process major number }
 *
 * Arguments:
 * imajnum: internal major number
 * lastemaj: last extended major
 *
 * Return value:
 * The external major number that corresponds to a device's internal
 * major number.
 * -1: External major number not found after lastemaj.
 */
int
itoemajor(major_t imajnum, int lastemaj)
{
	if (imajnum >= max(bdevcnt, cdevcnt))
		return (-1);

	/*
	 * if lastemaj == -1 then start from beginning of MAJOR table.
	 */
	if (lastemaj < -1)
		return (-1); 

	return ((int)imajnum);
}
