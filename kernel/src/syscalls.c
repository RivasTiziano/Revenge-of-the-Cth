#include "../include/kernel.h"
#include "../include/syscalls.h"
#include "../include/kernel_planificadores.h"
#include "../include/logs.h"

// Variables globales para búsquedas
t_cpu* cpu_encontrada_temp = NULL;
int pid_buscado_temp = 0;
int socket_libre_temp = -1;
char* nombre_io_temp = NULL;

// Funciones auxiliares para iteradores
void buscar_cpu_por_pid(char* key, void* value) {
    t_cpu* cpu = (t_cpu*)value;
    if (cpu->pcb != NULL && cpu->pcb->PID == pid_buscado_temp) {
        cpu_encontrada_temp = cpu;
    }
}

void buscar_dispositivo_libre(char* key, void* value) {
    t_dispositivo_io* disp = (t_dispositivo_io*)value;
    if (strcmp(disp->nombre_io, nombre_io_temp) == 0 && disp->pcb_io == NULL) {
        socket_libre_temp = atoi(key);
    }
}



// ---------------------------------------- SYSCALL INIT PROC ----------------------------------------

void syscall_init_proc(int pid, char* archivo, int tamanio){
    socket_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    log_syscall_recibida(pid, "INIT_PROC");

    
    if (socket_memoria == -1) {
        log_error(kernel_logger, "Fallo al conectar con Memoria");
        return;
    }

    // WIP falta una espera activa para poder iniciar el proceso?
        // o en realidad nunca se debería "llegar" a esta funcion syscall_init_proc si hay procesos en la cola de suspended ready
        // creo que este if es una validación que debería hacerse previo a la llamada de la syscall. pendiente revisar
    pthread_mutex_lock(&mutex_cola_susp_ready);
    bool cola_susp_ready_vacia = queue_is_empty(cola_susp_ready);
    pthread_mutex_unlock(&mutex_cola_susp_ready);
    
    if(cola_susp_ready_vacia){ 
        proximo_pid++;
        crear_proceso(proximo_pid, archivo, tamanio);
        // El planificador de largo plazo se ejecuta automaticamente en su hilo
    }
}


/**
 * @brief Inicializa el PCB del proceso, cambiar su estado a NEW (lo crea) y lo agrega a la cola NEW
 * @param pid Process ID
 * @param archivo El nombre del archivo que contiene el pseudocódigo
 * @param tamanio El tamaño en memoria que tendrá el proceso
*/
void crear_proceso(int pid, char* archivo, int tamanio) {
    // Inicializa PCB
    t_pcb* nuevo_pcb = malloc(sizeof(t_pcb));
    
    nuevo_pcb->PID = pid;
    nuevo_pcb->PC = 0;
    nuevo_pcb->estimacion_actual = ESTIMACION_INICIAL;
    nuevo_pcb->archivo = strdup(archivo);
    nuevo_pcb->tamanio = tamanio;
    nuevo_pcb->tiempo_ejecutado_cpu = NULL;
    nuevo_pcb->tiempo_en_blocked = NULL;

    for (int i = 0; i < NUM_ESTADOS; i++) {
        nuevo_pcb->ME[i] = 0;
        nuevo_pcb->MT[i] = NULL;
    }

    // Inicializa estado como NEW
    nuevo_pcb->estado_actual = -1; // Estado inválido para forzar el cambio
    cambiar_estado(nuevo_pcb, NEW);

    // Log de creación de proceso
    log_creacion_proceso(pid);

    // Agrega el proceso a la cola NEW
    pthread_mutex_lock(&mutex_cola_new);
    queue_push(cola_new, nuevo_pcb);
    pthread_mutex_unlock(&mutex_cola_new);
    
    // Despertar al planificador de largo plazo para que procese el nuevo proceso
    despertar_planificador_largo_plazo();
}


// ---------------------------------------- FINALIZAR PROCESO ----------------------------------------

void finalizar_proceso(t_pcb* pcb) {
    if (pcb == NULL) {
        log_error(kernel_logger, "Error: PCB nulo en finalizar_proceso");
        return;
    }
    
    int pid = pcb->PID;
    
    // Cambiar estado a EXIT_P
    cambiar_estado(pcb, EXIT_P);
    
    // Log de fin de proceso y métricas
    log_fin_proceso(pid);
    log_metricas_estado(pcb);
    
    // Notificar a memoria para finalizar el proceso
    socket_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (socket_memoria == -1) {
        log_error(kernel_logger, "Fallo al conectar con Memoria para finalizar proceso %d", pid);
        return;
    }
    
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, pid);
    t_paquete* paquete = crear_paquete(K_M_FINALIZAR_PROCESO, buffer);
    enviar_paquete(paquete, socket_memoria);
    
    int cod_op = recibir_operacion(socket_memoria);
    if (cod_op == M_K_PROCESO_FINALIZADO) {
        log_debug(kernel_logger, "Memoria confirmó finalización del proceso %d", pid);
        
        // Señalizar que hay memoria disponible para el planificador de largo plazo
        sem_post(&sem_memoria_disponible);
        log_debug(kernel_logger, "Señalizando memoria disponible tras finalización del proceso %d", pid);
        
        // Despertar al planificador de largo plazo para que procese procesos pendientes
        despertar_planificador_largo_plazo();
        
    } else {
        log_error(kernel_logger, "Error al finalizar proceso %d en memoria", pid);
    }
    
    // Liberar memoria del PCB
    if (pcb->archivo) {
        free(pcb->archivo);
    }
    
    // Liberar temporales
    for (int i = 0; i < NUM_ESTADOS; i++) {
        if (pcb->MT[i] != NULL) {
            temporal_destroy(pcb->MT[i]);
        }
    }
    
    if (pcb->tiempo_ejecutado_cpu != NULL) {
        temporal_destroy(pcb->tiempo_ejecutado_cpu);
    }
    
    if (pcb->tiempo_en_blocked != NULL) {
        temporal_destroy(pcb->tiempo_en_blocked);
    }
    
    free(pcb);
}


// ---------------------------------------- SYSCALL EXIT ----------------------------------------


void syscall_exit(int pid){
    log_syscall_recibida(pid, "EXIT");

    t_cpu* cpu_encontrada = NULL;

    cpu_encontrada_temp = NULL;
    pid_buscado_temp = pid;
    dictionary_iterator(cpus, buscar_cpu_por_pid);
    cpu_encontrada = cpu_encontrada_temp;

    if (cpu_encontrada == NULL) {
        log_error(kernel_logger, "No se encontró ninguna CPU ejecutando el PID %d", pid);
        return;
    }

    t_pcb* pcb = cpu_encontrada->pcb;
    cpu_encontrada->pcb = NULL;
    
    // Usar la función de finalización común
    finalizar_proceso(pcb);
}




// ---------------------------------------- SYSCALL DUMP MEMORY ----------------------------------------


void syscall_dump_memory(int pid, int pc){
    socket_memoria = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);

    log_syscall_recibida(pid, "DUMP_MEMORY");
    
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, pid);
    t_paquete* paquete = crear_paquete(K_M_MEMORY_DUMP, buffer);
    enviar_paquete(paquete, socket_memoria);

    t_pcb* pcb = NULL;
    t_cpu* cpu_encontrada = NULL;

    cpu_encontrada_temp = NULL;
    pid_buscado_temp = pid;
    dictionary_iterator(cpus, buscar_cpu_por_pid);
    cpu_encontrada = cpu_encontrada_temp;

    if (cpu_encontrada == NULL) {
        log_error(kernel_logger, "No se encontró ninguna CPU ejecutando el PID %d", pid);
        return;
    }

    
    pcb = cpu_encontrada->pcb;
    cpu_encontrada->pcb = NULL;
    
    pcb->PC = pc;
    cambiar_estado(pcb, BLOCKED);

    actualizar_estimacion(pcb);

    pthread_t hilo_dump;
    pthread_create(&hilo_dump, NULL, (void*)recibir_dump, pcb);
    pthread_detach(hilo_dump);

}

void recibir_dump(t_pcb* pcb) {
    int cod_op = recibir_operacion(socket_memoria);
    if (cod_op == M_K_DUMP_FINALIZADO) {
        log_debug(kernel_logger, "Memoria confirmó que el dump fue completado");
        pthread_mutex_lock(&mutex_cola_ready);
        queue_push(cola_ready, pcb);
        pthread_mutex_unlock(&mutex_cola_ready);
        cambiar_estado(pcb, READY);
        
        // Despertar al planificador de corto plazo
        despertar_planificador_corto_plazo();
        return;
    }else if (cod_op == M_K_DUMP_ERROR) {
        log_error(kernel_logger, "Error en dump de memoria para PID %d", pcb->PID);
        finalizar_proceso(pcb);
        return;
    }
}



// ---------------------------------------- SYSCALL ERROR ----------------------------------------

void syscall_error(int pid){
    
    log_error(kernel_logger, "Proceso %d generó error de syscall. Se enviará a EXIT.", pid);

    t_cpu* cpu_encontrada = NULL;

    cpu_encontrada_temp = NULL;
    pid_buscado_temp = pid;
    dictionary_iterator(cpus, buscar_cpu_por_pid);
    cpu_encontrada = cpu_encontrada_temp;

    if (cpu_encontrada == NULL) {
        log_error(kernel_logger, "No se encontró ninguna CPU ejecutando el PID %d", pid);
        return;
    }
    
    t_pcb* pcb = cpu_encontrada->pcb;
    cpu_encontrada->pcb = NULL;
    
    // Usar la función de finalización común
    finalizar_proceso(pcb);
}


//------------------------------------------------Entrada Salida-------------------------------------------------

/* Kernel deberá conocer todos los módulos de IO conectados,
   qué procesos están ejecutando IO en cada módulo y todos los procesos que están esperando una IO determinada. */

// Semáforos para sincronización
pthread_mutex_t mutex_dispositivos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_ios = PTHREAD_MUTEX_INITIALIZER;

void syscall_io(char* nombre_io, int tiempo, int pid, int pc) {

    t_pcb_io* pcb_io = malloc(sizeof(t_pcb_io)); //lina 212
    t_pcb* pcb = NULL;
    t_cpu* cpu_encontrada = NULL;

    if (pcb_io == NULL) {
        log_error(kernel_logger, "Fallo al reservar memoria para PCB IO");
        return;
    }

    pthread_mutex_lock(&mutex_cpus);
    
    cpu_encontrada_temp = NULL;
    pid_buscado_temp = pid;
    dictionary_iterator(cpus, buscar_cpu_por_pid);
    cpu_encontrada = cpu_encontrada_temp;

    if (cpu_encontrada == NULL) {
        pthread_mutex_unlock(&mutex_cpus);
        log_error(kernel_logger, "No se encontró ninguna CPU ejecutando el PID %d", pid);
        free(pcb_io);
        return;
    }

    log_syscall_recibida(pid, "I/O");

    pcb = cpu_encontrada->pcb;
    cpu_encontrada->pcb = NULL;
    
    pthread_mutex_unlock(&mutex_cpus);

    actualizar_estimacion(pcb);

    // Verifico que exista el io
    if (!dictionary_has_key(ios, nombre_io)) {
        log_warning(kernel_logger, "PID: %d - Intento de IO en dispositivo inexistente: %s", pid, nombre_io);
        log_info(kernel_logger, "PID: %d Pasa del estado \x1b[34mEXECUTE \033[0mal estado \x1b[34mEXIT", pid);
        finalizar_proceso(pcb);
        free(pcb_io);
        return;
    }

    // Bloquear la pcb y la encolo en el io solicitado
    cambiar_estado(pcb, BLOCKED);  //inicio un timer dentro de la funcion
    if (pcb->tiempo_en_blocked != NULL) { //ESTA ES LA LINEA 255
        temporal_stop(pcb->tiempo_en_blocked);
        temporal_destroy(pcb->tiempo_en_blocked);
    }
    pcb->tiempo_en_blocked = temporal_create();
    pcb->PC = pc; // Actualizo el PC para que vuelva al mismo lugar después de la IO
    pcb_io->pcb = pcb;
    pcb_io->tiempo_io = tiempo;
    
    pthread_mutex_lock(&mutex_lista_blocked);
    list_add(lista_blocked, pcb);
    pthread_mutex_unlock(&mutex_lista_blocked);
    planificador_mediano_plazo(pcb);
    
    t_io* io = dictionary_get(ios, nombre_io);
    queue_push(io->procesos, pcb_io);

    log_motivo_bloqueo(pcb->PID, nombre_io);

    ejecutar_siguiente(nombre_io);
}

void ejecutar_siguiente(char* nombre_io) {
    pthread_mutex_lock(&mutex_ios);
    //Me traigo la cola de procesos en espera de la io solicitada
    t_io* io = dictionary_get(ios, nombre_io);

    //Verifico si esa IO tiene procesos en espera
    if (io->procesos == NULL || queue_is_empty(io->procesos)) {
        pthread_mutex_unlock(&mutex_ios);
        return;
    }
    pthread_mutex_unlock(&mutex_ios);

    //Busco un dispositivo libre
    int socket_libre = -1;

    pthread_mutex_lock(&mutex_dispositivos);

    socket_libre_temp = -1;
    nombre_io_temp = nombre_io;
    dictionary_iterator(dispositivos, buscar_dispositivo_libre);
    socket_libre = socket_libre_temp;
    if (socket_libre == -1) {
        pthread_mutex_unlock(&mutex_dispositivos);
        return; //El proceso deberá esperar a que se libere algún dispositivo
    }

    //Saco el proceso de la lista de espera en IO y lo asigno a ese dispositivo libre
    pthread_mutex_lock(&mutex_ios);
    t_pcb_io* pcb_io = queue_pop(io->procesos);
    pthread_mutex_unlock(&mutex_ios);

    char* socket_str = string_itoa(socket_libre);
    t_dispositivo_io* dispositivo_libre = dictionary_get(dispositivos, socket_str);
    free(socket_str);
    dispositivo_libre->pcb_io = pcb_io;
    pthread_mutex_unlock(&mutex_dispositivos);

    //Ejecuto el proceso del respectivo dispositivo
    enviar_peticion_io(socket_libre, pcb_io->pcb->PID, pcb_io->tiempo_io);
    log_debug(kernel_logger, "Se envió el proceso %d al dispositivo IO (%s)", pcb_io->pcb->PID, nombre_io);
}

void atender_fin_io(int socket) {
    pthread_mutex_lock(&mutex_dispositivos);
    //Me traigo el dispositivo que terminó
    char* socket_str = string_itoa(socket);
    t_dispositivo_io* dispositivo = dictionary_get(dispositivos, socket_str);
    free(socket_str);
    if (dispositivo == NULL || dispositivo->pcb_io == NULL) {
        pthread_mutex_unlock(&mutex_dispositivos);
        return;
    }

    //Extraigo su proceso que estaba ejecutando
    t_pcb_io* pcb_io = dispositivo->pcb_io;
    
    // CRITICAL: Save the nombre_io before clearing the device reference
    char* nombre_io = strdup(dispositivo->nombre_io);
    
    dispositivo->pcb_io = NULL;
    pthread_mutex_unlock(&mutex_dispositivos);
    
    // Usar mutex para verificar si el proceso está en lista_susp_blocked
    pthread_mutex_lock(&mutex_lista_susp_blocked);
    int posicion = posicionDeProcesoEnLista(lista_susp_blocked, pcb_io->pcb->PID);
    pthread_mutex_unlock(&mutex_lista_susp_blocked);
    
    if (posicion != -1) {  //SUSP_BLOCKED => SUSP_READY
        agregar_a_susp_ready(pcb_io->pcb);
    }
    else { //BLOCKED => READY
        // Remover de lista_blocked si está ahí
        pthread_mutex_lock(&mutex_lista_blocked);
        int posicion_blocked = posicionDeProcesoEnLista(lista_blocked, pcb_io->pcb->PID);
        if (posicion_blocked != -1) {
            list_remove(lista_blocked, posicion_blocked);
        }
        pthread_mutex_unlock(&mutex_lista_blocked);
        
        cambiar_estado(pcb_io->pcb, READY);
        pthread_mutex_lock(&mutex_cola_ready);
        queue_push(cola_ready, pcb_io->pcb);
        pthread_mutex_unlock(&mutex_cola_ready);
        
        // Log de fin de IO
        log_fin_io(pcb_io->pcb->PID);
        
        // Despertar al planificador de corto plazo para evaluar si debe desalojar
        despertar_planificador_corto_plazo();
    }
    
    // Free the pcb_io structure
    free(pcb_io);

    //Ejecuto el siguiente en la espera de su IO
    ejecutar_siguiente(nombre_io);
    
    // Free the saved nombre_io
    free(nombre_io);
}

void atender_desconexion_io(int socket) {
    pthread_mutex_lock(&mutex_dispositivos);
    
    char* socket_str = string_itoa(socket);
    t_dispositivo_io* dispositivo = dictionary_get(dispositivos, socket_str);
    if (dispositivo == NULL) {
        pthread_mutex_unlock(&mutex_dispositivos);
        log_error(kernel_logger, "Dispositivo IO no encontrado");
        free(socket_str);
        return;
    }

    char* nombre_io = strdup(dispositivo->nombre_io);
    t_pcb_io* pcb_io = dispositivo->pcb_io;

    log_warning(kernel_logger, "Un dispositivo IO (%s) se desconectó", nombre_io);

    // Si el dispositivo estaba ejecutando un proceso, finalizarlo
    if (pcb_io != NULL) {
        int pid_proceso = pcb_io->pcb->PID; // Guardar PID antes de liberar
        t_pcb* pcb = pcb_io->pcb;
        
        // Remover de listas blocked si está ahí
        pthread_mutex_lock(&mutex_lista_blocked);
        int posicion_blocked = posicionDeProcesoEnLista(lista_blocked, pid_proceso);
        if (posicion_blocked != -1) {
            list_remove(lista_blocked, posicion_blocked);
        }
        pthread_mutex_unlock(&mutex_lista_blocked);
        
        pthread_mutex_lock(&mutex_lista_susp_blocked);
        int posicion_susp_blocked = posicionDeProcesoEnLista(lista_susp_blocked, pid_proceso);
        if (posicion_susp_blocked != -1) {
            list_remove(lista_susp_blocked, posicion_susp_blocked);
        }
        pthread_mutex_unlock(&mutex_lista_susp_blocked);
        
        // Finalizar proceso bloqueado por I/O que se desconectó
        log_info(kernel_logger, "## (%d) Pasa del estado BLOCKED al estado EXIT", pid_proceso);
        finalizar_proceso(pcb);
        free(pcb_io);
        dispositivo->pcb_io = NULL; // Clear reference before removing from dictionary
    }

    // Remove device from dictionary first, then free memory
    dictionary_remove(dispositivos, socket_str);
    
    // Liberar memoria del dispositivo DESPUÉS de remover del diccionario
    if (dispositivo->nombre_io) {
        free(dispositivo->nombre_io);
    }
    free(dispositivo);
    
    pthread_mutex_unlock(&mutex_dispositivos);

    // Manejar la desconexión del último dispositivo
    pthread_mutex_lock(&mutex_ios);
    t_io* io = dictionary_get(ios, nombre_io);
    if (io != NULL) {
        io->conectados--;

        if (io->conectados == 0) {
            // Si es el último dispositivo, finalizar todos los procesos en cola
            while (!queue_is_empty(io->procesos)) {
                t_pcb_io* pcb_io_encolado = queue_pop(io->procesos);
                if (pcb_io_encolado != NULL && pcb_io_encolado->pcb != NULL) {
                    int pid_encolado = pcb_io_encolado->pcb->PID;
                    t_pcb* pcb_encolado = pcb_io_encolado->pcb;
                    
                    // Remover de listas blocked si está ahí
                    pthread_mutex_lock(&mutex_lista_blocked);
                    int pos_blocked = posicionDeProcesoEnLista(lista_blocked, pid_encolado);
                    if (pos_blocked != -1) {
                        list_remove(lista_blocked, pos_blocked);
                    }
                    pthread_mutex_unlock(&mutex_lista_blocked);
                    
                    pthread_mutex_lock(&mutex_lista_susp_blocked);
                    int pos_susp_blocked = posicionDeProcesoEnLista(lista_susp_blocked, pid_encolado);
                    if (pos_susp_blocked != -1) {
                        list_remove(lista_susp_blocked, pos_susp_blocked);
                    }
                    pthread_mutex_unlock(&mutex_lista_susp_blocked);
                    
                    // Finalizar proceso encolado por I/O que se desconectó
                    log_info(kernel_logger, "## (%d) Pasa del estado BLOCKED al estado EXIT", pid_encolado);
                    finalizar_proceso(pcb_encolado);
                    free(pcb_io_encolado);
                }
            }
            
            // Eliminar la IO del diccionario para que no se puedan encolar más procesos
            queue_destroy(io->procesos);
            free(io);
            dictionary_remove(ios, nombre_io);
        }
    }
    pthread_mutex_unlock(&mutex_ios);

    free(socket_str);
    free(nombre_io);
}

void recibir_io(char* nombre_io, int socket) {
    pthread_mutex_lock(&mutex_ios);
    t_io* io;
    t_dispositivo_io* dispositivo;

    //Creo la nueva IO si es que no estaba
    if (!dictionary_has_key(ios, nombre_io)) {
        io = malloc(sizeof(t_io));
        io->procesos = queue_create();
        io->conectados = 0;
        dictionary_put(ios, nombre_io, io);
    }
    pthread_mutex_unlock(&mutex_ios);

    //Creo el dispositivo y lo añado a mi diccionario
    dispositivo = malloc(sizeof(t_dispositivo_io));
    dispositivo->nombre_io = strdup(nombre_io);
    dispositivo->pcb_io = NULL;

    char* clave = string_itoa(socket);
    pthread_mutex_lock(&mutex_dispositivos);
    dictionary_put(dispositivos, clave, dispositivo); // Don't duplicate the key here
    pthread_mutex_unlock(&mutex_dispositivos);
    // Don't free clave here since it's now owned by the dictionary

    pthread_mutex_lock(&mutex_ios);
    io = dictionary_get(ios, nombre_io);
    io->conectados++;
    pthread_mutex_unlock(&mutex_ios);

    log_debug(kernel_logger, "Dispositivo I/O conectado: %s", nombre_io);
}

void enviar_peticion_io(int socket, int PID, int tiempo) {
    //Armo la solicitud
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, PID);
    cargar_int_al_buffer(buffer, tiempo);
    t_paquete* paquete = crear_paquete(PAQUETE, buffer);

    //La envío
    enviar_paquete(paquete, socket);
}