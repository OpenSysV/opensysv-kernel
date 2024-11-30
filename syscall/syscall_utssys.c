/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#include <sys/types.h>
#include <sys/debug.h>

STATIC int uts_fusers(char *path, int flags, char *outbp, rval_t *rvp);

struct utssysa {
	union {
		char *cbuf;
		struct ustat *ubuf;
	} ub;
	union {
		int	mv;		/* for USTAT */
		int	flags;		/* for FUSERS */
	} un;
	int	type;
	char	*outbp;			/* for FUSERS */
};

int
utssys(struct utssysa *uap, rval_t *rvp)
{
	int error = 0;

	switch (uap->type) {
		case UTS_UNAME:
			char *buf = uap->ub.cbuf;

			if (copyout(utsname.sysname, buf, 8)) {
				error = EFAULT;
				break;
			}
			buf += 8;
			if (subyte(buf, 0) < 0) {
				error = EFAULT;
				break;
			}
			buf++;
			if (copyout(utsname.nodename, buf, 8)) {
				error = EFAULT;
				break;
			}
			buf += 8;
			if (subyte(buf, 0) < 0) {
				error = EFAULT;
				break;
			}
			buf++;
			if (copyout(utsname.release, buf, 8)) {
				error = EFAULT;
				break;
			}
			buf += 8;
			if (subyte(buf, 0) < 0) {
				error = EFAULT;
				break;
			}
			buf++;
			if (copyout(utsname.version, buf, 8)) {
				error = EFAULT;
				break;
			}
			buf += 8;
			if (subyte(buf, 0) < 0) {
				error = EFAULT;
				break;
			}
			buf++;
			if (copyout(utsname.machine, buf, 8)) {
				error = EFAULT;
				break;
			}
			buf += 8;
			if (subyte(buf, 0) < 0) {
				error = EFAULT;
				break;
			}
			rvp->r_val1 = 1;
			break;

		case UTS_USTAT:
			struct vfs *vfsp;
			struct ustat ust;
			struct statvfs stvfs;
			char *cp, *cp2;
			int i;
			extern int rf_ustat();

			/* NFA ustat hook */
			if (nfc_ustat())
				return;
			/* end NFA */
		
			/*
			 * RFS HOOK.
			 */
			if (uap->un.mv < 0)
				return rf_ustat((dev_t)uap->un.mv, uap->ub.ubuf);
			for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
				if (vfsp->vfs_dev == uap->un.mv || 
					cmpdev(vfsp->vfs_dev) == uap->un.mv)
					break;
			if (vfsp == NULL) {
				error = EINVAL;
				break;
			}
			if (error = VFS_STATVFS(vfsp, &stvfs))
				break;
			if (stvfs.f_ffree > USHRT_MAX) {
				error = EOVERFLOW;
				break;
			}
			ust.f_tfree = (daddr_t) (stvfs.f_bfree * (stvfs.f_frsize/512));
			ust.f_tinode = (o_ino_t) stvfs.f_ffree;

			cp = stvfs.f_fstr;
			cp2 = ust.f_fname;
			i = 0;
			while (i++ < sizeof(ust.f_fname))
				if (*cp != '\0')
					*cp2++ = *cp++;
				else
					*cp2++ = '\0';
			while (*cp != '\0' && (i++ < sizeof(stvfs.f_fstr) -
				sizeof(ust.f_fpack)))
				cp++;
			cp++;
			cp2 = ust.f_fpack;
			i = 0;
			while (i++ < sizeof(ust.f_fpack))
				if (*cp != '\0')
					*cp2++ = *cp++;
				else
					*cp2++ = '\0';
			if (copyout((caddr_t)&ust, uap->ub.cbuf, sizeof(ust)))
				error = EFAULT;
			break;

		case UTS_FUSERS:
			return uts_fusers(uap->ub.cbuf, uap->un.flags, uap->outbp, rvp);

		default:
			error = EINVAL;		/* ? */
			break;
	}

	return error;
}

/*
 * Determine the ways in which processes are using a named file or mounted
 * file system (path).  Normally return 0 with rvp->rval1 set to the number of 
 * processes found to be using it.  For each of these, fill a f_user_t to
 * describe the process and its useage.  When successful, copy this list
 * of structures to the user supplied buffer (outbp).
 *
 * In error cases, clean up and return the appropriate errno.
 */
STATIC int
uts_fusers(char *path, int flags, char *outbp, rval_t *rvp)
{
	vnode_t *fvp = NULL;
	int error;
	extern int lookupname();
	int dofusers();

	if ((error = lookupname(path, UIO_USERSPACE, FOLLOW, NULLVPP, &fvp)) != 0)
		return error;

	ASSERT(fvp);
	error = dofusers(fvp, flags, outbp, rvp);
	VN_RELE(fvp);
	return error;
}
