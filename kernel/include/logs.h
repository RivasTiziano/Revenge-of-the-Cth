#include <kernel.h>

void log_syscall_recibida(int pid, char* syscall);
void log_creacion_proceso(int pid);
void log_cambio_estado(int pid, char* anterior, char* actual);
void log_motivo_bloqueo(int pid, char* dispositivo_io);
void log_fin_io(int pid);
void log_desalojo(int pid, char* algoritmo);
void log_fin_proceso(int pid);
void log_metricas_estado(t_pcb* pcb);