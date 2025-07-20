#include "../include/cpu_kernel_interrupt.h"
#include "../include/kernel.h"
#include "../include/kernel_planificadores.h"
#include "../include/syscalls.h"

void* atender_cpu_kernel_interrupt(void* arg) {
    int socket_cpu_interrupt = *((int*)arg);
    free(arg);
    
    bool control_key = 1;
    while (control_key) {
        int cod_op = recibir_operacion(socket_cpu_interrupt);
            switch (cod_op) {
            case MENSAJE:

            break;

            case HANDSHAKE: // Recibo nombre interfaz 
            t_buffer* b_handshake_recv = recibir_buffer(socket_cpu_interrupt);
            int hand_recibido = extraer_int_del_buffer(b_handshake_recv);
            char* cpu_id = extraer_string_del_buffer(b_handshake_recv);
            eliminar_buffer(b_handshake_recv);

            t_buffer* b_handshake_respuesta = crear_buffer();

            if (hand_recibido == HAND_CPU_KERNEL_INT) {
                log_debug(kernel_logger, "ID de la CPU conectada: %s", cpu_id);

                t_cpu* cpu = dictionary_get(cpus, cpu_id);

                if (cpu == NULL) {
                    // No existe aún, la creamos
                    cpu = malloc(sizeof(t_cpu));
                    cpu->pcb = NULL;
                    dictionary_put(cpus, cpu_id, cpu); // Agregamos al diccionario
                }

                // Ya sea nueva o existente, actualizamos el socket de interrupt
                cpu->socket_interrupt = socket_cpu_interrupt;

                cargar_int_al_buffer(b_handshake_respuesta, RESULT_OK);
            }

            else {
                log_error(kernel_logger, "No se pudo completar el HANDSHAKE con la CPU: %s", cpu_id);
                log_error(kernel_logger, "Valor de HANDSHAKE recibido: %d, se esperaba: %d", hand_recibido, HAND_CPU_KERNEL_INT);

                cargar_int_al_buffer(b_handshake_respuesta, RESULT_ERROR);
            }

            //Envio la respuesta del HAND
            t_paquete* paquete = crear_paquete(HANDSHAKE, b_handshake_respuesta);
            enviar_paquete(paquete, socket_cpu_interrupt);

            //libero memoria
            free(cpu_id);

            break;

            case PAQUETE:

                break;
            case -1:
                log_warning(kernel_logger, "El CPU se desconecto");
                control_key = false;
                break;
            default:
                log_warning(kernel_logger,"Operacion desconocida de CPU Interrupt.");
                break;
        }
    }
    return NULL;
}