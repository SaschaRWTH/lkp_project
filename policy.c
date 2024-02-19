#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/rwsem.h>

#include "policy.h"
#include "ouichefs.h"

/**
 * A reader/writer semaphore for the current policy that allows
 * multiple readers to access the current policy (if no writer
 * has access) while allowing for only one writer.
 */
static DECLARE_RWSEM(policy_lock);

static struct inode *lru_compare(struct inode *, struct inode *);
static struct eviction_policy least_recently_used_policy = {
	.name = "LRU Policy",
	.description = "Evicts least-recently used file.",
	.compare = lru_compare,
};

static struct eviction_policy *current_policy = &least_recently_used_policy;
struct inode *dir_file_to_evict(struct inode *dir);
static struct inode *file_to_evict_inode_store(struct super_block *superblock);
static struct inode *search_inode_store_block(struct super_block *superblock,\
				       uint32_t inode_block); 

/**
 * lru_compare - Compares two inodes based on which was used least recently.
 * 
 * @first: First node to compare.
 * @second: Second node to compare.
 * 
 * Return: The node which was least recently used.
 */ 
static struct inode *lru_compare(struct inode *first, struct inode *second)
{
	struct inode *lru;

	if(!first)
		return second;
	if(!second)
		return first;

	// Compare access time of nodes
	// We assure seconds are detailed enough for this check.
	if(first->i_atime.tv_sec < second->i_atime.tv_sec)
		lru = first;
	else
		lru = second;
	
	// Return inode which was least recently accessed
	return lru;
}

/**
 *  get_file_to_evict - Gets a file from the fs to evict based on the 
 *                      current policy.
 * 
 * @sb: Super block of the file system.
 * 
 * Return: The inode to evict based on the current eviction policy.
 */
struct inode *get_file_to_evict(struct super_block *sb)
{
	pr_info("Current eviction policy is '%s'", current_policy->name);

	down_read(&policy_lock);
	struct inode *evict = file_to_evict_inode_store(sb);
	up_read(&policy_lock);

	if (!evict) {
		pr_warn("file_to_evict_inode_store did not return a file.\n");
		return NULL;
	}	
	
	if (!S_ISREG(evict->i_mode)) {
		pr_warn("file_to_evict_inode_store did not return a file.\n");
		iput(evict);
		return NULL;
	}

	return evict;
}

/**
 * Searches for a file in a directory to evict based on the current 
 * eviction policy.
 * 
 * @dir: directory to search for file to evict.
 * 
 * Return: inode to evict, NULL if no file could be found.
 * 
 * Note: function increases the i_count and the inode needs to be 
 * put afterwards.
 **/ 
struct inode *dir_get_file_to_evict(struct inode *dir)
{
	pr_info("Current eviction policy is '%s'", current_policy->name);

	// Check if given dir is null.
	if(!dir) {
		pr_warn("The given inode was NULL.\n");
		return NULL;
	}

	// Check that dir is a directory 
	if (!S_ISDIR(dir->i_mode)) {
		pr_warn("The given inode was not a directory.\n");
		return ERR_PTR(-ENOTDIR);
	}

	down_read(&policy_lock);
	struct inode* remove = dir_file_to_evict(dir);
	up_read(&policy_lock);

	
	return remove;
}

/**
 * dir_file_to_evict - searches a given directory for a file to evict based on
 * 		       the current policy.
 * 
 * @dir: directory to search.
 * 
 * Return: pointer to inode of file to evict, NULL if no file could be found.
 * 
 * Note: We assure that the policy is already locked for reading.
 */
struct inode *dir_file_to_evict(struct inode *dir)
{
	// Read the directory index block on disk 
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(dir);
	struct super_block *superblock = dir->i_sb;
	struct buffer_head *bufferhead = sb_bread(superblock, ci->index_block);
	if (!bufferhead) {
		pr_warn("The buffer head could not be read.\n");
		return ERR_PTR(-EIO);
	}
	struct ouichefs_dir_block *dblock = \
		(struct ouichefs_dir_block *)bufferhead->b_data;


	struct inode *remove = NULL;
	struct inode *temp = NULL;
	// Iterate over the index block
	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		struct ouichefs_file *f = &dblock->files[i];
		if (!f->inode) {
			pr_warn("The directory is not full.\n");
			break;
		}

		pr_info("Checking file with ino %lu.\n and name %s",\
			(unsigned long) f->inode, f->filename);

		// Get inode struct from superblock
		// Increases ref count of inode, need to put!
		struct inode *inode = ouichefs_iget(superblock, f->inode);

		// Check till first null inode 
		if (!inode) {
			pr_warn("The directory is not full.\n");
			break;
		}

		// Check that the node is a file
		if (!S_ISREG(inode->i_mode))
			continue;

		if (!remove) {
			remove = inode;
		}
		else {
			temp = current_policy->compare(remove, inode);
			// Put unused inode
			if (temp != remove) {
				iput(remove);
				remove = inode;
			}
			else {
				iput(inode);
			}
		} 
	}

	// Release buffer
	// What kind of ____ abbreviation for "release" is "relse".
	// Had to google because i was unsure.
	brelse(bufferhead);

	pr_info("Returning file with ino %lu.\n",\
			 (unsigned long) remove->i_ino);

	return remove;
}

/**
 * file_to_evict_inode_store - searches the inode store of a given super block
 * 			       for a file to evict based on the current policy.
 * 
 * @superblock: superblock of the inode store to search.
 * 
 * Return: pointer to file to evict, NULL if no file could be found.
 * 
 * Note: We assure that the current policy has already been locked for reading.
 */
static struct inode *file_to_evict_inode_store(struct super_block *superblock) 
{
	if (!superblock) {
		pr_warn("The given superblock was NULL.\n");
		return NULL;
	}

	// Search inode store for inode to evict 
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(superblock);

	// Loop through all inode store blocks
	struct inode *remove = NULL;
	for(int inode_block = 0; inode_block < sbi->nr_istore_blocks; \
				 inode_block++) {
		struct inode *inode = search_inode_store_block(superblock,\
							       inode_block + 1);
		if (!remove) {
			remove = inode;
			continue;
		}

		if (!inode || IS_ERR(inode)) 
			continue;

		if (current_policy->compare(remove, inode) == inode) {
			iput(remove);
			remove = inode;
		}
		else {
			iput(inode);
		}
	}

	return remove;
}

/**
 *  search_inode_store_block - searches a block of the inode store for the 
 * 			       file to evict based on the current policy.
 * 
 * @superblock: superblock of the filesystem
 * @inode_block: index of the inode block in the inode store
 * 
 * Return: pointer to inode to remove, NULL if non could be found.
 */
static struct inode *search_inode_store_block(struct super_block *superblock,\
				       uint32_t inode_block) 
{
	if (!superblock) 
		return NULL;

	if (inode_block < 1 ) 
		return NULL;

	struct buffer_head *bh = sb_bread(superblock, inode_block);
	if (!bh)
		return ERR_PTR(-EIO);

	struct ouichefs_sb_info *sbi = OUICHEFS_SB(superblock);
	struct ouichefs_inode *disk_inode = (struct ouichefs_inode *)bh->b_data;
	struct inode *remove = NULL;
	uint32_t ino = find_next_zero_bit(sbi->ifree_bitmap, \
				sbi->nr_inodes, \
				(inode_block - 1) * OUICHEFS_INODES_PER_BLOCK);
	pr_info("Start ino %d block %d\n", ino, inode_block);
	while (ino < (inode_block) * OUICHEFS_INODES_PER_BLOCK) {
		pr_info("Checking inode with ino %d\n", ino);
		uint32_t inode_shift = ino - \
				(inode_block - 1) * OUICHEFS_INODES_PER_BLOCK;
		struct ouichefs_inode *current_inode = disk_inode + inode_shift;
		
		
		// Something would be very wrong if this happened.
		if(!current_inode)
			goto while_cont;

		// Skip empty inodes
		if(current_inode->index_block == 0)
			goto while_cont;
		
		// Only regular files can be evicted.
		if(!S_ISREG(current_inode->i_mode)) 
			goto while_cont;
		
		struct inode *inode = ouichefs_iget(superblock, ino);
		if (!inode || IS_ERR(inode)) 
			goto while_cont;

		if (!remove) {
			remove = inode;
			goto while_cont;
		}
		
		if (current_policy->compare(remove, inode) == inode) {
			iput(remove);
			remove = inode;
		}
		else {
			iput(inode);
		}

while_cont:
		ino = find_next_zero_bit(sbi->ifree_bitmap, \
				     sbi->nr_inodes, ino + 1);
		if(ino == sbi->nr_inodes)
			break;
	}

	brelse(bh);
	return remove;
}



/**
 * #TODO: Write function to set another policy and export it.
 * register_policy - registers a given policy as the current policy.
 * 
 * @policy: policy to register.
 * 
 * Return: 0 is successfully registered, -POLICY_ALREADY_REGISTERED if a policy
 * 	   is already registered.
 *   
 */
int register_policy(struct eviction_policy *policy)
{
	if (!policy)
		return -EFAULT;
	
	if (!policy->compare)
		return -EFAULT;

	// Check if another policy is already registered
	if (current_policy != &least_recently_used_policy) {
		pr_debug("A policy is already registered.");
		return -POLICY_ALREADY_REGISTERED;
	}
	

	down_write(&policy_lock);
	current_policy = policy;
	up_write(&policy_lock);

	return 0;
}
EXPORT_SYMBOL(register_policy);

/**
 *  unregister_policy - unregisters a given policy and restores the default
 * 			policy.
 * 
 * @policy: policy to unregister.
 */
void unregister_policy(struct eviction_policy *policy) 
{
	if (policy != current_policy) {
		pr_err("Tried to unregister a policy that is not in use.\n");
		return;
	}
	
	down_write(&policy_lock);
	current_policy = &least_recently_used_policy;
	up_write(&policy_lock);
}
EXPORT_SYMBOL(unregister_policy);