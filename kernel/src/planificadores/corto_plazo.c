#include "../../include/kernel_planificadores.h"
#include "../../include/logs.h"
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/temporal.h>
#include <commons/string.h>


// Variable global temporal para utilizar la función de búsqueda de CPU libre
t_cpu* cpu_libre_temp = NULL;

// Variables globales temporales para encontrar CPU con mayor tiempo restante
t_cpu* cpu_mayor_tiempo_temp = NULL;
int mayor_tiempo_restante_temp = -1;

/**
 * @brief Busca una CPU libre. Es una función a ser llamada con dictionary_iterator().
*/
 void buscar_cpu_libre(char* key, void* value) {
    if (cpu_libre_temp != NULL) return; 

    t_cpu* cpu = (t_cpu*) value;
    if (cpu->pcb == NULL) {
        cpu_libre_temp = cpu;
    }

}

/**
 * @brief Busca la CPU con mayor tiempo restante. Es una función a ser llamada con dictionary_iterator().
*/
void buscar_mayor_tiempo(char* key, void* value) {
    t_cpu* cpu = (t_cpu*)value;
    if (cpu->pcb != NULL) {
        int tiempo_restante = calcular_tiempo_restante(cpu->pcb);
        if (tiempo_restante > mayor_tiempo_restante_temp) {
            mayor_tiempo_restante_temp = tiempo_restante;
            cpu_mayor_tiempo_temp = cpu;
        }
    }
}


// ---------------------------------------- FUNCION PRINCIPAL -----------------------------------------

void planificador_corto_plazo(){
    t_pcb* pcb = NULL;
    
    // ----- 1. Verificar si hay procesos en READY esperando a poder ejecutar -----

    // 1.1 No hay procesos en la cola READY -> No hay nada que hacer, return
    pthread_mutex_lock(&mutex_cola_ready);
    
    if (queue_is_empty(cola_ready)) {
        pthread_mutex_unlock(&mutex_cola_ready);
        return;
    }

    // ----- 2. Verificar si hay CPUs libres ANTES de seleccionar proceso -----
    pthread_mutex_lock(&mutex_cpus);
    cpu_libre_temp = NULL; // Resetear variable global
    dictionary_iterator(cpus, buscar_cpu_libre); // Si hay CPU libre, se guarda en cpu_libre_temp
    
    bool hay_cpu_libre = (cpu_libre_temp != NULL);
    pthread_mutex_unlock(&mutex_cpus);

    // Si no hay CPU libre y el algoritmo no permite desalojo, no hacer nada
    if (!hay_cpu_libre && (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") == 0 || strcmp(ALGORITMO_CORTO_PLAZO, "SJF") == 0)) {
        pthread_mutex_unlock(&mutex_cola_ready);
        return;
    }

    // 1.2 Hay procesos en la cola READY -> Obtengo el próximo a ejecutar según algoritmo (FIFO o SJF/SRT)
    if (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") == 0) {
        pcb = siguiente_exec_fifo();
    } else {
        pcb = siguiente_exec_sjf(); // Aplica para SJF y SRT
    }
    pthread_mutex_unlock(&mutex_cola_ready);

    if (pcb == NULL) {
        return;
    }

    // ----- 3. Para SRT, evaluar desalojo SIEMPRE antes de asignar CPU -----
    if (strcmp(ALGORITMO_CORTO_PLAZO, "SRT") == 0) {
        pthread_mutex_lock(&mutex_cpus);
        
        // Buscar CPU con mayor tiempo restante para evaluar desalojo
        t_cpu* cpu_candidata_desalojo = encontrar_cpu_con_mayor_tiempo_restante();
        
        if (cpu_candidata_desalojo != NULL && cpu_candidata_desalojo->pcb != NULL) {
            int64_t tiempo_ejecutado = temporal_gettime(cpu_candidata_desalojo->pcb->tiempo_ejecutado_cpu);
            int64_t rafaga_restante = cpu_candidata_desalojo->pcb->estimacion_actual - tiempo_ejecutado;

            if(pcb->estimacion_actual < rafaga_restante){
                // El nuevo proceso tiene menor tiempo -> DESALOJAR
                log_debug(kernel_logger, "## SRT: Desalojando PID %d (tiempo restante: %ld) por PID %d (estimación: %ld)", 
                         cpu_candidata_desalojo->pcb->PID, rafaga_restante, pcb->PID, pcb->estimacion_actual);
                desalojar_proceso(cpu_candidata_desalojo, pcb);
                pthread_mutex_unlock(&mutex_cpus);
                return;
            }
        }
        pthread_mutex_unlock(&mutex_cpus);
    }

    // ----- 4. Asignar CPU o devolver a READY -----
    pthread_mutex_lock(&mutex_cpus);

    // 4.1 Hay CPU libre
    if(cpu_libre_temp) {

        cambiar_estado(pcb, EXEC);
        temporal_destroy(pcb->tiempo_ejecutado_cpu);
        pcb->tiempo_ejecutado_cpu = temporal_create();
        cpu_libre_temp->pcb = pcb; // Se cambia la referencia de la CPU actual al nuevo proceso a ejecutar
        pthread_mutex_unlock(&mutex_cpus);

        t_buffer* buffer = crear_buffer();
        cargar_int_al_buffer(buffer, pcb->PID);
        cargar_int_al_buffer(buffer, pcb->PC);

        t_paquete* paquete = crear_paquete(K_CPU_EXEC_PROCESO, buffer);

        log_debug(kernel_logger, "CPU libre encontrada - socket_dispatch = %d", cpu_libre_temp->socket_dispatch);

        enviar_paquete(paquete, cpu_libre_temp->socket_dispatch);

        return;
    } else {
        // 3.2 No hay CPU libre

        // 3.2.1. FIFO o SJF: Toca esperar - devolver el proceso a READY
        if (strcmp(ALGORITMO_CORTO_PLAZO, "FIFO") == 0 || strcmp(ALGORITMO_CORTO_PLAZO, "SJF") == 0){
            pthread_mutex_unlock(&mutex_cpus);
            pthread_mutex_lock(&mutex_cola_ready);
            queue_push(cola_ready, pcb);
            pthread_mutex_unlock(&mutex_cola_ready);
            return;
        } else {

        // 3.2.2 SRT: Se desaloja
            
        // 4. Comparo ráfagas y determino si la ráfaga de mayor duración de un proceso ejecutando es mayor a la estimada del proceso entrante
            t_cpu* cpu_candidata_desalojo = encontrar_cpu_con_mayor_tiempo_restante(); // CPU candidata a ser desalojada
            
            if (cpu_candidata_desalojo != NULL && cpu_candidata_desalojo->pcb != NULL) {
                int64_t tiempo_ejecutado_cpu = temporal_gettime(cpu_candidata_desalojo->pcb->tiempo_ejecutado_cpu);
                int64_t rafaga_restante = cpu_candidata_desalojo->pcb->estimacion_actual - tiempo_ejecutado_cpu;

                if(pcb->estimacion_actual < rafaga_restante){
                // 4.1. Es menor (nueva estimación es mejor):
                    // Envío petición de desalojo a CPU (a través de socket Interrupt)
                    desalojar_proceso(cpu_candidata_desalojo, pcb);
                    pthread_mutex_unlock(&mutex_cpus);

                } else {
                // 4.2. Es mayor o igual:
                    // No desalojo. Devolver el proceso a READY
                    pthread_mutex_unlock(&mutex_cpus);
                    pthread_mutex_lock(&mutex_cola_ready);
                    queue_push(cola_ready, pcb);
                    pthread_mutex_unlock(&mutex_cola_ready);
                }
            } else {
                // No hay procesos ejecutándose, devolver a READY
                pthread_mutex_unlock(&mutex_cpus);
                pthread_mutex_lock(&mutex_cola_ready);
                queue_push(cola_ready, pcb);
                pthread_mutex_unlock(&mutex_cola_ready);
            }
        }
    }
}




// ---------------------------------------- FUNCIONES AUXILIARES -----------------------------------------


t_pcb* siguiente_exec_fifo() {
    // Nota: esta función debe ser llamada cuando ya se tiene el mutex de cola_ready
    if (queue_is_empty(cola_ready)) {
        return NULL;
    }
    
    t_pcb* pcb = queue_pop(cola_ready);
    return pcb;
}



t_pcb* siguiente_exec_sjf() {
    // Nota: esta función debe ser llamada cuando ya se tiene el mutex de cola_ready
    if (queue_is_empty(cola_ready)) {
        return NULL;
    }
    
    int num_procesos = queue_size(cola_ready);
    if (num_procesos == 1) {
        // Si solo hay un proceso, no necesitamos logs de selección
        return queue_pop(cola_ready);
    }
    
    // Convertir cola a lista para usar list_get_minimum
    t_list* lista_ready = list_create();
    while (!queue_is_empty(cola_ready)) {
        t_pcb* proceso = queue_pop(cola_ready);
        list_add(lista_ready, proceso);
    }
    
    // Encontrar el proceso con menor estimación usando la función de la biblioteca
    t_pcb* proceso_menor_estimacion = (t_pcb*)list_get_minimum(lista_ready, comparar_estimacion_procesos);
    
    // Remover el proceso seleccionado de la lista
    list_remove_element(lista_ready, proceso_menor_estimacion);
    
    // Restaurar el resto de procesos a la cola READY
    while (!list_is_empty(lista_ready)) {
        queue_push(cola_ready, list_remove(lista_ready, 0));
    }
    
    list_destroy(lista_ready);
    return proceso_menor_estimacion;
}



// Funciones SRT (Shortest Remaining Time) para desalojo

t_cpu* encontrar_cpu_con_mayor_tiempo_restante() {
    cpu_mayor_tiempo_temp = NULL;
    mayor_tiempo_restante_temp = -1;
    
    dictionary_iterator(cpus, buscar_mayor_tiempo);
    return cpu_mayor_tiempo_temp;
}



int calcular_tiempo_restante(t_pcb* pcb) {
    if (pcb == NULL || pcb->tiempo_ejecutado_cpu == NULL) {
        return 0;
    }
    
    int64_t tiempo_ejecutado_cpu = temporal_gettime(pcb->tiempo_ejecutado_cpu);
    int64_t tiempo_restante = pcb->estimacion_actual - tiempo_ejecutado_cpu;
    
    return (tiempo_restante > 0) ? (int)tiempo_restante : 0;
}



void desalojar_proceso(t_cpu* cpu_a_desalojar, t_pcb* nuevo_proceso) {

    t_pcb* proceso_desalojado = cpu_a_desalojar->pcb;
    
    // Log de desalojo usando la función correcta
    log_desalojo(proceso_desalojado->PID, "SJF/SRT");
    
    log_debug(kernel_logger, "## SRT: Enviando interrupción a CPU para desalojar PID %d", 
             proceso_desalojado->PID);
    
    // Enviar interrupción a la CPU
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, 1); // Señal de interrupción
    
    t_paquete* paquete = crear_paquete(K_CPU_INTERRUPT_PROCESO, buffer);
    enviar_paquete(paquete, cpu_a_desalojar->socket_interrupt);

    // Esperar respuesta de la CPU con el PC actualizado
    int cod_op = recibir_operacion(cpu_a_desalojar->socket_interrupt);
    if (cod_op == CPU_K_INTERRUPT_PROCESO) {
        t_buffer* buffer_resp = recibir_buffer(cpu_a_desalojar->socket_interrupt);

        int pc_actualizado = extraer_int_del_buffer(buffer_resp);
        
        // Actualizar el PC del proceso desalojado
        proceso_desalojado->PC = pc_actualizado;
        
        log_debug(kernel_logger, "## SRT: Proceso PID %d desalojado con PC actualizado: %d", 
                 proceso_desalojado->PID, pc_actualizado);
        
        // Liberar el buffer de respuesta
        eliminar_buffer(buffer_resp);
    }
    
    // Cambiar el estado del proceso desalojado a READY
    cambiar_estado(proceso_desalojado, READY);
    
    // IMPORTANTE: NO actualizar estimación aquí - solo cuando termina la ráfaga completa
    // La estimación se actualiza solo en syscalls (I/O, DUMP, EXIT)
    
    // Encolar el proceso desalojado de nuevo en READY
    pthread_mutex_lock(&mutex_cola_ready);
    queue_push(cola_ready, proceso_desalojado);
    pthread_mutex_unlock(&mutex_cola_ready);
    
    // Asignar el nuevo proceso a la CPU
    cpu_a_desalojar->pcb = nuevo_proceso;
    
    // Cambiar el estado del nuevo proceso a EXEC
    cambiar_estado(nuevo_proceso, EXEC);
    
    // Iniciar la rafaga del nuevo proceso
    temporal_destroy(nuevo_proceso->tiempo_ejecutado_cpu);
    nuevo_proceso->tiempo_ejecutado_cpu = temporal_create();

    t_buffer* buffer_despachar = crear_buffer();
    cargar_int_al_buffer(buffer_despachar, nuevo_proceso->PID);
    cargar_int_al_buffer(buffer_despachar, nuevo_proceso->PC);

    t_paquete* paquete_despachar = crear_paquete(K_CPU_EXEC_PROCESO, buffer_despachar);
    enviar_paquete(paquete_despachar, cpu_a_desalojar->socket_dispatch);
}
