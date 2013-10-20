/*
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009	   Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _NULLFS_H_
#define _NULLFS_H_

#include <linux/dcache.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <linux/seq_file.h>
#include <linux/statfs.h>
#include <linux/fs_stack.h>
#include <linux/magic.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/sched.h>

/* the file system name */
#define NULLFS_NAME "nullfs"

/* nullfs root inode number */
#define NULLFS_ROOT_INO     1

/* useful for tracking code reachability */
#define UDBG printk(KERN_DEFAULT "DBG:%s:%s:%d\n", __FILE__, __func__, __LINE__)

/* operations vectors defined in specific files */
extern const struct file_operations nullfs_main_fops;
extern const struct file_operations nullfs_dir_fops;
extern const struct inode_operations nullfs_main_iops;
extern const struct inode_operations nullfs_dir_iops;
extern const struct inode_operations nullfs_symlink_iops;
extern const struct super_operations nullfs_sops;
extern const struct dentry_operations nullfs_dops;
extern const struct address_space_operations nullfs_aops, nullfs_dummy_aops;
extern const struct vm_operations_struct nullfs_vm_ops;

extern int nullfs_init_inode_cache(void);
extern void nullfs_destroy_inode_cache(void);
extern int nullfs_init_dentry_cache(void);
extern void nullfs_destroy_dentry_cache(void);
extern int new_dentry_private_data(struct dentry *dentry);
extern void free_dentry_private_data(struct dentry *dentry);
extern int init_lower_nd(struct nameidata *nd, unsigned int flags);
extern struct dentry *nullfs_lookup(struct inode *dir, struct dentry *dentry,
				    unsigned int flags);
extern struct inode *nullfs_iget(struct super_block *sb,
				 struct inode *lower_inode);
extern int nullfs_interpose(struct dentry *dentry, struct super_block *sb,
			    struct path *lower_path);

/* file private data */
struct nullfs_file_info {
	struct file *lower_file;
	const struct vm_operations_struct *lower_vm_ops;
};

/* nullfs inode data in memory */
struct nullfs_inode_info {
	struct inode *lower_inode;
	struct inode vfs_inode;
};

/* nullfs dentry data in memory */
struct nullfs_dentry_info {
	spinlock_t lock;	/* protects lower_path */
	struct path lower_path;
};

/* nullfs super-block data in memory */
struct nullfs_sb_info {
	struct super_block *lower_sb;
};

/*
 * inode to private data
 *
 * Since we use containers and the struct inode is _inside_ the
 * nullfs_inode_info structure, NULLFS_I will always (given a non-NULL
 * inode pointer), return a valid non-NULL pointer.
 */
static inline struct nullfs_inode_info *NULLFS_I(const struct inode *inode)
{
	return container_of(inode, struct nullfs_inode_info, vfs_inode);
}

/* dentry to private data */
#define NULLFS_D(dent) ((struct nullfs_dentry_info *)(dent)->d_fsdata)

/* superblock to private data */
#define NULLFS_SB(super) ((struct nullfs_sb_info *)(super)->s_fs_info)

/* file to private Data */
#define NULLFS_F(file) ((struct nullfs_file_info *)((file)->private_data))

/* file to lower file */
static inline struct file *nullfs_lower_file(const struct file *f)
{
	return NULLFS_F(f)->lower_file;
}

static inline void nullfs_set_lower_file(struct file *f, struct file *val)
{
	NULLFS_F(f)->lower_file = val;
}

/* inode to lower inode. */
static inline struct inode *nullfs_lower_inode(const struct inode *i)
{
	return NULLFS_I(i)->lower_inode;
}

static inline void nullfs_set_lower_inode(struct inode *i, struct inode *val)
{
	NULLFS_I(i)->lower_inode = val;
}

/* superblock to lower superblock */
static inline struct super_block *nullfs_lower_super(
	const struct super_block *sb)
{
	return NULLFS_SB(sb)->lower_sb;
}

static inline void nullfs_set_lower_super(struct super_block *sb,
					  struct super_block *val)
{
	NULLFS_SB(sb)->lower_sb = val;
}

/* path based (dentry/mnt) macros */
static inline void pathcpy(struct path *dst, const struct path *src)
{
	dst->dentry = src->dentry;
	dst->mnt = src->mnt;
}
/* Returns struct path.  Caller must path_put it. */
static inline void nullfs_get_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&NULLFS_D(dent)->lock);
	pathcpy(lower_path, &NULLFS_D(dent)->lower_path);
	path_get(lower_path);
	spin_unlock(&NULLFS_D(dent)->lock);
	return;
}
static inline void nullfs_put_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	path_put(lower_path);
	return;
}
static inline void nullfs_set_lower_path(const struct dentry *dent,
					 struct path *lower_path)
{
	spin_lock(&NULLFS_D(dent)->lock);
	pathcpy(&NULLFS_D(dent)->lower_path, lower_path);
	spin_unlock(&NULLFS_D(dent)->lock);
	return;
}
static inline void nullfs_reset_lower_path(const struct dentry *dent)
{
	spin_lock(&NULLFS_D(dent)->lock);
	NULLFS_D(dent)->lower_path.dentry = NULL;
	NULLFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&NULLFS_D(dent)->lock);
	return;
}
static inline void nullfs_put_reset_lower_path(const struct dentry *dent)
{
	struct path lower_path;
	spin_lock(&NULLFS_D(dent)->lock);
	pathcpy(&lower_path, &NULLFS_D(dent)->lower_path);
	NULLFS_D(dent)->lower_path.dentry = NULL;
	NULLFS_D(dent)->lower_path.mnt = NULL;
	spin_unlock(&NULLFS_D(dent)->lock);
	path_put(&lower_path);
	return;
}

/* locking helpers */
static inline struct dentry *lock_parent(struct dentry *dentry)
{
	struct dentry *dir = dget_parent(dentry);
	mutex_lock_nested(&dir->d_inode->i_mutex, I_MUTEX_PARENT);
	return dir;
}

static inline void unlock_dir(struct dentry *dir)
{
	mutex_unlock(&dir->d_inode->i_mutex);
	dput(dir);
}
#endif	/* not _NULLFS_H_ */
