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

#include "nullfs.h"

struct inode *nullfs_iget(struct super_block *sb, struct inode *lower_inode)
{
	struct wrapfs_inode_info *info;
	struct inode *inode; /* the new inode to return */
	int err;

	inode = iget5_locked(sb, /* our superblock */
			     /*
			      * hashval: we use inode number, but we can
			      * also use "(unsigned long)lower_inode"
			      * instead.
			      */
			     lower_inode->i_ino, /* hashval */
			     0,	/* inode comparison function */
			     0, /* inode init function */
			     lower_inode); /* data passed to test+set fxns */
	if (!inode) {
		err = -EACCES;
		iput(lower_inode);
		return ERR_PTR(err);
	}
	/* if found a cached inode, then just return it */
	//if (!(inode->i_state & I_NEW))
	//	return inode;

	/* initialize new inode */
	info = NULLFS_I(inode);

	inode->i_ino = lower_inode->i_ino;
	if (!igrab(lower_inode)) {
		err = -ESTALE;
		return ERR_PTR(err);
	}
	nullfs_set_lower_inode(inode, lower_inode);

	inode->i_version++;

	/* use different set of inode ops for symlinks & directories */
	/*
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_op = &wrapfs_dir_iops;
	else if (S_ISLNK(lower_inode->i_mode))
		inode->i_op = &wrapfs_symlink_iops;
	else
*/
		inode->i_op = &nullfs_main_iops;
	/* use different set of file ops for directories */
	/*
	if (S_ISDIR(lower_inode->i_mode))
		inode->i_fop = &wrapfs_dir_fops;
	else
		inode->i_fop = &wrapfs_main_fops;

	inode->i_mapping->a_ops = &wrapfs_aops;
*/
	inode->i_atime.tv_sec = 0;
	inode->i_atime.tv_nsec = 0;
	inode->i_mtime.tv_sec = 0;
	inode->i_mtime.tv_nsec = 0;
	inode->i_ctime.tv_sec = 0;
	inode->i_ctime.tv_nsec = 0;

	/* properly initialize special inodes */
	if (S_ISBLK(lower_inode->i_mode) || S_ISCHR(lower_inode->i_mode) ||
	    S_ISFIFO(lower_inode->i_mode) || S_ISSOCK(lower_inode->i_mode))
		init_special_inode(inode, lower_inode->i_mode,
				   lower_inode->i_rdev);

	/* all well, copy inode attributes */
	fsstack_copy_attr_all(inode, lower_inode);
	fsstack_copy_inode_size(inode, lower_inode);

	unlock_new_inode(inode);
	return inode;
}

static int nullfs_permission(struct inode *inode, int mask)
{
	struct inode *lower_inode;
	int err;
	UDBG;
	lower_inode = nullfs_lower_inode(inode);
	err = inode_permission(lower_inode, mask);
	return err;
}


const struct inode_operations nullfs_main_iops = {
	.permission	= nullfs_permission,
};
