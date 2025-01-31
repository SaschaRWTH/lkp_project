/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _OUICHEFS_POLICY_H
#define _OUICHEFS_POLICY_H

#define MAX_EVICTION_NAME 16
#define MAX_EVICTION_DESCRIPTION 256

#define POLICY_ALREADY_REGISTERED 3
/**
 * Struct defining an eviction policy for the rotating fs feature.
 * The struct implments a compare function which is used to find
 * the inode to evict. The advantage is that, for additional policies, only
 * a compare function needs to be implemented instead of a complete search.
 * The disadvantage is general overhead and decreased performance.
 *
 * Additionally, comparing struct inodes instead of sturct ouichefs_inodes
 * is more inefficient but allows for more policies to be implemented.
 */
struct eviction_policy {
	/* Name of eviction policy */
	char name[MAX_EVICTION_NAME];

	/* Description of eviction policy*/
	char description[MAX_EVICTION_DESCRIPTION];

	/**
	 * @inode1: First inode to compare.
	 * @inode2: Second inode to compare.
	 *
	 * Comparison function used to search for file to evict.
	 * The function should return the inode which should be evicted.
	 * The search alogirthm will pass the current to-be-evicted inode
	 * as the first argument and the inode to compare with as the second.
	 */
	struct inode *(*compare)(struct inode *inode1, struct inode *inode2);
};

struct inode *get_file_to_evict(struct super_block *parent);

struct inode *dir_get_file_to_evict(struct inode *dir);

int register_policy(struct eviction_policy *policy);

void unregister_policy(struct eviction_policy *policy);

#endif /*_OUICHEFS_POLICY_H*/
