#include "../include/io_kernel.h"
#include "../include/kernel.h"
#include <commons/log.h>
#include <unistd.h>
#include "kernel_communications.h"

void* escuchar_io_kernel() {

    ios = dictionary_create();
    dispositivos = dictionary_create();

    while (1) {
        int socket_io_nuevo = esperar_cliente(socket_kernel_io, kernel_logger, "Nuevo I/O conectado a KERNEL");

        if (socket_io_nuevo == -1) {
            log_error(kernel_logger, "Fallo al aceptar una conexión IO.");
            continue;
        }

        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_io_nuevo;

        pthread_t hilo_io;
        pthread_create(&hilo_io, NULL, atender_io_kernel, socket_ptr);
        pthread_detach(hilo_io);

    }
}

void* escuchar_cpu_dispatch() {
    while (1) {
        int socket_nuevo = esperar_cliente(socket_kernel_dispatch, kernel_logger, "CPU Dispatch");

        if (socket_nuevo == -1) {
            log_error(kernel_logger, "Fallo al aceptar una conexión CPU Dispatch.");
            continue;
        }

        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_nuevo;

        pthread_t hilo_cpu;
        pthread_create(&hilo_cpu, NULL, atender_cpu_kernel_dispatch, socket_ptr);
        pthread_detach(hilo_cpu);
    }
}

void* escuchar_cpu_interrupt() {
    while (1) {
        int socket_nuevo = esperar_cliente(socket_kernel_interrupt, kernel_logger, "CPU Interrupt");

        if (socket_nuevo == -1) {
            log_error(kernel_logger, "Fallo al aceptar una conexión CPU Interrupt.");
            continue;
        }

        int* socket_ptr = malloc(sizeof(int));
        *socket_ptr = socket_nuevo;

        int cod_op = recibir_operacion(socket_nuevo);
        if (cod_op == HANDSHAKE) {

            t_buffer* b_handshake_recv = recibir_buffer(socket_nuevo);
            int hand_recibido = extraer_int_del_buffer(b_handshake_recv);
            char* cpu_id = extraer_string_del_buffer(b_handshake_recv);
            free(b_handshake_recv);

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
                cpu->socket_interrupt = socket_nuevo;

                cargar_int_al_buffer(b_handshake_respuesta, RESULT_OK);
            }

            else {
                log_error(kernel_logger, "No se pudo completar el HANDSHAKE con la CPU: %s", cpu_id);
                log_error(kernel_logger, "Valor de HANDSHAKE recibido: %d, se esperaba: %d", hand_recibido, HAND_CPU_KERNEL_INT);

                cargar_int_al_buffer(b_handshake_respuesta, RESULT_ERROR);
            }

            //Envio la respuesta del HAND
            t_paquete* paquete = crear_paquete(HANDSHAKE, b_handshake_respuesta);
            enviar_paquete(paquete, socket_nuevo);

            //libero memoria
            free(cpu_id);
        }         
    free(socket_ptr);
    }
}