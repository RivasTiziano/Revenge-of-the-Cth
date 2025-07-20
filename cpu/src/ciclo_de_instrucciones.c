#include "../include/cpu.h"
#include "../include/logs.h"

int cant_interrupciones;

/**
* @fn     void fetch(t_log* cpu_logger)
* @brief  Realiza el ciclo de instrucción principal: solicita la instrucción correspondiente al PID y PC a memoria, la decodifica y ejecuta según el tipo de operación. Si corresponde, reitera el ciclo para la siguiente instrucción. Gestiona interrupciones y SYSCALLs.
* @param  cpu_logger Logger para imprimir información de depuración y control.
* @return Ninguno
*/
void fetch(t_log* cpu_logger) {
    log_fetch_instruccion(cpu_logger, pid, pc);
    
    /*  Le mando el PID y PC a memoria para que me devuelva la instruccion*/
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, pc);
    cargar_int_al_buffer(buffer, pid);
    
    t_paquete* paquete = crear_paquete(CPU_M_SOLICITAR_INSTRUCCION, buffer);
    enviar_paquete(paquete, socket_memoria);

    int cod_op = recibir_operacion(socket_memoria);
    if (cod_op == -1) {
        log_error(cpu_logger, "Memoria se desconectó durante fetch");
        return;
    }

    if (cod_op == M_CPU_RESPUESTA_INSTRUCCION) {
        t_buffer* buffer_respuesta = recibir_buffer(socket_memoria); //Instruccion recibida
        
        /*   ETAPA DECODE   */
        t_instruccion* instruccion = decode(buffer_respuesta);  
        if (instruccion == NULL) {
            log_error(cpu_logger, "Error al decodificar instrucción");
            eliminar_buffer(buffer_respuesta);
            return;
        }

        t_operacion operacion = instruccion -> operacion;
        
        eliminar_buffer(buffer_respuesta);
        
        if (operacion == READ || operacion == WRITE) { //necesito traduccion de memoria (es READ o WRITE)
            execute (instruccion, cpu_logger);
            check_interrupt(instruccion, cpu_logger);
            destruir_instruccion(instruccion);
            fetch(cpu_logger);
        }
        else if (!es_syscall(instruccion)) { // es GOTO o NOOP
            execute (instruccion, cpu_logger);
            check_interrupt(instruccion, cpu_logger);
            destruir_instruccion(instruccion);
            fetch(cpu_logger);
        }
        else { // es una SYSCALL
            int fin_del_archivo = enviar_instruccion_a_kernel(instruccion, cpu_logger);
            check_interrupt(instruccion, cpu_logger);
            destruir_instruccion(instruccion);
            if(!fin_del_archivo) { //si la instruccion envaida no es EXIT
                fetch(cpu_logger);
            }
            else {
                log_debug(cpu_logger, "Instruccion recibida EXIT, no quedan mas instrucciones en el archivo\n");
                // NO hacer más fetch después de EXIT
                return;
            }
        }
    } else {
        log_error(cpu_logger, "Memoria respondió con código de operación inesperado: %d", cod_op);
    }
}


/**
* @fn     bool es_syscall(t_instruccion* instruccion)
* @brief  Determina si la instrucción recibida corresponde a una syscall (IO, EXIT, INIT_PROC o DUMP_MEMORY).
* @param  instruccion Puntero a la instrucción a analizar.
* @return true si es una syscall, false en caso contrario.
*/
bool es_syscall(t_instruccion* instruccion) {
    t_operacion operacion = instruccion->operacion;
    return (operacion == IO || operacion == EXIT || operacion == INIT_PROC || operacion == DUMP_MEMORY);
}

/**
* @fn     void execute(t_instruccion* instruccion, t_log* cpu_logger)
* @brief  Ejecuta la instrucción recibida según su tipo (NOOP, READ, WRITE, GOTO). Realiza la traducción de direcciones, acceso a memoria y logging de la operación. Si corresponde, utiliza la caché.
* @param  instruccion Puntero a la instrucción a ejecutar.
* @param  cpu_logger Logger para imprimir información de ejecución.
* @return Ninguno
*/
void execute (t_instruccion* instruccion, t_log* cpu_logger){
    int direccion_fisica;
    switch (instruccion->operacion) {
        case NOOP:  //solo consume el tiempo del ciclo de instruccion
            log_info(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - Ejecutando: NOOP", pid);
        break;

        case READ: {
            char* direccion_logica = instruccion->parametros[0];
            int tamanio = atoi(instruccion->parametros[1]);
            log_info(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - Ejecutando: READ - Direccion Logica: %s - Tamanio: %d", pid, direccion_logica, tamanio);

            if(cache_habilitada()) {
                // intentar leer desde la cahce directamente
                cargar_contenido_cache(cpu_logger, atoi(direccion_logica), READ, NULL); // Carga el contenido de la cache   
            }
            else {
                direccion_fisica = traducir_dir_logica(atoi(direccion_logica), cpu_logger); //Traduce dirección lógica a física
                t_buffer* buffer_rta = crear_buffer();
                cargar_int_al_buffer(buffer_rta, pid); // pid
                cargar_int_al_buffer(buffer_rta, direccion_fisica);      // número de marco
                cargar_int_al_buffer(buffer_rta, tamanio);       // cantidad de bytes a leer

                t_paquete* paquete = crear_paquete(CPU_M_LEER_MEMORIA, buffer_rta);
                enviar_paquete(paquete, socket_memoria);
    
                // Recibo respuesta de Memoria
                if(recibir_operacion(socket_memoria) == M_CPU_VALOR_LEIDO){
                    
                    t_buffer* buffer = recibir_buffer(socket_memoria);
                    char* contenido = extraer_string_del_buffer(buffer);

                    log_info(cpu_logger, "## PID: %d - Accion: Lectura - Direccion Fisica: %d - Valor: %s", pid, direccion_fisica, contenido);
                    free(contenido);
                    eliminar_buffer(buffer);

                } else {
                    log_info(cpu_logger, "Memoria me contestó otra cosa");
                }
            }
        }
        break;

        case WRITE:
            {
            char* direccion = instruccion->parametros[0];
            char* datos = instruccion->parametros[1];
            log_info(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - Ejecutando: WRITE - Direccion Logica: %s - Datos: %s", pid, direccion, datos);


            if(cache_habilitada()) {
                cargar_contenido_cache(cpu_logger, atoi(direccion), WRITE, datos); // Carga el contenido de la cache   

            }
            else {
                
                direccion_fisica = traducir_dir_logica(atoi(direccion), cpu_logger); // Traduzco la dirección lógica a física
                // Envio a Memoria lo que necesito escribir
                t_buffer* buffer = crear_buffer();
                cargar_int_al_buffer(buffer, pid);  
                cargar_int_al_buffer(buffer, direccion_fisica); 
                cargar_string_al_buffer(buffer, datos);

                t_paquete* paquete = crear_paquete(CPU_M_ESCRIBIR_MEMORIA, buffer);
                enviar_paquete(paquete, socket_memoria);
                
                // Recibo respuesta de Memoria
                if(recibir_operacion(socket_memoria) == M_CPU_CONFIRMACION_ESCRITURA){

                    t_buffer* buffer_rta = recibir_buffer(socket_memoria);
                    char* respuesta = extraer_string_del_buffer(buffer_rta);

                    if (strcmp(respuesta, "Escribí lo que pediste") == 0){ //Ojo que no cambie el mensaje que envia memoria
                        log_info(cpu_logger, "## PID: %d - Accion: ESCRIBIR - Direccion Fisica: %d - Valor: %s", pid, direccion_fisica, datos);

                    }
                    free(respuesta);
                    eliminar_buffer(buffer_rta);

                } else {
                    log_debug(cpu_logger, "Memoria me contestó otra cosa");
                }
            }
        }

        break;

        case GOTO:{
            int valor = atoi(instruccion->parametros[0]);
            pc = valor; //se actualiza el pc por direccion de memoria
            
            log_info(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - Ejecutando: GOTO - %d", pid, valor);
            }
        break;

        default:
        break;
    }

}

/**
* @fn     void check_interrupt(t_instruccion* instruccion, t_log* cpu_logger)
* @brief  Verifica si ocurrió una interrupción. Si es así, envía el PID y el PC actualizado al kernel por el canal de interrupciones. Además, incrementa el PC si la instrucción no es GOTO.
* @param  instruccion Puntero a la instrucción actual.
* @param  cpu_logger Logger para imprimir información de control.
* @return Ninguno
*/
void check_interrupt(t_instruccion* instruccion, t_log* cpu_logger){ //TODO: evaluar si el incremento de pc se puede hacer por fuera, asi me evito pasarle la instruccion por parametro
    if (hay_alguna_interrupcion()){
        interrupt = false; //reseteo la interrupcion
        
        log_info(cpu_logger, "## LLega interrupcion al puerto interrupt");
        //mandar pid y pc actualizado
        t_buffer* buffer_interrupt = crear_buffer();
        cargar_int_al_buffer(buffer_interrupt, pc);
    
        t_paquete* paquete = crear_paquete(CPU_K_INTERRUPT_PROCESO, buffer_interrupt);

        enviar_paquete(paquete, socket_kernel_interrupt);
        
        atender_kernel_cpu_dispatch(cpu_logger);
    }

    if (instruccion->operacion != GOTO){
        pc++;
    }

}


/**
* @fn     bool hay_alguna_interrupcion(void)
* @brief  Indica si hay una interrupción pendiente en el sistema.
* @param  Ninguno
* @return true si hay interrupción, false en caso contrario.
*/
bool hay_alguna_interrupcion(){
    return interrupt;
}

/**
* @fn     t_instruccion* decode(t_buffer* buffer)
* @brief  Deserializa y decodifica una instrucción recibida en un buffer, extrayendo la operación y sus parámetros.
* @param  buffer Puntero al buffer que contiene la instrucción serializada.
* @return Puntero a la instrucción deserializada.
*/
t_instruccion* decode(t_buffer* buffer) {
    t_instruccion* instruccion_deserializada = malloc(sizeof(t_instruccion));

    if (instruccion_deserializada == NULL) {
        return NULL;
    }

    instruccion_deserializada->operacion = extraer_int_del_buffer(buffer);
    instruccion_deserializada->cantidad_parametros = extraer_int_del_buffer(buffer);

    instruccion_deserializada->parametros = malloc(instruccion_deserializada->cantidad_parametros * sizeof(char*));
    if (instruccion_deserializada->parametros == NULL) {
        free(instruccion_deserializada);
        return NULL;
    }

    for (int i = 0; i < instruccion_deserializada->cantidad_parametros; i++) {
        instruccion_deserializada->parametros[i] = extraer_string_del_buffer(buffer);
        if (instruccion_deserializada->parametros[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(instruccion_deserializada->parametros[j]);
            }
            free(instruccion_deserializada->parametros);
            free(instruccion_deserializada);
            return NULL;
        }
    }

    return instruccion_deserializada;
}

/**
* @fn     int enviar_instruccion_a_kernel(t_instruccion* instruccion, t_log* cpu_logger)
* @brief  Envía la instrucción correspondiente al kernel según el tipo de operación (INIT_PROC, DUMP_MEMORY, IO, EXIT). Serializa los parámetros necesarios y gestiona la comunicación con el kernel. Devuelve 1 si la instrucción es EXIT, 0 en otros casos, -1 en caso de error.
* @param  instruccion Puntero a la instrucción a enviar.
* @param  cpu_logger Logger para imprimir información de control.
* @return 1 si la instrucción es EXIT, 0 si no es EXIT, -1 en caso de error.
*/
int enviar_instruccion_a_kernel(t_instruccion* instruccion, t_log* cpu_logger) {

    t_buffer* buffer = crear_buffer();
    
    cargar_int_al_buffer(buffer, pid);

    switch (instruccion -> operacion){
        case INIT_PROC:
            cargar_string_al_buffer(buffer, instruccion -> parametros[0]); //archivo de instrucciones
            cargar_string_al_buffer(buffer, instruccion -> parametros[1]); //tamaño
            log_debug(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - SYSCALL: INIT_PROC enviada a Kernel - Archivo: %s - Tamanio: %s", pid, instruccion -> parametros[0], instruccion -> parametros[1]);
            
            t_paquete* paquete_init_proc = crear_paquete(CPU_K_INIT_PROC, buffer);
            enviar_paquete(paquete_init_proc, socket_kernel_dispatch);
            return 0;
            
            break;
            case DUMP_MEMORY:
            cargar_int_al_buffer(buffer, pc+1); //Para salvar contexto
            t_paquete* paquete_dump_memory = crear_paquete(CPU_K_DUMP_MEMORY, buffer);
            log_debug(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - SYSCALL: DUMP_MEMORY enviada a Kernel", pid);
            enviar_paquete(paquete_dump_memory, socket_kernel_dispatch);
            
            atender_kernel_cpu_dispatch(cpu_logger);
            return 0;
            break;
            
            case IO:
            cargar_string_al_buffer(buffer, instruccion -> parametros[0]); //Dispositivo
            cargar_string_al_buffer(buffer, instruccion -> parametros[1]); //Tiempo
            cargar_int_al_buffer(buffer, pc+1); //Para salvar contexto
            
            log_debug(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - SYSCALL: IO enviada a Kernel - Dispositivo: %s - Tiempo: %s", pid, instruccion -> parametros[0], instruccion -> parametros[1]);
            
            t_paquete* paquete_io = crear_paquete(CPU_K_SOLICITAR_IO, buffer);
            enviar_paquete(paquete_io, socket_kernel_dispatch);
            
            atender_kernel_cpu_dispatch(cpu_logger);
            return 0;
            break;
            
            case EXIT:
            t_paquete* paquete_exit = crear_paquete(CPU_K_EXIT, buffer);
            log_debug(cpu_logger, "\x1B[38;2;255;128;0m## PID: %d - SYSCALL: EXIT enviada a Kernel", pid);
            enviar_paquete(paquete_exit, socket_kernel_dispatch);
            return 1;
        break;

        default:
            eliminar_buffer(buffer);
            return (-1);
    }
}