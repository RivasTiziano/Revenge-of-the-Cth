#ifndef KERNELH
#define KERNELH

#define NUM_ESTADOS 7

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/utils.h>
#include <pthread.h>
#include <semaphore.h>

#include <commons/config.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/temporal.h>

typedef enum {
    NEW,
    READY,
    EXEC,
    BLOCKED,
    SUSP_READY,
    SUSP_BLOCKED,
    EXIT_P //el P es porque se chocaba con el EXIT de utils.h
} t_estado;

typedef struct {
    int PID;
    int PC;
    //orden: [NUEVO, READY, READY SUSPEND, BLOCKED, BLOCKED SUSPEND, EXEC, FINISH]
    int ME[NUM_ESTADOS]; 
    t_temporal* MT[NUM_ESTADOS]; 

    t_estado estado_actual; // Estado actual del PCB
    char* archivo; // Nombre del archivo del proceso
    int tamanio; // Tamaño del proceso
    t_temporal* tiempo_en_blocked; // Estimación de tiempo de CPU
    int64_t estimacion_actual;
    t_temporal* tiempo_ejecutado_cpu; // Tiempo ejecutado en CPU
} t_pcb;

// VARIABLES GLOBALES
extern t_pcb** colas_estado[NUM_ESTADOS];
extern int colas_sizes[NUM_ESTADOS]; // tamaño de cada cola (cantidad de procesos en cada cola)

// ESTRUCTURAS DE DATOS /

typedef struct {
    int socket_dispatch;
    int socket_interrupt;
    t_pcb* pcb;  // El proceso que está siendo ejecutado por la CPU
} t_cpu;

extern t_dictionary* dispositivos; //key: socket
extern t_dictionary* ios; //key: nombre
typedef struct {
    t_pcb* pcb;
    int tiempo_io;
} t_pcb_io;

typedef struct {
    char* nombre_io;
    t_pcb_io* pcb_io;
} t_dispositivo_io;

typedef struct {
    t_queue* procesos;
    int conectados;
} t_io;



// Colas
extern t_queue* cola_new;
extern t_queue* cola_ready;
extern t_queue* cola_susp_ready;
extern t_list* lista_susp_blocked;
extern t_list* lista_blocked;
// Semaforos
extern pthread_mutex_t mutex_cola_ready;
extern pthread_mutex_t mutex_cola_new;
extern pthread_mutex_t mutex_cola_susp_ready;
extern pthread_mutex_t mutex_cpus;

// Semáforo para bloquear planificador largo plazo cuando no hay memoria
extern sem_t sem_memoria_disponible;

// Semáforos y variables condicionales para eliminar esperas activas
extern pthread_cond_t cond_planificador_largo_plazo;
extern pthread_mutex_t mutex_planificador_largo_plazo;
extern pthread_cond_t cond_planificador_corto_plazo;
extern pthread_mutex_t mutex_planificador_corto_plazo;

extern int proximo_pid; // PID del próximo proceso a crear

// CPU
extern t_dictionary * cpus_libres; // Lista de sockets de CPU dispatch libres
extern t_dictionary * cpus_ocupadas; // Lista de PCBs ejecutándose en CPUs

extern t_dictionary * cpus; 



// VARIABLES GLOBALES /
extern t_log* kernel_logger;
extern t_config* kernel_config;
extern int PID;

// Para las conexiones //
extern char* IP_MEMORIA;
extern char* PUERTO_MEMORIA;
extern char* PUERTO_ESCUCHA_DISPATCH;
extern char* PUERTO_ESCUCHA_INTERRUPT;
extern char* PUERTO_ESCUCHA_IO;

// Para otras cositas 
extern char* ALGORITMO_CORTO_PLAZO;
extern char* ALGORITMO_INGRESO_A_READY;
extern double ALFA;
extern int ESTIMACION_INICIAL;
extern int TIEMPO_SUSPENSION;
extern char* LOG_LEVEL;

// File Descriptor de sockets
extern int socket_memoria;
extern int socket_cpu_dispatch;
extern int socket_cpu_interrupt;
extern int socket_kernel_io;
extern int socket_kernel_dispatch;
extern int socket_kernel_interrupt;

// Diccionario de Dispositivos IO
extern t_dictionary* dispositivos_cpu_dispatch_conectados;
extern t_dictionary* dispositivos_cpu_interrupt_conectados;


void primer_proceso(char* nombre_archivo, char* size);

// Planificador de largo plazo en hilo separado
void* hilo_planificador_largo_plazo(void* arg);
extern pthread_t hilo_plp;
extern pthread_t hilo_pcp;
extern bool planificador_activo;

#include "cpu_kernel_dispatch.h"
#include "cpu_kernel_interrupt.h"
#include "kernel_communications.h"
#include "io_kernel.h"
#include "memoria_kernel.h"

#endif