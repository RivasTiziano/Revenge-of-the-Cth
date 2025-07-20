#include "../../include/kernel_planificadores.h"
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/temporal.h>
#include <commons/string.h>



// ---------------------------------------- FUNCION PRINCIPAL -----------------------------------------

void planificador_largo_plazo(){
    t_pcb* pcb = NULL;

    // Notificar a memoria para reanudar el proceso (moverlo de SWAP a memoria principal)
    //Tengo que crear de nuevo la conexion con memoria porque es efimera
    int socket_memoria_largo_plazo = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (socket_memoria_largo_plazo == -1) {
        log_error(kernel_logger, "Fallo al conectar con Memoria");
        return;
    }
    
    // Prioridad 1: Procesos en SUSP_READY (tienen prioridad sobre NEW)
    pthread_mutex_lock(&mutex_cola_susp_ready);
    if (!queue_is_empty(cola_susp_ready)) {
        pcb = queue_pop(cola_susp_ready);
        pthread_mutex_unlock(&mutex_cola_susp_ready);
        
        log_debug(kernel_logger, "Planificador Largo Plazo: Seleccionando proceso de SUSP_READY - PID: %d", pcb->PID);
        
        t_buffer* buffer = crear_buffer();
        cargar_int_al_buffer(buffer, pcb->PID);
        t_paquete* paquete = crear_paquete(K_M_REANUDAR_PROCESO, buffer);
        enviar_paquete(paquete, socket_memoria_largo_plazo);
        
        int cod_op = recibir_operacion(socket_memoria_largo_plazo);
        if (cod_op == M_K_PROCESO_REANUDADO) {

            t_buffer* buffer_rta = recibir_buffer(socket_memoria_largo_plazo);
            int se_pudo_reanudar = extraer_int_del_buffer(buffer_rta);
            eliminar_buffer(buffer_rta);

            if (se_pudo_reanudar == true) {
                log_debug(kernel_logger, "Proceso %d reanudado exitosamente desde SWAP", pcb->PID);
                
                // Cambiar estado a READY y agregarlo a la cola READY
                cambiar_estado(pcb, READY);
                pthread_mutex_lock(&mutex_cola_ready);
                queue_push(cola_ready, pcb);
                pthread_mutex_unlock(&mutex_cola_ready);
                
                // Despertar al planificador de corto plazo
                despertar_planificador_corto_plazo();
            }
            else {
                log_debug(kernel_logger, "No hay espacio suficiente para reanudar el proceso %d desde SWAP", pcb->PID);
                // En caso de error, volver a encolar en SUSP_READY
                pthread_mutex_lock(&mutex_cola_susp_ready);
                queue_push(cola_susp_ready, pcb);
                pthread_mutex_unlock(&mutex_cola_susp_ready);
                
                // Bloquear el planificador hasta que haya memoria disponible
                // Cerrar conexión antes de retornar
                close(socket_memoria_largo_plazo);
                
                log_debug(kernel_logger, "Sin memoria para reanudar proceso SUSP_READY %d - planificador retorna y esperará señalización", pcb->PID);
                return;
            }
            
            
        } else {
            log_error(kernel_logger, "Operacion No Esperada");

        }
        
        // Close the ephemeral connection
        close(socket_memoria_largo_plazo);
        return;
    } else {
        pthread_mutex_unlock(&mutex_cola_susp_ready);
    }

    // Prioridad 2: Procesos nuevos en NEW (solo si no hay procesos en SUSP_READY)
    pthread_mutex_lock(&mutex_cola_new);
    if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0) {
        pcb = siguiente_ready_fifo();
    } else if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0) {
        pcb = siguiente_ready_pmcp();
    }
    pthread_mutex_unlock(&mutex_cola_new);

    if (pcb == NULL) {
        close(socket_memoria_largo_plazo);
        return;
    }

    log_debug(kernel_logger, "Planificador Largo Plazo: Seleccionando proceso de NEW - PID: %d", pcb->PID);

    t_buffer* buffer = crear_buffer(); 
    cargar_string_al_buffer(buffer, pcb->archivo);
    //printf("\nNOMBRE DEL ARCHIVO: %s\n", pcb->archivo);

    cargar_int_al_buffer(buffer, pcb->tamanio);
    cargar_int_al_buffer(buffer, pcb->PID);

    log_debug(kernel_logger, "PLANI LARGO PLAZO: Quiero iniciar proceso %d con tamaño %d, le pregunto a Memoria si se puede", pcb->PID, pcb->tamanio);
    
    t_paquete* paquete = crear_paquete(K_M_INIT_PROCESO, buffer);
    enviar_paquete(paquete, socket_memoria_largo_plazo);

    int cod_op = recibir_operacion(socket_memoria_largo_plazo);
    if (cod_op == M_K_INIT_PROCESO_OK) {
        t_buffer* buffer_resp = recibir_buffer(socket_memoria_largo_plazo);
        log_debug(kernel_logger, "Memoria aceptó la creación del proceso");
        
        cambiar_estado(pcb, READY);
        
        if (strcmp(ALGORITMO_INGRESO_A_READY, "FIFO") == 0) {
            queue_pop(cola_new); // Eliminar de cola_new
        }
        
        pthread_mutex_lock(&mutex_cola_ready);
        queue_push(cola_ready, pcb);
        pthread_mutex_unlock(&mutex_cola_ready);
        
        // Despertar al planificador de corto plazo
        despertar_planificador_corto_plazo();
        
        eliminar_buffer(buffer_resp);
    } else if (cod_op == M_K_RESPUESTA_ERROR) {
        t_buffer* buffer_resp = recibir_buffer(socket_memoria_largo_plazo);
        char* mensaje_error = extraer_string_del_buffer(buffer_resp);
        
        log_debug(kernel_logger, "Error de memoria para proceso NEW %d: %s", pcb->PID, mensaje_error);
        
        // Para PMCP, devolver el proceso a la cola (ya que se removió antes)
        if (strcmp(ALGORITMO_INGRESO_A_READY, "PMCP") == 0) {
            queue_push(cola_new, pcb);
        }

        free(mensaje_error);
        eliminar_buffer(buffer_resp);
        
        // Bloquear planificador hasta que haya memoria disponible
        close(socket_memoria_largo_plazo);
        
        log_debug(kernel_logger, "Sin memoria para proceso NEW %d - planificador retorna y esperará señalización", pcb->PID);
        return;
    }
    
    // Close the ephemeral connection
    close(socket_memoria_largo_plazo);
}


// ---------------------------------------- FUNCIONES AUXILIARES -----------------------------------------

t_pcb* siguiente_ready_fifo() {
    // Nota: esta función debe ser llamada cuando ya se tiene el mutex de cola_new
    if (!queue_is_empty(cola_new)) {
        return queue_peek(cola_new);
    }
    return NULL;
}


t_pcb* siguiente_ready_pmcp() {
    // Nota: esta función debe ser llamada cuando ya se tiene el mutex de cola_new
    if (queue_is_empty(cola_new)) {
        return NULL;
    }
    
    // Convertir cola a lista para usar list_get_minimum
    t_list* lista_new = list_create();
    while (!queue_is_empty(cola_new)) {
        list_add(lista_new, queue_pop(cola_new));
    }
    
    // Comportamiento PMCP: encontrar el proceso más chico
    t_pcb* proceso_seleccionado = (t_pcb*)list_get_minimum(lista_new, comparar_tamaño_procesos);
    
    list_remove_element(lista_new, proceso_seleccionado);

    // Restaurar el resto de procesos a la cola NEW
    while (!list_is_empty(lista_new)) {
        queue_push(cola_new, list_remove(lista_new, 0));
    }
    
    list_destroy(lista_new);
    return proceso_seleccionado;
}