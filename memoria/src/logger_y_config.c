#include "../include/memoria.h"

// Para las conexiones
char* puerto_escucha() {
    return config_get_string_value(memoria_config, "PUERTO_ESCUCHA");
}

// Para otras cositas
int tam_memoria() {
    return config_get_int_value(memoria_config, "TAM_MEMORIA");
}
int tam_pagina() {
    return config_get_int_value(memoria_config, "TAM_PAGINA");
}
int entradas_por_tabla() {
    return config_get_int_value(memoria_config, "ENTRADAS_POR_TABLA");
}
int cantidad_niveles() {
    return config_get_int_value(memoria_config, "CANTIDAD_NIVELES");
}
int retardo_memoria() {
    return config_get_int_value(memoria_config, "RETARDO_MEMORIA");
}
char* path_swapfile() {
    return config_get_string_value(memoria_config, "PATH_SWAPFILE");
}
int retardo_swap() {
    return config_get_int_value(memoria_config, "RETARDO_SWAP");
}
char* log_level() {
    return config_get_string_value(memoria_config, "LOG_LEVEL");
}
char* dump_path() {
    return config_get_string_value(memoria_config, "DUMP_PATH");
}
char* path_instrucciones() {
    return config_get_string_value(memoria_config, "PATH_INSTRUCCIONES");
}

// LOGGER
void inicializar_logger() {
    memoria_logger = log_create("memoria.log", "LOGGER MEMORIA", 1, log_level_from_string(log_level()));
    if(memoria_logger == NULL) {
        perror("Fallo en MEMORIA logger: no se pudo crear. ");
        exit(EXIT_FAILURE);
    }
}

//CONFIG
void inicializar_config() {
    memoria_config = config_create("memoria.config");
    if(memoria_config == NULL){
        perror("Fallo en MEMORIA config: no se pudo crear. ");
        exit(EXIT_FAILURE);
    }
}

