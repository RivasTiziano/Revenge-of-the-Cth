#include "../include/cpu.h"

/**
* @fn    main
* @brief Punto de entrada del programa. Inicializa logs, configuración y conexiones, y luego cierra los recursos.
* @param argc Cantidad de argumentos pasados al programa.
* @param argv Array de argumentos pasados al programa.
* @return Código de salida del programa.
*/

// void liberar_recursos() {
//     if (cache_habilitada())
//         destruir_cache();  // implementá esto si aún no lo tenés

//     destruir_TLB();        // liberá recursos de la TLB
//     cerrar_cpu(logger);    // esto parece cerrar conexiones, etc.
//     log_debug(logger, "Recursos liberados por SIGINT.");
//     log_destroy(logger);
// }

void sigint_handler(int signo) {
    printf("\n[CPU] SIGINT recibida. Terminando ejecución...\n");
    liberar_conexion(socket_memoria);
    liberar_conexion(socket_kernel_dispatch);
    liberar_conexion(socket_kernel_interrupt);
    exit(EXIT_SUCCESS);
}

int main(int argc, char* argv[]) {

    if (argc < 2) {
        printf("Falta el identificador de la CPU.\nUso: %s [identificador]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* cpu_id = argv[1];
     
    signal(SIGINT, sigint_handler); // Ctrl + C

    inicializar_configCPU(cpu_id);
    t_log* logger = inicializar_logger(cpu_id);
    if(cache_habilitada()) {
        inicializar_cache();
    }
    if(tlb_habilitada()) {
        iniciar_TLB();
    }
    conexiones(cpu_id, logger);
    cerrar_cpu(logger);
    log_debug(cpu_logger, "Recursos liberados y CPU cerrada.\n");
	
	return EXIT_SUCCESS;
}

