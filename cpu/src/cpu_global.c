#include "cpu.h"
#include "mmu.h"

t_log* cpu_logger = NULL;
t_config* cpu_config = NULL;

int socket_cpu = -1;
int socket_memoria = -1;
int socket_kernel_dispatch = -1;
int socket_kernel_interrupt = -1;

bool interrupt = false;
int pid = 0;
int pc = 0;

// Variables para tracking de tiempo SRT
long tiempo_inicio_ejecucion = 0;

int tam_pagina;
int tam_memoria;
int entradas_tabla;
int cantidad_niveles;

t_list* lista_tlb;
t_list* lista_cache;
int desplazamiento;
int frame;

bool corriendo_hilos = true;