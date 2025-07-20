#include <utils/utils.h>


void destruir_instruccion(t_instruccion* instruccion) {
    if (instruccion == NULL) return;

    for (int i = 0; i < instruccion->cantidad_parametros; i++) {
        free(instruccion->parametros[i]); // Libera cada string
    }

    free(instruccion->parametros); // Libera el array de punteros
    free(instruccion); // Libera la instrucción en sí
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int bytes) {
	t_buffer *buffer = paquete->buffer;
	buffer->stream = realloc(buffer->stream, buffer->size + bytes);
	memcpy(buffer->stream + buffer->size, valor, bytes);
	buffer->size += bytes;
}


//-----------------------------CLIENTE---------------------------------------

void* serializar_paquete(t_paquete* paquete, int bytes)
{
	void * magic = malloc(bytes);
	int desplazamiento = 0;

	memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
	desplazamiento+= sizeof(int);
	memcpy(magic + desplazamiento, paquete->buffer->stream, paquete->buffer->size);
	desplazamiento+= paquete->buffer->size;

	return magic;
}


int crear_conexion(char *ip, char* puerto)
{
	struct addrinfo hints;
	struct addrinfo *server_info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(ip, puerto, &hints, &server_info);

	// Ahora vamos a crear el socket.
	//int socket_cliente = 0;
	int socket_cliente = socket(server_info->ai_family,
                         server_info->ai_socktype,
                         server_info->ai_protocol);

	// Ahora que tenemos el socket, vamos a conectarlo
	if(connect(socket_cliente,server_info->ai_addr,server_info->ai_addrlen) == -1){
        printf("No fue exitosa la conexion con el cliente %d.", socket_cliente);
        abort();
    }

	freeaddrinfo(server_info);

	return socket_cliente;
}



void enviar_mensaje(char* mensaje, int socket_cliente)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));

	paquete->codigo_operacion = MENSAJE;
	paquete->buffer = malloc(sizeof(t_buffer));
	paquete->buffer->size = strlen(mensaje) + 1;
	paquete->buffer->stream = malloc(paquete->buffer->size);
	memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

	int bytes = paquete->buffer->size + 2*sizeof(int);

	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}



t_buffer* crear_buffer()
{
	//Reserbamos memoria para el buffer
	t_buffer* un_buffer = malloc(sizeof(t_buffer));
	//iniciamos su size en 0 y su stream vacio
	un_buffer->size = 0;
	un_buffer->stream = NULL;

	return un_buffer;
}

t_paquete* crear_paquete(op_code cod_op, t_buffer* un_buffer)
{
	t_paquete* paquete = malloc(sizeof(t_paquete));
	paquete->codigo_operacion = cod_op;
	paquete->buffer = un_buffer;
	return paquete;
}


//Funciones para la serializacion
//recibe un buffer creado, algo y su tamanio para agregar al buffer
void agregar_a_buffer(t_buffer* un_buffer, void* valor, int tamanio)
{
	//si el buffer esta vacio
	if (un_buffer->size == 0)
	{
		//reservamos memoria para su size y el int
		un_buffer->stream = malloc(sizeof(int) + tamanio);
		//copiamos en el buffer lo que ingreso
		memcpy(un_buffer->stream, &tamanio, sizeof(int));
		//nos desplazamos y copiamos en el buffer el tamanio de lo que ingreso
		memcpy(un_buffer->stream + sizeof(int), valor, tamanio);
	}
	else{ //sino estaba vacio
		//agreandamos el espacio en memoria a lo que necesitemos
		un_buffer->stream = realloc(un_buffer->stream, un_buffer->size + tamanio + sizeof(int));
		//copiamos en el buffer lo que ingreso
		memcpy(un_buffer->stream + un_buffer->size, &tamanio, sizeof(int));
		//nos desplazamos y copiamos en el buffer el tamanio de lo que ingreso
		memcpy(un_buffer->stream + un_buffer->size + sizeof(int), valor, tamanio);
	}
	

	//actualizamos el buffer
	un_buffer->size += sizeof(int);
	un_buffer->size += tamanio;
	//un_buffer->size += tamanio + sizeof(int);
}

//Agrega una variable de tipo int al buffer
void cargar_int_al_buffer(t_buffer* un_buffer, int valor)
{
	agregar_a_buffer(un_buffer, &valor, sizeof(int));
}

//Agrega una variable de tipo uint32 al buffer
void cargar_uint32_al_buffer(t_buffer* un_buffer, uint32_t valor)
{
	agregar_a_buffer(un_buffer, &valor, sizeof(uint32_t));
}

//Agrega una variable de tipo string al buffer
void cargar_string_al_buffer(t_buffer* un_buffer, char* tamanio_string)
{
	agregar_a_buffer(un_buffer, tamanio_string, strlen(tamanio_string) + 1);
}

void enviar_paquete(t_paquete* paquete, int socket_cliente)
{
	int bytes = paquete->buffer->size + 2*sizeof(int);
	void* a_enviar = serializar_paquete(paquete, bytes);

	send(socket_cliente, a_enviar, bytes, 0);

	free(a_enviar);
	eliminar_paquete(paquete);
}


void eliminar_paquete(t_paquete* paquete)
{
	free(paquete->buffer->stream);
	free(paquete->buffer);
	free(paquete);
}

void eliminar_buffer(t_buffer* un_buffer)
{
	if (un_buffer != NULL)
	{
		free(un_buffer->stream);
	}
	free(un_buffer);
}


void liberar_conexion(int socket_cliente)
{
	close(socket_cliente);
}

//-----------------------------SERVIDOR---------------------------------------

//Se le pasa un puerto donde va estar escuchamdo, su logguer y el mensaje
int iniciar_servidor(char* puerto, t_log* un_logger, char* mensaje_server)
{
	int socket_servidor;

	//Nunca se uso la variable p
	struct addrinfo hints, *servinfo;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, puerto, &hints, &servinfo);

	//Para estas 3 copie lo que estaba en la guia de sockets y cambie nombre de variables
	// Creamos el socket de escucha del servidor
	socket_servidor = socket(servinfo->ai_family,
                         servinfo->ai_socktype,
                         servinfo->ai_protocol);


	int opt = 1;
	if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}					 
	// Asociamos el socket a un puerto
	bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
	// Escuchamos las conexiones entrantes
	listen(socket_servidor, SOMAXCONN);

	freeaddrinfo(servinfo);
	log_debug(un_logger, "\x1b[35mServer: %s", mensaje_server);

	return socket_servidor;
}


int esperar_cliente(int socket_servidor, t_log* un_logger, char* mensaje) {
    int socket_cliente = accept(socket_servidor, NULL, NULL);

    if (socket_cliente == -1) {
        log_debug(un_logger, "Falló al aceptar cliente en: %s", mensaje);
        return -1;
    }

    log_debug(un_logger, "\x1b[35mSe conectó el cliente: %s", mensaje);
    return socket_cliente;
}

int recibir_operacion(int socket_cliente)
{
	int cod_op;
	if(recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
		return cod_op;
	else
	{
		close(socket_cliente);
		return -1;
	}
}


t_buffer* recibir_buffer(int socket_cliente)
{
	t_buffer * buffer =malloc(sizeof(t_buffer));

	if (recv(socket_cliente, &(buffer->size), sizeof(int), MSG_WAITALL) > 0)
	{
		buffer->stream = malloc(buffer->size);

		if (recv(socket_cliente, buffer->stream, buffer->size, MSG_WAITALL) > 0)
		{
			return buffer;
		}
		else{
			perror("Error al recibir al void* del buffer de la conexion");
			exit(EXIT_FAILURE);
		}
		
	}
	else{
		perror("Error al recibir el tamanio del buffer de la conexion");
			exit(EXIT_FAILURE);
	}
	

	return buffer;
}


void* extraer_contenido_del_buffer2(t_buffer* un_buffer, int* desplazamiento)
{
	int tamanio_contenido;
	memcpy(&tamanio_contenido, un_buffer->stream + *desplazamiento, sizeof(int));
	*desplazamiento += sizeof(int);

	void* contenido = malloc(tamanio_contenido);
	memcpy(contenido, un_buffer->stream + *desplazamiento, tamanio_contenido);
	*desplazamiento += tamanio_contenido;

	return contenido;
}



void* extraer_contenido_del_buffer(t_buffer* un_buffer)
{
	if (un_buffer == NULL) {
		printf("\n[ERROR] Buffer NULL recibido\n\n");
		return NULL;
	}

	if (un_buffer->size == 0)
	{
		printf("\n[ERROR] Al intentar extraer contenido de un t_buffer vacio \n\n");
		return NULL;
	}
	
	if (un_buffer->size < 0)
	{
		printf("\n[ERROR] Esto es raro. El t_buffer contiene un size negativo \n\n");
		return NULL;
	}

	if (un_buffer->stream == NULL) {
		printf("\n[ERROR] Buffer stream es NULL\n\n");
		return NULL;
	}

	if (un_buffer->size < sizeof(int)) {
		printf("\n[ERROR] Buffer muy pequeño para contener tamaño\n\n");
		return NULL;
	}
	
	int tamanio_contenido;
	memcpy(&tamanio_contenido, un_buffer->stream, sizeof(int));

	if (tamanio_contenido < 0) {
		printf("\n[ERROR] Tamaño de contenido negativo: %d\n\n", tamanio_contenido);
		return NULL;
	}

	if (tamanio_contenido > un_buffer->size - sizeof(int)) {
		printf("\n[ERROR] Tamaño de contenido (%d) mayor que buffer disponible (%d)\n\n", 
			   tamanio_contenido, un_buffer->size - (int)sizeof(int));
		return NULL;
	}

	void* contenido = malloc(tamanio_contenido);
	if (!contenido) {
		printf("\n[ERROR] No se pudo reservar memoria para contenido\n\n");
		return NULL;
	}

	memcpy(contenido, un_buffer->stream + sizeof(int), tamanio_contenido);

	int nuevo_tamanio = un_buffer->size - sizeof(int) - tamanio_contenido;

	if (nuevo_tamanio == 0)
	{
		un_buffer->size = 0;
		free(un_buffer->stream);
		un_buffer->stream = NULL;
		return contenido;
	}
	
	if (nuevo_tamanio < 0)
	{
		printf("\n[ERROR] Buffer con tamanio negativo calculado: %d\n", nuevo_tamanio);
		free(contenido);
		return NULL;
	}
	
	void* nuevo_stream = malloc(nuevo_tamanio);
	if (!nuevo_stream) {
		printf("\n[ERROR] No se pudo reservar memoria para nuevo stream\n\n");
		free(contenido);
		return NULL;
	}

	memcpy(nuevo_stream, un_buffer->stream + sizeof(int) + tamanio_contenido, nuevo_tamanio);
	free(un_buffer->stream);
	un_buffer->size = nuevo_tamanio;
	un_buffer->stream = nuevo_stream;

	return contenido;
}


//Extrae del buffer el valor si es del tipo int
int extraer_int_del_buffer(t_buffer* un_buffer)
{
	int* valor_int = extraer_contenido_del_buffer(un_buffer);
	if (valor_int == NULL) {
		printf("\n[ERROR] No se pudo extraer int del buffer\n\n");
		return -1; // Valor de error
	}
	int valor_retorno = *valor_int;
	free(valor_int);
	return valor_retorno;
}

//Extrae del buffer el valor si es del tipo string
char* extraer_string_del_buffer(t_buffer* un_buffer)
{
	char* valor_string = extraer_contenido_del_buffer(un_buffer);
	if (valor_string == NULL) {
		printf("\n[ERROR] No se pudo extraer string del buffer\n\n");
		return NULL;
	}
	return valor_string;
}

//Extrae del buffer el valor si es del tipo uint32
uint32_t extraer_uint32_del_buffer(t_buffer* un_buffer)
{
	uint32_t* valor_uint = extraer_contenido_del_buffer(un_buffer);
	if (valor_uint == NULL) {
		printf("\n[ERROR] No se pudo extraer uint32 del buffer\n\n");
		return 0; // Valor de error
	}
	uint32_t valor_retorno = *valor_uint;
	free(valor_uint);
	return valor_retorno;
}


char* recibir_mensaje(int socket_cliente)
{
	t_buffer* buffer = recibir_buffer(socket_cliente);
	if (!buffer) return NULL;

	char* mensaje = strdup(buffer->stream);
	free(buffer);
	
	return mensaje;
}


t_buffer* recibir_buffer2(int* size, int socket_cliente)
{
	void * buffer;
	recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
	
	buffer = malloc(*size);
	
	recv(socket_cliente, buffer, *size, MSG_WAITALL);

	return buffer;
}


t_list* recibir_paquete(int socket_cliente)
{
	
	int size;
	int desplazamiento = 0;
	t_buffer * buffer;
	t_list* valores = list_create();
	int tamanio;

	buffer = recibir_buffer2(&size, socket_cliente);

	while(desplazamiento < size)
	{
		memcpy(&tamanio, buffer->stream + desplazamiento, sizeof(int));
		desplazamiento+=sizeof(int);
		char* valor = malloc(tamanio);
		memcpy(valor, buffer->stream + desplazamiento, tamanio);
		desplazamiento+=tamanio;
		list_add(valores, valor);
	}
	free(buffer);
	return valores;
}
