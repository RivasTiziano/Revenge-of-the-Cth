#include <commons/log.h>

void log_fetch_instruccion(t_log* logger, int pid, int pc);
void log_interrupcion_recibida(t_log* logger);
void log_lectura_escritura_memoria(t_log* logger, int pid, char* accion, char* dir_fisica, char* valor);
void log_obtener_marco(t_log* logger, int pid, int nro_pagina, int nro_marco);
void log_tlb_hit(t_log* logger, int pid, int nro_pagina);
void log_tlb_miss(t_log* logger, int pid, int nro_pagina);
void log_cache_hit(t_log* logger, int pid, int nro_pagina);
void log_cache_miss(t_log* logger, int pid, int nro_pagina);
void log_cache_add(t_log* logger, int pid, int nro_pagina);
void log_memory_update(t_log* logger, int pid, int nro_pagina, int nro_marco);