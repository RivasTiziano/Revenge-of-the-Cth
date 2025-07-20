#include "../include/init.h"

void inicializar_config(){
    
    kernel_config = config_create("kernel.config");
    if(kernel_config == NULL){
        perror("Fallo en KERNEL config: no se pudo crear.");
        exit(EXIT_FAILURE);
    }

    IP_MEMORIA = config_get_string_value(kernel_config, "IP_MEMORIA");
    PUERTO_MEMORIA = config_get_string_value(kernel_config, "PUERTO_MEMORIA");
    PUERTO_ESCUCHA_DISPATCH = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_DISPATCH");
    PUERTO_ESCUCHA_INTERRUPT = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_INTERRUPT");
    PUERTO_ESCUCHA_IO = config_get_string_value(kernel_config, "PUERTO_ESCUCHA_IO");
    ALGORITMO_CORTO_PLAZO = config_get_string_value(kernel_config, "ALGORITMO_CORTO_PLAZO");
    ALGORITMO_INGRESO_A_READY = config_get_string_value(kernel_config, "ALGORITMO_INGRESO_A_READY");
    ALFA = config_get_double_value(kernel_config, "ALFA");
    TIEMPO_SUSPENSION = config_get_int_value(kernel_config, "TIEMPO_SUSPENSION");
    LOG_LEVEL = config_get_string_value(kernel_config, "LOG_LEVEL");
    ESTIMACION_INICIAL = config_get_int_value(kernel_config, "ESTIMACION_INICIAL");
}


void inicializar_estructuras(){

    cola_new = queue_create();
    cola_ready = queue_create();
    cola_susp_ready = queue_create();
    lista_susp_blocked = list_create();
    lista_blocked = list_create();

    cpus = dictionary_create();
    cpus_libres = dictionary_create();
    cpus_ocupadas = dictionary_create();
    
    // Inicializar semáforo con valor 1 (hay memoria disponible inicialmente)
    if (sem_init(&sem_memoria_disponible, 0, 1) != 0) {
        log_error(kernel_logger, "Error al inicializar sem_memoria_disponible");
        exit(EXIT_FAILURE);
    }
    
    // Inicializar variables condicionales para planificadores (sin esperas activas)
    if (pthread_cond_init(&cond_planificador_largo_plazo, NULL) != 0) {
        log_error(kernel_logger, "Error al inicializar cond_planificador_largo_plazo");
        exit(EXIT_FAILURE);
    }
    
    if (pthread_cond_init(&cond_planificador_corto_plazo, NULL) != 0) {
        log_error(kernel_logger, "Error al inicializar cond_planificador_corto_plazo");
        exit(EXIT_FAILURE);
    }
}


void inicializar_logger(){

    kernel_logger = log_create("kernel.log", "LOGGER KERNEL", 1, log_level_from_string(LOG_LEVEL));
    if(kernel_logger == NULL) {
        perror("Fallo en KERNEL logger: no se pudo crear.");
        exit(EXIT_FAILURE);
    }
}
