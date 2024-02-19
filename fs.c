// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "%s:%s: " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include "eviction.h"
#include "ouichefs.h"

/*
 * Mount a ouiche_fs partition
 */
static struct super_block *sb;

struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *dentry = NULL;

	dentry =
		mount_bdev(fs_type, flags, dev_name, data, ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	sb = dentry->d_sb;
	return dentry;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	kill_block_super(sb);

	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};


static int eviction_enabled;

static ssize_t eviction_trigger_store(struct kobject *kobj, \
struct kobj_attribute *attr, const char *buf, size_t count)
{
	int value;
	int rc = sscanf(buf, "%d", &value);
	if (rc != 1 || value <= 0) {
		pr_err("invalid value\n");
		return -EINVAL;
	}
	eviction_enabled = value;

	//TODO: Let the eviction run
	// Print address of the superblock
	pr_info("Superblock address: %p\n", sb);
	trigger_eviction(sb);

	eviction_enabled = 0;
	return count;
}

static ssize_t eviction_trigger_show(struct kobject *kobj, \
struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "Eviction_trigger is %s running\n", \
	eviction_enabled ? "" : "not");
}



static struct kobj_attribute eviction_trigger_attr = __ATTR(eviction_enabled, \
0644, eviction_trigger_show, eviction_trigger_store);

static struct kobject *eviction_trigger_kobject;

static int __init ouichefs_init(void)
{
	int ret;
	int retval;

	eviction_trigger_kobject = kobject_create_and_add("eviction", kernel_kobj);
	if(!eviction_trigger_kobject)
    		goto error_init_1;
	
	retval = sysfs_create_file(eviction_trigger_kobject, \
	&eviction_trigger_attr.attr);
	if(retval)
		goto error_init_2;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto err;
	}

	ret = register_filesystem(&ouichefs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto err_inode;
	}

	pr_info("module loaded\n");
	return 0;
error_init_2:
	pr_err("sysfs_create_file() failed\n");
	kobject_put(eviction_trigger_kobject);	
error_init_1:
	pr_err("kobject_create_and_add() failed\n");
	return -ENOMEM;
err_inode:
	ouichefs_destroy_inode_cache();
err:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;
	kobject_put(eviction_trigger_kobject);

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();

	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@rwth-aachen.de>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
