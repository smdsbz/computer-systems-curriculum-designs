// SPDX-License-Identifier: GPL-2.0
/* System call table for x86-64. */

#include <linux/linkage.h>
#include <linux/sys.h>
#include <linux/cache.h>
#include <asm/asm-offsets.h>
#include <asm/syscall.h>
#include <linux/syscalls.h>
#include <linux/kernel.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>

/* this is a lie, but it does not hurt as sys_ni_syscall just returns -EINVAL */
extern asmlinkage long sys_ni_syscall(const struct pt_regs *);
#define __SYSCALL_64(nr, sym, qual) extern asmlinkage long sym(const struct pt_regs *);
#include <asm/syscalls_64.h>
#undef __SYSCALL_64

#define __SYSCALL_64(nr, sym, qual) [nr] = sym,

asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
	/*
	 * Smells like a compiler bug -- it doesn't work
	 * when the & below is removed.
	 */
	[0 ... __NR_syscall_max] = &sys_ni_syscall,
#include <asm/syscalls_64.h>
};


/* my new system calls, No.335 & No.336 */

asmlinkage long __xxx_sys_myhelloworld(const struct pt_regs *args) {
    printk(KERN_DEBUG "myhelloworld: 'Hello World!' from a custom "
            "system call with e/rdi = %ld, e/rdi * 2 will be returned!\n",
            args->di);
    return args->di * 2;
}

// /*
//  * NOTE: Must be invoked in userspace!
//  */
// static int __mystrlen(char *start, int maxlen) {
//     int len;
//     mm_segment_t old_fs;
//     old_fs = get_fs();
//     set_fs(KERNEL_DS);      // NOTE: Bypass kernel space address checking
//     printk(KERN_DEBUG "__mystrlen: entering __mystrlen!\n");
//     for (len = 0; start[len] != '\0' && len != maxlen; ++len) { /* pass */; }
//     if (start[len] != '\0') {
//         set_fs(old_fs);
//         return -1;
//     }
//     set_fs(old_fs);
//     printk(KERN_DEBUG "__mystrlen: returning from __mystrlen!\n");
//     return len;
// }

asmlinkage long __xxx_sys_mycopyfile(const struct pt_regs *args) {
    mm_segment_t old_fs;
    int src_fd, dst_fd;
    struct file *dst_fp;
    loff_t dst_pos;
    long ret;
    char buf[256];
    buf[255] = '\0';
    dst_pos = 0;
    /* printk(KERN_DEBUG "%s: entering mycopyfile!\n", __func__); */
    // switch to kernel space
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    // open source file
    // NOTE: Integrity of args->di and args->si will be checked by getname_kernel()
    src_fd = ksys_open((char *)args->di, O_RDONLY, 0);
    if (src_fd < 0) {
        printk(KERN_ALERT "%s: failed opening source file!\n", __func__);
        set_fs(old_fs);
        return -1;
    }
    /* printk(KERN_DEBUG "%s: successfully opened source file!\n", __func__); */
    // open / create destination file
    dst_fd = ksys_open((char *)args->si, O_WRONLY | O_CREAT, 0644);
    if (dst_fd < 0) {
        printk(KERN_ALERT "%s: failed opening / creating destination file!\n", __func__);
        ksys_close(src_fd);
        set_fs(old_fs);
        return -1;
    }
    /* printk(KERN_DEBUG "%s: successfully opened destination file!\n", __func__); */
    // convert to file *, needed by vfs_write(), for ksys_write() hasn't been exported in this context
    dst_fp = fget(dst_fd);      // NOTE: `dst_fp` is handled by `current->files` (in fs/file.c), DO NOT kfree()!
    if (!dst_fp) {
        printk(KERN_ALERT "%s: failed converting destination file descriptor to file pointer!\n", __func__);
        ksys_close(src_fd);
        ksys_close(dst_fd);
        set_fs(old_fs);
        return -1;
    }
    /* printk(KERN_DEBUG "%s: successfully converted destination file descriptor to file pointer!\n", __func__); */
    // start copying file
    while ((ret = ksys_read(src_fd, buf, 255)) > 0) {
        buf[ret] = '\0';
        vfs_write(dst_fp, buf, ret, &dst_pos);
    }
/* __mycopyfile_succend: */
    // release resources and unreference
    fput(dst_fp);   // release reference to dst_fd, previously fget()-ed in order to get dst_fp
    dst_fp = NULL;
    ksys_close(src_fd);
    ksys_close(dst_fd);
    // switch back to old space
    set_fs(old_fs);
    /* printk(KERN_DEBUG "%s: returning from mycopyfile!\n", __func__); */
    return 0;
}
