/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/mkdev.h>

/*
 * Function:
 * getmajor()
 *
 * Description:
 * Return internal major number corresponding to device number argument.
 *
 * Arguments:
 * dev: Device number.
 *
 * Return value:
 * Major device number in the new format.
 */
major_t
getmajor(dev_t dev)
{
	return ((major_t)(dev>>NBITSMINOR) & MAXMAJ);
}
