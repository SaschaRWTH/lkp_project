#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>

#include "policy.h"
#include "ouichefs.h"

static struct dentry *lru_compare(struct dentry *dentry1,\
				  struct dentry *dentry2);
static struct eviction_policy least_recently_used_policy = {
	.name = "LRU Policy",
	.description = "Evicts least-recently used file.",
	.compare = lru_compare,
};

static struct eviction_policy *current_policy = &least_recently_used_policy;

/*
Compares two inode and returns the one which was last-recently used.
*/ 
static struct dentry *lru_compare(struct dentry *dentry1,\
				  struct dentry *dentry2)
{
	struct dentry *lru;

	// TODO: Add more safety checks
	// e.g. checks for NULL and similar

	if (!dentry1 || !dentry1->d_inode) 
		return dentry2;

	if (!dentry2 || !dentry2->d_inode) 
		return dentry1;

	// Compare access time of nodes
	// We assure seconds are detailed enough for this check.
	if (dentry1->d_inode->i_atime.tv_sec < dentry2->d_inode->i_atime.tv_sec)
		lru = dentry1;
	else
		lru = dentry2;
	
	// Return inode which was least recently accessed
	return lru;
}

struct inode *get_file_to_evict(struct inode *parent)
{
	pr_info("Using '%s' policy to find file to evict.\n",\
		current_policy->name);

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
struct dentry *dir_get_file_to_evict(struct dentry *dir)
{	
	//TODO: Lock dentry?

	pr_info("Using '%s' policy to find file to evict. (dir)\n",\
		current_policy->name);

	// Check if given dir is null.
	if(!dir) {
		pr_warn("The given inode was NULL.\n");
		return NULL;
	}

	// Check that dir is a directory 
	if (!S_ISDIR(dir->d_inode->i_mode)) {
		pr_warn("The given inode was not a directory.\n");
		return ERR_PTR(-ENOTDIR);
	}

	struct dentry *remove = NULL;
	struct dentry *current_dentry;
	list_for_each_entry(current_dentry, &dir->d_subdirs, d_child) {
		pr_info("Current file is '%s'.\n", current_dentry->d_name.name);
		if (!remove)
			remove = current_dentry;
		else
			remove = current_policy->compare(remove,\
							 current_dentry);
		pr_info("rm is now '%s'.\n", remove->d_name.name);
		
	}

	// // Read the directory index block on disk 
	// struct ouichefs_inode_info *ci = OUICHEFS_INODE(dir);
	// struct super_block *superblock = dir->i_sb;
	// struct buffer_head *bufferhead = sb_bread(superblock, ci->index_block);
	// if (!bufferhead) {
	// 	pr_warn("The buffer head could not be read.\n");
	// 	return ERR_PTR(-EIO);
	// }
	// struct ouichefs_dir_block *dblock = (struct ouichefs_dir_block *)bufferhead->b_data;


	// // Iterate over the index block
	// for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
	// 	struct ouichefs_file *f = &dblock->files[i];

	// 	// Get inode struct from superblock
	// 	struct inode *inode = ouichefs_iget(superblock, f->inode);;

	// 	// Check till first null inode 
	// 	if (!inode) {
	// 		pr_warn("The directory is not full.\n");
	// 		break;
	// 	}

	// 	// Check that the node is a file
	// 	if(!S_ISREG(inode->i_mode))
	// 		continue;

	// 	if(!remove)
	// 		remove = inode;
	// 	else	// add check for ERRPTR ??  
	// 		remove = current_policy->compare(remove, inode);
	// }

	// Release buffer
	// What kind of ____ abbreviation for "release" is "relse".
	// Had to google because i was unsure.
	// brelse(bufferhead);

	pr_info("Returning file '%s'.\n", remove->d_name.name);
	return remove;
}