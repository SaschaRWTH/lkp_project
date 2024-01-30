#include <linux/kernel.h>
#include <linux/fs.h>

#include "policy.h"

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
	// TODO IMPLEMENT LEAST RECENTLY USED COMPARE
	return NULL;
}

struct inode *get_file_to_evict(struct inode *parent, struct dentry *dentry)
{
	pr_info("Current eviction policy is '%s'", current_policy->name);
	// TODO IMPLEMENT
	return NULL;
}
struct inode *dir_get_file_to_evict(struct inode *parent, struct dentry *dentry)
{
	pr_info("Current eviction policy is '%s'", current_policy->name);
	// TODO IMPLEMENT
	return NULL;
}