#include "../include/cpu.h"

/**
* @fn     void conexiones(char* cpu_id, t_log* cpu_logger)
* @brief  Establece las conexiones con memoria y kernel, y comienza a atender sus mensajes. Realiza el proceso de handshake con ambos módulos y deja la CPU lista para recibir instrucciones y atender interrupciones.
* @param  cpu_id Identificador de la CPU que se enviará en el handshake con el kernel.
* @param  cpu_logger Logger para imprimir información de control y errores.
* @return Ninguno
*/
void conexiones(char* cpu_id, t_log* cpu_logger) {
    conectar_memoria(cpu_logger);
    conectar_kernel(cpu_id, cpu_logger);
    atender_kernel(cpu_logger);
    //atender_memoria(cpu_logger);
}

/**
* @fn     void conectar_kernel(char* cpu_id, t_log* cpu_logger)
* @brief  Establece las conexiones con los sockets de kernel (dispatch e interrupt) y realiza el handshake correspondiente con ambos canales. Si alguna conexión falla, termina la ejecución.
* @param  cpu_id Identificador de la CPU que se enviará en el handshake.
* @param  cpu_logger Logger para imprimir información de control y errores.
* @return Ninguno
*/
void conectar_kernel(char* cpu_id, t_log* cpu_logger) {
    //DISPATCH
    socket_kernel_dispatch = crear_conexion(ip_kernel(), puerto_kernel_dispatch());
    
    if(socket_kernel_dispatch == -1) {
        log_error(cpu_logger, "ERROR al conectarse con kernel dispatch");
        exit(-1);
    }
    else { //HANDSHAKE
        
        log_debug(cpu_logger, "\x1b[35mConectado a KERNEL DISPATCH");

        t_buffer* b_handshake_envio = crear_buffer();
        cargar_int_al_buffer(b_handshake_envio, HAND_CPU_KERNEL_DIS);
        cargar_string_al_buffer(b_handshake_envio, cpu_id);

        t_paquete* paquete_disp = crear_paquete(HANDSHAKE, b_handshake_envio);
        enviar_paquete(paquete_disp, socket_kernel_dispatch);

        //espero la respuesta del handshake en atender_kernel_cpu_dispatch()        
    }


    //INTERRUPT
    socket_kernel_interrupt = crear_conexion(ip_kernel(), puerto_kernel_interrupt());
    if(socket_kernel_interrupt == -1) {
        log_error(cpu_logger, "ERROR al conectarse con kernel interrupt");
        exit(-1);
    }
    else { //HANDSHAKE
        log_debug(cpu_logger, "\x1b[35mConectado a KERNEL INTERRUPT");
        t_buffer* b_handshake_envio_int = crear_buffer();
        cargar_int_al_buffer(b_handshake_envio_int, HAND_CPU_KERNEL_INT);
        cargar_string_al_buffer(b_handshake_envio_int, cpu_id);

        t_paquete* paquete = crear_paquete(HANDSHAKE, b_handshake_envio_int);
        enviar_paquete(paquete, socket_kernel_interrupt);
    }
}

/**
* @fn     void conectar_memoria(t_log* cpu_logger)
* @brief  Establece la conexión con el socket de memoria, realiza el handshake y recibe los parámetros de configuración de memoria (tamaño de página, tamaño de memoria, entradas por tabla y cantidad de niveles). Si la conexión falla, termina la ejecución.
* @param  cpu_logger Logger para imprimir información de control y errores.
* @return Ninguno
*/
void conectar_memoria(t_log* cpu_logger) {
    socket_memoria = crear_conexion(ip_memoria(), puerto_memoria());
    if(socket_memoria == -1) {
        log_error(cpu_logger, "ERROR al conectarse con memoria");
        exit(-1);
    }
    else {
        log_debug(cpu_logger, "\x1b[35mConectado a MEMORIA");
        //Pedirle a memoria que nos envie los datos
        t_buffer* pedir_datos = crear_buffer();
        cargar_int_al_buffer(pedir_datos, RESULT_OK);
        t_paquete *paquete = crear_paquete(CPU_M_HANDSHAKE, pedir_datos); 
        enviar_paquete(paquete, socket_memoria);

        // op code que se recibe M_CPU_HANDSHAKE
        int op_code = recibir_operacion(socket_memoria);
        if (op_code ==  M_CPU_HANDSHAKE){
            t_buffer* buffer = recibir_buffer(socket_memoria);

            tam_pagina = extraer_int_del_buffer(buffer); // recibo el tamaño de página
            tam_memoria = extraer_int_del_buffer(buffer); // recibo el tamaño de memoria
            entradas_tabla = extraer_int_del_buffer(buffer); // recibo las entradas por tabla
            cantidad_niveles = extraer_int_del_buffer(buffer); // recibo la cantidad de niveles

            eliminar_buffer(buffer);
        }
    }
}

/**
* @fn     void atender_kernel(t_log* cpu_logger)
* @brief  Crea hilos para atender los mensajes de kernel dispatch e interrupt. Se asegura de que ambos canales estén siendo escuchados y gestionados correctamente.
* @param  cpu_logger Logger para imprimir información de control y errores.
* @return Ninguno
*/
void atender_kernel(t_log* cpu_logger) { //Atiendo los mensajes de KERNEL DISPATCH (como cliente)
    pthread_t hilo_cpu_interrupt;
    pthread_t hilo_cpu_dispatch;

    if(pthread_create(&hilo_cpu_dispatch, NULL, (void*) atender_kernel_cpu_dispatch, cpu_logger) != 0){ //LINEA 109
        log_error(cpu_logger, "ERROR al crear el hilo con para atender el kernel dispatch");
    }
    
    if(pthread_create(&hilo_cpu_interrupt, NULL, (void*) atender_kernel_cpu_interrupt, cpu_logger) != 0){ 
        log_error(cpu_logger, "ERROR al crear el hilo con para atender el kernel interrupt");
    }
    
    pthread_detach(hilo_cpu_dispatch);
    pthread_join(hilo_cpu_interrupt, NULL);
}


/**
* @fn     void cerrar_cpu(t_log* cpu_logger)
* @brief  Libera las conexiones abiertas, destruye la configuración y los logs, y limpia recursos asociados a la CPU. Es llamada al finalizar la ejecución para evitar fugas de memoria y recursos.
* @param  cpu_logger Logger para imprimir información de control y errores.
* @return Ninguno
*/
void cerrar_cpu(t_log* cpu_logger) {
    //Conexiones
    liberar_conexion(socket_memoria);
    liberar_conexion(socket_kernel_dispatch);
    liberar_conexion(socket_kernel_interrupt);

    //Config
    config_destroy(cpu_config);
    
    //Logs
    log_destroy(cpu_logger);

    //Semaforos ... (proximamente)
}
