#ifndef CPU_H_
#define CPU_H_

#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>
#include <pthread.h>
#include "kernel_cpu.h"
#include "memoria_cpu.h"
#include <utils/pcb.h>
#include <commons/log.h>
#include <commons/config.h>
#include "mmu.h"
#include <time.h>

/* VARIABLES GLOBALES */
extern t_log* cpu_logger;
extern t_config* cpu_config;

// File Descriptors
extern int socket_cpu;
extern int socket_memoria;
extern int socket_kernel_dispatch;
extern int socket_kernel_interrupt;

/* CONFIG */
// Conexiones
char* puerto_memoria();
char* ip_memoria();
char* puerto_kernel_dispatch();
char* puerto_kernel_interrupt();
char* ip_kernel();

int entradas_tlb();
char* reemplazo_tlb();
int entradas_cache();
char* reemplazo_cache();
char* retardo_cache();
char* log_level();

/* FUNCIONES */
void leer_config();
void inicializar_configCPU(char* id);
t_log* inicializar_logger(char* nombre);
void cerrar_cpu(t_log* cpu_logger);
void conexiones(char* cpu_id,t_log* cpu_logger);
void atender_memoria(t_log* cpu_logger);
void atender_kernel(t_log* cpu_logger);
void conectar_memoria(t_log* cpu_logger);
void conectar_kernel(char* cpu_id, t_log* cpu_logger); 

/* CICLO de INSTRUCCIONES */
void fetch(t_log* cpu_logger);
//bool decode(t_instruccion* instruccion);
void execute (t_instruccion* instruccion, t_log* cpu_logger);
void comenzar_ciclo_instruccion(t_log* cpu_logger);
t_instruccion* solicitar_instruccion_a_memoria();
t_instruccion* decode(t_buffer* buffer);
bool check_interrupt_before_execute(t_instruccion* instruccion, t_log* cpu_logger);
void check_interrupt_after_execute(t_instruccion* instruccion, t_log* cpu_logger);
void check_interrupt(t_instruccion* instruccion, t_log* cpu_logger);
void update_pc(t_instruccion* instruccion);

bool es_syscall(t_instruccion* instruccion);
int enviar_instruccion_a_kernel(t_instruccion* instruccion, t_log* cpu_logger);
bool hay_alguna_interrupcion();

// Funciones para tracking de tiempo SRT
long obtener_timestamp_milisegundos_cpu();


extern bool interrupt;
extern int pid;
extern int pc;

extern int tam_pagina;
extern int tam_memoria;
extern int entradas_tabla;
extern int cantidad_niveles;

extern int frame;
extern int desplazamiento;

extern t_list* lista_tlb;
extern t_list* lista_cache;

extern bool corriendo_hilos;
#endif