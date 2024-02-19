#ifndef _OUICHEFS_POLICY_H
#define _OUICHEFS_POLICY_H

#define MAX_EVICTION_NAME 16
#define MAX_EVICTION_DESCRIPTION 256
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

	/* Comparison function used to search for file to evict.*/
	struct inode *(*compare)(struct inode *, struct inode *);
};

struct inode *get_file_to_evict(struct super_block *parent);

struct inode *dir_get_file_to_evict(struct inode *dir);

#endif /*_OUICHEFS_POLICY_H*/