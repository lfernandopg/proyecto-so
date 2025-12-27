#ifndef SISTEMA_H
#define SISTEMA_H

#include "tipos.h"
#include "cpu.h"
#include "memoria.h"
#include "dma.h"
#include "interrupciones.h"
#include <pthread.h>

// Estructura principal del sistema
typedef struct {
    CPU_t cpu;
    Memoria_t memoria;
    ControladorDMA_t dma;
    VectorInterrupciones_t vector_int;
    pthread_mutex_t mutex_bus;
    pthread_mutex_t mutex_memoria;
    int ejecutando;
    int ciclos_reloj;
    int periodo_reloj;
} Sistema_t;

// Inicializa el sistema
void sistema_init(Sistema_t *sys);

// Ejecuta un programa
void sistema_ejecutar_programa(Sistema_t *sys, const char *archivo, int modo_debug);

// Ciclo principal de ejecucion
void sistema_ciclo(Sistema_t *sys);

// Limpia recursos del sistema
void sistema_cleanup(Sistema_t *sys);

// Consola interactiva
void sistema_consola(Sistema_t *sys);

// Modo debugger
void sistema_debugger(Sistema_t *sys);

#endif