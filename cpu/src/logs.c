#include "../include/logs.h"

// Log Fetch Instrucción
void log_fetch_instruccion(t_log* logger, int pid, int pc){
    log_info(logger, "## PID: %d - FETCH - Program Counter: %d", pid, pc);
}

// Log Interrupción Recibida
void log_interrupcion_recibida(t_log* logger){
    log_info(logger, "## Llega interrupción al puerto Interrupt");
}

// Este log está separado en 4: NOOP, READ, WRITE y GOTO en archivo ciclo_de_instrucciones.c
// Log Instrucción Ejecutada
/* void log_instruccion_ejecutada(char* logger, int pid, char* instruccion, char* parametros){
    log_info(logger, "## PID: <%d> - Ejecutando: <%s> - <%s>", pid, instruccion, parametros);
} */

// Log Lectura/Escritura Memoria
void log_lectura_escritura_memoria(t_log* logger, int pid, char* accion, char* dir_fisica, char* valor){
    log_info(logger, "PID: <%d> - Acción: <%s> - Dirección Física: <%s> - Valor: <%s>", pid, accion, dir_fisica, valor);
}

// Log Obtener Marco
void log_obtener_marco(t_log* logger, int pid, int nro_pagina, int nro_marco){
    log_info(logger, "PID: <%d> - OBTENER MARCO - Página: <%d> - Marco: <%d>", pid, nro_pagina, nro_marco);
}

// Log TLB Hit
void log_tlb_hit(t_log* logger, int pid, int nro_pagina){
    log_info(logger, "PID: <%d> - TLB HIT - Página: <%d>", pid, nro_pagina);
}

// Log TLB Miss
void log_tlb_miss(t_log* logger, int pid, int nro_pagina){
    log_info(logger, "PID: <%d> - TLB MISS - Página: <%d>", pid, nro_pagina);
}

// Log Cache Hit
void log_cache_hit(t_log* logger, int pid, int nro_pagina){
    log_info(logger, "PID: <%d> - Cache Hit - Página: <%d>", pid, nro_pagina);
}

// Log Cache Miss
void log_cache_miss(t_log* logger, int pid, int nro_pagina){
    log_info(logger, "PID: <%d> - Cache Miss - Página: <%d>", pid, nro_pagina);
}

// Log Cache Add
void log_cache_add(t_log* logger, int pid, int nro_pagina){
    log_info(logger, "PID: <%d> - Cache Add - Página: <%d>", pid, nro_pagina);
}

// Log Memory Update
void log_memory_update(t_log* logger, int pid, int nro_pagina, int nro_marco){
    log_info(logger, "PID: <%d> - Memory Update - Página: <%d> - Frame: <%d>", pid, nro_pagina, nro_marco);
}