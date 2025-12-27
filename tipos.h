#ifndef TIPOS_H
#define TIPOS_H

#include <stdint.h>

// Tamano de palabra: 8 digitos decimales
#define TAM_PALABRA 8
#define TAM_MEMORIA 2000
#define MEM_SO 300
#define MEM_USUARIO (TAM_MEMORIA - MEM_SO)

// Modos de operacion
#define MODO_USUARIO 0
#define MODO_KERNEL 1

// Codigos de interrupcion
#define INT_SYSCHECK_INVALID 0
#define INT_CODE_INVALID 1
#define INT_SYSCALL 2
#define INT_RELOJ 3
#define INT_IO_FINISH 4
#define INT_INST_INVALID 5
#define INT_DIR_INVALID 6
#define INT_UNDERFLOW 7
#define INT_OVERFLOW 8

// Estados de interrupciones
#define INT_DESHABILITADAS 0
#define INT_HABILITADAS 1

// Tipos de direccionamiento
#define DIR_DIRECTO 0
#define DIR_INMEDIATO 1
#define DIR_INDEXADO 2

// Codigos de condicion
#define CC_IGUAL 0
#define CC_MENOR 1
#define CC_MAYOR 2
#define CC_OVERFLOW 3

// Operaciones DMA
#define DMA_LEER 0
#define DMA_ESCRIBIR 1

// Estado DMA
#define DMA_EXITO 0
#define DMA_ERROR 1

// Dimensiones del disco
#define DISCO_PISTAS 10
#define DISCO_CILINDROS 10
#define DISCO_SECTORES 100
#define TAM_SECTOR 9

// Tipo para representar una palabra de 8 digitos
typedef int32_t palabra_t;

// Registro PSW: [CC][MODO][INT][PC(5 digitos)]
typedef struct {
    int codigo_condicion;  // 1 digito
    int modo;              // 1 digito
    int interrupciones;    // 1 digito
    int pc;                // 5 digitos
} PSW_t;

// Estructura de la CPU
typedef struct {
    palabra_t AC;       // Acumulador
    palabra_t MAR;      // Memory Address Register
    palabra_t MDR;      // Memory Data Register
    palabra_t IR;       // Instruction Register
    palabra_t RB;       // Registro Base
    palabra_t RL;       // Registro Limite
    palabra_t RX;       // Registro base de pila
    palabra_t SP;       // Stack Pointer
    PSW_t PSW;          // Program Status Word
} CPU_t;

// Estructura del DMA
typedef struct {
    int pista;
    int cilindro;
    int sector;
    int operacion;      // Leer o escribir
    int dir_memoria;    // Direccion de memoria
    int estado;         // Estado de la operacion
    int activo;         // Si esta trabajando
} DMA_t;

// Estructura del disco
typedef struct {
    char datos[DISCO_PISTAS][DISCO_CILINDROS][DISCO_SECTORES][TAM_SECTOR];
} Disco_t;

// Estructura de la instruccion decodificada
typedef struct {
    int codigo_op;          // 2 digitos
    int direccionamiento;   // 1 digito
    int valor;              // 5 digitos
} Instruccion_t;

#endif