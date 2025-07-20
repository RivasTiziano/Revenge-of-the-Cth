#include "io.h"

/* VARIABLES GLOBALES */
t_log* io_logger = NULL;
t_config* io_config = NULL;

// Para las conexiones //
char* IP_KERNEL = NULL;
char* PUERTO_KERNEL = NULL;

// Para otras cositas 
char* LOG_LEVEL = NULL;
char* nombre_interfaz = NULL;

// File Descriptor de sockets
int socket_kernel = -1;
