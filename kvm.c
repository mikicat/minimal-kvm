#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define RAM_SIZE 0x10000

int main(void) {
    // KVM Layer
    int kvm_fd = open("/dev/kvm", O_RDWR);
    int version = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    printf("KVM version: %d\n", version);

    // Create VM
    int vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);

    // Create VM Memory
    void *mem = mmap(NULL, RAM_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    struct kvm_userspace_memory_region region = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = RAM_SIZE,
        .userspace_addr = (uintptr_t) mem,
    };
    ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &region);

    // Create VCPU
    int vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    
    // Load VM
    int bin_fd = open("guest.bin", O_RDONLY);
    if (bin_fd < 0) {
        fprintf(stderr, "can not open binary file: %d\n", errno);
        return 1;
    }

    char *p = (char *)mem;
    for (;;) {
        int r = read(bin_fd, p, 4096);
        if (r <= 0) break;
        p += r;
    }
    close(bin_fd);

    struct kvm_sregs sregs;
    ioctl(vcpu_fd, KVM_GET_SREGS, &sregs);
    // Init selector and base with 0s
    sregs.cs.selector = sregs.cs.base = sregs.ss.selector = sregs.ss.base = sregs.ds.selector = sregs.ds.base = sregs.es.selector = sregs.es.base = sregs.fs.selector = sregs.fs.base = sregs.gs.selector = 0;
    // Save special registers
    ioctl(vcpu_fd, KVM_SET_SREGS, &sregs);

    // Init and save normal regs
    struct kvm_regs regs;
    regs.rflags = 2; // bit 1 set to 1 in EFLAGS and RFLAGS
    regs.rip = 0; // Code starts at @ 0x0
    ioctl(vcpu_fd, KVM_SET_REGS, &regs);

    // Run
    int runsz = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    struct kvm_run *run = (struct kvm_run *) mmap(NULL, runsz, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);

    for (;;) {
        ioctl(vcpu_fd, KVM_RUN, 0);
        switch (run->exit_reason) {
            case KVM_EXIT_IO:
                printf("IO port: %x, data: %x\n", run->io.port, *(int *)((char *)(run) + run->io.data_offset));
                break;
            case KVM_EXIT_SHUTDOWN:
                return 0;
        }
    }

    return 0;
}
