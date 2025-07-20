#ifndef KERNEL_COMMUNICATIONS_H_
#define KERNEL_COMMUNICATIONS_H_

#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>

void* escuchar_io_kernel();
void* escuchar_cpu_dispatch();
void* escuchar_cpu_interrupt();

#endif

