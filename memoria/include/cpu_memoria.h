#ifndef CPU_MEMORIA_H_
#define CPU_MEMORIA_H_

#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>

#include "../include/memoria.h"

int manejar_operacion_cpu(op_code_t operacion, int socket);

void handle_handshake_cpu(int socket);
void handle_solicitar_instruccion(int socket);
void handle_acceso_tabla_paginas(int socket);
void handle_leer_memoria(int socket);
void handle_escribir_memoria(int socket);
void handle_leer_pagina_completa(int socket);
void handle_escribir_pagina_completa(int socket);

#endif
