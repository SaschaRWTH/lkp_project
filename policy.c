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

/*
Compares two inode and returns the one which was last-recently used.
*/ 
static struct inode *lru_compare(struct inode *node1, struct inode *node2)
{
	struct inode *lru;

	if(!node1)
		return node2;
	if(!node2)
		return node1;

	// Compare access time of nodes
	// We assure seconds are detailed enough for this check.
	if(node1->i_atime.tv_sec < node2->i_atime.tv_sec)
		lru = node1;
	else
		lru = node2;
	
	// Return inode which was least recently accessed
	return lru;
}

struct inode *get_file_to_evict(struct inode *parent)
{
	pr_info("Current eviction policy is '%s'", current_policy->name);

	// TODO IMPLEMENT
	// Search all files, starting from root
	
	// Switch function parameters to just receive superblock?
	struct super_block *sb = parent->i_sb;
	// Get root directory
	struct inode *root = ouichefs_iget(sb, 0);
	if (IS_ERR(root)) {
		pr_warn("Could not retreive root directory.\n");
		return root;
	}

	// Implement recursive search.

	return NULL;
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