/*	Copyright (c) 1990 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

/*
 * Common kernel routines for VFS.
 * There are no system calls; they have been moved to vfs_syscalls.c.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/fstyp.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/pathname.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vfs.h>
#include <sys/statvfs.h>
#include <sys/statfs.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/dnlc.h>
#include <sys/file.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>

/*
 * vfs_mountroot() is called by main() to mount the root filesystem.
 */
void
vfs_mountroot(void)
{
	int i, error;
	struct vfssw *vsw;

	/*
	 * "rootfstype" will ordinarily have been initialized to
	 * contain the name of the fstype of the root file system
	 * (this is user-configurable). Use the name to find the
	 * vfssw table entry.
	 */
	if (vsw = vfs_getvfssw(rootfstype)) {
		VFS_INIT(rootvfs, vsw->vsw_vfsops, (caddr_t)0);
		error = VFS_MOUNTROOT(rootvfs, ROOT_INIT);
	} else {
		/*
		 * If rootfstype is not set or not found, step through
		 * all the fstypes until we find one that will accept
		 * a mountroot() request.
		 */
		for (i = 1; i < nfstype; i++) {
			if (vfssw[i].vsw_vfsops) {
				VFS_INIT(rootvfs, vfssw[i].vsw_vfsops,
				  (caddr_t)0);
				if ((error =
				  VFS_MOUNTROOT(rootvfs, ROOT_INIT)) == 0)
					break;
			}
		}
	}

	/*
	 * We have encountered an error while attempting to mount root.
	 * Inform the user with an error code, then panic.
	 */
	if (error) {
		cmn_err(CE_WARN, "Failed to mount root, error %d\n", error);
		cmn_err(CE_PANIC, "vfs_mountroot: cannot mount root");
	}

	/*
	 * Get vnode for '/'.  Set up rootdir, u.u_rdir and u.u_cdir
	 * to point to it.  These are used by lookuppn() so that it
	 * knows where to start from ('/' or '.').
	 */
	if (VFS_ROOT(rootvfs, &rootdir))
		cmn_err(CE_PANIC, "vfs_mountroot: no root vnode");
	u.u_cdir = rootdir;
	VN_HOLD(u.u_cdir);
	u.u_rdir = NULL;
}

/*
 * vfs_add is called by a specific filesystem's mount routine to add
 * the new vfs into the vfs list and to cover the mounted-on vnode.
 * The vfs should already have been locked by the caller.
 *
 * coveredvp is zero if this is the root.
 */
void
vfs_add(vnode_t *coveredvp, struct vfs *vfsp, int mflag)
{
	if (coveredvp != NULL) {
		vfsp->vfs_next = rootvfs->vfs_next;
		rootvfs->vfs_next = vfsp;
		coveredvp->v_vfsmountedhere = vfsp;
	} else {
		/*
		 * This is the root of the whole world.
		 */
		rootvfs = vfsp;
		vfsp->vfs_next = NULL;
	}
	vfsp->vfs_vnodecovered = coveredvp;

	if (mflag & MS_RDONLY)
		vfsp->vfs_flag |= VFS_RDONLY;
	else
		vfsp->vfs_flag &= ~VFS_RDONLY;

	if (mflag & MS_NOSUID)
		vfsp->vfs_flag |= VFS_NOSUID;
	else
		vfsp->vfs_flag &= ~VFS_NOSUID;
}

/*
 * Remove a vfs from the vfs list, and destroy pointers to it.
 * Should be called by filesystem "unmount" code after it determines
 * that an unmount is legal but before it destroys the vfs.
 */
void
vfs_remove(struct vfs *vfsp)
{
	struct vfs *tvfsp;
	vnode_t *vp;
	
	ASSERT(vfsp->vfs_flag & VFS_MLOCK);

	/*
	 * Can't unmount root.  Should never happen because fs will
	 * be busy.
	 */
	ASSERT(vfsp != rootvfs);

	for (tvfsp = rootvfs; tvfsp != NULL; tvfsp = tvfsp->vfs_next) {
		if (tvfsp->vfs_next == vfsp) {
			/*
			 * Remove vfs from list, unmount covered vp.
			 */
			tvfsp->vfs_next = vfsp->vfs_next;
			vp = vfsp->vfs_vnodecovered;
			vp->v_vfsmountedhere = NULL;
			/*
			 * Release lock and wakeup anybody waiting.
			 */
			vfs_unlock(vfsp);
			return;
		}
	}
	/*
	 * Can't find vfs to remove.
	 */
	cmn_err(CE_PANIC, "vfs_remove: vfs not found");
}

/*
 * Lock a filesystem to prevent access to it while mounting
 * and unmounting.  Returns error if already locked.
 */
int
vfs_lock(struct vfs *vfsp)
{
	if (vfsp->vfs_flag & VFS_MLOCK)
		return EBUSY;
	vfsp->vfs_flag |= VFS_MLOCK;
	return 0;
}

/*
 * Unlock a locked filesystem.
 */
void
vfs_unlock(struct vfs *vfsp)
{
	ASSERT(vfsp->vfs_flag & VFS_MLOCK);

	vfsp->vfs_flag &= ~VFS_MLOCK;
	/*
	 * Wake anybody (most likely lookuppn()) waiting for the
	 * lock to clear.
	 */
	if (vfsp->vfs_flag & VFS_MWAIT) {
		vfsp->vfs_flag &= ~VFS_MWAIT;
		wakeprocs((caddr_t)vfsp, PRMPT);
	}
}

struct vfs *
getvfs(fsid_t *fsid)
{
	struct vfs *vfsp;

	for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
		if (vfsp->vfs_fsid.val[0] == fsid->val[0]
		  && vfsp->vfs_fsid.val[1] == fsid->val[1]) {
			break;
		}
	}
	return vfsp;
}

/*
 * Search the vfs list for a specified device.  Returns a pointer to it
 * or NULL if no suitable entry is found.
 */
struct vfs *
vfs_devsearch(dev_t dev)
{
	struct vfs *vfsp;

	for (vfsp = rootvfs; vfsp != NULL; vfsp = vfsp->vfs_next)
		if (vfsp->vfs_dev == dev)
			return vfsp;
	return NULL;
}

/*
 * Find a vfssw entry given a file system type name.
 */
struct vfssw *
vfs_getvfssw(char *type)
{
	int i;

	if (type == NULL || *type == '\0')
		return NULL;
	for (i = 1; i < nfstype; i++)
		if (strcmp(type, vfssw[i].vsw_name) == 0)
			return &vfssw[i];
	return NULL;
}

/*
 * Map VFS flags to statvfs flags.  These shouldn't really be separate
 * flags at all.
 */
u_long
vf_to_stf(u_long vf)
{
	u_long stf = 0;

	if (vf & VFS_RDONLY)
		stf |= ST_RDONLY;
	if (vf & VFS_NOSUID)
		stf |= ST_NOSUID;
	if (vf & VFS_NOTRUNC)
		stf |= ST_NOTRUNC;
	
	return stf;
}

/*
 * Entries for (illegal) fstype 0.
 */

STATIC int vfsstray(void);

STATIC struct vfsops vfs_strayops = {
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
	vfsstray,
};

STATIC int
vfsstray(void)
{
	cmn_err(CE_PANIC, "stray vfs operation");
	/* NOTREACHED */
}

void
vfsinit(void)
{
	int i;

	/*
	 * fstype 0 is (arbitrarily) invalid.
	 */
	vfssw[0].vsw_vfsops = &vfs_strayops;
	vfssw[0].vsw_name = "BADVFS";

	/*
	 * Call all the init routines.
	 */
	for (i = 1; i < nfstype; i++)
		(*vfssw[i].vsw_init)(&vfssw[i], i);
}
