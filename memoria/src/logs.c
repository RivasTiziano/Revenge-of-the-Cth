
#include "../include/memoria.h"


//Conexión de Kernel
void log_conexion_kernel(int fd_socket){
    log_info(memoria_logger, "## Kernel Conectado - FD del socket: %d", fd_socket);
}


//Creación de Proceso
void log_creacion_proceso(int pid, int size){
    log_info(memoria_logger, "## PID: %d - Proceso Creado - Tamaño: %d", pid, size);
}



/**
 * El módulo memoria llevará un listado de métricas de cada proceso
 * a fin de poder dar seguimiento a la cantidad de operaciones
 * realizadas por cada proceso en la memoria. Se hace uso del struct t_metricas.
 * 
 * @param accesosTP Cantidad de accesos a Tablas de Páginas
 * @param instrSolicitadas Cantidad de Instrucciones solicitadas
 * @param bajadasSWAP Cantidad de bajadas a SWAP
 * @param subidasMP Cantidad de subidas a Memoria Principal o al espacio contiguo de memoria
 * @param lecturas Cantidad de Lecturas de memoria
 * @param escrituras Cantidad de Escrituras de memoria
*/
void log_destruccion_proceso(int pid, t_metricas* metricas){
    log_info(memoria_logger, "## PID: %d - Proceso Destruido - Métricas - Acc.T.Pag: %d; Inst.Sol.: %d; SWAP: %d; Mem.Prin.: %d; Lec.Mem.: %d; Esc.Mem.: %d",
    pid,
    metricas->accesosTP,
    metricas->instrSolicitadas,
    metricas->bajadasSWAP,
    metricas->subidasMP,
    metricas->lecturas,
    metricas->escrituras);
}




//Obtener instrucción
void log_obtener_instruccion(int pid, int PC, char* instruccion){
    log_info(memoria_logger, "## PID: %d - Obtener instrucción: %d - Instrucción: %s", pid, PC, instruccion);
}


//Escritura / lectura en espacio de usuario
// char* tipo: "Escritura" o "Lectura"
void log_escritura_lectura(int pid, char* tipo, int dirFisica, int size){
    log_info(memoria_logger, "## PID: %d - %s - Dir. Física: %d - Tamaño: %d", pid, tipo, dirFisica, size);
}


//Memory Dump
void log_memory_dump(int pid){
    log_info(memoria_logger, "## PID: %d - Memory Dump solicitado", pid);
}
