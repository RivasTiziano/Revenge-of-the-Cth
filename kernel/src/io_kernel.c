#include "../include/io_kernel.h"
#include "../include/kernel.h"
#include "../include/syscalls.h"
#include <commons/log.h>
#include <unistd.h>

void* atender_io_kernel(void* arg){
    int socket = *((int*)arg);
    free(arg);
    bool control_key = 1;
    while (control_key) {
        int cod_op = recibir_operacion(socket);

        switch (cod_op) {
            case IO_K_HANDSHAKE: { 
                // Recibo nombre interfaz mediante handshake       
                t_buffer* buffer = recibir_buffer(socket);
                char* nombre_io = extraer_string_del_buffer(buffer);

                // Lo guardo en el diccionario de interfaces io
                recibir_io(nombre_io, socket);

                // Verifico si tiene procesos en cola para ejecutarlos
                ejecutar_siguiente(nombre_io);

                eliminar_buffer(buffer);
                free(nombre_io);

                break;
            }
            case IO_K_FINALIZO_IO: {
                t_buffer* buffer = recibir_buffer(socket);
                atender_fin_io(socket); //Solo atiendo la finalizacion
                // El planificador de corto plazo se ejecuta automaticamente en su hilo

                eliminar_buffer(buffer);
                break;
            }
            case -1: {
                atender_desconexion_io(socket);
                control_key = 0;
                break;
            }
            default: {
                log_error(kernel_logger, "Operacion desconocida de IO: %d", cod_op);
                break;
            }
        }
    }
    return NULL;
}
