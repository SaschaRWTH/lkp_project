#ifndef _OUICHEFS_EVICTION_H
#define _OUICHEFS_EVICTION_H

#define ONLY_CONTAINS_DIR 1
#define EVICTION_NOT_NECESSARY 2

int check_for_eviction(struct inode *dir);
int dir_eviction(struct inode *dir);

#endif /*_OUICHEFS_EVICTION_H*/