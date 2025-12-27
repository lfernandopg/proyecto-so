#ifndef INTERRUPCIONES_H
#define INTERRUPCIONES_H

#include "tipos.h"

// Vector de interrupciones
typedef struct {
    int manejadores[9]; // Direcciones de los manejadores
} VectorInterrupciones_t;

// Variables globales para control de interrupciones
extern int g_interrupcion_pendiente;
extern int g_codigo_interrupcion;

// Inicializa el vector de interrupciones
void interrupciones_init(VectorInterrupciones_t *vec);

// Lanza una interrupcion
void lanzar_interrupcion(int codigo);

// Procesa la interrupcion pendiente
void procesar_interrupcion(CPU_t *cpu, palabra_t *memoria, VectorInterrupciones_t *vec);

// Obtiene descripcion de la interrupcion
const char* obtener_nombre_interrupcion(int codigo);

#endif