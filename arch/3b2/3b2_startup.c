/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

STATIC void datainit(void);
STATIC int sysseginit(int);
STATIC int kvm_init(int);
STATIC void p0init(void);
STATIC void devinit(void);

/*
 * Machine-dependent startup code.
 */
void
startup(void)
{
	/*
	 * Confirm that the configured number of supplementary groups
	 * is between the min and the max.  If not, print a message
	 * and assign the right value.
	 */
	if (ngroups_max < NGROUPS_UMIN) {
		cmn_err(CE_NOTE,
			"Configured value of NGROUPS_MAX (%d) is less than min (%d). "
			"NGROUPS_MAX set to %d\n", ngroups_max, NGROUPS_UMIN, NGROUPS_UMIN);
		ngroups_max = NGROUPS_UMIN;
	}

	if (ngroups_max > NGROUPS_UMAX) {
		cmn_err(CE_NOTE,
		  "Configured value of NGROUPS_MAX (%d) is greater than max (%d). "
		  "NGROUPS_MAX set to %d\n", ngroups_max, NGROUPS_UMAX, NGROUPS_UMAX);
		ngroups_max = NGROUPS_UMAX;
	}

	devinit();
	dmainit();
}
