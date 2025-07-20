#include "../include/memoria.h"

/** Bytes de la memoria de usuario en uso */
int memoria_usada(void){
            log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA ANTES");

    pthread_mutex_lock(&mutex_memoria);
    int memoria_usada = MEMORIA_USUARIO_EN_USO;
    pthread_mutex_unlock(&mutex_memoria);
            log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA DESPUES");

    return memoria_usada;
}


/** Bytes de la memoria de usuario disponibles */
int memoria_disponible(void){

        log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA ANTES");

    pthread_mutex_lock(&mutex_memoria);
    int memoria_disponible = (tam_memoria() - MEMORIA_USUARIO_EN_USO);
    pthread_mutex_unlock(&mutex_memoria);

            log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA DESPUES");


    return memoria_disponible;
}


void registrar_uso_memoria(int memoria_usada){
        log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA ANTES");

    pthread_mutex_lock(&mutex_memoria);
    MEMORIA_USUARIO_EN_USO += memoria_usada;
    pthread_mutex_unlock(&mutex_memoria);
            log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA DESPUES");


    log_debug(memoria_logger, "Se reservaron %d bytes de memoria, quedan %d bytes disponibles", memoria_usada, memoria_disponible());
}


void registrar_disponibilidad_memoria(int memoria_liberada){
            log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA ANTES");

    pthread_mutex_lock(&mutex_memoria);
    MEMORIA_USUARIO_EN_USO -= memoria_liberada;
    pthread_mutex_unlock(&mutex_memoria);
                log_debug(memoria_logger, "\033[41m### LOCK MUTEX MEMORIA DESPUES");


    log_debug(memoria_logger, "Se liberaron %d bytes de memoria, quedan %d bytes disponibles", memoria_liberada, memoria_disponible());
}



// --------------------------------------- FUNCIONES DE TABLA DE PAGINAS  ---------------------------------------


TABLA_PAGINAS *crear_tabla_paginas(int nivel) {
    TABLA_PAGINAS *tp = calloc(1, sizeof(TABLA_PAGINAS));
    if (!tp) {
        perror("calloc TABLA_PAGINAS");
        exit(EXIT_FAILURE);
    }

    tp->entradas = calloc(entradas_por_tabla(), sizeof(ENTRADA_TP));
    if (!tp->entradas) {
        perror("calloc entradas");
        exit(EXIT_FAILURE);
    }

    //log_debug(memoria_logger, "Tabla de páginas nivel %d creada", nivel);

    bool es_hoja = (nivel == cantidad_niveles() - 1); //Quizas habria que restarle 1. (nivel == cantidad_niveles() -1)

    for (uint32_t i = 0; i < entradas_por_tabla(); i++) {
        tp->nivel = nivel;
        tp->entradas[i].nroPag = -1;
        tp->entradas[i].presente = false;
        tp->entradas[i].es_hoja = es_hoja;
        if (es_hoja) {
            tp->entradas[i].link.nroFrame = -1;
        } else {
            tp->entradas[i].link.sgteTP = NULL;  // subtabla aún no asignada
        }
    }

    return tp;
}



// Calcula el índice a usar en nivel_actual para la página global nro_pag
int indice_nivel(int nro_pag, int nivel_actual, int niveles_tot) {
    int base = 1;
    for (int n = 0; n < niveles_tot - nivel_actual; ++n)
        base *= entradas_por_tabla();
    
    return (nro_pag / base) % entradas_por_tabla();
}


/* Crea recursivamente las sub-tablas que hagan falta y
   deja apuntada la entrada hoja al frame indicado.          */
void mapear_pagina(TABLA_PAGINAS *tabla, int nro_pag, int nro_frame, int nivel_actual) {

    int niveles_tot = cantidad_niveles();
    int idx = indice_nivel(nro_pag, nivel_actual, niveles_tot);
    ENTRADA_TP *e = &tabla->entradas[idx];

    /* ¿Estás en último nivel? ⇒ hoja                          */
    if (nivel_actual == niveles_tot) {
        e->nroPag   = (int)nro_pag;
        e->presente = true;
        e->es_hoja  = true;
        e->link.nroFrame = nro_frame;

        //log_debug(memoria_logger, "Mapeando página %d → frame %d (nivel %d)", nro_pag, nro_frame, nivel_actual);
        return;
    }

    /* No es hoja: asegurar sub-tabla */
    if (!e->presente) {
        e->nroPag   = -1;
        e->presente = true;
        e->es_hoja  = false;
        e->link.sgteTP = crear_tabla_paginas(nivel_actual + 1);
    }


    /* Bajar un nivel */
    mapear_pagina(e->link.sgteTP, nro_pag, nro_frame, nivel_actual + 1);
}



/**
 * Busca el número de marco asociado al número de página,
 * siguiendo el camino correspondiente en las tablas de páginas.
 *
 * @param pid Process ID
 * @param nro_pagina Número de página calculado por la CPU.
 * @param camino_tabla_paginas   Array con las entradas a consultar en cada nivel
 *                   (tamaño fijo: CANTIDAD_NIVELES).
 *                   Se debe alocar memoria antes de llamar a la función.
 * @return nroFrame  Si la página está presente.
 *         -1       Si no está la página en el camino correspondiente.
 */
int obtener_numero_marco(int pid, int *camino_tabla_paginas)
{    
    char* pid_str = string_itoa(pid);
    TABLA_PAGINAS *raiz = dictionary_get(PROCESO_TABLAS, pid_str);
    free(pid_str);

    if (raiz == NULL){
        log_error(memoria_logger, "No existe tabla de páginas raíz asociada al PID %d", pid);  
        return -1;
    }

    TABLA_PAGINAS *tabla_actual = raiz; 

    for (int nivel = 0; nivel < cantidad_niveles(); nivel++) {

        int indice = camino_tabla_paginas[nivel];

        // Busca entrada especificada entre todas las entradas de la tabla
        ENTRADA_TP *entrada = &tabla_actual->entradas[indice];

        // Si la entrada es hoja, devuelve el número de frame asociado al número de página presente en la entrada
        if (entrada->es_hoja) {
            return entrada->link.nroFrame;

        // Si la entrada no es hoja, sobreescribe tabla_actual a la próxima tabla
        } else {
            if (entrada->link.sgteTP == NULL) {
                return -1;
            } else {
                tabla_actual = entrada->link.sgteTP;
            }
        }
    }

    // Si el bucle termina sin devolver, la jerarquía es inconsistente
    return -1;
}



TABLA_PAGINAS* obtenerTablaRaiz(int pid) {
    char key[16];
    snprintf(key, sizeof(key), "%d", pid);
    return (TABLA_PAGINAS*) dictionary_get(PROCESO_TABLAS, key);
}



void destruir_tabla_paginas_recursiva(TABLA_PAGINAS* tabla, int nivel) {
    if (!tabla) return;

    int entradas = entradas_por_tabla();
    bool es_hoja = (nivel == cantidad_niveles());

    for (int i = 0; i < entradas; i++) {
        ENTRADA_TP* entrada = &tabla->entradas[i];

        if (entrada->presente) {
            if (!es_hoja && entrada->link.sgteTP != NULL) {
                destruir_tabla_paginas_recursiva(entrada->link.sgteTP, nivel + 1);
            }

            // Si es hoja, liberar el frame y actualizar contadores de memoria
            if (es_hoja && entrada->link.nroFrame != -1) {
                            log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP ANTES");

                pthread_mutex_lock(&mutex_bitmap);
                liberar_frame(entrada->link.nroFrame);
                pthread_mutex_unlock(&mutex_bitmap);
                        log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP DESPUES");
                
                // Actualizar contador de memoria en uso
                registrar_disponibilidad_memoria(tam_pagina());
            }
        }
    }

    free(tabla->entradas); // ✅ Liberar el array de entradas
    free(tabla);           // ✅ Liberar la estructura principal
}




// --------------------------------------- FUNCIONES DE BITMAP  ---------------------------------------



/* Devuelve un índice de frame libre o -1 si no hay espacio */
int reservar_frame_libre(void){
    
    for (int f = 0; f < CANTIDAD_MARCOS; ++f) {
        if (frame_libre(f)) {
            ocupar_frame(f);
            return f;
        }
    }
    return -1;
}

// Retorna true si el bit está en 1 (ocupado), false si está en 0 (libre)
bool esta_ocupado(int frame) {

    bool esta_ocupado = (BITMAP_MARCOS[frame / 8] & (1u << (frame % 8))) != 0;

    return esta_ocupado;
}


int contar_marcos_ocupados(void) {
    log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP ANTES");
    pthread_mutex_lock(&mutex_bitmap);
    log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP DESPUES");


    int total_ocupados = 0;

    for (int f = 0; f < CANTIDAD_MARCOS; ++f) {
        if (BITMAP_MARCOS[f / 8] & (1u << (f % 8))) {
            total_ocupados++;
        }
    }

    pthread_mutex_unlock(&mutex_bitmap);

    return total_ocupados;
}


int contar_marcos_libres(void) {
    
    int total_libres = 0;
    for (int f = 0; f < CANTIDAD_MARCOS; ++f) {
        if ((BITMAP_MARCOS[f / 8] & (1u << (f % 8))) == 0) {
            total_libres++;
        }
    }
    
    return total_libres;
}



// --------------------------------------- FINALIZACION  ---------------------------------------

void liberarMemoriaPaginacion(){
    free(MEMORIA_USUARIO);
    free(BITMAP_MARCOS);
    free(PROCESO_TABLAS);
}
