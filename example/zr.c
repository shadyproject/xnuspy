#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <mach-o/getsect.h>

#include "xnuspy_ctl.h"

static long SYS_xnuspy_ctl = 0;

static long i = 0;

static void (*kprintf)(const char *, ...);

static void (*zone_require_orig)(void *addr, void *expected_zone);

static void zone_require(void *addr, void *expected_zone){
    uint64_t mpidr_el1;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr_el1));
    uint8_t cpuid = mpidr_el1 & 0xff;

    uint64_t caller = (uint64_t)__builtin_return_address(0);

    char *zname = *(char **)((uint8_t *)expected_zone + 0x120);

    kprintf("%d: CPU %d, caller %#llx: zone_require called with addr %#llx,"
            " expected zone", i, cpuid, caller, addr);

    i++;

    if(zname)
        kprintf(" '%s'\n", zname);
    else
        kprintf(" %#llx\n", expected_zone);

    zone_require_orig(addr, expected_zone);
}

static void *t(void *arg){
    for(;;){
        printf("%s: i = %ld\n", __func__, i);
        sleep(1);
    }

    return NULL;
}

static int number = 0x55667788;

int main(int argc, char **argv){
    size_t oldlen = sizeof(long);
    int ret = sysctlbyname("kern.xnuspy_ctl_callnum", &SYS_xnuspy_ctl,
            &oldlen, NULL, 0);

    if(ret == -1){
        printf("sysctlbyname with kern.xnuspy_ctl_callnum failed: %s\n",
                strerror(errno));
        return 1;
    }

    syscall(SYS_xnuspy_ctl, XNUSPY_GET_FUNCTION, KPRINTF, &kprintf, 0);

    printf("got kprintf @ %#llx\n", kprintf);

    /* printf("NOT INSTALLING HOOK\n"); */


    /* printf("number: %#x\n", __func__); */
    syscall(SYS_xnuspy_ctl, XNUSPY_INSTALL_HOOK, 0xFFFFFFF007C4B420,
            zone_require, &zone_require_orig);

    extern struct mach_header_64 *_mh_execute_header;
    printf("%#llx: %#x\n", &_mh_execute_header, *(uint32_t *)(&_mh_execute_header));

    printf("zone_require_orig = %#llx\n", zone_require_orig);


    /* printf("    program text (etext)      %10p\n", (void*)get_etext()); */
    /* printf("    initialized data (edata)  %10p\n", (void*)get_edata()); */
    /* printf("    uninitialized data (end)  %10p\n", (void*)get_end()); */

    /* extern char  etext, edata, end; */
    /* printf("%#llx %#llx %#llx\n", &etext, &edata, &end); */
    /* extern void *_DATA; */
    /* printf("DATA: %#llx\n", _DATA); */
    /* syscall(SYS_xnuspy_ctl, XNUSPY_INSTALL_HOOK, 0xFFFFFFF007C4B420, */
    /*         main, &zone_require_orig); */

    /* printf("zone_require_orig = %#llx\n", zone_require_orig); */

    pthread_t pt;
    pthread_create(&pt, NULL, t, NULL);

    printf("Ctrl C to quit\n");
    getchar();

    printf("Goodbye\n");

    return 0;
}