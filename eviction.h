#ifndef _OUICHEFS_EVICTION_H
#define _OUICHEFS_EVICTION_H

#define ONLY_CONTAINS_DIR 1
#define EVICTION_NOT_NECESSARY 2

int check_for_eviction(struct inode *dir);
int dir_eviction(struct inode *dir);
int trigger_eviction(struct super_block *sb);

/**
 *  istore_for_each_inode - iterates over all (alive) inodes of a inode store.
 *
 * @ino: uint32_t current inode number.
 * @sbi: superblock information of the inode
 * @block_index: index of the data block to iterate over
 */
#define istore_for_each_inode(ino, sbi, block_index) 			  \
for ((ino) = find_next_zero_bit((sbi)->ifree_bitmap,  			  \
			(sbi)->nr_inodes, 				  \
			((inode_block) - 1) * OUICHEFS_INODES_PER_BLOCK); \
     ((ino) < (block_index) * OUICHEFS_INODES_PER_BLOCK) && 		  \
     ((ino) < (sbi)->nr_inodes); 					  \
     (ino) = find_next_zero_bit(sbi->ifree_bitmap, 			  \
			      sbi->nr_inodes, 				  \
			      ino + 1))

#endif /*_OUICHEFS_EVICTION_H*/