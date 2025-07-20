#ifndef MMU_H_
#define MMU_H_
#include "cpu.h"
#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>

/* TLB y MEMORIA*/
typedef struct {
    int pid;
    int numero_pagina;
    int marco;
    time_t time_creado;
    time_t time_usado;
} t_entrada_TLB; //cada cuadradito

typedef struct {
    int pid;
    int marco;
    int numero_pagina;
    char* contenido;             // puntero a los datos de la página
    bool bit_modificado;        // usado en CLOCK-M
    bool bit_uso;               // usado en CLOCK / CLOCK-M
    bool presente;              // indica si la página está en caché
} t_entrada_cache;

void iniciar_TLB(void);
t_entrada_TLB* buscar_en_TLB(int numero_pagina);
int traducir_dir_logica(int direccion_logica, t_log* logger);
int obtener_marco(int nro_pagina, int vec[], t_log* logger);
void actualizar_TLB(t_entrada_TLB* registro_tlb_nuevo);
void reemplazar_TLB_FIFO(t_entrada_TLB* registro_tlb_nuevo);
void reemplazar_TLB_LRU(t_entrada_TLB* registro_tlb_nuevo);
int verificar_reemplazo_TLB(void);
int existe_entrada_con_marco(t_entrada_TLB* registro_tlb_nuevo);

int buscar_marco_en_memoria(int vec[], t_log* cpu_logger, int nro_pagina);
char* obtener_contenido_memoria(int marco, int nro_pagina, t_log* cpu_logger);

void inicializar_cache(void);
bool cache_habilitada(void);
bool tlb_habilitada(void);

void calcular_indices_tablas(int nro_pagina, int vec[]);
t_entrada_cache* crear_entrada_cache(int marco, int nro_pagina, t_log* logger);
void leer_en_cache(t_log* cpu_logger, t_entrada_cache* entrada_cache, int nro_pagina);
void escribir_en_cache(t_log* cpu_logger, t_entrada_cache* entrada_cache, int nro_pagina, char* contenido);
void escribir_en_cache2(t_log* cpu_logger, t_entrada_cache* entrada_cache, int nro_pagina, char* contenido);
void cargar_contenido_cache(t_log* cpu_logger, int direccion_logica, int operacion,char* datos);
void cargar_contenido_cache2(t_log* cpu_logger, int direccion_logica, int operacion,char* datos);
t_entrada_cache* buscar_en_cache(int nro_pagina);
void actualizar_entrada_cache(t_entrada_cache* entrada_cache_aux, t_log* logger);
int encontrar_vacio(void);

void avanzar_puntero(void);
void reemplazar_cache_CLOCK(t_entrada_cache* nueva_entrada , t_log* logger);
void reemplazar_cache_CLOCK_M(t_entrada_cache* nueva_entrada , t_log* logger);

#define ESTA_LLENA -1
#endif