#include "../include/logs.h"

void log_syscall_recibida(int pid, char* syscall){
    log_info(kernel_logger, "## (%d) - Solicitó syscall: %s", pid, syscall);
}

void log_creacion_proceso(int pid){
    log_info(kernel_logger, "## (%d) Se crea el proceso - Estado: NEW", pid);
}

void log_cambio_estado(int pid, char* anterior, char* actual){
    log_info(kernel_logger, "## (%d) Pasa del estado \x1B[34m%s\033[0m al estado \x1B[34m%s\033[0m", pid, anterior, actual);
}

void log_motivo_bloqueo(int pid, char* dispositivo_io){
    log_info(kernel_logger, "## (%d) - Bloqueado por IO: %s", pid, dispositivo_io);
}

void log_fin_io(int pid){
    log_info(kernel_logger, "## (%d) finalizó IO y pasa a READY", pid);
}

void log_desalojo(int pid, char* algoritmo){
    log_info(kernel_logger, "## (%d) - Desalojado por algoritmo %s", pid, algoritmo);
}

void log_fin_proceso(int pid){
    log_info(kernel_logger, "## (%d) - Finaliza el proceso", pid);
}


void log_metricas_estado(t_pcb* pcb) {
    if (pcb == NULL) {
        log_error(kernel_logger, "Error: PCB nulo en mostrar_metricas_proceso");
        return;
    }

    // El orden debe coincidir con el enum: NEW, READY, EXEC, BLOCKED, SUSP_READY, SUSP_BLOCKED, EXIT_P
    char* estado_nombres[] = {"NEW", "READY", "EXEC", "BLOCKED", "SUSPEND_READY", "SUSPEND_BLOCKED", "EXIT"};
    char* buffer = string_new();

    string_append_with_format(&buffer, "## (%d) - Métricas de estado:", pcb->PID);

    for (int i = 0; i < NUM_ESTADOS; i++) {
        // Obtener el contador de veces que estuvo en este estado
        int contador = pcb->ME[i];
        
        // Obtener el tiempo total en este estado
        int64_t tiempo = 0;
        if (pcb->MT[i] != NULL) {
            tiempo = temporal_gettime(pcb->MT[i]);
        }
        
        // Agregar al buffer el estado con su formato: ESTADO (CONTADOR) (TIEMPO)
        string_append_with_format(&buffer, " %s (%d) (%ld)", 
                                 estado_nombres[i], contador, tiempo);
        
        // Agregar coma si no es el último estado
        if (i < NUM_ESTADOS - 1) {
            string_append(&buffer, ",");
        }
    }

    log_info(kernel_logger, "%s", buffer);
    free(buffer);
}
