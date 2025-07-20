typedef struct {
    int accesosTP;
    int instrSolicitadas;
    int bajadasSWAP;
    int subidasMP;
    int lecturas;
    int escrituras;
} t_metricas;

typedef enum{
    accesoTP,
    instrSolicitadas,
    bajadasSWAP,
    subidasMP,
    lecturas,
    escrituras
} t_metricas_code;

// Logs mínimos y obligatorios
void log_conexion_kernel(int fd_socket);
void log_creacion_proceso(int pid, int size);
void log_destruccion_proceso(int pid, t_metricas*);
void log_obtener_instruccion(int pid, int PC, char* instruccion);
void log_escritura_lectura(int pid, char* tipo, int dirFisica, int size);
void log_memory_dump(int pid);