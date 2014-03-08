#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <uio.h>
#include <syscall.h>
#include <vnode.h>
#include <vfs.h>
#include <current.h>
#include <proc.h>

#include "opt-A2.h"

#if OPT_A2

#include <synch.h>
#include <kern/limits.h>

/*
 * handler for open() system call
 * `filename` should be a string in user space.
 * See kern/fcntl.h for information on flags.
 */
int
sys_open(userptr_t filename, int flags) {
	(void)filename;
	(void)flags;
	return -1;
}

/*
 * handler for close() system call
 * TODO: Add docs here
 */
int
sys_close(int fd, int *retval) {

	struct vnode* vn;

	KASSERT(curproc != NULL);

	if((fd < 3) || (fd >= __OPEN_MAX) || (curproc->file_arr[fd] == NULL)){
		return EBADF;
	}

	vn = curproc->file_arr[fd];
	vfs_close(vn);
	curproc->file_arr[fd] = NULL;

	//success
	*retval = 0;

	return 0;
}

/*
 * handler for read() system call
 * TODO: Add docs here
 */
int
sys_read(int fdesc, userptr_t ubuf, unsigned int nbytes) {
  
  struct iovec iov;
  struct uio u;

  DEBUG(DB_SYSCALL,"Syscall: read(%d,%x,%d)\n",fdesc,(unsigned int)ubuf,nbytes);

  /* only stdout and stderr writes are currently implemented */
  if (!((fdesc==STDOUT_FILENO)||(fdesc==STDERR_FILENO))) {
    return EUNIMP;
  }
  KASSERT(curproc != NULL); // current process
  KASSERT(curproc->console != NULL);
  KASSERT(curproc->p_addrspace != NULL);

  /* set up a uio structure to refer to the user program's buffer (ubuf) */
  // read, from kernal to userspace
  
  iov.iov_ubase = ubuf;
  iov.iov_len = nbytes;
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_offset = 0;  /* not needed for the console */
  u.uio_resid = nbytes; // initialized to total amount of data
  u.uio_segflg = UIO_USERSPACE; // user process data
  u.uio_rw = UIO_READ; // from kernel to uio_seg
  u.uio_space = curproc->p_addrspace;

  res = VOP_READ(curproc->console,&u);
  if (res) {
    return res;
  }

}

#endif

/* handler for write() system call                  */
/*
 * n.b.
 * This implementation handles only writes to standard output
 * and standard error, both of which go to the console.
 * Also, it does not provide any synchronization, so writes
 * are not atomic.
 *
 * You will need to improve this implementation
 */

int
sys_write(int fdesc,userptr_t ubuf,unsigned int nbytes,int *retval)
{
  struct iovec iov;
  struct uio u;
  int res;

  DEBUG(DB_SYSCALL,"Syscall: write(%d,%x,%d)\n",fdesc,(unsigned int)ubuf,nbytes);

  /* only stdout and stderr writes are currently implemented */
  if (!((fdesc==STDOUT_FILENO)||(fdesc==STDERR_FILENO))) {
    return EUNIMP;
  }
  KASSERT(curproc != NULL);
  KASSERT(curproc->file_arr[0] != NULL);
  KASSERT(curproc->p_addrspace != NULL);

  /* set up a uio structure to refer to the user program's buffer (ubuf) */
  iov.iov_ubase = ubuf;
  iov.iov_len = nbytes;
  u.uio_iov = &iov;
  u.uio_iovcnt = 1;
  u.uio_offset = 0;  /* not needed for the console */
  u.uio_resid = nbytes;
  u.uio_segflg = UIO_USERSPACE;
  u.uio_rw = UIO_WRITE;
  u.uio_space = curproc->p_addrspace;

  res = VOP_WRITE(curproc->file_arr[0],&u);
  if (res) {
    return res;
  }

  /* pass back the number of bytes actually written */
  *retval = nbytes - u.uio_resid;
  KASSERT(*retval >= 0);
  return 0;
}
