#include "../../include/kernel_planificadores.h"
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/temporal.h>
#include <commons/string.h>


// ---------------------------------------- FUNCION PRINCIPAL -----------------------------------------

void* hilo_suspension_proceso(void* arg) {
    t_datos_suspension* datos = (t_datos_suspension*)arg;
    int pid = datos->pid;
    int tiempo_suspension = datos->tiempo_suspension;
    
    log_debug(kernel_logger, "Hilo de suspensión iniciado para proceso %d - esperando %d ms", 
             pid, tiempo_suspension);
    
    // Esperar el tiempo de suspensión (sin espera activa)
    usleep(tiempo_suspension * 1000);
    
    log_debug(kernel_logger, "Tiempo de suspensión cumplido para proceso %d - verificando estado", pid);
    
    // Verificar si el proceso sigue bloqueado usando mutex para thread safety
    pthread_mutex_lock(&mutex_lista_blocked);
    t_pcb* pcb = buscar_proceso_en_blocked(pid);
    
    if (pcb != NULL) {
        // El proceso sigue bloqueado -> suspenderlo
        log_debug(kernel_logger, "El proceso %d se suspende por exceso de tiempo bloqueado", pid);
        
        // Remover de lista_blocked
        int posicion = posicionDeProcesoEnLista(lista_blocked, pid);
        if (posicion != -1) {
            list_remove(lista_blocked, posicion);
        }
        pthread_mutex_unlock(&mutex_lista_blocked);
        
        // Agregar a lista_susp_blocked
        pthread_mutex_lock(&mutex_lista_susp_blocked);
        list_add(lista_susp_blocked, pcb);
        pthread_mutex_unlock(&mutex_lista_susp_blocked);
        
        // Cambiar estado del proceso
        cambiar_estado(pcb, SUSP_BLOCKED);
        
        // Notificar a memoria para suspender el proceso
        avisoParaMemoria(pcb->PID);
        
    } else {
        // El proceso ya no está bloqueado (fue desbloqueado antes del tiempo límite)
        pthread_mutex_unlock(&mutex_lista_blocked);
        log_debug(kernel_logger, "Proceso %d ya fue desbloqueado antes del tiempo límite - hilo terminando", pid);
    }
    
    // Liberar memoria de los datos
    free(datos);
    
    log_debug(kernel_logger, "Hilo de suspensión para proceso %d terminado", pid);
    return NULL;
}



// ---------------------------------------- FUNCIONES AUXILIARES -----------------------------------------

t_pcb* buscar_proceso_en_blocked(int pid) {
    // NOTA: Esta función debe ser llamada CON el mutex de lista_blocked ya tomado
    for (int i = 0; i < list_size(lista_blocked); i++) {
        t_pcb* pcb = list_get(lista_blocked, i);
        if (pcb->PID == pid) {
            return pcb;
        }
    }
    return NULL;
}

int posicionDeProcesoEnLista (t_list* lista, int id) {
    int i;
    t_pcb* pcb;
    for (i=0; i<list_size(lista); i++) {
        pcb = list_get(lista,i);
        if (pcb->PID==id) return i;
    }
    return -1;
}


void agregar_a_susp_ready(t_pcb* pcb) {
    // Validación de parámetros
    if (pcb == NULL) {
        log_error(kernel_logger, "Error: PCB nulo en agregar_a_susp_ready");
        return;
    }
    
    if (cola_susp_ready == NULL) {
        log_error(kernel_logger, "Error: cola_susp_ready no inicializada");
        return;
    }
    
    // Remover de lista_susp_blocked si está ahí
    pthread_mutex_lock(&mutex_lista_susp_blocked);
    int posicion = posicionDeProcesoEnLista(lista_susp_blocked, pcb->PID);
    if (posicion != -1) {
        list_remove(lista_susp_blocked, posicion);
    }
    pthread_mutex_unlock(&mutex_lista_susp_blocked);
    
    // Cambiar estado a SUSP_READY
    cambiar_estado(pcb, SUSP_READY);
    
    log_debug(kernel_logger, "Mediano plazo: Proceso %d movido a SUSP_READY", pcb->PID);
    
    // Intentar reanudar el proceso directamente desde el planificador de mediano plazo
    bool se_pudo_reanudar = intentar_reanudar_proceso(pcb);
    
    if (!se_pudo_reanudar) {
        // Si no se pudo reanudar, agregarlo a la cola SUSP_READY para que el planificador de largo plazo lo procese más tarde
        log_debug(kernel_logger, "Mediano plazo: No se pudo reanudar proceso %d directamente, agregando a SUSP_READY", pcb->PID);
        
        pthread_mutex_lock(&mutex_cola_susp_ready);
        queue_push(cola_susp_ready, pcb);
        pthread_mutex_unlock(&mutex_cola_susp_ready);
        
        // Despertar al planificador de largo plazo para que lo procese cuando sea posible
        despertar_planificador_largo_plazo();
    } else {
        log_debug(kernel_logger, "Mediano plazo: Proceso %d reanudado directamente desde SUSP_BLOCKED a READY", pcb->PID);
    }
}


void avisoParaMemoria (int numeroProceso) {
    //Tengo que crear de nuevo la conexion con memoria porque es efimera
    int socketcito = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (socketcito == -1) {
        log_error(kernel_logger, "Fallo al conectar con Memoria");
        return;
    }
    
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, numeroProceso);
    t_paquete* paquete = crear_paquete(K_M_SUSPENDER_PROCESO, buffer);
    enviar_paquete(paquete, socketcito);
    
    int cod_op = recibir_operacion(socketcito);
    
    if (cod_op == M_K_PROCESO_SUSPENDIDO) { 

        t_buffer* buffer_rta = recibir_buffer(socketcito);
        int respuesta = extraer_int_del_buffer(buffer_rta);
        eliminar_buffer(buffer_rta);
        
        if (respuesta == 0) {
            log_debug(kernel_logger, "Memoria confirmó suspensión del proceso %d", numeroProceso);
        }
        else {
            log_error(kernel_logger, "Error al suspender proceso %d en memoria", numeroProceso);
        }
    } else {
        log_error(kernel_logger, "Operacion No Esperada");
    }
}


// Función para obtener el mejor proceso de READY sin extraerlo
t_pcb* obtener_mejor_proceso_ready() {
    if (queue_is_empty(cola_ready)) {
        return NULL;
    }
    
    // Convertir cola a lista para usar list_get_minimum sin modificar la cola
    t_list* lista_ready = list_create();
    t_queue* temp_queue = queue_create();
    
    // Copiar todos los procesos a lista temporal
    while (!queue_is_empty(cola_ready)) {
        t_pcb* proceso = queue_pop(cola_ready);
        list_add(lista_ready, proceso);
        queue_push(temp_queue, proceso); // También guardar para restaurar
    }
    
    // Encontrar el proceso con menor estimación
    t_pcb* mejor_proceso = (t_pcb*)list_get_minimum(lista_ready, comparar_estimacion_procesos);
    
    // Restaurar la cola READY
    while (!queue_is_empty(temp_queue)) {
        queue_push(cola_ready, queue_pop(temp_queue));
    }
    
    // Limpiar estructuras temporales
    list_destroy(lista_ready);
    queue_destroy(temp_queue);
    
    return mejor_proceso;
}


void actualizar_estimacion(t_pcb* pcb) {
    int64_t rafaga_actual = temporal_gettime(pcb->tiempo_ejecutado_cpu); // En milisegundos
    
    // Calcular nueva estimación usando aritmética de punto flotante
    double nueva_estimacion = ALFA * (double)rafaga_actual + (1.0 - ALFA) * (double)pcb->estimacion_actual;
    pcb->estimacion_actual = (int64_t)nueva_estimacion;
}