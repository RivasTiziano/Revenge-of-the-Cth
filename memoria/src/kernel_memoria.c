#include "../include/kernel_memoria.h"
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

int manejar_operacion_kernel(op_code_t operacion, int socket){
    
    log_conexion_kernel(socket);
 
    switch (operacion){
        case K_M_INIT_PROCESO:
            log_debug(memoria_logger, "Operacion recibida: K_M_INIT_PROCESO");
            handle_init_proceso(socket);
            break;

        case K_M_SUSPENDER_PROCESO:
            log_debug(memoria_logger, "Operacion recibida: K_M_SUSPENDER_PROCESO");      
            handle_suspender_proceso(socket);
            break;

        case K_M_REANUDAR_PROCESO:
            log_debug(memoria_logger, "Operacion recibida: K_M_REANUDAR_PROCESO");      
            handle_reanudar_proceso(socket);
            break;

        case K_M_FINALIZAR_PROCESO:
            log_debug(memoria_logger, "Operacion recibida: K_M_FINALIZAR_PROCESO");      
            handle_finalizar_proceso(socket);
            break;

        case K_M_MEMORY_DUMP:
            log_debug(memoria_logger, "Operacion recibida: K_M_MEMORY_DUMP");      
            handle_memory_dump(socket);
            break;

        default:
            log_error(memoria_logger, "Operación Kernel desconocida: %d", operacion);
            return -1;
    }

    return 1;
}



// ---------------------------------------------------- HANDLE INIT PROCESO ----------------------------------------------------


void handle_init_proceso(int socket)
{
    // Recibo mensaje de Kernel
    t_buffer *buffer_in = recibir_buffer(socket);

    char *nombre_archivo = extraer_string_del_buffer(buffer_in);
    //printf("\nNOMBRE DEL ARCHIVO: %s\n", nombre_archivo);
    int tamanio = extraer_int_del_buffer(buffer_in);
    int pid = extraer_int_del_buffer(buffer_in);
    eliminar_buffer(buffer_in);

    log_debug(memoria_logger, "Kernel me pide inicializar proceso %d de tamanio %d", pid, tamanio);

    // Verifico espacio disponible
    char rta_txt[256];
    op_code_t rta_code = M_K_INIT_PROCESO_OK;

    char *path = string_new();
    string_append_with_format(&path, "%s%s", path_instrucciones(), nombre_archivo);
    FILE *nuevo_archivo = fopen(path, "r");

    log_debug(memoria_logger, "\033[43m### LOCK MUTEX RESERVAR MEMORIA ANTES");
    pthread_mutex_lock(&mutex_reservar_memoria);
    log_debug(memoria_logger, "\033[43m### LOCK MUTEX RESERVAR MEMORIA DESPUES");

    // Caso 1: archivo no encontrado
    if (nuevo_archivo == NULL) {
        snprintf(rta_txt, sizeof(rta_txt),
                "Archivo no encontrado: %s", path);
        rta_code = M_K_RESPUESTA_ERROR;
    }

    // Caso 2: memoria insuficiente
    else if (tamanio > memoria_disponible()) {
        
        snprintf(rta_txt, sizeof(rta_txt),
                "Memoria insuficiente. Kernel me pidió %d y yo tengo %d",
                tamanio, memoria_disponible());

        pthread_mutex_unlock(&mutex_reservar_memoria);
        
        rta_code = M_K_RESPUESTA_ERROR;
    }

    // Si el pedido de memoria fue exitoso
    if (rta_code == M_K_INIT_PROCESO_OK)
    {
        // 0 - Registro memoria en uso
        int cantidad_paginas = (tamanio + tam_pagina() - 1) / tam_pagina();
        registrar_uso_memoria(tam_pagina() * cantidad_paginas);

        pthread_mutex_unlock(&mutex_reservar_memoria);

        // 1 - Crea tabla de páginas para proceso PID
        TABLA_PAGINAS *raiz = crear_tabla_paginas(1);

        // 2 - Agrega la referencia a la tabla de páginas raíz al diccionario PROCESO_TABLAS
        char* pid_str = string_itoa(pid);
        dictionary_put(PROCESO_TABLAS, pid_str, raiz);
        // NO liberar raiz aquí, está siendo usada por el diccionario

        // 3 - Carga instrucciones a memoria
        leer_instrucciones_desde_archivo(nuevo_archivo, pid, tamanio);

        // 4 - Mapea y asigna frames a las páginas del proceso
        int paginas = (tamanio + tam_pagina() - 1) / tam_pagina();

        for (int i = 0; i < paginas; i++)
        {
            int frame = reservar_frame_libre();
            mapear_pagina(raiz, i, frame, 1);
        }

        log_debug(memoria_logger, "PID %d: Se mapearon %d páginas", pid, paginas);

        imprimir_tabla_paginas(raiz, 1, pid);

        // 5 - Creo y cargo las métricas del proceso
        t_metricas *metricas = inicializar_metricas();
        dictionary_put(METRICAS, pid_str, metricas);
        // NO liberar metricas aquí, está siendo usada por el diccionario

        // 6 - Log obligatorio de creación de proceso
        log_creacion_proceso(pid, tamanio);

        free(pid_str);
        free(path);
    }

    // Envío respuesta a Kernel
    t_buffer *buffer_out = crear_buffer();
    cargar_string_al_buffer(buffer_out, (char *)rta_txt);

    t_paquete *paquete = crear_paquete(rta_code, buffer_out);
    enviar_paquete(paquete, socket);
    
    free(nombre_archivo);
}



// ---------------------------------------------------- HANDLE SUSPENDER PROCESO ----------------------------------------------------


void handle_suspender_proceso(int socket)
{
    // Recibir PID de proceso a suspender
    t_buffer *buffer_in = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer_in);
    eliminar_buffer(buffer_in);

    escribir_proceso_en_swap(pid);

    // Loguear métrica
    actualizar_metrica(pid, bajadasSWAP, 1);

    t_buffer *buffer_rta = crear_buffer();
    cargar_int_al_buffer(buffer_rta, 0);
    t_paquete *paquete = crear_paquete(M_K_PROCESO_SUSPENDIDO, buffer_rta);
    enviar_paquete(paquete, socket);
}



// ---------------------------------------------------- HANDLE REANUDAR PROCESO ----------------------------------------------------


void handle_reanudar_proceso(int socket)
{
    // Recibir PID de proceso a reanudar
    t_buffer *buffer_in = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer_in);
    eliminar_buffer(buffer_in);

    log_debug(memoria_logger, "## handle_reanudar_proceso - INICIO - PID <%d>", pid);

    if (pid < 0) {
        log_error(memoria_logger, "## handle_reanudar_proceso - PID <%d> inválido", pid);
        t_buffer *buffer_rta = crear_buffer();
        cargar_int_al_buffer(buffer_rta, 0);
        t_paquete *paquete = crear_paquete(M_K_PROCESO_REANUDADO, buffer_rta);
        enviar_paquete(paquete, socket);
        return;
    }

    log_debug(memoria_logger, "## handle_reanudar_proceso - ANTES de restaurar_proceso_desde_swap - PID <%d>", pid);
    int success = restaurar_proceso_desde_swap(pid);
    log_debug(memoria_logger, "## handle_reanudar_proceso - DESPUÉS de restaurar_proceso_desde_swap - PID <%d> - success: %d", pid, success);
    
    mostrar_memoria_proceso(pid);

    if(success){
        // Loguear métrica
        actualizar_metrica(pid, subidasMP, 1);

        log_debug(memoria_logger, "## handle_reanudar_proceso - ENVIANDO respuesta SUCCESS - PID <%d>", pid);
        t_buffer *buffer_rta = crear_buffer();
        cargar_int_al_buffer(buffer_rta, success);
        t_paquete *paquete = crear_paquete(M_K_PROCESO_REANUDADO, buffer_rta);
        enviar_paquete(paquete, socket);

    } else {
        // No se reanudó el proceso
        log_debug(memoria_logger, "## handle_reanudar_proceso - ENVIANDO respuesta FAILED - PID <%d>", pid);
        t_buffer *buffer_rta = crear_buffer();
        cargar_int_al_buffer(buffer_rta, success);
        t_paquete *paquete = crear_paquete(M_K_PROCESO_REANUDADO, buffer_rta);
        enviar_paquete(paquete, socket);
    }
    
    log_debug(memoria_logger, "## handle_reanudar_proceso - FIN - PID <%d>", pid);
}


void mostrar_memoria_proceso(int pid) {
    char* pid_str = string_itoa(pid);
    t_info_p* info = dictionary_get(procesos, pid_str);
    free(pid_str);

    if (!info) {
        log_warning(memoria_logger, "No se encontró información del proceso PID %d para mostrar memoria.", pid);
        return;
    }

    int paginas = (info->tamanio + tam_pagina() - 1) / tam_pagina();
    int* camino = malloc(sizeof(int) * cantidad_niveles());

    log_debug(memoria_logger, "=== ASI SE VEN LAS PAGINAS EN MEMORIA LUEGO DE RESTAURAR PID %d ===", pid);

    for (int i = 0; i < paginas; i++) {
        // Calcular camino para página i
        for (int nivel = 0; nivel < cantidad_niveles(); nivel++) {
            camino[nivel] = indice_nivel(i, nivel + 1, cantidad_niveles());
        }

        int frame = obtener_numero_marco(pid, camino);

        char linea[1024] = {0};
        char* ptr = linea;
        ptr += sprintf(ptr, "Página %02d | ", i);

        if (frame >= 0 && frame < CANTIDAD_MARCOS) {
            void* base = MEMORIA_USUARIO + frame * tam_pagina();
            for (int j = 0; j < tam_pagina(); j++) {
                ptr += sprintf(ptr, "%02X ", ((unsigned char*)base)[j]);
            }
        } else {
            for (int j = 0; j < tam_pagina(); j++) {
                ptr += sprintf(ptr, "00 ");
            }
        }

        log_debug(memoria_logger, "%s", linea);
    }

    log_debug(memoria_logger, "=== FIN DE PAGINAS EN MEMORIA ===");

    free(camino);
}

// ---------------------------------------------------- HANDLE MEMORY DUMP ----------------------------------------------------


void handle_memory_dump(int socket)
{
    t_buffer *buffer_in = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer_in);
    eliminar_buffer(buffer_in);

    log_debug(memoria_logger, "Memory dump solicitado para PID %d", pid);

    // Buscar info del proceso en el diccionario global
    char *pid_key = string_itoa(pid);
    t_info_p *info = dictionary_get(procesos, pid_key);
    free(pid_key);

    if (info == NULL)
    {
        log_error(memoria_logger, "No se pudo hacer dump: Proceso PID %d no encontrado.", pid);
        // Enviar respuesta de error al Kernel
        t_buffer *buffer_out = crear_buffer();
        cargar_string_al_buffer(buffer_out, "DUMP_ERROR");
        t_paquete *paquete = crear_paquete(M_K_DUMP_FINALIZADO, buffer_out);
        enviar_paquete(paquete, socket);
        return;
    }

    // Crear directorio dump si no existe
    struct stat st = {0};
    char *dump_dir = dump_path();
    if (stat(dump_dir, &st) == -1)
    {
        mkdir(dump_dir, 0777);
    }

    // Crear archivo dump con timestamp legible
    char *timestamp = temporal_get_string_time("%H:%M:%S:%MS");
    char *filename = string_from_format("%s%d-%s.dmp", dump_dir, pid, timestamp);

    FILE *dump = fopen(filename, "wb");
    if (!dump)
    {
        log_error(memoria_logger, "No se pudo crear el archivo dump para PID %d.", pid);
        free(filename);
        free(timestamp);
        t_buffer *buffer_out = crear_buffer();
        cargar_string_al_buffer(buffer_out, "DUMP_ERROR");
        t_paquete *paquete = crear_paquete(M_K_DUMP_FINALIZADO, buffer_out);
        enviar_paquete(paquete, socket);
        return;
    }
    free(filename);
    free(timestamp);

    int paginas = (info->tamanio + tam_pagina() - 1) / tam_pagina();

    log_debug(memoria_logger, "TAMAÑO PROCESO: %d, CANTIDAD DE PAGINAS NUEVO: %d", info->tamanio, paginas);

    int *camino = malloc(sizeof(int) * cantidad_niveles());

    log_debug(memoria_logger, "######## DUMP MEMORY - Calcula caminos para páginas del proceso");
    for (int i = 0; i < paginas; i++)
    {
            log_debug(memoria_logger, "######## DUMP MEMORY - Pagina: %d", i);

        // Calcula el camino para la página i
        for (int nivel = 0; nivel < cantidad_niveles(); nivel++)
        {
                log_debug(memoria_logger, "######## DUMP MEMORY - Nivel: %d", nivel);

            camino[nivel] = indice_nivel(i, nivel + 1, cantidad_niveles());

                log_debug(memoria_logger, "######## DUMP MEMORY - camino[nivel]: %d", camino[nivel]);
        }
        int frame = obtener_numero_marco(pid, camino);

            log_debug(memoria_logger, "######## DUMP MEMORY - Frame de la pagina: %d", frame);

        if (frame >= 0 && frame < CANTIDAD_MARCOS)
        {
                log_debug(memoria_logger, "######## DUMP MEMORY - Frame válido, se escribe en dump");

            void *base = MEMORIA_USUARIO + frame * tam_pagina();
            fwrite(base, 1, tam_pagina(), dump);
        }
        else
        {
                log_debug(memoria_logger, "######## DUMP MEMORY - Frame inválido");

            char vacio[tam_pagina()];
            memset(vacio, 0, tam_pagina());
            fwrite(vacio, 1, tam_pagina(), dump);
        }
    }

    free(camino);
    fclose(dump);

    // Log obligatorio
    log_memory_dump(pid);

    mostrar_dump_proceso(pid);

    // Enviar confirmación a Kernel
    t_buffer *buffer_out = crear_buffer();
    cargar_string_al_buffer(buffer_out, "DUMP_OK");
    t_paquete *paquete = crear_paquete(M_K_DUMP_FINALIZADO, buffer_out);
    enviar_paquete(paquete, socket);
}



void mostrar_dump_proceso(int pid) {
    // Buscar último dump generado para este PID
    char *dump_dir = dump_path();

    DIR *dir = opendir(dump_dir);
    if (!dir) {
        log_error(memoria_logger, "No se pudo abrir el directorio de dumps: %s", dump_dir);
        return;
    }

    struct dirent *entry;
    char *archivo_mas_reciente = NULL;
    time_t t_mas_reciente = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".dmp") == NULL) continue;

        char nombre_pid[32];
        snprintf(nombre_pid, sizeof(nombre_pid), "%d-", pid);
        if (!strstr(entry->d_name, nombre_pid)) continue;

        // Obtener path completo
        char full_path[256];
        snprintf(full_path, sizeof(full_path), "%s%s", dump_dir, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (st.st_mtime > t_mas_reciente) {
                t_mas_reciente = st.st_mtime;
                free(archivo_mas_reciente);
                archivo_mas_reciente = strdup(full_path);
            }
        }
    }

    closedir(dir);

    if (!archivo_mas_reciente) {
        log_warning(memoria_logger, "No se encontró dump para PID %d", pid);
        return;
    }

    FILE *dump = fopen(archivo_mas_reciente, "rb");
    if (!dump) {
        log_error(memoria_logger, "No se pudo abrir el archivo dump: %s", archivo_mas_reciente);
        free(archivo_mas_reciente);
        return;
    }

    int tam_pagina_bytes = tam_pagina();
    int pagina = 0;
    char *buffer = malloc(tam_pagina_bytes);

    log_debug(memoria_logger, "=== Dump de memoria para PID %d ===", pid);

    while (fread(buffer, 1, tam_pagina_bytes, dump) == tam_pagina_bytes) {
        char linea[1024] = {0};
        char *ptr = linea;
        ptr += sprintf(ptr, "Página %02d | ", pagina);

        for (int i = 0; i < tam_pagina_bytes; i++) {
            ptr += sprintf(ptr, "%02X ", (unsigned char)buffer[i]);
        }

        log_debug(memoria_logger, "%s", linea);
        pagina++;
    }

    log_debug(memoria_logger, "=== Fin del dump ===");

    free(buffer);
    fclose(dump);
    free(archivo_mas_reciente);
}



// ---------------------------------------------------- HANDLE FINALIZACION PROCESO ----------------------------------------------------


/** Finalización de proceso
 * Al ser finalizado un proceso:
 * - Se libera su espacio de memoria.
 * - Se marcan como libres sus entradas en SWAP.
 * - Se genera el log obligatorio con las métricas del proceso.
 *
 * @param pid Process ID
 */
void handle_finalizar_proceso(int socket)
{
    // Recibir PID de proceso a finalizar
    t_buffer *buffer_in = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer_in);
    eliminar_buffer(buffer_in);

    char* pid_str = string_itoa(pid);

    // Obtener la tabla raíz del proceso
    TABLA_PAGINAS *raiz = obtenerTablaRaiz(pid);
    if (raiz != NULL)
    {
        // Liberar marcos y estructuras de tablas de páginas recursivamente
        destruir_tabla_paginas_recursiva(raiz, 1);

        // Log de métricas de proceso
        t_metricas *metricas = dictionary_get(METRICAS, pid_str);
        log_destruccion_proceso(pid, metricas);
    }
    else
    {
        log_warning(memoria_logger, "Intento de finalizar proceso inexistente PID %d.", pid);
    }

    // Eliminar proceso de diccionarios
    t_info_p* info = dictionary_remove(procesos, pid_str);       // Instrucciones    
    dictionary_remove(PROCESO_TABLAS, pid_str); // Tablas de procesos
    t_metricas* metricas = dictionary_remove(METRICAS, pid_str);       // Métricas
    free(metricas);
    
    if (info != NULL){
        destruir_info_proceso(info);
    }
    
    free(pid_str);

    //muestro cuanta memoria queda y que proceso finalizo
    log_debug(memoria_logger, "Memoria liberada por finalización del proceso %d. Memoria disponible: %d bytes", pid, memoria_disponible());

    // Enviar confirmación a Kernel
    t_buffer *buffer_out = crear_buffer();
    cargar_string_al_buffer(buffer_out, "OK");
    t_paquete *paquete = crear_paquete(M_K_PROCESO_FINALIZADO, buffer_out);
    enviar_paquete(paquete, socket);
}



// ---------------------------------------------------- AUXILIARES ----------------------------------------------------


void destruir_info_proceso(t_info_p* info) {

    void destruir_instruccion(void* elem) {
        t_instruccion* inst = (t_instruccion*) elem;
        for (int i = 0; i < inst->cantidad_parametros; i++) {
            free(inst->parametros[i]);
        }
        free(inst->parametros);
        free(inst);
    }

    list_destroy_and_destroy_elements(info->lista_instrucciones, destruir_instruccion);
    free(info);
}


void imprimir_tabla_paginas(TABLA_PAGINAS *tabla, int indent, int pid) {
    if (!tabla) return;

    int cant_entradas = entradas_por_tabla();

    for (int i = 0; i < cant_entradas; i++) {
        ENTRADA_TP *entrada = &tabla->entradas[i];

        char linea[512];              // Línea de log más larga por ASCII
        char indentacion[64] = {0};   // Indentación ASCII

        // Crear indentación visual
        for (int j = 0; j < indent && j < 32; j++) {
            strcat(indentacion, (j == indent - 1) ? "  └─" : "  │ ");
        }

        if (entrada->es_hoja) {
            snprintf(linea, sizeof(linea),
                "%s[H] PID %d | Nivel %d | Entrada %2d | Página: %2d | Presente: %s | Frame: %2d",
                indentacion, pid, tabla->nivel, i,
                entrada->nroPag,
                entrada->presente ? "sí" : "no",
                entrada->link.nroFrame);
        } else {
            snprintf(linea, sizeof(linea),
                "%s[T] PID %d | Nivel %d | Entrada %2d | Tabla intermedia | Presente: %s",
                indentacion, pid, tabla->nivel, i,
                entrada->presente ? "sí" : "no");
        }

        log_debug(memoria_logger, "%s", linea);

        // Recorrer siguiente tabla si existe
        if (!entrada->es_hoja && entrada->presente && entrada->link.sgteTP != NULL) {
            imprimir_tabla_paginas(entrada->link.sgteTP, indent + 1, pid);
        }
    }
}