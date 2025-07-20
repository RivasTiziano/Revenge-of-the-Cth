#ifndef PCB_H_
#define PCB_H_

#include <stdlib.h>
#include <stdio.h>
#include <utils/utils.h>

void enviar_pcb(t_PCB *pcb, int socket, op_code cod_op);
t_PCB *recibir_pcb(int socket);
t_PCB *deserializar_pcb(t_buffer *buffer);
void serializar_pcb(t_paquete *paquete, t_PCB *pcb);
void agregar_a_paquete(t_paquete* paquete, void* valor, int bytes);

#endif