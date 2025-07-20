#include "../include/memoria.h"


// --------------------------------------- VARIABLES GLOBALES ---------------------------------------

t_log* memoria_logger = NULL;
t_config* memoria_config = NULL;
int socket_server = -1;
t_dictionary* procesos = NULL; // Diccionario de instrucciones por PID
t_dictionary* METRICAS = NULL; // Diccionario de métricas por proceso

void *MEMORIA_USUARIO = NULL;
int MEMORIA_USUARIO_EN_USO = 0;
uint8_t *BITMAP_MARCOS = NULL;
int CANTIDAD_MARCOS = 0;
int PAGINAS_TOTALES_PROCESO = 1;
int MAX_PROCESOS = 0;
t_dictionary *PROCESO_TABLAS = NULL;

pthread_mutex_t mutex_memoria = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_reservar_memoria = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_bitmap = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_metricas = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_swap = PTHREAD_MUTEX_INITIALIZER;


// --------------------------------------- INICIALIZACION ---------------------------------------

/** Reserva bytes para la memoria de usuario.
 * El tamaño está explícito en configuración: TAM_MEMORIA.
 * */
void inicializar_MEMORIA_USUARIO(){

    MEMORIA_USUARIO = calloc(1, tam_memoria());
    if (!MEMORIA_USUARIO) {
        perror("calloc USER_MEM");
        exit(EXIT_FAILURE);
    }
    
    //log_debug(memoria_logger, "%d bytes reservados para MEMORIA_USUARIO", tam_memoria());
}



/** Reserva bytes para el bitmap de marcos.
 * El tamaño se calcula por configuración: TAM_MEMORIA / TAM_PAGINA.
*/
void inicializar_BITMAP_MARCOS() {
    CANTIDAD_MARCOS = tam_memoria() / tam_pagina(); // Por ejemplo: 4096 / 64 = 64 marcos

    // Cada bit representa un marco. Cantidad de bytes necesarios:
    int bytes_bitmap = (CANTIDAD_MARCOS + 7) / 8; // Redondeo hacia arriba
    BITMAP_MARCOS = calloc(bytes_bitmap, sizeof(uint8_t)); // Inicializa en 0 (todos libres)

    if (!BITMAP_MARCOS) {
        log_error(memoria_logger, "No se pudo reservar memoria para el bitmap.");
        exit(EXIT_FAILURE);
    }

    //log_debug(memoria_logger, "%d / %d = %d bytes reservados para el bitmap de marcos", tam_memoria(), tam_pagina(), bytes_bitmap);
}


/**inicializar_PROCESO_TABLAS
 * Utiliza la librería dictionary para llevar registro de los procesos y su tabla de página raíz.
 * key->value ==> pid->tabla_raiz 
*/
void inicializar_PROCESO_TABLAS()
{
    PROCESO_TABLAS = dictionary_create();

    if (!PROCESO_TABLAS) {
        log_error(memoria_logger, "dictionary_create");
        exit(EXIT_FAILURE);
    }

    //log_debug(memoria_logger, "Diccionario PROCESO_TABLAS creado");
}


// --------------------------------------- HANDLE CONNECTIONS ---------------------------------------


void* handle_connection(void* arg) {
    int client_socket = *((int*)arg);
    free(arg);  // Liberar la memoria para el socket

    int operacion = recibir_operacion(client_socket);

    // Según el código de operación, se aplica el retardo de memoria correspondiente
    aplicar_retardo_memoria(operacion);

    // Si el que se conectó es Kernel --> Conexión efímera
    if (operacion >= K_M_HANDSHAKE && operacion <= K_M_MEMORY_DUMP) { 
    
        manejar_operacion_kernel(operacion, client_socket);
        close(client_socket);
        //log_debug(memoria_logger, "\nManejé operación de Kernel, socket cerrado.\n");

    // Si el que se conectó es una CPU --> Conexión persistente
    } else if (operacion >= CPU_M_HANDSHAKE && operacion <= CPU_M_ELIMINAR_CACHE_POR_PROCESO) {
        
        manejar_operacion_cpu(operacion, client_socket);
        
        while(true) {
            // log_debug(memoria_logger, "Mandó un mensaje una CPU");

            int bytes = recv(client_socket, &operacion, sizeof(op_code_t), 0);
            if (bytes <= 0) {
                log_warning(memoria_logger, "CPU desconectada.");
                break;
            }

            // Aplicar retardo de memoria para cada operación subsecuente
            aplicar_retardo_memoria(operacion);
            
            manejar_operacion_cpu(operacion, client_socket);
        }

    } else if (operacion == -1) {
        log_warning(memoria_logger, "CPU desconectada");

    } else {    
        log_error(memoria_logger, "Código de operación desconocido: %d", operacion);
    }

    return NULL;
}


void aplicar_retardo_memoria(int operacion){
    // Aplicar retardo solo a operaciones que realmente acceden a memoria
    // Excluir handshakes y operaciones de control
    if (operacion == CPU_M_HANDSHAKE || operacion == K_M_HANDSHAKE) {
        return; // No aplicar retardo a handshakes
    }
    
    // Multiplico por 1000 para convertir milisegundos a microsegundos. usleep() recibe microsegundos.
    int retardo = retardo_memoria() * 1000;
    
    // Si la petición fue un acceso a tabla de páginas de CPU, el retardo se aplica tantas veces como cantidad de niveles de paginación
    if(operacion == CPU_M_ACCESO_TABLA_PAGINAS){
        retardo *= cantidad_niveles();
    }

    usleep(retardo);
}


/**
 * Crea y devuelve un t_metricas inicializado en cero
*/
t_metricas* inicializar_metricas() {

    t_metricas* metricas = malloc(sizeof(t_metricas));
    if (metricas == NULL) {
        log_error(memoria_logger, "Fallo al reservar memoria para métricas");
        return NULL;
    }

    metricas->instrSolicitadas = 0;
    metricas->accesosTP = 0;
    metricas->subidasMP = 0;
    metricas->bajadasSWAP = 0;
    metricas->lecturas = 0;
    metricas->escrituras = 0;

    return metricas;
}



/**
 * Actualiza una métrica de un proceso en el dictionary global METRICAS.
 * @param pid Process ID
 * @param metrica La métrica a actualizar
 * @param valor La cantidad a adicionar
*/
void actualizar_metrica(int pid, int metrica, int valor){
    // Busco proceso en el dictionary global METRICAS
    char* pid_str = string_itoa(pid);
    t_metricas *metricas = dictionary_get(METRICAS, pid_str);
    free(pid_str);
    
    if(!metricas){
        log_error(memoria_logger, "No se encontró el dictionary de métricas para el proceso %d", pid);
        return;
    }
    
        log_debug(memoria_logger, "\033[44m### LOCK MUTEX METRICAS ANTES");

    pthread_mutex_lock(&mutex_metricas);

        log_debug(memoria_logger, "\033[44m### LOCK MUTEX METRICAS DESPUES");

    switch(metrica){
        case accesoTP:
            metricas->accesosTP += valor;
            break;
        
        case instrSolicitadas:
            metricas->instrSolicitadas += valor;
            break;
        
        case subidasMP:
            metricas->subidasMP += valor;
            break;
        
        case bajadasSWAP:
            metricas->bajadasSWAP += valor;
            break;
        
        case lecturas:
            metricas->lecturas += valor;
            break;
        
        case escrituras:
            metricas->escrituras += valor;
            break;
        
        default:
            log_warning(memoria_logger, "Métrica desconocida: %d para proceso %d", metrica, pid);
            break;
    }

    pthread_mutex_unlock(&mutex_metricas);
}