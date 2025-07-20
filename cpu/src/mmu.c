#include "../include/mmu.h"
#include "../include/logs.h"
#include <stdbool.h>
#include <pthread.h>


static int contador_tiempo_global = 0;
static pthread_mutex_t mutex_contador = PTHREAD_MUTEX_INITIALIZER;

// Función para obtener el próximo timestamp
int obtener_proximo_timestamp() {
    int timestamp;
    pthread_mutex_lock(&mutex_contador);
    timestamp = contador_tiempo_global++;
    pthread_mutex_unlock(&mutex_contador);

    return timestamp;
}



void destruir_entrada_cache(t_entrada_cache* entrada) {
    if (entrada->contenido != NULL) free(entrada->contenido);
    free(entrada);
}

//-------------TLB------------------

/**
* @fn     void iniciar_TLB(void)
* @brief  Inicializa la lista de entradas de la TLB, reservando memoria para cada entrada y configurando sus valores iniciales. Deja todas las entradas listas para ser utilizadas por el sistema de traducción de direcciones, con valores por defecto que indican que están vacías.
* @param  Ninguno
* @return Ninguno
*/
void iniciar_TLB(void){
    lista_tlb = list_create();

    pthread_mutex_lock(&mutex_contador); //reseteo el contador global
    contador_tiempo_global = 0;
    pthread_mutex_unlock(&mutex_contador);

    for(int i = 0; i < entradas_tlb(); i++){
        t_entrada_TLB* registro_tlb = malloc(sizeof(t_entrada_TLB)); //realloc
        if (!registro_tlb) {
            perror("No se pudo reservar memoria para la TLB");
            exit(EXIT_FAILURE);
        } 
        registro_tlb->pid = -1;
        registro_tlb->numero_pagina = -1;
        registro_tlb->marco = -1;
        registro_tlb->time_creado = 0;
        registro_tlb->time_usado = 0;

        list_add(lista_tlb, registro_tlb);
    }
}


void imprimir_tlb(int numero_pagina, int marco, int pid, time_t creado, time_t usado){
    t_entrada_TLB* registro_tlb;

    printf("\x1b[36m\n%-10s | %-10s | %-10s | %-10s | %-10s\n", "PAG", "FRAME", "PID", "CREADO", "USADO");
    printf("\x1b[36m---------------------------------------------------------------\n");

    for(int i = 0; i < list_size(lista_tlb); i++){
        registro_tlb = list_get(lista_tlb, i);
        
        if(registro_tlb->numero_pagina == numero_pagina && registro_tlb->pid == pid){
            printf("\x1b[35m%-10d | %-10d | %-10d | %-10ld | %-10ld\n\033[0m", registro_tlb->numero_pagina, registro_tlb->marco, registro_tlb->pid, registro_tlb->time_creado, registro_tlb->time_usado);
        }
        else
            { printf("\x1b[36m%-10d | %-10d | %-10d | %-10ld | %-10ld\n\033[0m", registro_tlb->numero_pagina, registro_tlb->marco, registro_tlb->pid, registro_tlb->time_creado, registro_tlb->time_usado);
        }
    }
    return; 
}
/**
* @fn     t_entrada_TLB* buscar_en_TLB(int numero_pagina)
* @brief  Busca una entrada en la TLB que corresponda al número de página solicitado. Recorre la lista de entradas y retorna un puntero a la entrada si la encuentra, o NULL si no existe.
* @param  numero_pagina Número de página a buscar en la TLB.
* @return Puntero a la entrada encontrada o NULL si no existe.
*/
t_entrada_TLB* buscar_en_TLB(int numero_pagina){
    t_entrada_TLB* registro_tlb;

    for(int i = 0; i < list_size(lista_tlb); i++){
        registro_tlb = list_get(lista_tlb, i);
        
        if(registro_tlb->numero_pagina == numero_pagina && registro_tlb->pid == pid){
            return registro_tlb;
        }
    }
    return NULL; // No se encontro la pagina en la t_entrada_TLB
}

/**
* @fn     int traducir_dir_logica(int direccion_logica, t_log* logger)
* @brief  Traduce una dirección lógica a una dirección física utilizando la MMU. Calcula el número de página y el desplazamiento, obtiene el marco correspondiente y construye la dirección física final. Registra información relevante en el logger.
* @param  direccion_logica Dirección lógica a traducir.
* @param  logger Logger para imprimir información de depuración.
* @return Dirección física resultante.
*/
int traducir_dir_logica(int direccion_logica, t_log* logger) {

    //Tiene que estar fuera de la funcion para que cache verifique si posee el numero de pagina
    int direccion_fisica;
    int nro_pagina = direccion_logica / tam_pagina;
    desplazamiento = direccion_logica % tam_pagina;

    int vec[cantidad_niveles];
    calcular_indices_tablas(nro_pagina, vec);

    log_debug(logger, "Direccion logica: %d", direccion_logica);
    log_debug(logger, "Numero de pagina: %d | Desplazamiento: %d", nro_pagina, desplazamiento);

    int marco = obtener_marco(nro_pagina, vec, logger); //obtiene el marco, ya sea desde la tlb o desde memoria

    direccion_fisica = marco * tam_pagina + desplazamiento;

    return direccion_fisica;
}

/**
* @fn     int obtener_marco(int nro_pagina, int vec[])
* @brief  Obtiene el marco correspondiente a una página. Primero busca en la TLB; si no está, consulta a memoria y actualiza la TLB con la nueva entrada. Devuelve el número de marco obtenido.
* @param  nro_pagina Número de página a buscar.
* @param  vec Vector de índices de tablas de páginas.
* @return Número de marco correspondiente.
*/
int obtener_marco (int nro_pagina, int vec[], t_log* cpu_logger) {
    int marco;
    if(tlb_habilitada()){
        t_entrada_TLB* entrada_tlb_aux = buscar_en_TLB(nro_pagina);
            
        if(entrada_tlb_aux != NULL){ //Existe la pagina en t_entrada_TLB
            
            log_tlb_hit(cpu_logger, pid, nro_pagina);
            entrada_tlb_aux->time_usado = obtener_proximo_timestamp(); //intenta "Resetear" el tiempo para que sea un valor mas chico
            imprimir_tlb(entrada_tlb_aux->numero_pagina, entrada_tlb_aux->marco, entrada_tlb_aux->pid, entrada_tlb_aux->time_creado, entrada_tlb_aux->time_usado);
            
            marco = entrada_tlb_aux->marco;
        }
        else { //No esta en la t_entrada_TLB
            
            log_tlb_miss(cpu_logger, pid, nro_pagina);
            //log_debug(cpu_logger, "No se usa TLB. Se obtuvo el marco %d desde memoria", marco);
            entrada_tlb_aux = malloc(sizeof(t_entrada_TLB));
            marco = buscar_marco_en_memoria(vec, cpu_logger, nro_pagina);
            
            // mutex
            int timestamp_actual = obtener_proximo_timestamp();
            //mutex
            entrada_tlb_aux->marco = marco;
            entrada_tlb_aux->numero_pagina = nro_pagina;
            entrada_tlb_aux->time_creado = timestamp_actual; //Establece el tiempo de creado y usado en 0 y empiezan a correr 
            entrada_tlb_aux->time_usado = timestamp_actual;
            actualizar_TLB(entrada_tlb_aux);    
            imprimir_tlb(entrada_tlb_aux->numero_pagina, entrada_tlb_aux->marco, entrada_tlb_aux->pid, entrada_tlb_aux->time_creado, entrada_tlb_aux->time_usado);
        }
    }
    else {
        marco = buscar_marco_en_memoria(vec, cpu_logger, nro_pagina);
    }

    log_obtener_marco(cpu_logger, pid, nro_pagina, marco);
    return marco;
}

/**
* @fn     void actualizar_TLB(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Actualiza la TLB con una nueva entrada. Si ya existe una entrada con el mismo marco, la reemplaza. Si no hay lugar, aplica el algoritmo de reemplazo configurado (FIFO o LRU). Si hay lugar vacío, inserta la nueva entrada.
* @param  registro_tlb_nuevo Nueva entrada de TLB a insertar.
* @return Ninguno
*/
void actualizar_TLB(t_entrada_TLB* registro_tlb_nuevo){
    registro_tlb_nuevo->pid = pid;
    // registro_tlb_nuevo->time_usado = time(NULL);


    int indice = existe_entrada_con_marco(registro_tlb_nuevo);
    if(indice != -1){
        list_replace_and_destroy_element(lista_tlb, indice, registro_tlb_nuevo, (void*)free);
   
    }
    else{
        indice = verificar_reemplazo_TLB();

        if(indice == -1){ // No hay lugares vacios
            if(strcmp(reemplazo_tlb(), "FIFO") == 0){
                reemplazar_TLB_FIFO(registro_tlb_nuevo);
            }
            else if(strcmp(reemplazo_tlb(), "LRU") == 0){
                reemplazar_TLB_LRU(registro_tlb_nuevo);
            }
        }
        else{ // Hay lugares vacios 
            list_replace_and_destroy_element(lista_tlb, indice, registro_tlb_nuevo, (void*)free);
        }
    }
}

/**
* @fn     void reemplazar_TLB_FIFO(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Reemplaza la entrada más antigua de la TLB utilizando el algoritmo FIFO. Busca la entrada con menor timestamp de creación y la reemplaza por la nueva entrada.
* @param  registro_tlb_nuevo Nueva entrada de TLB a insertar.
* @return Ninguno
*/
void reemplazar_TLB_FIFO(t_entrada_TLB* registro_tlb_nuevo){
    int i, indice_registro_tlb_mas_viejo = 0;

    t_entrada_TLB* entrada_tlb_aux;

    t_entrada_TLB* registro_tlb_mas_viejo = list_get(lista_tlb, 0);

    for(i = 0; i < list_size(lista_tlb); i++){

        entrada_tlb_aux = list_get(lista_tlb, i);

     // if(difftime(registro_tlb_mas_viejo -> time_creado, entrada_tlb_aux -> time_creado) > 0){
        if(entrada_tlb_aux->time_creado < registro_tlb_mas_viejo->time_creado){
            indice_registro_tlb_mas_viejo = i;
            registro_tlb_mas_viejo = entrada_tlb_aux;
        }
    }

    list_replace_and_destroy_element(lista_tlb, indice_registro_tlb_mas_viejo, registro_tlb_nuevo, (void*)free);
}

/**
* @fn     void reemplazar_TLB_LRU(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Reemplaza la entrada menos recientemente usada de la TLB utilizando el algoritmo LRU. Busca la entrada con menor timestamp de último uso y la reemplaza por la nueva entrada.
* @param  registro_tlb_nuevo Nueva entrada de TLB a insertar.
* @return Ninguno
*/
void reemplazar_TLB_LRU(t_entrada_TLB* registro_tlb_nuevo){
    int indice_mas_viejo_tlb = 0;

    t_entrada_TLB* entrada_tlb_aux;

    t_entrada_TLB* tlb_mas_viejo = list_get(lista_tlb, 0);

    for(int i = 0; i < list_size(lista_tlb); i++){

        entrada_tlb_aux = list_get(lista_tlb, i);

     // if(difftime(tlb_mas_viejo -> time_usado, entrada_tlb_aux -> time_usado) > 0){
        if(entrada_tlb_aux->time_usado < tlb_mas_viejo->time_usado){
            indice_mas_viejo_tlb = i;
            tlb_mas_viejo = entrada_tlb_aux;
        }
    }
    
    list_replace_and_destroy_element(lista_tlb, indice_mas_viejo_tlb, registro_tlb_nuevo, (void*)free);
}

/**
* @fn     int verificar_reemplazo_TLB(void)
* @brief  Busca un espacio vacío en la TLB. Recorre la lista de entradas y retorna el índice de la primera entrada vacía encontrada, o -1 si no hay lugar disponible.
* @param  Ninguno
* @return Índice del espacio vacío o -1 si no hay lugar.
*/
int verificar_reemplazo_TLB(void){
    t_entrada_TLB* entrada_tlb_aux;

    for(int i = 0; i < list_size(lista_tlb); i++){
        entrada_tlb_aux = list_get(lista_tlb, i);

        if(entrada_tlb_aux->numero_pagina == -1)
            return i; //Retorna el indice del registro t_entrada_TLB vacio
    }

    return -1; // Retorna -1 si no hay registros t_entrada_TLB vacios
}

/**
* @fn     int existe_entrada_con_marco(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Verifica si ya existe una entrada en la TLB con el mismo marco que la nueva entrada. Si la encuentra, retorna el índice; si no, retorna -1.
* @param  registro_tlb_nuevo Entrada de TLB a comparar.
* @return Índice de la entrada encontrada o -1 si no existe.
*/
int existe_entrada_con_marco(t_entrada_TLB* registro_tlb_nuevo){
    t_entrada_TLB* entrada_tlb_aux;
    for(int i = 0; i < list_size(lista_tlb); i++){
        entrada_tlb_aux = list_get(lista_tlb, i);

        if(entrada_tlb_aux->marco == registro_tlb_nuevo->marco && entrada_tlb_aux->pid == registro_tlb_nuevo->pid)
            return i;
    }
    return -1;
}

//------------------ MMU ------------------

/**
* @fn     int buscar_marco_en_memoria(int vec[], t_log* cpu_logger, int nro_pagina)
* @brief  Solicita a memoria el marco correspondiente a una página. Envía una petición a memoria con los datos necesarios y espera la respuesta. Devuelve el número de marco recibido o -1 en caso de error.
* @param  vec Vector de índices de tablas de páginas.
* @param  cpu_logger Logger para imprimir información.
* @param  nro_pagina Número de página.
* @return Número de marco recibido o -1 en caso de error.
*/
int buscar_marco_en_memoria(int vec[], t_log* cpu_logger, int nro_pagina) { //MMU
    int marco;
    t_buffer* buffer_peticion = crear_buffer();
    cargar_int_al_buffer(buffer_peticion, pid);

    for(int j=0 ; j<cantidad_niveles ; j++){
        cargar_int_al_buffer(buffer_peticion, vec[j]); //indices de tabla de paginas
    }
    
    t_paquete* paquete = crear_paquete(CPU_M_ACCESO_TABLA_PAGINAS, buffer_peticion);
    enviar_paquete(paquete, socket_memoria);

    if(recibir_operacion(socket_memoria) == M_CPU_RESPUESTA_DIRECCION_FISICA) {
        t_buffer* buffer = recibir_buffer(socket_memoria);
        marco = extraer_int_del_buffer(buffer);    
        eliminar_buffer(buffer);
    } 
    else {
        log_error(cpu_logger, "Memoria me contestó otra cosa");
        marco = -1; // Si no se pudo obtener el marco, retornar un valor de error
    }

    if (marco == -1){
        log_error(cpu_logger, "Marco obtenido -1");
        return -1;
    }
    return marco;
}

//------------------ CACHE ------------------

/**
* @fn     void inicializar_cache(void)
* @brief  Inicializa la caché de páginas, reservando memoria para cada entrada y configurando sus valores iniciales. Deja todas las entradas listas para ser utilizadas por el sistema de caché de páginas.
* @param  Ninguno
* @return Ninguno
*/
void inicializar_cache() {
    lista_cache = list_create();
    for (int i = 0; i < entradas_cache(); i++) {
        t_entrada_cache* cache = malloc(sizeof(t_entrada_cache));
        cache->pid = -1;
        cache->marco = -1;
        cache->numero_pagina = -1;
        cache->contenido = NULL;
        cache->bit_uso = false;
        cache->bit_modificado = false;
        cache->presente = false;
        list_add(lista_cache, cache); // Agregar a la lista de caché
    }
}


/**
* @fn     bool cache_habilitada(void)
* @brief  Indica si la caché está habilitada y tiene entradas disponibles. Devuelve true si la caché está habilitada, false en caso contrario.
* @param  Ninguno
* @return true si la caché está habilitada, false en caso contrario.
*/
bool cache_habilitada() {
    return (entradas_cache() > 0);
}


bool tlb_habilitada() {
    return(entradas_tlb() > 0);
}

/**
* @fn     char* obtener_contenido_memoria(int marco, int nro_pagina, t_log* cpu_logger)
* @brief  Solicita a memoria el contenido de una página. Envía una petición a memoria y retorna el contenido recibido como un puntero a char, o NULL en caso de error.
* @param  marco Número de marco.
* @param  nro_pagina Número de página.
* @param  cpu_logger Logger para imprimir información.
* @return Puntero al contenido recibido o NULL en caso de error.
*/
char* obtener_contenido_memoria(int marco, int nro_pagina, t_log* cpu_logger) {
    t_buffer* buffer_peticion = crear_buffer();

    cargar_int_al_buffer(buffer_peticion, pid);
    cargar_int_al_buffer(buffer_peticion, marco*tam_pagina); // Enviar dirección física

    t_paquete* paquete = crear_paquete(CPU_M_LEER_PAGINA_COMPLETA, buffer_peticion);
    enviar_paquete(paquete, socket_memoria);

    if (recibir_operacion(socket_memoria) == M_CPU_LEER_PAGINA_COMPLETA) {
        t_buffer* buffer = recibir_buffer(socket_memoria);

        char* contenido = extraer_string_del_buffer(buffer); 
        eliminar_buffer(buffer);
        return contenido;
    } 
    else {
        log_error(cpu_logger, "Error al obtener la página desde memoria");
    }
    return NULL; // Error al obtener la página
}


void calcular_indices_tablas(int nro_pagina, int vec[]) {
    printf("\nCALCULAR INDICE DE PAGINAS: [");
    for (int X = 1; X <= cantidad_niveles; X++) {
        int divisor = (int)pow(entradas_tabla, cantidad_niveles - X);
        vec[X - 1] = (nro_pagina / divisor) % entradas_tabla;
        printf("%d, ", vec[X-1]);
    }
        printf("]\n");

}

t_entrada_cache* crear_entrada_cache(int marco, int nro_pagina, t_log* logger) {
    t_entrada_cache* entrada = malloc(sizeof(t_entrada_cache));
    entrada->marco = marco;
    entrada->numero_pagina = nro_pagina;
    entrada->pid = pid;
    entrada->bit_uso = true;
    entrada->bit_modificado = false;
    entrada->presente = true;

    entrada->contenido = obtener_contenido_memoria(marco, nro_pagina, logger);
    if (entrada->contenido == NULL) {
        log_error(logger, "No se pudo obtener el contenido de la página %d. Abortando operación de caché.", nro_pagina);
        free(entrada);
        return NULL;
    }

    return entrada;
}

void leer_en_cache(t_log* cpu_logger, t_entrada_cache* entrada_cache, int nro_pagina){
    log_debug(cpu_logger, "Pagina %d - Leido en Cache: %s", nro_pagina, entrada_cache -> contenido); // Imprimir el contenido de la página
}
void escribir_en_cache(t_log* cpu_logger, t_entrada_cache* entrada_cache, int nro_pagina, char* contenido){ //No estoy escribiendo en cache cuando hago crear_entrada_cache y actualizar_cache
    if (entrada_cache->contenido != NULL) {
        free(entrada_cache->contenido);
    }
    entrada_cache->contenido = malloc(tam_pagina);
    memset(entrada_cache->contenido, 0, tam_pagina); // Opcional, para limpiar
    memcpy(entrada_cache->contenido, contenido, strlen(contenido) + 1); // Copia solo el string
    log_debug(cpu_logger, "Pagina: %d - Escribí en Cache: %s", nro_pagina, contenido); // Imprimir el contenido de la página
    
}
/**
* @fn     void cargar_contenido_cache(t_log* cpu_logger, int direccion_logica, int operacion, char* origen)
* @brief  Carga el contenido de una página en la caché, leyendo o escribiendo según la operación. Si la página está en caché, la utiliza directamente; si no, la carga desde memoria y la almacena en la caché. Permite operaciones de lectura y escritura.
* @param  cpu_logger Logger para imprimir información.
* @param  direccion_logica Dirección lógica de la operación.
* @param  operacion Tipo de operación (READ o WRITE).
* @param  origen Contenido a escribir en caso de WRITE.
* @return Ninguno
*/
void cargar_contenido_cache(t_log* cpu_logger, int direccion_logica, int operacion, char* contenido) { 
    int nro_pagina = direccion_logica / tam_pagina;
    
    t_entrada_cache* entrada_cache = buscar_en_cache(nro_pagina);
    if (entrada_cache != NULL) { // HIT en cache

        log_cache_hit(cpu_logger, pid, nro_pagina);

        entrada_cache->bit_uso = true; // Para CLOCK/CLOCK-M        
    }
    else { // MISS en CACHE

        log_cache_miss(cpu_logger, pid, nro_pagina);

        int vec[cantidad_niveles];
        calcular_indices_tablas(nro_pagina, vec);

        int marco = obtener_marco(nro_pagina, vec, cpu_logger);
        t_entrada_cache* nueva_entrada = crear_entrada_cache(marco, nro_pagina, cpu_logger);
        if (nueva_entrada == NULL){
            log_error(cpu_logger, "No se pudo agregar a la cache correctamente");
            return;
        }
       
        actualizar_entrada_cache(nueva_entrada, cpu_logger); //actualizar me libera la entrada

        log_cache_add(cpu_logger, pid, nro_pagina);

        //Actualizar_entrada_cache me libera nueva_entrada. Si es el caso de WRITE la voy a necesitar, asique la vuelvo a buscar en la cache
        entrada_cache = buscar_en_cache(nro_pagina);
    }
    
    if (operacion == READ) {
        leer_en_cache(cpu_logger, entrada_cache, nro_pagina);
    }
    else if (operacion == WRITE) {
        if (entrada_cache == NULL) {
            log_error(cpu_logger, "La entrada recién agregada no está en la caché.");
            return;
        }
       
        entrada_cache -> bit_modificado = true;
        escribir_en_cache(cpu_logger, entrada_cache, nro_pagina, contenido);
    } 
}


/**
* @fn     t_entrada_cache* buscar_en_cache(int nro_pagina)
* @brief  Busca una entrada en la caché por número de página. Recorre la lista de entradas y retorna un puntero a la entrada si la encuentra y está presente, o NULL si no existe.
* @param  nro_pagina Número de página a buscar en la caché.
* @return Puntero a la entrada encontrada o NULL si no existe.
*/
t_entrada_cache* buscar_en_cache(int nro_pagina) {
    usleep(1000 * atoi(retardo_cache()));
    for (int i = 0; i < list_size(lista_cache); i++) {
        t_entrada_cache* entrada = list_get(lista_cache, i);
        if (entrada->presente && 
            entrada->numero_pagina == nro_pagina && 
            entrada->pid == pid) {
            return entrada;
        }
    }
    return NULL;  // No encontrado
}

/**
* @fn     void actualizar_entrada_cache(t_entrada_cache* entrada_cache_aux)
* @brief  Actualiza la caché con una nueva entrada. Si hay lugar disponible, la inserta directamente; si no, aplica el algoritmo de reemplazo configurado (CLOCK o CLOCK-M) para decidir qué entrada reemplazar.
* @param  entrada_cache_aux Nueva entrada de caché a insertar.
* @return Ninguno
*/

void actualizar_entrada_cache(t_entrada_cache* entrada_cache_aux, t_log* logger){

    int indice_reemplazo_cache = encontrar_vacio(); // quizas no es necesario, probar de sacarlo, los algoritmos de clock y clock-m por como esta inicializada la lista funcionarian sin esto
    if(indice_reemplazo_cache == ESTA_LLENA){ //Siendo -1 que no hay lugares vacio
        
        if(strcmp(reemplazo_cache(), "CLOCK") == 0){ // aca los llamamos SOLO si la lista esta llena 
            reemplazar_cache_CLOCK(entrada_cache_aux, logger);
        }
        else if(strcmp(reemplazo_cache(), "CLOCK-M") == 0){
            reemplazar_cache_CLOCK_M(entrada_cache_aux, logger);
        }
    }
    else{ // Hay lugares vacios 
        list_replace_and_destroy_element(lista_cache, indice_reemplazo_cache, entrada_cache_aux, (void (*)(void*)) destruir_entrada_cache);
    }
}

/**
* @fn     int encontrar_vacio(void)
* @brief  Busca un espacio vacío en la caché. Recorre la lista de entradas y retorna el índice de la primera entrada vacía encontrada, o ESTA_LLENA si no hay lugar disponible.
* @param  Ninguno
* @return Índice del espacio vacío o ESTA_LLENA si no hay lugar.
*/
int encontrar_vacio(void){
    t_entrada_cache* entrada_cache_aux;
    for(int i = 0; i < list_size(lista_cache); i++){
        entrada_cache_aux = list_get(lista_cache, i);

        if(entrada_cache_aux->numero_pagina == -1)
            return i; //Retorna el indice del registro t_entrada_cache vacio
    }

    return ESTA_LLENA; // Retorna -1 si no hay registros t_entrada_cache vacios
}

//Algoritmos de reemplazo de cache
int clock_pointer = 0;  // Para algoritmo CLOCK

/**
* @fn     void avanzar_puntero(void)
* @brief  Avanza el puntero del algoritmo CLOCK/CLOCK-M a la siguiente posición de la caché, de manera circular.
* @param  Ninguno
* @return Ninguno
*/
void avanzar_puntero() {
    clock_pointer = (clock_pointer + 1) % entradas_cache();
}

/**
* @fn     void reemplazar_cache_CLOCK(t_entrada_cache* nueva_entrada)
* @brief  Reemplaza una entrada en la caché utilizando el algoritmo CLOCK. Busca una entrada con bit de uso en 0 para reemplazarla; si el bit de uso está en 1, lo pone en 0 y avanza el puntero. Si la entrada a reemplazar está modificada, la escribe en memoria antes de reemplazarla.
* @param  nueva_entrada Nueva entrada de caché a insertar.
* @return Ninguno
*/
void reemplazar_cache_CLOCK(t_entrada_cache* nueva_entrada , t_log* logger) {
    while (true) {
        t_entrada_cache* actual = list_get(lista_cache, clock_pointer);


        if (!actual->bit_uso) {
             if (actual->bit_modificado) {

                log_memory_update(logger, pid, actual->numero_pagina, actual->marco);
                
                t_buffer* buffer_escritura = crear_buffer();

                cargar_int_al_buffer(buffer_escritura, pid);
                int direccion_fisica = (actual->marco) * tam_pagina;
                cargar_int_al_buffer(buffer_escritura, direccion_fisica); 
                cargar_string_al_buffer(buffer_escritura, actual->contenido); // contenido a escribir
               
                t_paquete* paquete = crear_paquete(CPU_M_ESCRIBIR_PAGINA_COMPLETA, buffer_escritura);
                
                enviar_paquete(paquete, socket_memoria);
            }
            
            list_replace_and_destroy_element(lista_cache, clock_pointer, nueva_entrada, (void (*)(void*)) destruir_entrada_cache);
            avanzar_puntero();  // Mover a la próxima posición
            break;
        } 
        else {
            actual->bit_uso = false;
            avanzar_puntero();
        }
    }
}

/**
* @fn     void reemplazar_cache_CLOCK_M(t_entrada_cache* nueva_entrada)
* @brief  Reemplaza una entrada en la caché utilizando el algoritmo CLOCK-M. Busca primero una entrada con bit de uso y modificado en 0; si no la encuentra, realiza un segundo ciclo considerando solo el bit de uso. Si la entrada a reemplazar está modificada, la escribe en memoria antes de reemplazarla.
* @param  nueva_entrada Nueva entrada de caché a insertar.
* @return Ninguno
*/
void reemplazar_cache_CLOCK_M(t_entrada_cache* nueva_entrada , t_log* logger) {
    int entradas = entradas_cache();
    bool reemplazo_realizado = false;

    while (!reemplazo_realizado) {
        // (U=0, M=0)
        for (int i = 0; i < entradas; i++) {
            t_entrada_cache* actual = list_get(lista_cache, clock_pointer);

            if (!actual->bit_uso && !actual->bit_modificado) {
                list_replace_and_destroy_element(lista_cache, clock_pointer, nueva_entrada, (void (*)(void*)) destruir_entrada_cache);
                avanzar_puntero();
                reemplazo_realizado = true;
                return;
            }

            avanzar_puntero();
        }

        //(U=0, M=1)
        for(int i = 0; i < entradas; i++) {
            t_entrada_cache* actual = list_get(lista_cache, clock_pointer);
            //printf("Reemplazo CLOCK-M: Revisando entrada en posición %d\n", clock_pointer);
            if (!actual->bit_uso && actual->bit_modificado) {
                //escribir en memroia el contenido de la pagina
                log_memory_update(logger, pid, actual->numero_pagina, actual->marco);
                //printf("PID: <%d> - Memory Update - Página: <%d> - Frame: <%d>", pid, actual->numero_pagina, actual->marco);
                t_buffer* buffer_escritura = crear_buffer();

                cargar_int_al_buffer(buffer_escritura, pid);
                cargar_int_al_buffer(buffer_escritura, (actual->marco)*tam_pagina); // numero de pagina
                cargar_string_al_buffer(buffer_escritura, actual->contenido); // contenido a escribir
              
                t_paquete* paquete = crear_paquete(CPU_M_ESCRIBIR_PAGINA_COMPLETA, buffer_escritura);
                
                enviar_paquete(paquete, socket_memoria);
                list_replace_and_destroy_element(lista_cache, clock_pointer, nueva_entrada, (void (*)(void*)) destruir_entrada_cache);
                avanzar_puntero();
                reemplazo_realizado = true;
                return;
            }

            actual->bit_uso = false;
            avanzar_puntero();
        }
    }
}
