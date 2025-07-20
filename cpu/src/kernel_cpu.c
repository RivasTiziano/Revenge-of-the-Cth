
#include "../include/kernel_cpu.h"
#include "../include/cpu.h"

/**
 @fn atender_kernel_cpu_dispatch
 @brief Función específica para manejar las operaciones recibidas desde el socket de despacho del kernel.
 */

void atender_kernel_cpu_dispatch(t_log* cpu_logger) {
    while (corriendo_hilos) {
        int cod_op = recibir_operacion(socket_kernel_dispatch);

        switch (cod_op) {
            case HANDSHAKE:

            t_buffer* b_handshake_recv = recibir_buffer(socket_kernel_dispatch);
            int respuesta = extraer_int_del_buffer(b_handshake_recv);
            eliminar_buffer(b_handshake_recv);
            
            if(respuesta == RESULT_OK) {
                log_debug(cpu_logger, "\x1b[35mHANDSHAKE OK");
            }
            else {
                log_error(cpu_logger, "FALLO en HANDSHAKE");
                exit(-1);
            }
            break;
           
            case K_CPU_EXEC_PROCESO: 

                t_buffer* buffer = recibir_buffer(socket_kernel_dispatch);
                pid = extraer_int_del_buffer(buffer);
                pc = extraer_int_del_buffer(buffer);
                eliminar_buffer(buffer);

                log_debug(cpu_logger, "KERNEL me envio PID: %d y PC %d", pid, pc);

                fetch(cpu_logger); 
                            
            break;
            
            case -1:
                log_error(cpu_logger, "El kernel (%d) se desconecto. Terminando servidor", socket_kernel_dispatch);
                corriendo_hilos = false;
            break;

            default:
                log_error(cpu_logger, "Operacion desconocida de kernel (%d).", socket_kernel_dispatch);
            break;
        }
    }

    log_debug(cpu_logger, "FINALIZO HILO DISPATCH");
}

/**
 @fn atender_kernel_cpu_interrupt
 @brief Función específica para manejar las operaciones recibidas desde el socket de interrupciones del kernel.
 */
void atender_kernel_cpu_interrupt(t_log* cpu_logger) {
    while (corriendo_hilos) {
        int cod_op = recibir_operacion(socket_kernel_interrupt);
        switch (cod_op) {
            case HANDSHAKE:
                t_buffer* b_handshake_recv = recibir_buffer(socket_kernel_interrupt);
                int respuesta = extraer_int_del_buffer(b_handshake_recv);
                eliminar_buffer(b_handshake_recv);
                
                if(respuesta == RESULT_OK) {
                    log_debug(cpu_logger, "\x1b[35mHANDSHAKE OK");
                }
                else {
                    log_error(cpu_logger, "FALLO en HANDSHAKE");
                    exit(-1);
                }
            break;

            case K_CPU_INTERRUPT_PROCESO:
                t_buffer* buffer = recibir_buffer(socket_kernel_interrupt);
                interrupt = true;
                eliminar_buffer(buffer);
            break;

            case -1:
                log_error(cpu_logger, "El kernel (%d) se desconecto. Terminando servidor", socket_kernel_interrupt);
                corriendo_hilos = false;
            break;
            
            default:
                log_error(cpu_logger, "Operacion desconocida de kernel (%d).", socket_kernel_interrupt);
            break;
        }
    }

    log_debug(cpu_logger, "FINALIZO HILO INTERRUPT");
}





