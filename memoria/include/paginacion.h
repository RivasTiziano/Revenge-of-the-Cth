#include <memoria.h>

// --------------------------------------- ESTRUCTURAS ---------------------------------------

typedef struct ENTRADA_TP ENTRADA_TP;

typedef struct {
    int nivel;
    ENTRADA_TP *entradas; // Array dinámico de entradas
} TABLA_PAGINAS;

struct ENTRADA_TP {
    int nroPag;
    bool presente;
    bool es_hoja;
    union {
        int nroFrame;              // Si es hoja
        TABLA_PAGINAS *sgteTP;     // Si no es hoja
    } link;
};


// --------------------------------------- VARIABLES GLOBALES ---------------------------------------

extern int CANTIDAD_NIVELES; // Viene de archivo de configuración
extern void* MEMORIA_USUARIO; // Espacio de memoria reservado para procesos
extern int MEMORIA_USUARIO_EN_USO; // Bytes en uso de la memoria de usuario
extern uint8_t* BITMAP_MARCOS; // Bitmap para control de marcos libres y en uso
extern int CANTIDAD_MARCOS; // Cantidad de marcos en la memoria de usuario
extern int PAGINAS_TOTALES_PROCESO; // Páginas totales que puede llegar a tener un proceso
extern int MAX_PROCESOS; // Cantidad máxima de procesos que se pueden tener en memoria si todos ocupan todas sus páginas
extern t_dictionary *PROCESO_TABLAS; // Array de relaciones PID/Tabla de páginas raíz.


// --------------------------------------- FUNCIONES ---------------------------------------

// INICIALIZACION

void inicializar_MEMORIA_USUARIO();
void inicializar_BITMAP_MARCOS();
void inicializar_PROCESO_TABLAS();

// FUNCIONES DE PROCESO_TABLAS

TABLA_PAGINAS *obtenerTablaRaiz(int);
void destruir_tabla_paginas_recursiva(TABLA_PAGINAS*, int);
void destruir_tabla_paginas(void*);
bool registrarProceso(int, TABLA_PAGINAS*);
void destruir_PROCESO_TABLAS();

// FUNCIONES DE MEMORIA

int memoria_usada(void);
int memoria_disponible(void);
void registrar_uso_memoria(int);
void mapear_pagina(TABLA_PAGINAS *tabla, int nro_pag, int nro_frame, int nivel_actual);
int tam_pagina(void);
int entradas_por_tabla(void);
int tam_memoria(void);
void calcular_camino_tabla(int nro_pagina, int* camino_tabla);

// FUNCIONES DE BITMAP

static inline bool frame_libre(int frame) { return (BITMAP_MARCOS[frame / 8] &   (1u << (frame % 8))) == 0; }
static inline void ocupar_frame(int frame) { BITMAP_MARCOS[frame / 8] |=  (1u << (frame % 8)); }
static inline void liberar_frame(int frame) { BITMAP_MARCOS[frame / 8] &= ~(1u << (frame % 8)); }
int reservar_frame_libre(void);
bool esta_ocupado(int frame);

// FUNCIONES DE TABLA DE PAGINAS

TABLA_PAGINAS *crear_tabla_paginas(int);
int obtener_numero_marco(int pid, int *camino_tabla_paginas);
int indice_nivel(int nro_pag, int nivel_actual, int niveles_tot);


// FINALIZACION
void liberarMemoriaPaginacion();
