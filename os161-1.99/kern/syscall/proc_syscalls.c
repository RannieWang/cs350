#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <machine/trapframe.h>
#include "opt-A2.h"

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  #if OPT_A2

  pid_setExitCode(p->p_pid,exitcode);
  pid_setisExited(p->p_pid,true);
  struct semaphore *sem = pid_getsem(p->p_pid);
  V(sem);
  #else

  (void)exitcode;
  #endif /*OPT_A2*/

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  
  *retval = curproc->p_pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  #if OPT_A2

  if (!pidExist(pid)){
    return ESRCH;
  }  

  if(getParentPid(pid)!=curproc->p_pid){
    return ECHILD;
  }

  if(status == NULL){
    return EFAULT;
  }

  struct semaphore *sem = pid_getsem(pid);

  P(sem);
  exitstatus = _MKWAIT_EXIT(pid_getExitCode(pid));

  #else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  #endif

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}



void thread_fork_init(void * ptr, unsigned long val);
void thread_fork_init(void * ptr, unsigned long val){
  //Stop the compiler from complaining
  (void)val;

  enter_forked_process((struct trapframe *) ptr);

}
pid_t sys_fork(struct trapframe *tf, pid_t *retval){
    int result; 

    struct trapframe *temp = kmalloc(sizeof(struct trapframe));
    if (temp == NULL){
        return ENOMEM;
    }



    struct proc * child = proc_create_runprogram(curproc->p_name);
    if(child == NULL) {
        kfree(temp);
        return ENOMEM;
    }    

    *temp = *tf; // copied

    result = as_copy(curproc->p_addrspace, &child->p_addrspace);
    if(result){
        kfree(temp);
        proc_destroy(child);
        return result;
    }

    setParentPid(child->p_pid, curproc->p_pid);

    result = thread_fork(curthread->t_name, child, thread_fork_init, temp,0);

    if (result){
        as_destroy(child->p_addrspace);
        proc_destroy(child);
        kfree(temp);
        return result;
    }

    *retval = child->p_pid;
    return 0;

}

