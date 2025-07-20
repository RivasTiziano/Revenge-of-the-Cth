
t_operacion obtener_operacion_cpu(const char *);
void leer_instrucciones_desde_archivo(FILE* nuevo_archivo, int pid_asociado, int tamanio);
t_list *obtener_instrucciones_proceso(int pid);
void serializar_instruccion(t_instruccion *instruccion, t_buffer *buffer);
void enviar_instruccion(int *socket_cpu, t_buffer *buffer);

int get_cant_parametros(t_operacion); 
