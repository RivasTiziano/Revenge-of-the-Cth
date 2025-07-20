#include "../include/kernel.h"
#include "../include/syscalls.h"
#include "../include/init.h"
#include "../include/kernel_planificadores.h"


int main(int argc, char* argv[]) {

    // ---------------------------------------- INITIATION -----------------------------------------

    inicializar_config();
    inicializar_estructuras();
    inicializar_logger();

    // ---------------------------------------- VALIDATIONS -----------------------------------------

    // Validar que se haya iniciado Kernel con los parámetros iniciales
    char* nombre_archivo;
    int size;

    if (argc < 3) {
        fprintf(stderr, "Uso: %s [nombre_archivo] - %s [size]\n", argv[0], argv[1]);
        exit(EXIT_FAILURE);
    } else{
        nombre_archivo = argv[1];
        size = atoi(argv[2]);
    }

    // ---------------------------------------- CONNECTIONS -----------------------------------------

    // Iniciar servidor de KERNEL CPU
    socket_kernel_dispatch = iniciar_servidor(PUERTO_ESCUCHA_DISPATCH, kernel_logger, "KERNEL DISPATCH INICIADO COMO SERVIDOR.");
    socket_kernel_interrupt = iniciar_servidor(PUERTO_ESCUCHA_INTERRUPT, kernel_logger, "KERNEL INTERRUPT INICIADO COMO SERVIDOR.");

    // Iniciar servidor de KERNEL I/O
    socket_kernel_io = iniciar_servidor(PUERTO_ESCUCHA_IO, kernel_logger, "KERNEL INICIADO esperando a I/O.");

    // Iniciar hilos
    pthread_t hilo_dispatch, hilo_interrupt, hilo_io;

    pthread_create(&hilo_dispatch, NULL, escuchar_cpu_dispatch, NULL);
    pthread_create(&hilo_interrupt, NULL, escuchar_cpu_interrupt, NULL);
    pthread_create(&hilo_io, NULL, escuchar_io_kernel, NULL);

    // ----------------------------------------- PLANIFICAR PRIMER PROCESO  -----------------------------------------

    getchar();
    
    // Iniciar planificadores en hilos separados
    iniciar_planificadores();
    
    syscall_init_proc(0, nombre_archivo, size);

    getchar();

    // ---------------------------------------- FINALIZAR  ----------------------------------------

    // Detener planificadores
    detener_planificadores();

    // Esperar a que terminen los hilos de CPU e IO */
    pthread_detach(hilo_dispatch);
    pthread_detach(hilo_interrupt);
    pthread_join(hilo_io, NULL);

    liberar_conexion(socket_kernel_io);
    liberar_conexion(socket_kernel_dispatch);
    liberar_conexion(socket_kernel_interrupt);

    // Destruir semáforo
    sem_destroy(&sem_memoria_disponible);
    
    // Destruir variables condicionales para planificadores
    pthread_cond_destroy(&cond_planificador_largo_plazo);
    pthread_cond_destroy(&cond_planificador_corto_plazo);

    config_destroy(kernel_config);
    log_destroy(kernel_logger);

    return EXIT_SUCCESS;
}