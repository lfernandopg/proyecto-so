#include "interrupciones.h"
#include "cpu.h"
#include "logger.h"
#include <stdio.h>

int g_interrupcion_pendiente = 0;
int g_codigo_interrupcion = 0;

void interrupciones_init(VectorInterrupciones_t *vec) {
    int i;
    for (i = 0; i < 9; i++) {
        vec->manejadores[i] = 0; // Direcciones por defecto
    }
    log_mensaje("Vector de interrupciones inicializado");
}

void lanzar_interrupcion(int codigo) {
    if (codigo < 0 || codigo > 8) {
        lanzar_interrupcion(INT_CODE_INVALID);
        return;
    }
    
    g_interrupcion_pendiente = 1;
    g_codigo_interrupcion = codigo;
    
    char msg[200];
sprintf(msg, "INTERRUPCION ARROJADA: Codigo %d - %s", 
            codigo, obtener_nombre_interrupcion(codigo));
    log_interrupcion(msg);
    printf("\n*** %s ***\n", msg);
}

const char* obtener_nombre_interrupcion(int codigo) {
    switch(codigo) {
        case INT_SYSCHECK_INVALID:
            return "Codigo de llamada al sistema invalido";
        case INT_CODE_INVALID:
            return "Codigo de interrupcion invalido";
        case INT_SYSCALL:
            return "Llamada al sistema";
        case INT_RELOJ:
            return "Interrupcion de reloj";
        case INT_IO_FINISH:
            return "Finalizacion de operacion E/S";
        case INT_INST_INVALID:
            return "Instruccion invalida";
        case INT_DIR_INVALID:
            return "Direccionamiento invalido";
        case INT_UNDERFLOW:
            return "Underflow";
        case INT_OVERFLOW:
            return "Overflow";
        default:
            return "Interrupcion desconocida";
    }
}

void procesar_interrupcion(CPU_t *cpu, palabra_t *memoria, VectorInterrupciones_t *vec) {
    if (!g_interrupcion_pendiente) {
        return;
    }
    
    // Verificar si interrupciones estan habilitadas
    if (cpu->PSW.interrupciones == INT_DESHABILITADAS) {
        // Solo algunas interrupciones criticas se procesan
        if (g_codigo_interrupcion != INT_OVERFLOW && 
            g_codigo_interrupcion != INT_DIR_INVALID &&
            g_codigo_interrupcion != INT_INST_INVALID) {
            return; // Postponer interrupcion
        }
    }
    
    char msg[200];
    sprintf(msg, "PROCESANDO INTERRUPCION: Codigo %d - %s", 
            g_codigo_interrupcion, obtener_nombre_interrupcion(g_codigo_interrupcion));
    log_interrupcion(msg);
    
    // Salvar contexto
    cpu_salvar_contexto(cpu, memoria);
    
    // Cambiar a modo kernel
    int modo_anterior = cpu->PSW.modo;
    cpu->PSW.modo = MODO_KERNEL;
    
    // Deshabilitar interrupciones durante el manejo
    cpu->PSW.interrupciones = INT_DESHABILITADAS;
    
    // Obtener direccion del manejador desde el vector
    int dir_manejador = vec->manejadores[g_codigo_interrupcion];
    
    if (dir_manejador > 0) {
        cpu->PSW.pc = dir_manejador;
    } else {
        // Manejador por defecto: simplemente retornar
        sprintf(msg, "No hay manejador para interrupcion %d, continuando...", 
                g_codigo_interrupcion);
        log_mensaje(msg);
    }
    
    // Limpiar bandera de interrupcion
    g_interrupcion_pendiente = 0;
    
    // Restaurar contexto
    // En una implementacion real, esto lo haria el manejador al terminar
    cpu_restaurar_contexto(cpu, memoria);
    cpu->PSW.modo = modo_anterior;
    cpu->PSW.interrupciones = INT_HABILITADAS;
}