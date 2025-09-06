# Sistema Operativo Distribuido en C

*Revenge of the Cth* es un sistema operativo simulado desarrollado en C que implementa una arquitectura distribuida modular. Cada componente replica de manera simplificada comportamientos reales de un sistema operativo moderno, incluyendo planificación de procesos, manejo de memoria paginada, interacción con dispositivos de entrada/salida y ejecución de instrucciones en CPU. Hecho con un equipo de 4 personas con un plazo de 3-4 meses, principalmente para fines academicos en la Universidad Tecnologica Nacional de Buenos Aires con el apoyo de los ayudantes y profesores de la asignatura de Sistemas Operativos.

## Características Principales

* Arquitectura modular distribuida: CPU, Kernel, Memoria (con SWAP), IO.
* Planificación de procesos en tres niveles: largo, mediano y corto plazo.
* Soporte de distintos algoritmos de planificación:

  * Largo plazo: FIFO, Proceso Más Chico Primero (PMCP).
  * Corto plazo: FIFO, SJF sin desalojo, SJF con desalojo (SRT).
* Administración de memoria con:

  * Paginación multinivel.
  * TLB (FIFO o LRU).
  * Caché de páginas (CLOCK o CLOCK-M).
  * Archivo de SWAP.
* Ejecución de pseudocódigo interpretado por CPU.
* Soporte de syscall simuladas: IO, INIT\_PROC, DUMP\_MEMORY, EXIT.
* Sistema de logs detallados por módulo y por proceso.

---

## Módulos

### Kernel

* Gestiona el ciclo de vida completo de los procesos.
* Mantiene colas de estados y transiciones: NEW, READY, EXEC, BLOCKED, SUSP\_READY, SUSP\_BLOCKED, EXIT.
* Administra dispositivos IO y comunicaciones con CPU y Memoria.
* Calcula métricas de uso por proceso (cantidad y tiempo en cada estado).

### CPU

* Ejecuta instrucciones paso a paso con ciclo de instrucción: Fetch → Decode → Execute → Check Interrupt.
* Traduce direcciones lógicas a físicas mediante MMU.
* Implementa TLB y caché de páginas.
* Ejecuta instrucciones como `NOOP`, `READ`, `WRITE`, `IO`, `EXIT`, etc.

### Memoria + SWAP

* Simula espacio contiguo de usuario y estructuras administrativas (tablas de páginas).
* Maneja lectura/escritura de memoria, suspensión y recuperación desde SWAP.
* Paginación multinivel configurable.
* Genera archivos de dump automáticos por proceso.

### IO

* Simula dispositivos de entrada/salida (como impresoras, discos).
* Ejecuta operaciones con retardo configurado.
* Comunicación asincrónica con Kernel.

---

## Comunicación

* Los módulos se comunican mediante sockets TCP.
* Cada CPU mantiene dos canales con el Kernel (dispatch e interrupt).
* El Kernel crea conexiones efímeras con Memoria para cada operación.

---

## 🚀 Cómo levantar el proyecto

### 🧱 Compilar los módulos

Desde el directorio raíz de cada módulo, ejecutar `make` para compilar:

```bash
utnso@utnso:~/Desktop/tp-2025-1c-Grupo-Operativos-/memoria$ make
utnso@utnso:~/Desktop/tp-2025-1c-Grupo-Operativos-/kernel$ make
utnso@utnso:~/Desktop/tp-2025-1c-Grupo-Operativos-/cpu$ make
utnso@utnso:~/Desktop/tp-2025-1c-Grupo-Operativos-/io$ make
```
### ▶️ Orden de ejecución

Los módulos deben iniciarse en el siguiente orden:

1. `memoria`
2. `kernel instrucciones2.txt 128`
3. `cpu 0`
4. Presionar `ENTER` en la consola del kernel
5. `io`

> **Importante:** Cuando se ejecute el Kernel, este pedirá un `ENTER`. **No lo presiones aún** — primero asegurate de levantar la CPU.

---

###  🎯 Argumentos de ejecución

| Módulo   | Argumentos                                |
|----------|-------------------------------------------|
| memoria  | *(sin argumentos)*                        |
| kernel   | `<archivo> <tamaño>` (por ejemplo: instrucciones2.txt 128) |
| cpu      | `<id_cpu>`   (por ejemplo: `0`)               |
| io       | `<dispositivo>` (por ejemplo: `DISCO`)|

---

###  📦 Comandos para ejecutar los módulos

Desde la raíz de cada módulo, ejecutar:

#### 🧠 Memoria
```bash
./bin/memoria
```

#### 🧠 Kernel
```bash
./bin/kernel instrucciones2.txt 128
```

> 🔴 **No presionar ENTER todavía en la terminal del kernel.**

#### 🧮 CPU
```bash
./bin/cpu 0
```

#### ⌨️ Volver al kernel y presionar ENTER
Una vez levantada la CPU, ir a la terminal del kernel y presionar `ENTER` para comenzar la ejecución.

#### 💿 I/O
```bash
./bin/io DISCO
```

La consola de I/O deberia mostrar el siguiente mensaje:
```
[INFO] hh:mm:ss:ms LOGGER I/O/(PID): ## PID: 1 - Fin de IO
```

Y en la terminal de kernel:
```
[INFO] hh:mm:ss:ms LOGGER KERNEL/(PID): ## (1) finalizó IO y pasa a READY
```


## Dependencias

Para poder compilar y ejecutar el proyecto, es necesario tener instalada la
biblioteca [so-commons-library] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-commons-library
cd so-commons-library
make debug
make install
```

## Compilación y ejecución

Cada módulo del proyecto se compila de forma independiente a través de un
archivo `makefile`. Para compilar un módulo, es necesario ejecutar el comando
`make` desde la carpeta correspondiente.

El ejecutable resultante de la compilación se guardará en la carpeta `bin` del
módulo. Ejemplo:

```sh
cd kernel
make
./bin/kernel
```



