#include "../include/memoria.h"
#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void serializar_instruccion(t_instruccion *instruccion, t_buffer *buffer)
{    
    // Validar que la instrucción no sea NULL
    if (instruccion == NULL) {
        log_error(memoria_logger, "Error: Instrucción NULL en serializar_instruccion");
        return;
    }

    // Validar que la operación sea válida
    if (instruccion->operacion < 0 || instruccion->operacion > 7) {
        log_error(memoria_logger, "Error: Operación inválida (%d) en serializar_instruccion", instruccion->operacion);
        return;
    }

    // Validar cantidad de parámetros
    if (instruccion->cantidad_parametros < 0) {
        log_error(memoria_logger, "Error: Cantidad de parámetros inválida (%d) en serializar_instruccion", instruccion->cantidad_parametros);
        return;
    }

    cargar_int_al_buffer(buffer, instruccion->operacion); 
    cargar_int_al_buffer(buffer, instruccion->cantidad_parametros); 
    
    // Validar que si hay parámetros, el array no sea NULL
    if (instruccion->cantidad_parametros > 0) {
        if (instruccion->parametros == NULL) {
            log_error(memoria_logger, "Error: Array de parámetros es NULL pero cantidad_parametros > 0");
            return;
        }
        
        for (int i = 0; i < instruccion->cantidad_parametros; i++) {
            if (instruccion->parametros[i] == NULL) {
                log_error(memoria_logger, "Error: Parámetro %d es NULL", i);
                cargar_string_al_buffer(buffer, ""); // Cargar string vacío como fallback
            } else {
                cargar_string_al_buffer(buffer, instruccion->parametros[i]);
            }
        }
    }
}

/**
 * Leer instrucciones desde archivo
 * Abre el archivo de instrucciones y las carga en un list de instrucciones.
 * La lista de instruciones las carga en un struct t_instruciones_proceso con su PID asociado.
 * Finalmente, dicho struct lo carga a una lista global.
 * 
 * @param nombre_archivo Nombre del archivo que tiene las instrucciones, ej: "instrucciones.txt"
 *                       Este archivo es buscado en la ruta scripts/
 * @param pid_asociado PID del proceso
 * 
*/
void leer_instrucciones_desde_archivo(FILE* nuevo_archivo, int pid_asociado, int tamanio) {
    // Crear la lista de instrucciones para este proceso
    t_list* lista_instrucciones = list_create();

    char *linea = malloc(100);

    while (fgets(linea, 100, nuevo_archivo) != NULL) {
        size_t len = strlen(linea);

        // Saltear líneas vacías o con solo espacios
        if (len == 0 || strspn(linea, " \t\r\n") == len) {
            continue;
        }

        if (linea[len - 1] == '\n') {
            linea[len - 1] = '\0';
        }

        //log_debug(memoria_logger, "Línea original: %s", linea);

        char* contenido = strtok(linea, " ");
        if (contenido == NULL) {
            log_error(memoria_logger, "Línea vacía o sin tokens. Se ignora.");
            continue;
        }

        t_instruccion* instruccion = malloc(sizeof(t_instruccion));
        instruccion->operacion = obtener_operacion_cpu(contenido);
        instruccion->cantidad_parametros = get_cant_parametros(instruccion->operacion);

        if (instruccion->cantidad_parametros < 0) {
            log_error(memoria_logger, "Error: operación no reconocida. Se descarta la instrucción.");
            free(instruccion);
            continue;
        }

        instruccion->parametros = malloc(sizeof(char*) * instruccion->cantidad_parametros);

        for (int i = 0; i < instruccion->cantidad_parametros; i++) {
            contenido = strtok(NULL, " ");
            if (contenido == NULL) {
                log_error(memoria_logger, "Error: faltan parámetros para la instrucción. Se descarta.");
                for (int j = 0; j < i; j++) {
                    free(instruccion->parametros[j]);
                }
                free(instruccion->parametros);
                free(instruccion);
                goto continuar;
            }
            instruccion->parametros[i] = strdup(contenido);
        }

        list_add(lista_instrucciones, instruccion);

    continuar:
        continue;
    }

    fclose(nuevo_archivo);
    free(linea);

    // Crear la estructura t_info_p y guardar en el diccionario global
    t_info_p* info = malloc(sizeof(t_info_p));
    info->lista_instrucciones = lista_instrucciones;
    info->tamanio = tamanio;

    char* pid_key = string_itoa(pid_asociado);
    dictionary_put(procesos, pid_key, info);
    free(pid_key);
}



int get_cant_parametros(t_operacion identificador)
{
    int cant_parametros = 0;
    switch (identificador)
    {
        case NOOP: case DUMP_MEMORY: case EXIT:
            return 0;
        break;

         case GOTO: 
            return cant_parametros = 1;
        break;
 
        case READ: case WRITE: case IO: case INIT_PROC:
            return cant_parametros = 2;
            break;
        default:
            log_error(memoria_logger, "Error: Operacion no reconocida.");
            return -1;
            break;
    }
}

t_operacion obtener_operacion_cpu(const char *operacion) {
    if (strcmp(operacion, "NOOP") == 0) {
        return NOOP;
    }
    if (strcmp(operacion, "READ") == 0) {
        return READ;
    }
    if (strcmp(operacion, "WRITE") == 0) {
        return WRITE;
    }
    if (strcmp(operacion, "GOTO") == 0) {
        return GOTO;
    }
    if (strcmp(operacion, "IO") == 0) {
        return IO;
    }
    if (strcmp(operacion, "INIT_PROC") == 0) {
        return INIT_PROC;
    }
    if (strcmp(operacion, "DUMP_MEMORY") == 0) {
        return DUMP_MEMORY;
    }
    if (strcmp(operacion, "EXIT") == 0) {
        return EXIT;
    }
    return ACAROMPE;
}
