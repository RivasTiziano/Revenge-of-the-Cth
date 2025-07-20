#include "kernel_planificadores.h"
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <commons/temporal.h>
#include <commons/string.h>

 

void iniciar_planificadores() {
    planificador_activo = true;
    
    if (pthread_create(&hilo_plp, NULL, hilo_planificador_largo_plazo, NULL) != 0) {
        log_error(kernel_logger, "Error al crear hilo del planificador de largo plazo");
        planificador_activo = false;
        return;
    }
    
    if (pthread_create(&hilo_pcp, NULL, hilo_planificador_corto_plazo, NULL) != 0) {
        log_error(kernel_logger, "Error al crear hilo del planificador de corto plazo");
        planificador_activo = false;
        pthread_cancel(hilo_plp);
        return;
    }
    
    log_debug(kernel_logger, "Planificadores iniciados en hilos separados");
}


void* hilo_planificador_largo_plazo(void* arg) {
    log_debug(kernel_logger, "Planificador de largo plazo iniciado en hilo separado");
    
    pthread_mutex_lock(&mutex_planificador_largo_plazo);
    
    while (planificador_activo) {   
        // Verificar si hay trabajo que hacer
        pthread_mutex_lock(&mutex_cola_susp_ready);
        bool hay_susp_ready = !queue_is_empty(cola_susp_ready);
        pthread_mutex_unlock(&mutex_cola_susp_ready);
        
        pthread_mutex_lock(&mutex_cola_new);
        bool hay_new = !queue_is_empty(cola_new);
        pthread_mutex_unlock(&mutex_cola_new);
        
        if (hay_susp_ready || hay_new) {
            // Hay trabajo que hacer, ejecutar planificador
            pthread_mutex_unlock(&mutex_planificador_largo_plazo);
            
            if (hay_susp_ready) {
                planificador_largo_plazo();
            }
            else if (hay_new) {
                planificador_largo_plazo();
            }
            
            pthread_mutex_lock(&mutex_planificador_largo_plazo);
        } else {
            // No hay trabajo, esperar hasta ser señalizado
            log_debug(kernel_logger, "Planificador largo plazo: No hay trabajo, esperando...");
            pthread_cond_wait(&cond_planificador_largo_plazo, &mutex_planificador_largo_plazo);
            log_debug(kernel_logger, "Planificador largo plazo: Despertado, verificando trabajo...");
        }
    }
    
    pthread_mutex_unlock(&mutex_planificador_largo_plazo);
    log_debug(kernel_logger, "Planificador de largo plazo terminado");
    return NULL;
}


void* hilo_planificador_corto_plazo(void* arg) {
    log_debug(kernel_logger, "Planificador de corto plazo iniciado en hilo separado");
    
    pthread_mutex_lock(&mutex_planificador_corto_plazo);
    
    while (planificador_activo) {     
        if (!queue_is_empty(cola_ready)) {
            // Hay procesos listos para ejecutar
            pthread_mutex_unlock(&mutex_planificador_corto_plazo);
            planificador_corto_plazo();
            pthread_mutex_lock(&mutex_planificador_corto_plazo);
        } else {
            // No hay procesos listos, esperar hasta ser señalizado
            pthread_cond_wait(&cond_planificador_corto_plazo, &mutex_planificador_corto_plazo);
        }
    }
    
    pthread_mutex_unlock(&mutex_planificador_corto_plazo);
    log_debug(kernel_logger, "Planificador de corto plazo terminado");
    return NULL;
}


void detener_planificadores() {
    if (planificador_activo) {
        planificador_activo = false;
        
        // Despertar a los planificadores para que puedan terminar
        pthread_cond_signal(&cond_planificador_largo_plazo);
        pthread_cond_signal(&cond_planificador_corto_plazo);
        
        pthread_join(hilo_plp, NULL);
        pthread_join(hilo_pcp, NULL);
        
        log_debug(kernel_logger, "Planificadores detenidos");
    }
}


void planificador_mediano_plazo(t_pcb* pcb) {
    // Crear estructura de datos para pasar al hilo
    t_datos_suspension* datos = malloc(sizeof(t_datos_suspension));
    datos->pid = pcb->PID;
    datos->tiempo_suspension = TIEMPO_SUSPENSION;
    
    log_debug(kernel_logger, "Creando hilo de suspensión para el proceso %d", pcb->PID);
    
    // Crear hilo que manejará la suspensión de este proceso específico
    pthread_t hilo_suspension;
    if (pthread_create(&hilo_suspension, NULL, hilo_suspension_proceso, (void*)datos) != 0) {
        log_error(kernel_logger, "Error al crear hilo de suspensión para PID %d", pcb->PID);
        free(datos);
        return;
    }
    
    // Detach el hilo para que se auto-libere cuando termine
    pthread_detach(hilo_suspension);
}



// ===============================================
// FUNCIONES DE MÉTRICAS Y LOGS
// ===============================================

// Función para mostrar métricas de un proceso al finalizar
void mostrar_metricas_proceso(t_pcb* pcb) {
    if (pcb == NULL) {
        log_error(kernel_logger, "Error: PCB nulo en mostrar_metricas_proceso");
        return;
    }
    
    log_info(kernel_logger, "## PID: %d - Métricas del proceso", pcb->PID);
    
    // Mostrar métricas de entradas a cada estado
    char* estado_nombres[] = {"NEW", "READY", "EXEC", "BLOCKED", "SUSP_READY", "SUSP_BLOCKED", "EXIT"};
    for (int i = 0; i < NUM_ESTADOS; i++) {
        if (pcb->ME[i] > 0) {
            log_info(kernel_logger, "  Estado %s: %d entradas", estado_nombres[i], pcb->ME[i]);
        }
    }
    
    // Mostrar tiempos en cada estado (si están disponibles)
    for (int i = 0; i < NUM_ESTADOS; i++) {
        if (pcb->MT[i] != NULL) {
            int64_t tiempo = temporal_gettime(pcb->MT[i]);
            if (tiempo > 0) {
                log_info(kernel_logger, "  Tiempo en %s: %ld ms", estado_nombres[i], tiempo);
            }
        }    }
}

void cambiar_estado(t_pcb* pcb, t_estado nuevo_estado) {
    if (pcb == NULL) {
        log_error(kernel_logger, "Error: PCB nulo en cambiar_estado");
        return;
    }

    // Validar que el nuevo estado esté en rango válido
    if (nuevo_estado < 0 || nuevo_estado >= NUM_ESTADOS) {
        log_error(kernel_logger, "Error: Estado inválido %d para PID %d", nuevo_estado, pcb->PID);
        return;
    }

    // Obtener el estado anterior
    t_estado estado_anterior = pcb->estado_actual;
    
    // Solo hacer el cambio si es necesario
    if (estado_anterior == nuevo_estado) {
        return;
    }
    
    // Validar que el estado anterior esté en rango válido
    if (estado_anterior >= 0 && estado_anterior < NUM_ESTADOS) {
        // Detener el cronómetro del estado anterior (si existe)
        if (pcb->MT[estado_anterior] != NULL) {
            temporal_stop(pcb->MT[estado_anterior]);
        }
    }
    
    // Actualizar el estado actual del PCB
    pcb->estado_actual = nuevo_estado;
    
    // Incrementar el contador de entradas al nuevo estado
    pcb->ME[nuevo_estado]++;
    
    // Crear/reanudar cronómetro para el nuevo estado
    if (pcb->MT[nuevo_estado] == NULL) {
        pcb->MT[nuevo_estado] = temporal_create();
    } else {
        temporal_resume(pcb->MT[nuevo_estado]);
    }
    
    // Log de transición de estado
    char* estado_nombres[] = {"NEW", "READY", "EXEC", "BLOCKED", "SUSPEND_READY", "SUSPEND_BLOCKED", "EXIT"};
    if (estado_anterior >= 0 && estado_anterior < NUM_ESTADOS) {
        log_cambio_estado(pcb->PID, estado_nombres[estado_anterior], estado_nombres[nuevo_estado]);
    }
}

// ===============================================
// FUNCIONES AUXILIARES DE PLANIFICACIÓN
// ===============================================

// Función comparadora para encontrar el proceso con menor tamaño
void* comparar_tamaño_procesos(void* proceso1, void* proceso2) {
    t_pcb* pcb1 = (t_pcb*)proceso1;
    t_pcb* pcb2 = (t_pcb*)proceso2;
    
    // Retorna el proceso con menor tamaño
    return (pcb1->tamanio <= pcb2->tamanio) ? proceso1 : proceso2;
}

// Función comparadora para encontrar el proceso con menor estimación
void* comparar_estimacion_procesos(void* proceso1, void* proceso2) {
    t_pcb* pcb1 = (t_pcb*)proceso1;
    t_pcb* pcb2 = (t_pcb*)proceso2;
    
    // Retorna el proceso con menor estimación
    return (pcb1->estimacion_actual <= pcb2->estimacion_actual) ? proceso1 : proceso2;
}

// ===============================================
// FUNCIONES PARA SEÑALIZAR PLANIFICADORES (ELIMINAR ESPERAS ACTIVAS)
// ===============================================

void despertar_planificador_largo_plazo() {
    pthread_cond_signal(&cond_planificador_largo_plazo);
}

void despertar_planificador_corto_plazo() {
    pthread_cond_signal(&cond_planificador_corto_plazo);
}

// ===============================================
// FUNCIÓN COMPARTIDA PARA REANUDAR PROCESOS (CON MUTEX DE PROTECCIÓN)
// ===============================================

// Mutex para proteger las operaciones de reanudación y evitar concurrencia
static pthread_mutex_t mutex_reanudar_proceso = PTHREAD_MUTEX_INITIALIZER;

/**
 * Función que intenta reanudar un proceso desde SWAP a memoria principal.
 * Puede ser llamada tanto desde el planificador de largo plazo como desde el de mediano plazo.
 * Usa un mutex estático para evitar concurrencia y deadlocks.
 * 
 * @param pcb El PCB del proceso a reanudar
 * @return true si el proceso se pudo reanudar exitosamente, false en caso contrario
 */
bool intentar_reanudar_proceso(t_pcb* pcb) {
    if (pcb == NULL) {
        log_error(kernel_logger, "Error: PCB nulo en intentar_reanudar_proceso");
        return false;
    }
    
    // Usar mutex para evitar que múltiples hilos intenten reanudar procesos concurrentemente
    if (pthread_mutex_trylock(&mutex_reanudar_proceso) != 0) {
        // Si no puede obtener el mutex, significa que otro hilo está reanudando un proceso
        log_debug(kernel_logger, "Otro hilo está reanudando un proceso, proceso %d quedará en SUSP_READY", pcb->PID);
        return false;
    }
    
    // Crear conexión efímera con memoria
    int socket_reanudar_proceso = crear_conexion(IP_MEMORIA, PUERTO_MEMORIA);
    if (socket_reanudar_proceso == -1) {
        log_error(kernel_logger, "Fallo al conectar con Memoria para reanudar proceso %d", pcb->PID);
        pthread_mutex_unlock(&mutex_reanudar_proceso);
        return false;
    }
    
    log_debug(kernel_logger, "Intentando reanudar proceso PID: %d desde SWAP", pcb->PID);
    
    // Enviar solicitud de reanudación a memoria
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, pcb->PID);
    t_paquete* paquete = crear_paquete(K_M_REANUDAR_PROCESO, buffer);
    enviar_paquete(paquete, socket_reanudar_proceso);
    
    // Recibir respuesta de memoria
    int cod_op = recibir_operacion(socket_reanudar_proceso);
    bool se_pudo_reanudar = false;
    
    if (cod_op == M_K_PROCESO_REANUDADO) {
        t_buffer* buffer_rta = recibir_buffer(socket_reanudar_proceso);
        se_pudo_reanudar = extraer_int_del_buffer(buffer_rta);
        eliminar_buffer(buffer_rta);
        
        if (se_pudo_reanudar) {
            log_debug(kernel_logger, "Proceso %d reanudado exitosamente desde SWAP", pcb->PID);
            
            // Cambiar estado a READY y agregarlo a la cola READY
            cambiar_estado(pcb, READY);
            pthread_mutex_lock(&mutex_cola_ready);
            queue_push(cola_ready, pcb);
            pthread_mutex_unlock(&mutex_cola_ready);
            
            // Despertar al planificador de corto plazo
            despertar_planificador_corto_plazo();
        } else {
            log_debug(kernel_logger, "No hay espacio suficiente para reanudar el proceso %d desde SWAP", pcb->PID);
        }
    } else {
        log_error(kernel_logger, "Operación no esperada al reanudar proceso %d: cod_op = %d", pcb->PID, cod_op);
    }
    
    // Cerrar conexión efímera
    close(socket_reanudar_proceso);
    
    // Liberar mutex
    pthread_mutex_unlock(&mutex_reanudar_proceso);
    
    return se_pudo_reanudar;
}