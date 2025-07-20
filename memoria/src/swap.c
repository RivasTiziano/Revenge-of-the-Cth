#include "../include/swap.h"


void crear_swapfile() {
    const char* path = path_swapfile();

    // Abrir el archivo en modo escritura binaria (crea si no existe, trunca si existe)
    FILE* archivo = fopen(path, "wb");

    if (!archivo) {
        log_error(memoria_logger, "No se pudo crear o truncar el archivo swapfile.bin en: %s", path);
        return;
    }

    fclose(archivo);
    log_debug(memoria_logger, "Archivo swapfile.bin creado o truncado correctamente en: %s", path);
}



void escribir_proceso_en_swap(int pid) {

    // Obtener tabla raíz
    TABLA_PAGINAS* raiz = obtenerTablaRaiz(pid);
    if (!raiz) {
        log_warning(memoria_logger, "No se encontró tabla de páginas para PID %d al escribir en SWAP.", pid);
        return;
    }

    log_debug(memoria_logger, "\033[46m### LOCK MUTEX SWAP ANTES");

    pthread_mutex_lock(&mutex_swap);

    log_debug(memoria_logger, "\033[46m### LOCK MUTEX SWAP DESPUES");

    
    // Abrimos el archivo en modo append binario
    const char* path = path_swapfile();
    FILE* swapfile = fopen(path, "ab");
    if (!swapfile) {
        log_error(memoria_logger, "No se pudo abrir swapfile.bin para escritura.");
        return;
    }

    // Escribimos encabezado
    char inicio[12];
    snprintf(inicio, sizeof(inicio), "INICIO_%d\n", pid);
    fwrite(inicio, sizeof(char), strlen(inicio), swapfile);

    // Escribir páginas presentes
    int paginas_liberadas = 0;
    escribir_paginas_recursivo(swapfile, raiz, &paginas_liberadas);

    log_debug(memoria_logger, "#### ESCRIBIR PROCESO EN SWAP - PID <%d> - Paginas liberadas = %d", pid, paginas_liberadas);

    // Escribimos final
    char fin[12];
    snprintf(fin, sizeof(fin), "FIN_%d\n", pid);
    fwrite(fin, sizeof(char), strlen(fin), swapfile);

    //fseek(swapfile, 0, SEEK_END);
    //long tam = ftell(swapfile);
    //log_debug(memoria_logger, "SWAP: Tamaño del archivo después de escribir PID %d: %ld bytes", pid, tam);

    fclose(swapfile);
    pthread_mutex_unlock(&mutex_swap);

    int memoria_liberada = tam_pagina() * paginas_liberadas;
    registrar_disponibilidad_memoria(memoria_liberada);
}



// Escribe las páginas del proceso en SWAP, y devuelve la cantidad de páginas a liberar
int escribir_paginas_recursivo(FILE* swapfile, TABLA_PAGINAS* tabla, int* paginas_escritas) {

    int entradas = entradas_por_tabla();

    for (int i = 0; i < entradas; i++) {
        ENTRADA_TP* entrada = &tabla->entradas[i];

        if (!entrada->presente)
            continue;

        // Si es hoja
        if (entrada->es_hoja) {

            // Validar frame
            if (entrada->link.nroFrame == -1) {
                continue;
            }

            void* contenido = obtener_contenido_de_frame(entrada->link.nroFrame);

            if (!contenido) {
                log_warning(memoria_logger, "SWAP: No se pudo obtener contenido del frame %d (entrada %d, nivel %d)",
                            entrada->link.nroFrame, i, tabla->nivel);
                continue;
            }

            // Escribir número de página lógica primero (4 bytes)
            int nro_pagina_logica = entrada->nroPag;
            fwrite(&nro_pagina_logica, sizeof(int), 1, swapfile);
            
            // Escribir contenido de la página
            fwrite(contenido, 1, tam_pagina(), swapfile);
            (*paginas_escritas)++;
            //log_debug(memoria_logger, "SWAP: Escribiendo página lógica %d frame %d (nivel %d), total hasta ahora: %d", 
            //         nro_pagina_logica, entrada->link.nroFrame, tabla->nivel, *paginas_escritas);
            //log_debug(memoria_logger, "Contenido del frame: %s", (char*)contenido);
            free(contenido);

            entrada->presente = false;
            liberar_frame(entrada->link.nroFrame);

        } else {
            if (entrada->link.sgteTP == NULL) {
                log_warning(memoria_logger, "SWAP: Entrada intermedia en nivel %d sin sgteTP (entrada %d)", tabla->nivel, i);
                continue;
            }
            escribir_paginas_recursivo(swapfile, entrada->link.sgteTP, paginas_escritas);
        }
    }
    //log_debug(memoria_logger, "SWAP: Escribiendo %d páginas del nivel %d", *paginas_escritas, tabla->nivel);
    return *paginas_escritas;
}


void* obtener_contenido_de_frame(int frame) {
    // Validar que el número de frame esté dentro del rango
    if (frame < 0 || frame >= CANTIDAD_MARCOS) {
        log_error(memoria_logger, "Número de frame inválido: %d", frame);
        return NULL;
    }

                            log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP ANTES");

    pthread_mutex_lock(&mutex_bitmap);

                            log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP DESPUES");

    // Validar que el frame esté ocupado
    if (!esta_ocupado(frame)) {
        log_warning(memoria_logger, "El frame %d está libre. No se puede leer contenido.", frame);
        return NULL;
    }

    // Calcular dirección base del frame en memoria
    void* base_frame = (char*)MEMORIA_USUARIO + (frame * tam_pagina());

    //log_debug(memoria_logger, "base_frame %p = MEMORIA_USUARIO %p + (nro_frame %d * tam_pagina %d)", base_frame, MEMORIA_USUARIO, frame, tam_pagina());
    //log_debug(memoria_logger, "base del frame %d: %p", frame, base_frame);

    // Copiar contenido a una nueva zona de memoria
    void* contenido = malloc(tam_pagina());
    if (!contenido) {
        log_error(memoria_logger, "Fallo al reservar memoria para el contenido del frame %d", frame);
        pthread_mutex_unlock(&mutex_bitmap);
        return NULL;
    }
    
    pthread_mutex_unlock(&mutex_bitmap);

    memcpy(contenido, base_frame, tam_pagina());
    return contenido;
}


bool restaurar_proceso_desde_swap(int pid) {

    log_debug(memoria_logger, "\033[46m### LOCK MUTEX SWAP ANTES");

    pthread_mutex_lock(&mutex_swap);

    log_debug(memoria_logger, "\033[46m### LOCK MUTEX SWAP DESPUES");


    // Validar que existe el archivo de SWAP
    const char* path = path_swapfile();
    FILE* archivo = fopen(path, "r+b");
    if (!archivo) {
        log_error(memoria_logger, "No se pudo abrir swapfile.bin");
        return false;
    }

    // Leer archivo completo
    fseek(archivo, 0, SEEK_END);
    long tam_archivo = ftell(archivo);
    rewind(archivo);

    char* contenido_total = malloc(tam_archivo);
    fread(contenido_total, 1, tam_archivo, archivo);

    // Buscar INICIO y FIN del proceso usando memmem para datos binarios
    char inicio_tag[32], fin_tag[32];
    snprintf(inicio_tag, sizeof(inicio_tag), "INICIO_%d\n", pid);
    snprintf(fin_tag, sizeof(fin_tag), "FIN_%d\n", pid);

    char* inicio = memmem(contenido_total, tam_archivo, inicio_tag, strlen(inicio_tag));
    char* fin = memmem(contenido_total, tam_archivo, fin_tag, strlen(fin_tag));

    if (!inicio || !fin) {
        log_warning(memoria_logger, "No se encontraron datos en SWAP para el proceso PID %d", pid);
        pthread_mutex_unlock(&mutex_swap);
        free(contenido_total);
        fclose(archivo);
        return false;
    }

    inicio += strlen(inicio_tag);
    size_t tam_bloque = fin - inicio;
    
    // Cada página ahora incluye 4 bytes para el número de página lógica + tamaño de página
    int tamanio_entrada_swap = sizeof(int) + tam_pagina();
    int cantidad_paginas = tam_bloque / tamanio_entrada_swap;

    log_debug(memoria_logger, "PID %d se quiere reanudar - Tamaño en SWAP: %d - Cantidad de paginas que se quieren escribir: %d",
    pid, (int)tam_bloque, cantidad_paginas);


    log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP ANTES");

    pthread_mutex_lock(&mutex_bitmap);

    log_debug(memoria_logger, "\x1b[35m### LOCK MUTEX BITMAP DESPUES");
    
    // Verificar marcos libres suficientes
    int marcos_libres = contar_marcos_libres();

    // Si no hay marcos libres suficientes, cierra el archivo de SWAP y libera mutexs
    if (marcos_libres < cantidad_paginas) {
        log_debug(memoria_logger, "No hay marcos suficientes para restaurar PID %d. Marcos libres %d/%d, marcos necesitados %d",
            pid, marcos_libres, CANTIDAD_MARCOS, cantidad_paginas);
        pthread_mutex_unlock(&mutex_bitmap);
        fclose(archivo);
        pthread_mutex_unlock(&mutex_swap);
        free(contenido_total);

        log_debug(memoria_logger, "RETORNA FALSE");

        return false;
    }

    // Obtener tabla de páginas raíz del proceso
    TABLA_PAGINAS* tabla_raiz = obtenerTablaRaiz(pid);

            log_debug(memoria_logger, "OBTIENE TABLA RAIZ");


    // Si no encuentra la tabla raíz, cierra el archivo SWAP y libera mutexs
    if (!tabla_raiz) {
        log_error(memoria_logger, "No se encontró tabla de páginas para PID %d", pid);
        free(contenido_total);
        pthread_mutex_unlock(&mutex_bitmap);
        fclose(archivo);
        pthread_mutex_unlock(&mutex_swap);

                    log_debug(memoria_logger, "RETORNA FALSE");

        return false;
    }

                    log_debug(memoria_logger, "ANTES DE RESTAURAR CONTENIDO");

    // Restaurar contenido página por página
    char* pos_actual = inicio;
    for (int i = 0; i < cantidad_paginas; i++) {
        // Leer número de página lógica
        int nro_pagina_logica;
        memcpy(&nro_pagina_logica, pos_actual, sizeof(int));
        pos_actual += sizeof(int);
        
                            log_debug(memoria_logger, "ANTES DE LEER DATOS DE LA PAGINA");

        // Leer datos de la página
        void* datos_pagina = malloc(tam_pagina());
        memcpy(datos_pagina, pos_actual, tam_pagina());
        pos_actual += tam_pagina();

        log_debug(memoria_logger, "\033[43m### LOCK MUTEX RESERVAR MEMORIA ANTES");
        pthread_mutex_lock(&mutex_reservar_memoria);
        log_debug(memoria_logger, "\033[43m### LOCK MUTEX RESERVAR MEMORIA DESPUES");

        int frame = reservar_frame_libre();

        log_debug(memoria_logger, "Asignado frame %d a página lógica %d (PID %d)", frame, nro_pagina_logica, pid);

        // Si devuelve -1
        if (frame == -1) {
            log_error(memoria_logger, "reservar_frame_libre() devolvió -1 cuando debería haber habido suficientes marcos porque tiene el mutex de BITMAP");
            free(datos_pagina);
            pthread_mutex_unlock(&mutex_reservar_memoria);
            pthread_mutex_unlock(&mutex_bitmap);
            fclose(archivo);
            pthread_mutex_unlock(&mutex_swap);
            break;
        }

        registrar_uso_memoria(tam_pagina());
        pthread_mutex_unlock(&mutex_reservar_memoria);

        escribir_en_frame(frame, datos_pagina);
        free(datos_pagina);

        // Calcular el camino en la tabla de páginas para llegar a la página lógica específica
        int* camino_tablas = malloc(sizeof(int) * cantidad_niveles());
        calcular_camino_tabla(nro_pagina_logica, camino_tablas);

        // Actualizar entrada de la página
        ENTRADA_TP* entrada = obtener_entrada_tp(pid, camino_tablas);
        if (!entrada) {
            log_error(memoria_logger, "No se encontró entrada de página lógica %d para PID %d", nro_pagina_logica, pid);
            free(camino_tablas);
            continue;
        }

        entrada->presente = true;
        entrada->link.nroFrame = frame;
        entrada->nroPag = nro_pagina_logica;
        
        log_debug(memoria_logger, "SWAP: Restaurada página lógica %d en frame %d para PID %d", nro_pagina_logica, frame, pid);
        free(camino_tablas);
    }

    pthread_mutex_unlock(&mutex_bitmap);

    // Eliminar bloque del swapfile
    char* inicio_bloque = inicio - strlen(inicio_tag); // Volver al inicio del tag
    char* pos_fin = fin + strlen(fin_tag);
    size_t tam_bloque_completo = pos_fin - inicio_bloque;
    size_t tam_restante = tam_archivo - (pos_fin - contenido_total);

    // Mover el contenido restante para eliminar el bloque del proceso
    memmove(inicio_bloque, pos_fin, tam_restante);
    
    // Calcular nuevo tamaño del archivo
    long nuevo_tam_archivo = tam_archivo - tam_bloque_completo;

    // Reescribir archivo con el nuevo contenido
    fclose(archivo);
    archivo = fopen(path, "w+b");
    if (archivo && nuevo_tam_archivo > 0) {
        fwrite(contenido_total, 1, nuevo_tam_archivo, archivo);
    }

    free(contenido_total);
    if (archivo) fclose(archivo);
    
    pthread_mutex_unlock(&mutex_swap);

    log_debug(memoria_logger, "PID %d restaurado desde SWAP", pid);
    return true;
}


/**
 * @brief Calcula el camino a seguir en las tablas de páginas para una página lógica.
 * 
 * @param nro_pagina Número de página lógica.
 * @param camino_out Array de salida (de tamaño cantidad_niveles()) que contendrá los índices en cada nivel.
 */
void calcular_camino_tabla(int nro_pagina, int* camino_tabla){
    int niveles = cantidad_niveles();
    int entradas = entradas_por_tabla();

    for (int nivel = 0; nivel < niveles; nivel++) {
        int divisor = (int) pow(entradas, niveles - nivel - 1);
        camino_tabla[nivel] = (nro_pagina / divisor) % entradas;
    }
}


ENTRADA_TP* obtener_entrada_tp(int pid, int* camino_tabla_paginas) {
    char* pid_str = string_itoa(pid);
    TABLA_PAGINAS* raiz = dictionary_get(PROCESO_TABLAS, pid_str);
    free(pid_str);
    
    if (raiz == NULL) {
        log_error(memoria_logger, "No existe tabla de páginas raíz asociada al PID %d", pid);
        return NULL;
    }

    TABLA_PAGINAS* tabla_actual = raiz;

    for (int nivel = 0; nivel < cantidad_niveles(); nivel++) {
        int indice = camino_tabla_paginas[nivel];

        if (indice >= entradas_por_tabla()) {
            log_error(memoria_logger, "Índice fuera de rango en nivel %d para PID %d", nivel, pid);
            return NULL;
        }

        ENTRADA_TP* entrada = &tabla_actual->entradas[indice];

        if (nivel == cantidad_niveles() - 1) {
            // Último nivel: debe ser hoja
            if (!entrada->es_hoja) {
                log_error(memoria_logger, "Se esperaba hoja en el último nivel para PID %d", pid);
                return NULL;
            }
            return entrada;
        } else {
            // Nivel intermedio: debe ser tabla intermedia
            if (entrada->es_hoja) {
                log_error(memoria_logger, "Hoja prematura en nivel %d para PID %d", nivel, pid);
                return NULL;
            }
            
            // Si no existe la subtabla, crearla
            if (entrada->link.sgteTP == NULL) {
                log_debug(memoria_logger, "Creando subtabla en nivel %d para PID %d", nivel + 1, pid);
                entrada->presente = true;
                entrada->es_hoja = false;
                //entrada->nroPag = -1;
                entrada->link.sgteTP = crear_tabla_paginas(nivel + 1);
            }
            
            tabla_actual = entrada->link.sgteTP;
        }
    }

    return NULL; // Nunca debería llegar acá
}


// Busca la base del frame y escribe el contenido en memoria
void escribir_en_frame(int frame, void* contenido) {
    void* direccion_real = (char*)MEMORIA_USUARIO + (frame * tam_pagina());
    memcpy(direccion_real, contenido, tam_pagina());
}