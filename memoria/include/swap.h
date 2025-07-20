#include <memoria.h>


// ---------------- INICIALIZACION ----------------

void crear_swapfile();


// ---------------- SUSPENDER PROCESO ----------------

void escribir_proceso_en_swap(int pid);
int escribir_paginas_recursivo(FILE* swapfile, TABLA_PAGINAS* tabla, int* paginas_escritas);
void* obtener_contenido_de_frame(int frame);

// ---------------- REANUDAR PROCESO ----------------

bool restaurar_proceso_desde_swap(int pid);
void calcular_camino_tabla(int nro_pagina, int* camino_tabla);
ENTRADA_TP* obtener_entrada_tp(int pid, int* camino_tabla_paginas);
int contar_marcos_libres();
void escribir_en_frame(int frame, void* contenido);

