#include "../include/cpu_memoria.h"

int manejar_operacion_cpu(op_code_t operacion, int socket) {

    switch (operacion) {
        case CPU_M_HANDSHAKE:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_HANDSHAKE");
            handle_handshake_cpu(socket);
            break;

        case CPU_M_SOLICITAR_INSTRUCCION:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_SOLICITAR_INSTRUCCION");
            handle_solicitar_instruccion(socket);
            break;

        case CPU_M_ACCESO_TABLA_PAGINAS:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_ACCESO_TABLA_PAGINAS");
            handle_acceso_tabla_paginas(socket); 
            break;
            
        case CPU_M_LEER_MEMORIA:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_LEER_MEMORIA");
            handle_leer_memoria(socket); 
            break;

        case CPU_M_ESCRIBIR_MEMORIA:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_ESCRIBIR_MEMORIA");
            handle_escribir_memoria(socket); 
            break;

        case CPU_M_LEER_PAGINA_COMPLETA:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_LEER_PAGINA_COMPLETA");
            handle_leer_pagina_completa(socket); 
            break;

        case CPU_M_ESCRIBIR_PAGINA_COMPLETA:
            log_debug(memoria_logger, "Operacion recibida: CPU_M_ESCRIBIR_PAGINA_COMPLETA");
            handle_escribir_pagina_completa(socket); 
            break;

        default:
            log_error(memoria_logger, "Operación CPU desconocida: %d", operacion);
            return -1;
    }

    return 1;
}



// ---------------------------------------------------- HANDLE HANDSHAKE ----------------------------------------------------

void handle_handshake_cpu(int socket) {
    t_buffer* b_hand = recibir_buffer(socket);
    int hand = extraer_int_del_buffer(b_hand);
    eliminar_buffer(b_hand); // Usar eliminar_buffer en lugar de free

    if (hand == RESULT_OK) {
        t_buffer* bufferEntrega = crear_buffer();
        cargar_int_al_buffer(bufferEntrega, tam_pagina()); // Envio el tamaño de página
        cargar_int_al_buffer(bufferEntrega, tam_memoria()); // Envio el tamaño de memoria
        cargar_int_al_buffer(bufferEntrega, entradas_por_tabla()); // Envio las entradas por tabla
        cargar_int_al_buffer(bufferEntrega, cantidad_niveles()); // Envio la cantidad de niveles
        t_paquete* paquete = crear_paquete(M_CPU_HANDSHAKE, bufferEntrega);   
        enviar_paquete(paquete,socket);
    }
}



// ---------------------------------------------------- HANDLE SOLICITAR INSTRUCCION ----------------------------------------------------

/** Solicitar siguiente instrucción
 * CPU le solicita a Memoria la próxima instrucción a ejecutar.
 * Los parámetros se acceden desde el buffer recibido.
 * 
 * @param pc Program Counter
 * @param pid Process ID
 * 
 * @return Envía a CPU una t_instruccion serializada
*/
void handle_solicitar_instruccion(int socket_cpu) {
    // 1. Recibir mensaje de CPU
    t_buffer *buffer = recibir_buffer(socket_cpu);
    int pc  = extraer_int_del_buffer(buffer);
    int pid = extraer_int_del_buffer(buffer);

    // 2. Buscar la info del proceso en el diccionario
    char* pid_key = string_itoa(pid);
    t_info_p* info = dictionary_get(procesos, pid_key);
    free(pid_key); // Corregir memory leak

    if(info == NULL || info->lista_instrucciones == NULL) {
        log_debug(memoria_logger, "No se encontraron instrucciones para el PID %d", pid);
        eliminar_buffer(buffer);
        return;
    }

    // Chequear rango de PC
    if(pc < 0 || pc >= list_size(info->lista_instrucciones)){
        log_error(memoria_logger, "Error: program_counter fuera de rango para PID %d.", pid);
        eliminar_buffer(buffer);
        return;
    }

    // 3. Obtener la instrucción y enviarla a CPU
    t_instruccion* instruccion_enviar = list_get(info->lista_instrucciones, pc);
    
    // Validar que la instrucción extraída sea válida
    if (instruccion_enviar == NULL) {
        log_error(memoria_logger, "Error: Instrucción NULL para PID %d, PC %d", pid, pc);
        eliminar_buffer(buffer);
        return;
    }

    t_buffer* buffer_instruccion = crear_buffer();
    serializar_instruccion(instruccion_enviar, buffer_instruccion); 
    
    // Verificar que la serialización fue exitosa (buffer no vacío)
    if (buffer_instruccion->size == 0) {
        log_error(memoria_logger, "Error: Fallo en serialización de instrucción para PID %d, PC %d", pid, pc);
        eliminar_buffer(buffer);
        eliminar_buffer(buffer_instruccion);
        return;
    }

    t_paquete *paquete = crear_paquete(M_CPU_RESPUESTA_INSTRUCCION, buffer_instruccion); 
    enviar_paquete(paquete, socket_cpu);
    eliminar_buffer(buffer);

    // 4. Actualizar métricas
    actualizar_metrica(pid, instrSolicitadas, 1);
}



// ---------------------------------------------------- HANDLE ACCESO A TABLA DE PAGINAS ----------------------------------------------------

/** Acceso a tabla de páginas
 * El módulo deberá responder con el número de marco correspondiente.
 * En este evento se deberá tener en cuenta la cantidad de niveles de tablas de páginas accedido,
 * debiendo considerar un acceso (con su respectivo conteo de métricas
 * y retardo de acceso) por cada nivel de tabla de páginas accedido.
 * Los parámetros se acceden desde el buffer recibido.
 * 
 * @param pid Process ID
 * @param nroPag Número de página a buscar
 * @param path_TP el camino a seguir en las tablas de página del proceso
 * 
*/
void handle_acceso_tabla_paginas(int socket){
    t_buffer *buffer_recibido = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer_recibido);

    log_debug(memoria_logger, "########## HANDLE ACCESO A TABLA DE PAGINAS ##########");

    int *camino_tabla_paginas = malloc(sizeof(int) * cantidad_niveles());
    for (int i = 0; i < cantidad_niveles(); i++){
        camino_tabla_paginas[i] = extraer_int_del_buffer(buffer_recibido);
    }
    eliminar_buffer(buffer_recibido);

    // Log del PID y camino recibido (en una sola línea)
    char camino_str[128] = {0};
    strcat(camino_str, "[");

    for (int i = 0; i < cantidad_niveles(); i++) {
        char num[12];
        snprintf(num, sizeof(num), "%d", camino_tabla_paginas[i]);
        strcat(camino_str, num);
        if (i != cantidad_niveles() - 1) {
            strcat(camino_str, ", ");
        }
    }
    strcat(camino_str, "]");

    log_debug(memoria_logger, "Camino recibido: %s", camino_str);

    int nroMarco = obtener_numero_marco(pid, camino_tabla_paginas);

    // Log del resultado
    if (nroMarco == -1) {
        log_warning(memoria_logger, "Marco no encontrado para camino dado de PID %d", pid);
    } else {
        log_debug(memoria_logger, "PID: %d, Marco encontrado: %d", pid, nroMarco);
    }

    // Actualizar métricas
    actualizar_metrica(pid, accesoTP, cantidad_niveles());

    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, nroMarco);
    t_paquete *paquete = crear_paquete(M_CPU_RESPUESTA_DIRECCION_FISICA, buffer); 
    enviar_paquete(paquete, socket);

    free(camino_tabla_paginas);

    log_debug(memoria_logger, "########## HANDLE ACCESO A TABLA DE PAGINAS FIN ##########");

}


// ---------------------------------------------------- HANDLE LEER MEMORIA ----------------------------------------------------

void handle_leer_memoria(int socket){

    t_buffer* buffer = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer);
    int direccion_fisica = extraer_int_del_buffer(buffer); // dirección física absoluta
    int size = extraer_int_del_buffer(buffer); // cantidad de bytes a leer
    eliminar_buffer(buffer);
    
    log_debug(memoria_logger, "########## HANDLE LEER MEMORIA ##########");

    log_escritura_lectura(pid, "LECTURA", direccion_fisica, size);

    t_buffer* buffer_rta = crear_buffer();

    // Validar que la dirección esté en el rango de MEMORIA_USUARIO
    if(direccion_fisica < 0){
        cargar_string_al_buffer(buffer_rta, "ERROR: Acceso fuera de rango");
        
    // Validar que no se pase del final de página
    } else if ((direccion_fisica + size) > tam_pagina()){
        cargar_string_al_buffer(buffer_rta, "ERROR: Acceso fuera de rango");

    } else {
        void* direccion_real = (char*)MEMORIA_USUARIO + direccion_fisica;
        log_debug(memoria_logger, "Direccion real: %p", direccion_real);

        //base memoria
        log_debug(memoria_logger, "Base de memoria: %p", MEMORIA_USUARIO);

        // Leer size bytes
        char* buffer_str = malloc(sizeof(char) * size);
        if (!buffer_str) {
            log_error(memoria_logger, "Error al reservar memoria para leer.");
            return;
        }
        memcpy(buffer_str, direccion_real, size);

        cargar_string_al_buffer(buffer_rta, (char*)direccion_real);
    }

    t_paquete* paquete = crear_paquete(M_CPU_VALOR_LEIDO, buffer_rta);
    enviar_paquete(paquete, socket);

    // Actualizar métricas
    actualizar_metrica(pid, lecturas, 1);

    log_debug(memoria_logger, "########## HANDLE LEER MEMORIA FIN ##########");
}


// ---------------------------------------------------- HANDLE ESCRIBIR MEMORIA ----------------------------------------------------

void handle_escribir_memoria(int socket){

    t_buffer* buffer = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer);
    int direccion_fisica = extraer_int_del_buffer(buffer); // dirección física absoluta
    char* datos = extraer_string_del_buffer(buffer);
    eliminar_buffer(buffer);

    log_debug(memoria_logger, "########## HANDLE ESCRIBIR MEMORIA ##########");

    int size = strlen(datos) + 1;
    log_escritura_lectura(pid, "ESCRITURA", direccion_fisica, size);

    void* direccion_real = (char*)MEMORIA_USUARIO + direccion_fisica;

    t_buffer* buffer_rta = crear_buffer();

    if (direccion_fisica < 0 || direccion_fisica + size > tam_memoria()) {
        cargar_string_al_buffer(buffer_rta, "Dirección física enviada supera el límite de la memoria.");
    } else {
        memcpy(direccion_real, datos, size);
        cargar_string_al_buffer(buffer_rta, "Escribí lo que pediste");
    }

    t_paquete* paquete = crear_paquete(M_CPU_CONFIRMACION_ESCRITURA, buffer_rta);
    enviar_paquete(paquete, socket);
    free(datos);

    // Actualizar métricas
    actualizar_metrica(pid, escrituras, 1);

        log_debug(memoria_logger, "########## HANDLE ESCRIBIR MEMORIA FIN ##########");
}



// ---------------------------------------------------- HANDLE LEER PAGINA COMPLETA ----------------------------------------------------

void handle_leer_pagina_completa(int socket){
    t_buffer* buffer = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer);
    int direccion_fisica = extraer_int_del_buffer(buffer);
    eliminar_buffer(buffer);

    log_debug(memoria_logger, "########## HANDLE LEER PAGINA COMPLETA ##########");

    log_escritura_lectura(pid, "LECTURA", direccion_fisica, tam_pagina());

    void* direccion_real = (char*)MEMORIA_USUARIO + direccion_fisica;

    log_debug(memoria_logger, "Direccion real %p: MEMORIA USUARIO %p + direccion física %d", direccion_real, MEMORIA_USUARIO, direccion_fisica);

    // Leer contenido de la página
    char* buffer_str = malloc(tam_pagina() + 1);
    if (!buffer_str) {
        log_error(memoria_logger, "Error al reservar memoria para mostrar contenido como string.");
        return;
    }
    memcpy(buffer_str, direccion_real, tam_pagina());
    buffer_str[tam_pagina()] = '\0';
    log_debug(memoria_logger, "Contenido como string: '%s'", buffer_str);

    // Loguear contenido de la página en bytes
    int tamanio_pagina = tam_pagina();
    int largo_buffer = tamanio_pagina * 3 + 1; // 2 dígitos hex + espacio por byte, + '\0'
    char* hex_str = malloc(largo_buffer);
    if (!hex_str) {
        log_error(memoria_logger, "No se pudo reservar memoria para loguear bytes en hexadecimal.");
        return;
    }
    char* ptr = hex_str;
    for (int i = 0; i < tamanio_pagina; i++) {
        ptr += sprintf(ptr, "%02X ", ((unsigned char*)direccion_real)[i]);
    }
    if (tamanio_pagina > 0)
        hex_str[(tamanio_pagina * 3) - 1] = '\0';

    log_debug(memoria_logger, "Bytes: %s", hex_str);
    free(hex_str);

    // Respuesta a CPU
    t_buffer* buffer_rta = crear_buffer();
    cargar_string_al_buffer(buffer_rta, buffer_str);
    t_paquete* paquete = crear_paquete(M_CPU_LEER_PAGINA_COMPLETA, buffer_rta);
    enviar_paquete(paquete, socket);

    // Actualizar métricas
    actualizar_metrica(pid, lecturas, 1);

    free(buffer_str);

    log_debug(memoria_logger, "########## HANDLE LEER PAGINA COMPLETA FIN ##########");
}



// ---------------------------------------------------- HANDLE ESCRIBIR PAGINA COMPLETA ----------------------------------------------------

// PENDIENTE REVISAR

void handle_escribir_pagina_completa(int socket) {
    t_buffer* buffer = recibir_buffer(socket);
    int pid = extraer_int_del_buffer(buffer);
    int direccion_fisica = extraer_int_del_buffer(buffer);
    char* contenido = extraer_string_del_buffer(buffer); 
    eliminar_buffer(buffer);

    log_debug(memoria_logger, "########## HANDLE ESCRIBIR PAGINA COMPLETA ##########");

    // Log: dirección física y contenido a escribir
    log_debug(memoria_logger, "Dirección física: %d", direccion_fisica);
    log_debug(memoria_logger, "Contenido a escribir: '%s'", contenido);

    void* direccion_real = (char*)MEMORIA_USUARIO + direccion_fisica;

    // Limpiar la página completa primero
    memset(direccion_real, 0, tam_pagina());
    
    // Copiar el contenido (máximo el tamaño de la página)
    int size = strlen(contenido);
    if (size > tam_pagina() - 1) {
        size = tam_pagina() - 1;
    }
    memcpy(direccion_real, contenido, size);

    log_escritura_lectura(pid, "ESCRITURA", direccion_fisica, size);

    // Log: contenido escrito como bytes hexadecimales
    /* printf("Bytes escritos: ");
    for (int i = 0; i < tam_pagina(); i++)
        printf("%02X ", ((unsigned char*)direccion_real)[i]);
    printf("\n"); */
    
    free(contenido);

    // Actualizar métricas
    actualizar_metrica(pid, escrituras, 1);

    log_debug(memoria_logger, "########## HANDLE ESCRIBIR PAGINA COMPLETA FIN ##########");
}