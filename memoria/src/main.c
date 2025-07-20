#include "../include/memoria.h"


int main(int argc, char* argv[]) {

    // ---------------------------------------- INITIATION  -----------------------------------------

    inicializar_config();
    inicializar_logger();
    inicializar_MEMORIA_USUARIO();
    inicializar_BITMAP_MARCOS();
    inicializar_PROCESO_TABLAS();
    procesos = dictionary_create();
    METRICAS = dictionary_create();
    crear_swapfile();

    // ---------------------------------------- OPEN SERVER  ----------------------------------------

    socket_server = iniciar_servidor(puerto_escucha(), memoria_logger, "MEMORIA INICIADA COMO SERVIDOR");

    // ------------------------------------ ACCEPT CONNECTIONS  -------------------------------------

    int client_socket;
    
    while(true){
        // Aceptar conexión
        client_socket = accept(socket_server, NULL, NULL);

        // Validar conexión
        if(client_socket < 0){
            log_error(memoria_logger, "Error aceptando conexión con cliente nuevo.");
        } else{
            //log_debug(memoria_logger, "Nuevo cliente aceptado.");
        }

        // Crear un hilo para gestionar la conexión
        pthread_t thread;
        int* p_socket = malloc(sizeof(int)); // Asignar memoria para el socket

        *p_socket = client_socket;

        if (pthread_create(&thread, NULL, handle_connection, p_socket) != 0) {
            log_error(memoria_logger, "No se pudo crear hilo para nueva conexión.");
            close(client_socket);
            free(p_socket);
        } else {
            pthread_detach(thread); // Liberar recursos automáticamente cuando el hilo termine
        }
    }

    // ---------------------------------------- TERMINATION ----------------------------------------

    liberar_conexion(socket_server);
    config_destroy(memoria_config);
    log_destroy(memoria_logger);
    liberarMemoriaPaginacion();

    return EXIT_SUCCESS;
}
