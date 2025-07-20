#ifndef UTILS_H_
#define UTILS_H_

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include<signal.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netdb.h>
#include<string.h>

#include<commons/log.h>
#include<commons/collections/list.h>
#include<assert.h>
#include<commons/config.h> 
#include<commons/string.h>
//#include<commons/temporal.h>
// #include "pcb.h"


//-------------Structs de Cliente--------------------
typedef struct{
	int pid;
	int pc;
	//t_temporal* ME;
	//t_temporal* MT;
}t_PCB;
//Falta crear el Temporal(?





// PROTOCOLO – CÓDIGOS DE OPERACIÓN ENTRE MÓDULOS
typedef enum {
    // ─── Kernel → Memoria ──────────────────────────────────────
    K_M_HANDSHAKE,               // Kernel → Memoria (inicial)
    K_M_INIT_PROCESO,            // inicializar proceso (PID, tamaño)
    K_M_SUSPENDER_PROCESO,       // suspender proceso (escribir páginas en SWAP)
    K_M_REANUDAR_PROCESO,        // reanudar proceso (cargar páginas desde SWAP)
    K_M_FINALIZAR_PROCESO,       // finalizar proceso (liberar marcos y SWAP)
    K_M_MEMORY_DUMP,             // solicitar dump de memoria

    // ─── Memoria → Kernel (respuestas/eventos) ─────────────────
    M_K_INIT_PROCESO_OK,         // proceso inicializado (p. ej. tabla nivel-1)
    M_K_PROCESO_SUSPENDIDO,      // confirmación de suspensión
    M_K_PROCESO_REANUDADO,       // confirmación de reanudación
    M_K_PROCESO_FINALIZADO,      // confirmación de finalización
    M_K_DUMP_FINALIZADO,         // dump completado
    M_K_RESPUESTA_OK,            // respuesta genérica “OK”
    M_K_RESPUESTA_ERROR,         // respuesta genérica “ERROR”

    // ─── CPU → Memoria ─────────────────────────────────────────
    CPU_M_HANDSHAKE,                 // CPU    → Memoria (inicial)
    CPU_M_SOLICITAR_INSTRUCCION,     // fetch: pedir instrucción (PC)
    CPU_M_ACCESO_TABLA_PAGINAS,      // traducción: pedir marco
    CPU_M_LEER_MEMORIA,              // READ: leer bytes
    CPU_M_ESCRIBIR_MEMORIA,          // WRITE: escribir bytes
    CPU_M_LEER_PAGINA_COMPLETA,      // leer página completa
    CPU_M_ESCRIBIR_PAGINA_COMPLETA,  // escribir página completa
    CPU_M_ESCRIBIR_PAGINA_MODIFICADA,// escribir página modificada al desalojar
    CPU_M_ELIMINAR_TLB_POR_PROCESO,  // limpiar TLB al desalojar proceso
    CPU_M_ELIMINAR_CACHE_POR_PROCESO,// limpiar caché al desalojar proceso

    // ─── Memoria → CPU (respuestas) ────────────────────────────
    M_CPU_HANDSHAKE,              // Memoria → CPU (inicial)
    M_CPU_RESPUESTA_INSTRUCCION,     // instrucción (string)
    M_CPU_RESPUESTA_DIRECCION_FISICA,// marco o dirección física
    M_CPU_VALOR_LEIDO,               // valor leído (payload)
    M_CPU_CONFIRMACION_ESCRITURA,    // confirmación de escritura OK
    M_CPU_LEER_PAGINA_COMPLETA,          // página completa (bytes)
    M_CPU_ESCRIBIR_PAGINA_COMPLETA,

    // ─── Kernel → CPU ───────────────────────────────────────────
    K_CPU_EXEC_PROCESO,           // enviar PID + PC para ejecutar
    K_CPU_INTERRUPT_PROCESO,      // notificar interrupción de proceso

    // ─── CPU → Kernel ───────────────────────────────────────────
    CPU_K_HANDSHAKE,              // CPU envía su identificador
    CPU_K_REPLANIFICAR,           // motivo: pedido de replanificación
    CPU_K_SOLICITAR_IO,           // syscall IO (PID + tiempo)
    CPU_K_INIT_PROC,              // syscall INIT_PROC (archivo + tamaño)
    CPU_K_DUMP_MEMORY,            // syscall DUMP_MEMORY
    M_K_DUMP_ERROR,
    CPU_K_EXIT,                   // syscall EXIT
    CPU_K_SYSCALL_ERROR,          // error en syscall (enviar a EXIT)
    CPU_K_INTERRUPT_PROCESO,

    // ─── Kernel → IO ────────────────────────────────────────────
    K_IO_SOLICITUD,               // Kernel solicita operación de IO (PID + tiempo)

    // ─── IO → Kernel ────────────────────────────────────────────
    IO_K_HANDSHAKE,               // Kernel envía su nombre
    IO_K_FINALIZO_IO              // IO informa fin de operación
} op_code_t;
typedef enum
{
	OK, //todo ok
	MENSAJE,
	PAQUETE,
    HANDSHAKE,
	CPU_ID,
	FIN_IO,
	SYSCALL_P, //de cpu a kernel
	
    //------Kernel-Memoria---------
	CREAR_PROCESO,
    //------Kernel-Cpu---------
	// --------- Memoria <-> CPU ---------
	ENVIAR_INSTRUCCION,
	PEDIR_INSTRUCCION//Se envia de Memoria a CPU y viceversa
    //------Kernel-I/O---------
	
}op_code;

typedef enum {
    HAND_CPU_KERNEL_DIS,
    HAND_CPU_KERNEL_INT,
    HAND_IO_KERNEL,
    RESULT_ERROR,
    RESULT_OK
}t_handshake;

/**
* @enum t_operacion
* @brief Representa las operaciones posibles de una instrucción.
*/
typedef enum {
	NOOP,
	READ,
	WRITE,
	GOTO,
	IO,
	EXIT,
	INIT_PROC,
	DUMP_MEMORY,
	ACAROMPE
} t_operacion; // Operacion de instruccion.
/**
* @struct t_instruccion
* @brief Representa una instrucción con su operación y parámetros.
* @var t_instruccion::operacion
* Operación de la instrucción (NOOP, WRITE, READ, etc.).
* @var t_instruccion::cantidad_parametros
* Cantidad de parámetros de la instrucción.
* @var t_instruccion::parametros
* Array de parámetros de la instrucción.
*/
typedef struct{    
	t_operacion operacion; //NOOP, READ, WRITE
    int cantidad_parametros;
    char **parametros;
} t_instruccion;

//Structs para envio/recibo paquetes
typedef struct
{
	int size;
	void* stream;
} t_buffer;

typedef struct
{
	op_code_t codigo_operacion;
	t_buffer* buffer;
} t_paquete;

extern t_log* logger;


//-------------Funciones de Cliente--------------------
void* serializar_paquete(t_paquete* paquete, int bytes);
int crear_conexion(char *ip, char* puerto);
void enviar_mensaje(char* mensaje, int socket_cliente);
t_buffer* crear_buffer();
t_paquete* crear_paquete(op_code cod_op, t_buffer* un_buffer);
void agregar_a_buffer(t_buffer* un_buffer, void* valor, int tamanio);
void cargar_int_al_buffer(t_buffer* un_buffer, int tamanio_int);
void cargar_uint32_al_buffer(t_buffer* un_buffer, uint32_t tamanio_uint32);
void cargar_string_al_buffer(t_buffer* un_buffer, char* tamanio_string);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void eliminar_paquete(t_paquete* paquete);
void eliminar_buffer(t_buffer* un_buffer);
void liberar_conexion(int socket_cliente);

//-------------Funciones de Server--------------------
int iniciar_servidor(char* puerto, t_log* un_logger, char* mensaje_server);
int esperar_cliente(int socket_servidor, t_log* un_logger, char* mensaje);
int recibir_operacion(int socket_cliente);
t_buffer* recibir_buffer(int socket_cliente);
void* extraer_contenido_del_buffer(t_buffer* un_buffer);
void* extraer_contenido_del_buffer2(t_buffer* un_buffer, int* desplazamiento);

int extraer_int_del_buffer(t_buffer* un_buffer);
char* extraer_string_del_buffer(t_buffer* un_buffer);
uint32_t extraer_uint32_del_buffer(t_buffer* un_buffer);
char* recibir_mensaje(int socket_cliente);

t_list* recibir_paquete(int socket_cliente);
t_buffer* recibir_buffer2(int*, int);


void destruir_instruccion(t_instruccion* instruccion);
void agregar_a_paquete(t_paquete* paquete, void* valor, int bytes);

#endif
