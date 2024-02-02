#include <linux/kernel.h>
#include <linux/dcache.h>
#include <linux/buffer_head.h>
#include <linux/audit.h>
#include <linux/security.h>
#include <linux/mount.h>

#include "policy.h"
#include "eviction.h"
#include "ouichefs.h"

/**
 * Functions for eviction a file.
 * 
 * Return: -1 if the eviction fails (e.g. file in use)
 */
static int evict_file(struct mnt_idmap *idmap, struct inode *dir,\
		      struct inode *file);
static struct dentry *inode_to_dentry(struct inode *parent,\
				      struct inode *inode);
static char *get_name_of_inode(struct inode *dir, struct inode *inode);
static int may_delete(struct mnt_idmap *idmap, struct inode *dir,
		      struct dentry *victim, bool isdir);
/**
 * Percentage threshold at which the eviction of a file is triggered.
 */
const u16 eviction_threshhold = 95;

/** 
 * Evition that is triggered when a certin threshold is met.
 * Evicts a file from the partition based on the current policy.
 *
 * Returns -1 if the eviction fails (e.g. file in use)
 */
int general_eviction(void /*function parameters here*/)
{
	// TODO: Find correct place to call general eviction
	// Create function to check on each creation?
	// TODO: IMPLEMENT DIR_EVICTION
	return -1;
}


int dir_eviction(struct mnt_idmap *idmap, struct inode *dir)
{
	struct inode *remove = dir_get_file_to_evict(dir);
	if (IS_ERR(remove)) {
		long errc = PTR_ERR(remove);
		return errc; 
	}

	// Check if no files to remove could be found
	if (!remove)
		return ONLY_DIR;

	if(inode_is_locked(remove))
		return -EBUSY;

	return evict_file(idmap, dir, remove);
}

/**
 * Evicts a given file. 
 * @parent: Parent directory of file.
 * @file: File to evict. 
 */
static int evict_file(struct mnt_idmap *idmap, struct inode *dir,\
		      struct inode *file)
{
	if (!dir) {
		pr_info("The given parent is NULL.\n");
		return -1;
	}
	if (!file) {
		pr_info("The given file is NULL.\n");
		return -1;
	}
	


	struct dentry *dentry = inode_to_dentry(dir, file);

	if (!dentry || IS_ERR(dentry)) {
		pr_info("The dentry could not be found.\n");
	}

	pr_info("Beginning to remve file.\n");
	// Currently fails, likly in may_delete, at least for dentry not in 
	// dcache. https://elixir.bootlin.com/linux/v4.15.15/source/fs/namei.c#L2775
	//errc = vfs_unlink(idmap, dir, dentry, NULL); 
	// what does "returns victim inode, if the inode is delegated" mean??
	// cannot find any information on inode delegation
	int error = may_delete(idmap, dir, dentry, false);
	if (error) {
		pr_info("(may_delete): Was not allowed to remove file.\n");
		return error;
	}

	// if (IS_SWAPFILE(target))
	// 	return -EPERM;
	// if (is_local_mountpoint(dentry)) {
	// 	pr_info("is_local_mountpoint was true.\n");
	// 	return -EBUSY;
	// }

	error = security_inode_unlink(dir, dentry);
	if (error) {
		pr_info("(security_inode_unlink): Was not allowed to remove file.\n");
		return error;
	}

	error = dir->i_op->unlink(dir, dentry);
	if (error) {
		pr_info("(unlink): Could not unlink file.\n");
		return error;
	}
	

	pr_info("Detaching mounts.\n");
	dont_mount(dentry);
	detach_mounts(dentry);
	
	return error;
}

static struct dentry *inode_to_dentry(struct inode *dir, struct inode *inode)
{
	char* name = get_name_of_inode(dir, inode);
	if (!name) {
		pr_warn("Could not find name of inode.\n");
		return NULL;
	}

	if(IS_ERR(name))
		return ERR_PTR(PTR_ERR(name));

	pr_info("Found the name '%s' of the dentry.\n", name);

	// Did i find the correct function??
	struct dentry *dentry = d_obtain_alias(inode);

	pr_info("Found dentry the corrisponding dentry.\n");
	dentry->d_name.name = name;	
	dentry->d_parent->d_inode = dir;

	return dentry;
}

static char *get_name_of_inode(struct inode *dir, struct inode *inode)
{
	char *name = NULL;
	struct buffer_head *bh;
	struct super_block *sb = dir->i_sb;
	struct ouichefs_dir_block *dblock = NULL;
	struct ouichefs_file *f = NULL;
	struct ouichefs_inode_info *ci_dir = OUICHEFS_INODE(dir);

	/* Read the directory index block on disk */
	bh = sb_bread(sb, ci_dir->index_block);
	if (!bh)
		return ERR_PTR(-EIO);
	dblock = (struct ouichefs_dir_block *)bh->b_data;

	/* Search for the file in directory */
	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &dblock->files[i];
		if (!f->inode)
			break;
			
		if (f->inode == inode->i_ino) {
			name = f->filename;
			break;
		}

	}
	brelse(bh);

	return name;
}

/* 
 *	Check whether we can remove a link victim from directory dir, check
 *  whether the type of victim is right.
 *  1. We can't do it if dir is read-only (done in permission())
 *  2. We should have write and exec permissions on dir
 *  3. We can't remove anything from append-only dir
 *  4. We can't do anything with immutable dir (done in permission())
 *  5. If the sticky bit on dir is set we should either
 *	a. be owner of dir, or
 *	b. be owner of victim, or
 *	c. have CAP_FOWNER capability
 *  6. If the victim is append-only or immutable we can't do antyhing with
 *     links pointing to it.
 *  7. If the victim has an unknown uid or gid we can't change the inode.
 *  8. If we were asked to remove a directory and victim isn't one - ENOTDIR.
 *  9. If we were asked to remove a non-directory and victim isn't one - EISDIR.
 * 10. We can't remove a root or mountpoint.
 * 11. We don't allow removal of NFS sillyrenamed files; it's handled by
 *     nfs_async_unlink().
 */
static int may_delete(struct mnt_idmap *idmap, struct inode *dir,
		      struct dentry *victim, bool isdir)
{
	struct inode *inode = d_backing_inode(victim);
	int error;

	if (d_is_negative(victim))
		return -ENOENT;
	BUG_ON(!inode);

	BUG_ON(victim->d_parent->d_inode != dir);

	/* Inode writeback is not safe when the uid or gid are invalid. */
	if (!vfsuid_valid(i_uid_into_vfsuid(idmap, inode)) ||
	    !vfsgid_valid(i_gid_into_vfsgid(idmap, inode)))
		return -EOVERFLOW;

	audit_inode_child(dir, victim, AUDIT_TYPE_CHILD_DELETE);

	error = inode_permission(idmap, dir, MAY_WRITE | MAY_EXEC);
	if (error)
		return error;
	if (IS_APPEND(dir))
		return -EPERM;

	if (check_sticky(idmap, dir, inode) || IS_APPEND(inode) ||
	    IS_IMMUTABLE(inode) || IS_SWAPFILE(inode) ||
	    HAS_UNMAPPED_ID(idmap, inode))
		return -EPERM;
	if (isdir) {
		if (!d_is_dir(victim))
			return -ENOTDIR;
		if (IS_ROOT(victim))
			return -EBUSY;
	} else if (d_is_dir(victim))
		return -EISDIR;
	if (IS_DEADDIR(dir))
		return -ENOENT;
	if (victim->d_flags & DCACHE_NFSFS_RENAMED)
		return -EBUSY;
	return 0;
}