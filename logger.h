#ifndef LOGGER_H
#define LOGGER_H

#include "tipos.h"

// Inicializa el sistema de logging
void log_init(void);

// Cierra el archivo de log
void log_close(void);

// Registra un mensaje general
void log_mensaje(const char *mensaje);

// Registra una operacion
void log_operacion(const char *op, palabra_t op1, palabra_t op2, palabra_t resultado);

// Registra un error
void log_error(const char *mensaje, int codigo);

// Registra una interrupcion
void log_interrupcion(const char *mensaje);

#endif