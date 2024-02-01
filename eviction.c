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

	if (!parent) {
		pr_info("The given parent is NULL.\n");
		return -1;
	}
	if (!file) {
		pr_info("The given file is NULL.\n");
		return -1;
	}
	struct dentry dentry = {
		.d_inode = file,
	};



	// Use vfs_unlink instead!
	int errc = parent->i_op->unlink(parent, &dentry); 
	// TODO: Clear dentry of file from dcache
	// How? I really do not know.

	return errc;
}

static struct dentry *inode_to_dentry(struct inode *inode)
{


}