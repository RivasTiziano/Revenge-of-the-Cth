#ifndef IO_H_
#define IO_H_

#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>
#include "kernel_io.h"

/* VARIABLES GLOBALES */
extern t_log* io_logger;
extern t_config* io_config;

// Para las conexiones //
extern char* IP_KERNEL;
extern char* PUERTO_KERNEL;

// Para otras cositas 
extern char* LOG_LEVEL;
extern char* nombre_interfaz;

// File Descriptor de sockets
extern int socket_kernel;


#endif