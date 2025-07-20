#ifndef KERNEL_CPU_H_
#define KERNEL_CPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>


void atender_kernel_cpu_interrupt(t_log* cpu_logger);
void atender_kernel_cpu_dispatch(t_log* cpu_logger);
#endif