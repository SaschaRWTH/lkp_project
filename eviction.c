#include <linux/kernel.h>
#include <linux/dcache.h>
#include <linux/buffer_head.h>
#include <linux/audit.h>
#include <linux/security.h>
#include <linux/mount.h>

#include "policy.h"
#include "eviction.h"
#include "ouichefs.h"

static int is_threshold_met(struct inode *dir);
static int evict_file(struct mnt_idmap *idmap, struct inode *dir,\
		      struct inode *file);
static struct dentry *inode_to_dentry(struct inode *parent,\
				      struct inode *inode);
static char *get_name_of_inode(struct inode *dir, struct inode *inode);


/**
 * Percentage threshold at which the eviction of a file is triggered.
 */
const u16 eviction_threshhold = 95;

/**
 * general_eviction - Checks the remaining space and evicts a file based on
 * the current policy, if a certin threshold is met. 
 * 
 * @dir: Directory where a new node was created.
 * @idmap: Idmap of the mount the inode was found from.
 * 
 * Return: EVICTION_NOT_NECESSARY if the general eviction was not necessary, 
 * 0 if it could be performed
 * and < 0 if the eviction was failed.
 */
int general_eviction(struct mnt_idmap *idmap, struct inode *dir)
{
	int errc = 0;
	// TODO: Find correct place to call general eviction
	// Create function to check on each creation?
	errc = is_threshold_met(dir);
	if (!errc) {
		pr_info("The threshold is not met.\n");
		return EVICTION_NOT_NECESSARY;
	}

	if (errc < 0) 
		return errc;

	// TODO: IMPLEMENT DIR_EVICTION
	return errc;
}
/**
 * is_threshold_met - Checks whether the threshold for a general eviction
 * is met.
 * 
 * @dir: Directory where a new node was created.
 * 
 * Return: 0 if threshold is not met, > 0 if it is met, < 0 on error.
 */
static int is_threshold_met(struct inode *dir) {
	
	if (dir == NULL) 
		return -1;

	struct super_block *sb = dir->i_sb;
	if (sb == NULL)
		return -1;

	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	if (sbi == NULL) 
		return -1;

	if (sbi->nr_free_blocks > (sbi->nr_blocks * eviction_threshhold) / 100)
		return 1;

	return 0;
}

/**
 * dir_eviction - Eviction that is triggered when a node is created in a full 
 * directory.
 * 
 * @dir: Directory from which to evict.
 * @idmap: Idmap of the mount the inode was found from.
 * 
 * Return: 0 if the directory eviction could be performed.
 */
int dir_eviction(struct mnt_idmap *idmap, struct inode *dir)
{
	int errc = 0;

	// Should we be locking dir? 
	// Module hangs if i try to

	struct inode *remove = dir_get_file_to_evict(dir);
	if (IS_ERR(remove)) {
		long errc = PTR_ERR(remove);
		return errc; 
	}

	// Check if the node is locked
	if (inode_is_locked(remove)) {
		errc = -EBUSY;
		goto put_node;
	}

	// Check if node is in use
	// Check reference count to inode
	// WHY IS i_count 2 instead of 1
	// This should NOT be and is also not always the case????
	// if (remove->i_count.counter - 1 > 0) {
	// 	pr_info("The file to remove is in use by another process.\n");
	// 	errc = 1;
	// 	goto put_node;
	// }

	// Check if no files to remove could be found
	if (!remove) {
		errc = ONLY_CONTAINS_DIR;
		goto put_node;
	}


	// Evict the node
	errc = evict_file(idmap, dir, remove);

put_node:
	iput(remove);
	return errc;
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
		return -1;
	}

	pr_info("Beginning to remove file.\n");
	int error = dir->i_op->unlink(dir, dentry);
	if (error) 
		pr_info("(unlink): Could not unlink file.\n");
	
	dput(dentry);
	
	return error;
}

/**
 * inode_to_dentry - Gets the dentry of a given inode.
 * 
 * @dir: directory of the inode.
 * @inode: inode for which to find the dentry.
 * 
 * Return: The dentry of the inode.
 *  
 **/
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
