#ifndef KERNEL_MEMORIA_H_
#define KERNEL_MEMORIA_H_

#include "swap.h"
#include "memoria.h"

int manejar_operacion_kernel(op_code_t operacion, int client_socket);

void handle_init_proceso(int socket);
void handle_suspender_proceso(int socket);
void handle_reanudar_proceso(int socket);
void handle_memory_dump(int socket);
void handle_finalizar_proceso(int socket);

void imprimir_tabla_paginas(TABLA_PAGINAS *tabla, int indent, int pid); // función de debug
void mostrar_memoria_proceso(int pid);
void mostrar_dump_proceso(int pid);
#endif