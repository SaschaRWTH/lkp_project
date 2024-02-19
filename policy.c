#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "policy.h"
#include "ouichefs.h"

static struct inode *lru_compare(struct inode *node1, struct inode *node2);
static struct eviction_policy least_recently_used_policy = {
	.name = "LRU Policy",
	.description = "Evicts least-recently used file.",
	.compare = lru_compare,
};

static struct eviction_policy *current_policy = &least_recently_used_policy;
struct inode *file_to_evict_rec(struct inode *inode);
struct inode *file_to_evict_non_rec(struct super_block *superblock);
struct inode *search_inode_store_block(struct super_block *superblock,\
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
 * @dir: A directory in the fs. The super_bock would also suffice, to give
 *       an inode is just easier.
 * 
 * Return: The inode to evict based on the current eviction policy.
 */
struct inode *get_file_to_evict(struct inode *dir)
{
	pr_info("Current eviction policy is '%s'", current_policy->name);
	
	// Switch function parameters to just receive superblock?
	struct super_block *sb = dir->i_sb;

	struct inode *evict = file_to_evict_non_rec(sb);

	if (!evict) {
		pr_warn("file_to_evict_non_rec did not return a file.\n");
		return NULL;
	}	
	
	if (!S_ISREG(evict->i_mode)) {
		pr_warn("file_to_evict_non_rec did not return a file.\n");
		iput(evict);
		return NULL;
	}

	return evict;
}

/**
 * #TODO: REWRITE function to not be recursive.
 * 	  Recursion should be avoided in the kernel.
 * 
 * file_to_evict_rec - Implements a recursive DFS search through the 
 *      	       given directory and its subdirectories for the file
 *  		       to evict based on the current policy.
 * 
 * @dir: Directory from which to search from.
 * 
 * Return: File to evict in the directory or its subdirectories.
 */
[[deprecated("Search function is recursive and should not be used.")]]
struct inode *file_to_evict_rec(struct inode *inode)
{
	if (!inode) 
		return inode;

	// If inode is a file, return the inode
	if (S_ISREG(inode->i_mode)) 
		return inode;

	if (!S_ISDIR(inode->i_mode)) {
		pr_warn("inode %lu is neither a file nor a directory.\n", 
			inode->i_ino);
		return inode;
	}

	// Search directory for inode based on policy
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *superblock = inode->i_sb;
	struct buffer_head *bufferhead = sb_bread(superblock, ci->index_block);
	if (!bufferhead) {
		pr_warn("The buffer head could not be read.\n");
		return ERR_PTR(-EIO);
	}
	struct ouichefs_dir_block *dblock = \
		(struct ouichefs_dir_block *)bufferhead->b_data;


	struct inode *remove = NULL;
	struct inode *temp = NULL;
	struct inode *res = NULL;
	// Iterate over the index block
	for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
		struct ouichefs_file *f = &dblock->files[i];
		if (!f->inode) 
			break;

		// Get inode struct from superblock
		// Increases ref count of inode, need to put!
		temp = ouichefs_iget(superblock, f->inode);

		// Check till first null inode 
		if (!temp) 
			break;
		
		// Comment out, otherwise deprecation warning
		// temp = file_to_evict_rec(temp);
		

		if (!remove) {
			remove = temp;
			continue;
		}

		res = current_policy->compare(remove, temp);
		if (res == remove) {
			iput(temp);
		}
		else {
			iput(remove);
			remove = res;
		}
	}
	brelse(bufferhead);

	return remove;
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

struct inode *file_to_evict_non_rec(struct super_block *superblock) 
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

struct inode *search_inode_store_block(struct super_block *superblock,\
				       uint32_t inode_block) 
{
	if (!superblock) 
		return NULL;

	if (inode_block < 1 ) 
		return NULL;

	struct buffer_head *bh = sb_bread(superblock, inode_block);
	if (!bh)
		return ERR_PTR(-EIO);

	struct ouichefs_inode *disk_inode = (struct ouichefs_inode *)bh->b_data;
	struct inode *remove = NULL;
	for(uint32_t inode_shift = 0; inode_shift < OUICHEFS_INODES_PER_BLOCK; \
				      inode_shift++) {
		struct ouichefs_inode *current_inode = disk_inode + inode_shift;
		
		// Something would be very wrong if this happened.
		if(!current_inode)
			continue;

		// Skip empty inodes
		if(current_inode->index_block == 0)
			continue;
		
		// Only regular files can be evicted.
		if(!S_ISREG(current_inode->i_mode)) 
			continue;
		
		int ino = (inode_block - 1) * OUICHEFS_INODES_PER_BLOCK + \
			  inode_shift;
		struct inode *inode = ouichefs_iget(superblock, ino);
		if (!inode || IS_ERR(inode)) 
			continue;
		
		if (!remove) {
			remove = inode;
			continue;
		}
		
		if (current_policy->compare(remove, inode) == inode) {
			iput(remove);
			remove = inode;
		}
		else {
			iput(inode);
		}
	}

	brelse(bh);
	return remove;
}

/**
 * #TODO: Write function to set another policy and export it.
 * 
 * #TODO: Should probably lock the current policy  while iterating over 
 * it or changing policy so that the policy cant be changed while it is in use.  
 */