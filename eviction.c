#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

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
static struct inode *get_parent_of_inode_non_rec(struct inode *inode);
static struct inode *search_parent_isb(struct inode *inode, \
				       uint32_t inode_block);
static bool dir_contains_ino(struct super_block *superblock, \
			     struct ouichefs_inode *dir, uint32_t ino);

/**
 * Currently we have a 
 * kernel BUG at fs/inode.c:1804!
 * iput wants to put inode but I_CLEAR is set
 * (Added by clear_inode().  In this state the inode is
 * clean and can be destroyed.)
 * 
 * Dont know why the kernel bug occures. 
 * Not putting inode correctly?
 */

/**
 * Percentage threshold at which the eviction of a file is triggered.
 */
const u16 eviction_threshhold = 80;

/** 
 * #TODO: _create should call a check for general eviction
 * or similar with the eviction check and general 
 * eviction should then evit.
 * -> we can let the user trigger eviction easier. 
*/

/**
 * general_eviction - Checks the remaining space and evicts a file based on
 * 		      the current policy, if a certin threshold is met. 
 * 
 * @dir: Directory where a new node was created.
 * @idmap: Idmap of the mount the inode was found from.
 * 
 * Return: EVICTION_NOT_NECESSARY if the general eviction was not necessary, 
 * 	   0 if it could be performed
 * 	   and < 0 if the eviction was failed.
 */
int general_eviction(struct mnt_idmap *idmap, struct inode *dir)
{
	int errc = 0;
	
	errc = is_threshold_met(dir);
	if (errc == 0) 
		return EVICTION_NOT_NECESSARY;

	if (errc < 0) {
		pr_info("The threshold check failed.\n");
		return errc;
	}

	pr_info("The threshold was met. Finding file to evict.\n");

	struct inode *evict = get_file_to_evict(dir);
	pr_info("Found inode with ino %lu.\n", evict->i_ino);

	if (!evict) {
		pr_warn("Could not find a file to evict.\n");
		return -1;
	}

	if (IS_ERR(evict)) {
		pr_warn("get_file_to_evict return an error.\n");
		return PTR_ERR(evict);
	}

	if (!S_ISREG(evict->i_mode)) {
		pr_warn("Eviction search did not return a reg file.\n");
		errc = -1;
		goto general_put;
	}
	
	struct inode *parent = get_parent_of_inode_non_rec(evict);

	if (!parent) {
		pr_warn("Could not find parent of file to evict.\n");
		errc = -1;
		goto general_put;
	}
	if (IS_ERR(parent)) {
		pr_warn("Find parent return an error.\n");
		errc = PTR_ERR(parent);
		goto general_put;
	}

	loff_t evicted_bytes = evict->i_size;
	errc = evict_file(idmap, parent, evict);
	if (!errc) 
		pr_info("Successfully evicted %lld bytes.\n", evicted_bytes);
	else 
		pr_info("An error occured in eviction.\n");

	iput(parent);
general_put:
	iput(evict);
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
static int is_threshold_met(struct inode *dir) 
{
	if (dir == NULL) 
		return -1;

	struct super_block *sb = dir->i_sb;
	if (sb == NULL)
		return -1;

	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	if (sbi == NULL) 
		return -1;


	u32 threshold_number = \
		(sbi->nr_blocks * (100 - eviction_threshhold)) / 100;
	if (sbi->nr_free_blocks < threshold_number)
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

	// TODO: Should we be locking dir? 
	// Module hangs if i try to

	struct inode *remove = dir_get_file_to_evict(dir);
	// Check if no files to remove could be found
	if (!remove) {
		errc = ONLY_CONTAINS_DIR;
		return errc;
	}

	// Check if dir_get_file_to_evict returned an error
	if (IS_ERR(remove)) {
		long errc = PTR_ERR(remove);
		return errc; 
	}

	// Check if the node is locked
	if (inode_is_locked(remove)) {
		errc = -EBUSY;
		goto dir_put;
	}

	// TODO: Check if node is in use
	// Check reference count to inode
	// WHY IS i_count 2 instead of 1
	// This should NOT be and is also not always the case????
	// if (remove->i_count.counter - 1 > 0) {
	// 	pr_info("The file to remove is in use by another process.\n");
	// 	errc = 1;
	// 	goto dir_put;
	// }

	// Evict the node
	errc = evict_file(idmap, dir, remove);

dir_put:
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
	pr_info("dentry ino = %lu.\n", dentry->d_inode->i_ino);

	if (!dentry || IS_ERR(dentry)) {
		pr_info("The dentry could not be found.\n");
		return -1;
	}

	pr_info("dir ino: %lu, dentry.inode.ino: %lu.\n", dir->i_ino,
		dentry->d_inode->i_ino);
	int error = dir->i_op->unlink(dir, dentry);
	if (error) 
		pr_info("(unlink): Could not unlink file.\n");
	
	/*
	 * Unnecessary? I dont know. 
	 * dput(dentry);
	 * Is it causing errors? Probably
	 * Ok, dput(dentry) was causing error and does not need to be called.
	 * 
	 * what about 				
	 * dont_mount(dentry);
	 * detach_mounts(dentry);
	 * Do i need them?
	 * I have no clue.
	 */ 
	
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

	/**
	 *  Did i find the correct function??
	 *  I am still not sure, but it works for now. 
	 */
	struct dentry *dentry = d_obtain_alias(inode);

	dentry->d_name.name = name;	


	/* SOMEHOW this can set dentry->d_inode->i_ino to 0.
	 * but also not always, only after shutting down vm and 
	 * restarting??	  
	 * 
	 * So apparently, for whatever reason, 
	 * 'dentry->d_parent->d_inode' sets the dentry->d_inode
	 * property to the parent as well, if i understand correctly???
	 * 	
	 * Jap, thats whats i happening, but only after reboot?
	 * Why? 
	 * What?
	 * 
	 * Probably sth to do with if the dentry is in the dcache or not
	 */
	dentry->d_parent->d_inode = dir;
	/* 
	 * Can we just set the inode of the dentry to its original 
	 * or "supposed to be"-values?
	 * 
	 * bug:
	 * Kind of working, but now we have
	 * kernel BUG at fs/inode.c:1804!
	 * the put kernel bug, described above.
	 * FIX: Removed 'dput(dentry)'
	 */
	dentry->d_inode = inode;

	return dentry;
}

/**
 *  get_name_of_inode - gets the name of a given inode
 *  @dir: parent directory of the inode
 *  @inode: inode of which we want to know the name
 * 
 *  Return: name of inode.
 */
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

static struct inode *get_parent_of_inode_non_rec(struct inode *inode) 
{
	if (!inode) {
		pr_warn("The given inode was NULL.\n");
		return NULL;
	}

	// Search inode store for inode to evict 
	struct super_block *superblock = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(superblock);

	pr_debug("Number of istore blocks: %d.\n", sbi->nr_istore_blocks);
	for(int inode_block = 0; inode_block < sbi->nr_istore_blocks; \
				 inode_block++) {
		pr_debug("Checking inode store block %d.\n", inode_block);
		struct inode *parent = search_parent_isb(inode, inode_block + 1);
		if (parent) {
			return parent;
		}
	}

	return NULL;
}
/**
 * isb = inode store block
*/
static struct inode *search_parent_isb(struct inode *inode, \
				       uint32_t inode_block) 
{
	if (!inode) 
		return NULL;

	if (inode_block < 1 ) 
		return NULL;

	struct super_block *superblock = inode->i_sb;
	struct buffer_head *bh = sb_bread(superblock, inode_block);
	if (!bh)
		return ERR_PTR(-EIO);

	struct ouichefs_inode *disk_inode = (struct ouichefs_inode *)bh->b_data;
	struct inode *parent = NULL;
	for(uint32_t inode_shift = 0; inode_shift < OUICHEFS_INODES_PER_BLOCK; \
				      inode_shift++) {
		struct ouichefs_inode *current_inode = disk_inode + inode_shift;
		unsigned long ino = (inode_block - 1) * \
				OUICHEFS_INODES_PER_BLOCK\
					+ inode_shift;

		pr_debug("Checking NON REC inode with ino %lu.\n", ino);
		// Something would be very wrong if this happened.
		if (!current_inode) {
			pr_debug("Skipping NULL inode.\n");
			continue;
		}

		// Skip empty inodes
		if (current_inode->index_block == 0) {
			pr_debug("Skipping inode with index_block 0.\n");
			continue;
		}
		
		// Only regular files can be evicted.
		if (!S_ISDIR(current_inode->i_mode)) {
			pr_debug("Inode is not a directory.\n");
			continue;
		}

		if (dir_contains_ino(superblock, current_inode, inode->i_ino)) {
			pr_debug("Found parent inode with ino %lu.\n", \
				ino);
			parent = ouichefs_iget(superblock, ino);
			break;	
		}
	}

	brelse(bh);
	return parent;
}

static bool dir_contains_ino(struct super_block *superblock, \
			     struct ouichefs_inode *dir, uint32_t ino)
{
	if (!dir) 
		return false;

	struct buffer_head *bh = sb_bread(superblock, dir->index_block);
	if (!bh) {
		pr_warn("could not read buffer head.\n");
		return false;
	}

	bool contains = false;
	struct ouichefs_dir_block *dblock = \
		(struct ouichefs_dir_block *)bh->b_data;
	struct ouichefs_file *f = NULL;
	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		f = &dblock->files[i];
		if (!f->inode)
			break;
		if (f->inode == ino) {
			contains = true;
			break;
		}
	}
	brelse(bh);
	return contains;
}