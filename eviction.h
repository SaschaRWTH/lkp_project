#ifndef _OUICHEFS_EVICTION_H
#define _OUICHEFS_EVICTION_H

#define ONLY_DIR 1

/**
 * dir_eviction - Eviction that is triggered when a node is created in a full 
 * directory.
 * @dir: Directory from which to evict.
 * 
 * Return: 0 if the directory eviction could be performed.
 */
int dir_eviction(struct inode *dir);

#endif /*_OUICHEFS_EVICTION_H*/