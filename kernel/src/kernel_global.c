#include "../include/kernel.h"


// Logger y config
t_log* kernel_logger = NULL;
t_config* kernel_config = NULL;
int PID = 0;

// Conexiones
char* IP_MEMORIA = NULL;
char* PUERTO_MEMORIA = NULL;
char* PUERTO_ESCUCHA_DISPATCH = NULL;
char* PUERTO_ESCUCHA_INTERRUPT = NULL;
char* PUERTO_ESCUCHA_IO = NULL;

// Configuraciones generales
char* ALGORITMO_CORTO_PLAZO = NULL;
char* ALGORITMO_INGRESO_A_READY = NULL;
double ALFA = 0.0;
int ESTIMACION_INICIAL = 0;
int TIEMPO_SUSPENSION = 0;
char* LOG_LEVEL = NULL;

// File descriptors
int socket_memoria = -1;
int socket_cpu_dispatch = -1;
int socket_cpu_interrupt = -1;
int socket_kernel_io = -1;
int socket_kernel_dispatch = -1;
int socket_kernel_interrupt = -1;

// Estructuras de datos
t_queue* cola_new = NULL;
t_queue* cola_ready = NULL;
t_list* lista_susp_blocked = NULL;
t_queue* cola_susp_ready = NULL;
t_list* lista_blocked = NULL;

t_dictionary * cpus_libres = NULL;
t_dictionary * cpus_ocupadas = NULL;
t_dictionary* cpus = NULL;

// WIP me parece que esto no va, porque el pid nos lo pasa CPU. pendiente revisar
int proximo_pid = -1;

// Diccionario de IOS
t_dictionary * ios = NULL;
t_dictionary * dispositivos = NULL;


t_dictionary * dispositivos_cpu_dispatch_conectados = NULL;
t_dictionary * dispositivos_cpu_interrupt_conectados = NULL;

// Inicialización de diccionarios para CPU
t_dictionary * dispositivos_cpu_dispatch_bloqueados = NULL;
t_dictionary * dispositivos_cpu_interrupt_bloqueados = NULL;

// Semaforos
pthread_mutex_t mutex_cola_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_new = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cola_susp_ready = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_cpus = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lista_blocked = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lista_susp_blocked = PTHREAD_MUTEX_INITIALIZER;

// Semáforo para controlar memoria disponible 
sem_t sem_memoria_disponible;

// Variables condicionales y mutex para planificadores (eliminar esperas activas)
pthread_cond_t cond_planificador_largo_plazo = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_planificador_largo_plazo = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_planificador_corto_plazo = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex_planificador_corto_plazo = PTHREAD_MUTEX_INITIALIZER;

// Planificador de largo plazo
pthread_t hilo_plp;
pthread_t hilo_pcp;
bool planificador_activo = false;



