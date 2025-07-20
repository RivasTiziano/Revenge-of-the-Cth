#include "../include/kernel_io.h"
#include "../include/io.h"
#include <pthread.h>

// Estructura para pasar datos al hilo de I/O
typedef struct {
    int pid;
    int tiempo_io;
} t_datos_io;

// Función que ejecuta la operación de I/O en un hilo separado
void* ejecutar_io_en_hilo(void* arg) {
    t_datos_io* datos = (t_datos_io*)arg;
    int pid = datos->pid;
    int tiempo_io = datos->tiempo_io;
    
    log_info(io_logger, "## PID: %d - Inicio de IO - Tiempo: %d", pid, tiempo_io);
    
    // Ejecutar el sleep en este hilo separado
    usleep(tiempo_io * 1000);
    
    log_info(io_logger, "## PID: %d - Fin de IO", pid);
    
    // Crear respuesta y enviar al kernel
    t_buffer* respuesta = crear_buffer();
    cargar_int_al_buffer(respuesta, 1);
    t_paquete* paquete = crear_paquete(IO_K_FINALIZO_IO, respuesta);
    
    // Enviar el paquete de vuelta al kernel
    enviar_paquete(paquete, socket_kernel);
    
    // Liberar memoria
    free(datos);
    
    log_debug(io_logger, "Hilo de I/O terminado para PID %d", pid);
    return NULL;
}

void atender_kernel_io(){
	bool control_key = 1;
    while (control_key) {
	    int cod_op = recibir_operacion(socket_kernel);
	    switch (cod_op) {
	    case MENSAJE:
           
		    break;
	    case PAQUETE:
			 // Petición recibida del kernel
		    t_buffer* buffer = recibir_buffer(socket_kernel);
            int pid = extraer_int_del_buffer(buffer);
            int tiempo_io = extraer_int_del_buffer(buffer);
            
            // Crear estructura de datos para el hilo
            t_datos_io* datos = malloc(sizeof(t_datos_io));
            datos->pid = pid;
            datos->tiempo_io = tiempo_io;
            
            // Crear hilo para ejecutar la operación de I/O
            pthread_t hilo_io;
            if (pthread_create(&hilo_io, NULL, ejecutar_io_en_hilo, (void*)datos) != 0) {
                log_error(io_logger, "Error al crear hilo de I/O para PID %d", pid);
                free(datos);
            } else {
                // Detach el hilo para que se auto-libere cuando termine
                pthread_detach(hilo_io);
                log_debug(io_logger, "Hilo de I/O creado para PID %d", pid);
            }
            
			eliminar_buffer(buffer);
		    break;
	    case -1:
		    log_error(io_logger, "El Kernel se desconecto. Terminando servidor");
		    control_key = 0;
            break;
	    default:
		    log_warning(io_logger,"Operacion desconocida de kernel.");
		    break;
	}
}}