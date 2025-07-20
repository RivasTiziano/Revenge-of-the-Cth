#include "pcb.h"
#include <utils/utils.h>

void enviar_pc_pid(t_PCB *pcb, int socket, op_code cod_op)
{
    t_buffer *buffer = crear_buffer(); 
    t_paquete *paquete = crear_paquete(cod_op, buffer);
    
    serializar_pcb(paquete,pcb);
    
    enviar_paquete(paquete,socket);
}

// t_PCB* recibir_pc_pid(int socket)
// {
//     t_buffer *buffer = crear_buffer();
    
//     buffer->stream = recibir_buffer(socket);

//     deserializar_pc_pid(buffer,);
    
//     eliminar_buffer(buffer);

//     return pcb;
// }



void deserializar_pc_pid(t_buffer *buffer, int* pc, int* pid) // OJO, recibe direcciones de memoria deserializar_pc_pid(buffer, &pc, &pid)
{   
    //Asigno memoria dinamica para el contexto
    *pc = extraer_int_del_buffer(buffer);
    *pid = extraer_int_del_buffer(buffer);
}

void serializar_pc_pid(t_paquete *paquete,t_PCB *pcb)
{
    agregar_a_paquete(paquete, &(pcb->pc), sizeof(int));
    agregar_a_paquete(paquete, &(pcb->pid), sizeof(int));
}

