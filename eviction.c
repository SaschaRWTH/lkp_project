#include <linux/kernel.h>
#include <linux/fs.h>

#include "policy.h"
#include "eviction.h"

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

int dir_eviction(void /*function parameters here*/)
{
	// TODO: IMPLEMENT DIR_EVICTION
	return -1;
}

/* 
Functions for eviction a file. 

Returns -1 if the eviction fails (e.g. file in use)
*/
static int evict_file(struct inode *dir, struct dentry *dentry
		      /*additional parameters for eviction*/)
{
	// TODO: IMPLEMENT DIR_EVICTION
	// Call inode.ouichefs_unlink(...) ?
}

