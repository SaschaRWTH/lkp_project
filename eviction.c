#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/dcache.h>

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

	// TODO: Check if this function is actually the correct one?
	// I am quite unsure if what i am doing here is correct.
	// Get dentry from remove->i_dentry?
	// Function below uses i_dentry list, still not sure if correct,
	// but more confident. 
	struct dentry *dentry = d_find_alias(file);
	
	return parent->i_op->unlink(parent, dentry);
}

