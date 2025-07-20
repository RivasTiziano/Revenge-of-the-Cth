#ifndef KERNEL_PLANIFICADORES_H_
#define KERNEL_PLANIFICADORES_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/utils.h>
#include <commons/log.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <unistd.h>
#include <pthread.h>

// Archivos locales
#include "io_kernel.h"
#include "kernel.h"
#include "logs.h"

// Estructura para pasar datos al hilo de suspensión
typedef struct {
    int pid;
    int tiempo_suspension;
} t_datos_suspension;

// Creación y gestión de PCB
void crear_proceso(int pid, char* archivo, int tamanio);
void cambiar_estado(t_pcb* pcb, t_estado nuevo_estado);

// Planificadores
void planificador_largo_plazo();
void planificador_corto_plazo();

// Hilos de planificadores
void* hilo_planificador_largo_plazo(void* arg);
void* hilo_planificador_corto_plazo(void* arg);
void iniciar_planificadores();
void detener_planificadores();

// Planificador de largo plazo - algoritmos de ingreso a READY
t_pcb* siguiente_ready_fifo();
t_pcb* siguiente_ready_pmcp();
void* comparar_tamaño_procesos(void* proceso1, void* proceso2);
void* comparar_estimacion_procesos(void* proceso1, void* proceso2);

// Algoritmos de selección de procesos
t_pcb* siguiente_exec_fifo();
t_pcb* siguiente_exec_sjf();
void actualizar_estimacion(t_pcb* pcb);

// SJF con Desalojo (SRT)
bool verificar_desalojo_srt(t_pcb* nuevo_proceso);
t_cpu* encontrar_cpu_con_mayor_tiempo_restante();
int calcular_tiempo_restante(t_pcb* pcb);
void desalojar_proceso(t_cpu* cpu_a_desalojar, t_pcb* nuevo_proceso);

// Funciones auxiliares SRT
t_pcb* obtener_mejor_proceso_ready();

// Funciones para mostrar métricas
void mostrar_metricas_proceso(t_pcb* pcb);

//planificador de medio plazo
void avisoParaMemoria(int PID);
void agregar_a_susp_ready(t_pcb* pcb);
int posicionDeProcesoEnLista(t_list* lista, int id);
void planificador_mediano_plazo(t_pcb* pcb);

// Función compartida para reanudar procesos (con protección de concurrencia)
bool intentar_reanudar_proceso(t_pcb* pcb);

// Nuevas funciones para hilos de suspensión
void* hilo_suspension_proceso(void* arg);
t_pcb* buscar_proceso_en_blocked(int pid);

// Mutex para lista_blocked
extern pthread_mutex_t mutex_lista_blocked;
extern pthread_mutex_t mutex_lista_susp_blocked;

// Funciones para señalizar planificadores (eliminar esperas activas)
void despertar_planificador_largo_plazo();
void despertar_planificador_corto_plazo();

// Otros
void enviar_peticion_cpu(int socket, int PID, int PC);

#endif // KERNEL_PLANIFICADORES_H_
