#ifndef MEMORIA_H
#define MEMORIA_H

#include "tipos.h"

// Estructura para control de memoria
typedef struct {
    palabra_t datos[TAM_MEMORIA];
    int ocupado[TAM_MEMORIA];
} Memoria_t;

// Inicializa la memoria
void memoria_inicializar(Memoria_t *mem);

// Lee de memoria
palabra_t memoria_leer(Memoria_t *mem, int direccion);

// Escribe en memoria
void memoria_escribir(Memoria_t *mem, int direccion, palabra_t dato);

// Carga programa desde archivo
int memoria_cargar_programa(Memoria_t *mem, const char *archivo, int dir_inicio, int *cant_palabras);

// Carga programa desde un buffer
int memoria_cargar_desde_buffer(Memoria_t *mem, const palabra_t *buffer, int cant_palabras, int dir_inicio);

// Asigna espacio en memoria
int memoria_asignar_espacio(Memoria_t *mem, int tam_requerido);

// Libera espacio en memoria
void memoria_liberar_espacio(Memoria_t *mem, int base, int limite);

#endif