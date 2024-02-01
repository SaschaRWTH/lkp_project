#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/buffer_head.h>

#include "policy.h"
#include "eviction.h"
#include "ouichefs.h"

/**
 * Functions for eviction a file.
 * 
 * Return: -1 if the eviction fails (e.g. file in use)
 */
static int evict_file(struct inode *parent, struct inode *file);

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


int dir_eviction(struct inode *dir)
{
	struct inode *remove = dir_get_file_to_evict(dir);
	if (IS_ERR(remove)) {
		long errc = PTR_ERR(remove);
		return errc; 
	}

	// Check if no files to remove could be found
	if (!remove)
		return ONLY_DIR;

	return evict_file(dir, remove);
}

/**
 * Evicts a given file. 
 * @parent: Parent directory of file.
 * @file: File to evict. 
 */
static int evict_file(struct inode *parent, struct inode *file)
{
	// TODO: Add additional checks 
	if (!parent) {
		pr_info("The given parent is NULL.\n");
		return -1;
	}
	if (!file) {
		pr_info("The given file is NULL.\n");
		return -1;
	}

	// TODO: Check if this function is actually the correct one?
	// I am quite unsure if what i am doing here is correct.
	// Get dentry from remove->i_dentry?
	// Function below uses i_dentry list, still not sure if correct,
	// but more confident. 
	// Function is NOT correct
	
	// strscpy(dblock->files[i].filename, dentry->d_name.name,
		// OUICHEFS_FILENAME_LEN);

	// Walk though all files in parent dir to find name of file
	// Read the directory index block on disk 
	// struct ouichefs_inode_info *ci = OUICHEFS_INODE(parent);
	// struct super_block *superblock = parent->i_sb;
	// struct buffer_head *bufferhead = sb_bread(superblock, ci->index_block);
	// if (!bufferhead) {
	// 	pr_warn("The buffer head could not be read.\n");
	// 	return -EIO;
	// }
	// struct ouichefs_dir_block *dblock = 
	// 	(struct ouichefs_dir_block *)bufferhead->b_data;

	// char *name = NULL;
	// // Iterate over the index block
	// for (int i = 0; i < OUICHEFS_MAX_SUBFILES; i++) {
	// 	struct ouichefs_file *f = &dblock->files[i];

	// 	if (!f->inode)
	// 		break;

	// 	if (f->inode == file->i_ino)
	// 		name = f->filename;
	// }
	// Release buffer
	// What kind of ____ abbreviation for "release" is "relse".
	// Had to google because i was unsure.
	// brelse(bufferhead); 

	// // Check if a name was found.
	// if (!name) {
	// 	pr_info("Could not find name of file.\n");
	// 	return -1;
	// }

	// pr_info("Trying to lookup '%s'.\n", name);
	struct dentry dentry = {
		.d_inode = file,
	};
	// parent->i_op->lookup(parent, &dentry, 0);

	// // Check if dentry was found.
	// if (!dentry.d_inode) {
	// 	pr_info("Could not find a dentry for name.\n");
	// 	return -1;
	// }
	int errc = parent->i_op->unlink(parent, &dentry); 
	return errc;
}

