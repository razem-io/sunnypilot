#pragma once
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/ion.h>
#include "msm_ion.h"
static void check(int rc, const char *what) {
  if (rc < 0) throw std::runtime_error(std::string(what)+": "+strerror(errno));
}
static void call(int fd, unsigned long op, void *arg, const char *what) {
  int rc; do {rc=ioctl(fd,op,arg);} while(rc<0 && errno==EINTR); check(rc,what);
}
struct Ion {
  int owner,fd=-1; ion_user_handle_t handle=0; size_t size; void *ptr=MAP_FAILED;
  Ion(int owner, size_t size):owner(owner),size(size) {
    ion_allocation_data a{}; a.len=size+8; a.align=4096;
    a.heap_id_mask=1U<<ION_IOMMU_HEAP_ID; a.flags=ION_FLAG_CACHED;
    call(owner,ION_IOC_ALLOC,&a,"ION_ALLOC"); handle=a.handle;
    ion_fd_data d{}; d.handle=handle; call(owner,ION_IOC_SHARE,&d,"ION_SHARE"); fd=d.fd;
    ptr=mmap(nullptr,size+8,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    if(ptr==MAP_FAILED) throw std::runtime_error("ION mmap");
    memset(ptr,0,size);
  }
  void sync(bool from) {
    ion_flush_data f{}; f.handle=handle; f.vaddr=ptr; f.length=size;
    ion_custom_data c{}; c.cmd=from?ION_IOC_INV_CACHES:ION_IOC_CLEAN_CACHES;
    c.arg=(unsigned long)&f; call(owner,ION_IOC_CUSTOM,&c,"ION_SYNC");
  }
  ~Ion(){if(ptr!=MAP_FAILED)munmap(ptr,size+8); if(fd>=0)close(fd);
    if(handle){ion_handle_data h{handle}; ioctl(owner,ION_IOC_FREE,&h);}}
};
