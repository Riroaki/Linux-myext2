/*
 *  linux/fs/myext2/file.c
 *
 * Copyright (C) 1992, 1993, 1994, 1995
 * Remy Card (card@masi.ibp.fr)
 * Laboratoire MASI - Institut Blaise Pascal
 * Universite Pierre et Marie Curie (Paris VI)
 *
 *  from
 *
 *  linux/fs/minix/file.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  myext2 fs regular file handling primitives
 *
 *  64-bit file support on 64-bit platforms by Jakub Jelinek
 * 	(jj@sunsite.ms.mff.cuni.cz)
 */

#include <linux/uio.h>		/* Modified codes. */

#include <linux/time.h>
#include <linux/pagemap.h>
#include <linux/dax.h>
#include <linux/quotaops.h>
#include "myext2.h"
#include "xattr.h"
#include "acl.h"

static ssize_t new_sync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
        struct iovec iov = { .iov_base = (void __user *)buf, .iov_len = len };
        struct kiocb kiocb;
        struct iov_iter iter;
        ssize_t ret;

        init_sync_kiocb(&kiocb, filp);
        kiocb.ki_pos = *ppos;
        iov_iter_init(&iter, WRITE, &iov, 1, len);

        ret = filp->f_op->write_iter(&kiocb, &iter);
        BUG_ON(ret == -EIOCBQUEUED);
        if (ret > 0)
                *ppos = kiocb.ki_pos;
        return ret;
}
static ssize_t new_sync_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
        struct iovec iov = { .iov_base = buf, .iov_len = len };
        struct kiocb kiocb;
        struct iov_iter iter;
        ssize_t ret;

        init_sync_kiocb(&kiocb, filp);
        kiocb.ki_pos = *ppos;
        iov_iter_init(&iter, READ, &iov, 1, len);

        ret = filp->f_op->read_iter(&kiocb, &iter);
        BUG_ON(ret == -EIOCBQUEUED);
        *ppos = kiocb.ki_pos;
        return ret;
}
static 
ssize_t new_sync_write_crypt(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	int i;
	char* mybuf = (char*)kmalloc(sizeof(char)*len,GFP_KERNEL);
	copy_from_user(mybuf,buf,len);
	for(i=0;i<len;i++){
		mybuf[i] = (mybuf[i] + 25)%128;
	}
	copy_to_user(buf, mybuf, len);
	printk("haha encrypt %ld\n", len);
	return new_sync_write(filp, buf, len, ppos);
}

static ssize_t new_sync_read_crypt(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
        int i;
	char* mybuf = (char*)kmalloc(sizeof(char)*len,GFP_KERNEL);
        ssize_t ret = new_sync_read(filp, buf, len, ppos);
	copy_from_user(mybuf, buf, len);
	for(i=0;i<len;i++){
		mybuf[i] = (mybuf[i] - 25 + 128)%128;
	}
	copy_to_user(buf, mybuf, len);
	printk("haha decrypt %ld\n", len);
        return ret;
}

#ifdef CONFIG_FS_DAX
/*
 * The lock ordering for myext2 DAX fault paths is:
 *
 * mmap_sem (MM)
 *   sb_start_pagefault (vfs, freeze)
 *     myext2_inode_info->dax_sem
 *       address_space->i_mmap_rwsem or page_lock (mutually exclusive in DAX)
 *         myext2_inode_info->truncate_mutex
 *
 * The default page_lock and i_size verification done by non-DAX fault paths
 * is sufficient because myext2 doesn't support hole punching.
 */
static int myext2_dax_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct inode *inode = file_inode(vma->vm_file);
	struct myext2_inode_info *ei = MYEXT2_I(inode);
	int ret;

	if (vmf->flags & FAULT_FLAG_WRITE) {
		sb_start_pagefault(inode->i_sb);
		file_update_time(vma->vm_file);
	}
	down_read(&ei->dax_sem);

	ret = dax_fault(vma, vmf, myext2_get_block);

	up_read(&ei->dax_sem);
	if (vmf->flags & FAULT_FLAG_WRITE)
		sb_end_pagefault(inode->i_sb);
	return ret;
}

static int myext2_dax_pmd_fault(struct vm_area_struct *vma, unsigned long addr,
						pmd_t *pmd, unsigned int flags)
{
	struct inode *inode = file_inode(vma->vm_file);
	struct myext2_inode_info *ei = MYEXT2_I(inode);
	int ret;

	if (flags & FAULT_FLAG_WRITE) {
		sb_start_pagefault(inode->i_sb);
		file_update_time(vma->vm_file);
	}
	down_read(&ei->dax_sem);

	ret = dax_pmd_fault(vma, addr, pmd, flags, myext2_get_block);

	up_read(&ei->dax_sem);
	if (flags & FAULT_FLAG_WRITE)
		sb_end_pagefault(inode->i_sb);
	return ret;
}

static int myext2_dax_pfn_mkwrite(struct vm_area_struct *vma,
		struct vm_fault *vmf)
{
	struct inode *inode = file_inode(vma->vm_file);
	struct myext2_inode_info *ei = MYEXT2_I(inode);
	loff_t size;
	int ret;

	sb_start_pagefault(inode->i_sb);
	file_update_time(vma->vm_file);
	down_read(&ei->dax_sem);

	/* check that the faulting page hasn't raced with truncate */
	size = (i_size_read(inode) + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (vmf->pgoff >= size)
		ret = VM_FAULT_SIGBUS;
	else
		ret = dax_pfn_mkwrite(vma, vmf);

	up_read(&ei->dax_sem);
	sb_end_pagefault(inode->i_sb);
	return ret;
}

static const struct vm_operations_struct myext2_dax_vm_ops = {
	.fault		= myext2_dax_fault,
	.pmd_fault	= myext2_dax_pmd_fault,
	.page_mkwrite	= myext2_dax_fault,
	.pfn_mkwrite	= myext2_dax_pfn_mkwrite,
};

static int myext2_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	if (!IS_DAX(file_inode(file)))
		return generic_file_mmap(file, vma);

	file_accessed(file);
	vma->vm_ops = &myext2_dax_vm_ops;
	vma->vm_flags |= VM_MIXEDMAP | VM_HUGEPAGE;
	return 0;
}
#else
#define myext2_file_mmap	generic_file_mmap
#endif

/*
 * Called when filp is released. This happens when all file descriptors
 * for a single struct file are closed. Note that different open() calls
 * for the same file yield different struct file structures.
 */
static int myext2_release_file (struct inode * inode, struct file * filp)
{
	if (filp->f_mode & FMODE_WRITE) {
		mutex_lock(&MYEXT2_I(inode)->truncate_mutex);
		myext2_discard_reservation(inode);
		mutex_unlock(&MYEXT2_I(inode)->truncate_mutex);
	}
	return 0;
}

int myext2_fsync(struct file *file, loff_t start, loff_t end, int datasync)
{
	int ret;
	struct super_block *sb = file->f_mapping->host->i_sb;
	struct address_space *mapping = sb->s_bdev->bd_inode->i_mapping;

	ret = generic_file_fsync(file, start, end, datasync);
	if (ret == -EIO || test_and_clear_bit(AS_EIO, &mapping->flags)) {
		/* We don't really know where the IO error happened... */
		myext2_error(sb, __func__,
			   "detected IO error when writing metadata buffers");
		ret = -EIO;
	}
	return ret;
}

/*
 * We have mostly NULL's here: the current defaults are ok for
 * the myext2 filesystem.
 */
const struct file_operations myext2_file_operations = {
	.read		= new_sync_read_crypt,
	.write		= new_sync_write_crypt,
	.llseek		= generic_file_llseek,
	.read_iter	= generic_file_read_iter,
	.write_iter	= generic_file_write_iter,
	.unlocked_ioctl = myext2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= myext2_compat_ioctl,
#endif
	.mmap		= myext2_file_mmap,
	.open		= dquot_file_open,
	.release	= myext2_release_file,
	.fsync		= myext2_fsync,
	.splice_read	= generic_file_splice_read,
	.splice_write	= iter_file_splice_write,
};

const struct inode_operations myext2_file_inode_operations = {
#ifdef CONFIG_MYEXT2_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= myext2_listxattr,
	.removexattr	= generic_removexattr,
#endif
	.setattr	= myext2_setattr,
	.get_acl	= myext2_get_acl,
	.set_acl	= myext2_set_acl,
	.fiemap		= myext2_fiemap,
};
