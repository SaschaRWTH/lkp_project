#include <linux/kernel.h>
#include <linux/fs.h>

#include "policy.h"
#include "eviction.h"

/* 
Functions for eviction a file. 

Returns -1 if the eviction fails (e.g. file in use)
*/
static int evict_file(struct inode *dir);

/*
Percentage threshold at which the eviction of a file is triggered.
*/
const u16 eviction_threshhold = 95;

/* 
Evition that is triggered when a certin threshold is met.
Evicts a file from the partition based on the current policy.

Returns -1 if the eviction fails (e.g. file in use)
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

	return evict_file(remove);
}


static int evict_file(struct inode *dir)
{
	// TODO: IMPLEMENT DIR_EVICTION
	// Call inode.ouichefs_unlink(...) ?
	return -1;
}

