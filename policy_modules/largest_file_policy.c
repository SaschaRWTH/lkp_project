#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>

#include "../policy.h"
#include "../ouichefs.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sascha Thiemann");
MODULE_DESCRIPTION("Largest File Policy Module");

static struct inode *lf_compare(struct inode *, struct inode *);
static struct eviction_policy file_size_policy = {
	.name = "LF Policy",
	.description = "Evicts the largest file.",
	.compare = lf_compare,
};

/**
 * lf_compare - Compares two inodes and returns the largest.
 *
 * @first: First node to compare.
 * @second: Second node to compare.
 *
 * Return: The node with larger size, NULL if both nodes are NULL.
 */
static struct inode *lf_compare(struct inode *first, struct inode *second)
{
	if (!first)
		return second;
	if (!second)
		return first;

	/* Compare size of files */
	if (first->i_size < second->i_size)
		return second;
	else
		return first;
}

static int __init largest_file_policy_init(void)
{
	int errc = register_policy(&file_size_policy);

	return errc;
}
module_init(largest_file_policy_init);

static void __exit largest_file_policy_exit(void)
{
	unregister_policy(&file_size_policy);
}
module_exit(largest_file_policy_exit);


