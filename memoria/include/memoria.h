#ifndef MEMORIA_H_
#define MEMORIA_H_
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <utils/utils.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include <commons/temporal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "paginacion.h"
#include "cpu_memoria.h"
#include "logs.h"
#include "instrucciones.h"
#include "kernel_memoria.h"

typedef struct {
    t_list* lista_instrucciones;
    int tamanio;
} t_info_p;

/* VARIABLES GLOBALES */
extern t_log* memoria_logger;
extern t_config* memoria_config;
extern t_dictionary* procesos;
extern t_dictionary* METRICAS;

/* File Descriptors */
extern int socket_server;

// MUTEX
extern pthread_mutex_t mutex_memoria;
extern pthread_mutex_t mutex_reservar_memoria;
extern pthread_mutex_t mutex_bitmap;
extern pthread_mutex_t mutex_metricas;
extern pthread_mutex_t mutex_swap;


/* FUNCIONES */

void inicializar_config(); 
void inicializar_logger();
t_metricas* inicializar_metricas();

char* puerto_escucha();
int tam_memoria(void);
int tam_pagina(void);
int entradas_por_tabla(void);
int cantidad_niveles(void);
int retardo_memoria(void);
char* path_swapfile(void);
int retardo_swap(void);
char* log_level(void);           
char* dump_path(void);
char* path_instrucciones(void);

void* handle_connection(void* arg);
void aplicar_retardo_memoria(int operacion);
void actualizar_metrica(int pid, int metrica, int valor);

void destruir_info_proceso(t_info_p*);

void registrar_disponibilidad_memoria(int memoria_liberada);


#endif
