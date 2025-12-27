#include "cpu.h"
#include "interrupciones.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

extern int g_modo_debug;

void cpu_init(CPU_t *cpu) {
    cpu->AC = 0;
    cpu->MAR = 0;
    cpu->MDR = 0;
    cpu->IR = 0;
    cpu->RB = MEM_SO;
    cpu->RL = TAM_MEMORIA - 1;
    cpu->RX = 0;
    cpu->SP = 0;
    cpu->PSW.codigo_condicion = 0;
    cpu->PSW.modo = MODO_KERNEL;
    cpu->PSW.interrupciones = INT_HABILITADAS;
    cpu->PSW.pc = MEM_SO;
    
    log_mensaje("CPU inicializada");
}

void cpu_fetch(CPU_t *cpu, palabra_t *memoria) {
    // MAR obtiene PC
    cpu->MAR = cpu->PSW.pc;
    
    // MDR obtiene contenido de memoria[MAR]
    cpu->MDR = memoria[cpu->MAR];
    
    // IR obtiene MDR
    cpu->IR = cpu->MDR;
    
    // Incrementar PC
    cpu->PSW.pc++;
    
    if (g_modo_debug) {
        printf("FETCH: MAR=%d, MDR=%08d, IR=%08d, PC=%d\n", 
               cpu->MAR, cpu->MDR, cpu->IR, cpu->PSW.pc);
    }
}

Instruccion_t cpu_decode(palabra_t instruccion_raw) {
    Instruccion_t inst;
    
    // Extraer campos de la instruccion
    inst.codigo_op = instruccion_raw / 1000000;
    inst.direccionamiento = (instruccion_raw / 100000) % 10;
    inst.valor = instruccion_raw % 100000;
    
    return inst;
}

int cpu_calcular_direccion(CPU_t *cpu, Instruccion_t inst) {
    int direccion = 0;
    
    switch(inst.direccionamiento) {
        case DIR_DIRECTO:
            direccion = inst.valor;
            break;
        case DIR_INMEDIATO:
            direccion = -1; // No aplica
            break;
        case DIR_INDEXADO:
            direccion = cpu->AC + inst.valor;
            break;
    }
    
    return direccion;
}

palabra_t cpu_obtener_operando(CPU_t *cpu, Instruccion_t inst, palabra_t *memoria) {
    palabra_t operando = 0;
    
    switch(inst.direccionamiento) {
        case DIR_DIRECTO:
            if (cpu->PSW.modo == MODO_USUARIO) {
                int dir_fisica = cpu->RB + inst.valor;
                if (!cpu_verificar_memoria(cpu, dir_fisica)) {
                    lanzar_interrupcion(INT_DIR_INVALID);
                    return 0;
                }
                operando = memoria[dir_fisica];
            } else {
                operando = memoria[inst.valor];
            }
            break;
        case DIR_INMEDIATO:
            operando = inst.valor;
            break;
        case DIR_INDEXADO:
            if (cpu->PSW.modo == MODO_USUARIO) {
                int dir_fisica = cpu->RB + cpu->AC + inst.valor;
                if (!cpu_verificar_memoria(cpu, dir_fisica)) {
                    lanzar_interrupcion(INT_DIR_INVALID);
                    return 0;
                }
                operando = memoria[dir_fisica];
            } else {
                operando = memoria[cpu->AC + inst.valor];
            }
            break;
    }
    
    return operando;
}

int cpu_verificar_memoria(CPU_t *cpu, int direccion) {
    return (direccion >= cpu->RB && direccion <= cpu->RL);
}

void cpu_actualizar_cc(CPU_t *cpu, palabra_t resultado) {
    // Detectar overflow (mas de 7 digitos de magnitud)
    if (abs(resultado) > 9999999) {
        cpu->PSW.codigo_condicion = CC_OVERFLOW;
        lanzar_interrupcion(INT_OVERFLOW);
    } else if (resultado == 0) {
        cpu->PSW.codigo_condicion = CC_IGUAL;
    } else if (resultado < 0) {
        cpu->PSW.codigo_condicion = CC_MENOR;
    } else {
        cpu->PSW.codigo_condicion = CC_MAYOR;
    }
}

void cpu_execute(CPU_t *cpu, Instruccion_t inst, palabra_t *memoria) {
    palabra_t operando;
    palabra_t resultado;
    int direccion;
    
    if (g_modo_debug) {
        printf("EXECUTE: OP=%02d, DIR=%d, VAL=%05d\n", 
               inst.codigo_op, inst.direccionamiento, inst.valor);
    }
    
    switch(inst.codigo_op) {
        case 0: // sum
            operando = cpu_obtener_operando(cpu, inst, memoria);
            resultado = cpu->AC + operando;
            cpu_actualizar_cc(cpu, resultado);
            cpu->AC = resultado;
            log_operacion("SUM", cpu->AC, operando, resultado);
            break;
            
        case 1: // res
            operando = cpu_obtener_operando(cpu, inst, memoria);
            resultado = cpu->AC - operando;
            cpu_actualizar_cc(cpu, resultado);
            cpu->AC = resultado;
            log_operacion("RES", cpu->AC, operando, resultado);
            break;
            
        case 2: // mult
            operando = cpu_obtener_operando(cpu, inst, memoria);
            resultado = cpu->AC * operando;
            cpu_actualizar_cc(cpu, resultado);
            cpu->AC = resultado;
            log_operacion("MULT", cpu->AC, operando, resultado);
            break;
            
        case 3: // divi
            operando = cpu_obtener_operando(cpu, inst, memoria);
            if (operando == 0) {
                lanzar_interrupcion(INT_OVERFLOW);
            } else {
                resultado = cpu->AC / operando;
                cpu_actualizar_cc(cpu, resultado);
                cpu->AC = resultado;
                log_operacion("DIVI", cpu->AC, operando, resultado);
            }
            break;
            
        case 4: // load
            operando = cpu_obtener_operando(cpu, inst, memoria);
            cpu->AC = operando;
            log_operacion("LOAD", cpu->AC, operando, cpu->AC);
            break;
            
        case 5: // str
            direccion = cpu_calcular_direccion(cpu, inst);
            if (cpu->PSW.modo == MODO_USUARIO) {
                int dir_fisica = cpu->RB + direccion;
                if (!cpu_verificar_memoria(cpu, dir_fisica)) {
                    lanzar_interrupcion(INT_DIR_INVALID);
                    break;
                }
                memoria[dir_fisica] = cpu->AC;
            } else {
                memoria[direccion] = cpu->AC;
            }
            log_operacion("STR", cpu->AC, direccion, memoria[direccion]);
            break;
            
        case 6: // loadrx
            cpu->AC = cpu->RX;
            log_operacion("LOADRX", cpu->AC, cpu->RX, cpu->AC);
            break;
            
        case 7: // strrx
            cpu->RX = cpu->AC;
            log_operacion("STRRX", cpu->AC, cpu->RX, cpu->RX);
            break;
            
        case 8: // comp
            operando = cpu_obtener_operando(cpu, inst, memoria);
            resultado = cpu->AC - operando;
            cpu_actualizar_cc(cpu, resultado);
            log_operacion("COMP", cpu->AC, operando, resultado);
            break;
            
        case 9: // jmpe
            if (cpu->PSW.codigo_condicion == CC_IGUAL) {
                cpu->PSW.pc = memoria[inst.valor];
                log_operacion("JMPE", cpu->AC, inst.valor, cpu->PSW.pc);
            }
            break;
            
        case 10: // jmpne
            if (cpu->PSW.codigo_condicion != CC_IGUAL) {
                cpu->PSW.pc = memoria[inst.valor];
                log_operacion("JMPNE", cpu->AC, inst.valor, cpu->PSW.pc);
            }
            break;
            
        case 11: // jmplt
            if (cpu->PSW.codigo_condicion == CC_MENOR) {
                cpu->PSW.pc = memoria[inst.valor];
                log_operacion("JMPLT", cpu->AC, inst.valor, cpu->PSW.pc);
            }
            break;
            
        case 12: // jmpgt
            if (cpu->PSW.codigo_condicion == CC_MAYOR) {
                cpu->PSW.pc = memoria[inst.valor];
                log_operacion("JMPGT", cpu->AC, inst.valor, cpu->PSW.pc);
            }
            break;
            
        case 13: // svc
            lanzar_interrupcion(INT_SYSCALL);
            log_operacion("SVC", cpu->AC, 0, 0);
            break;
            
        case 14: // retrn
            cpu->PSW.pc = memoria[cpu->SP];
            cpu->SP--;
            log_operacion("RETRN", cpu->PSW.pc, cpu->SP, cpu->PSW.pc);
            break;
            
        case 15: // hab
            cpu->PSW.interrupciones = INT_HABILITADAS;
            log_mensaje("Interrupciones habilitadas");
            break;
            
        case 16: // dhab
            cpu->PSW.interrupciones = INT_DESHABILITADAS;
            log_mensaje("Interrupciones deshabilitadas");
            break;
            
        case 17: // tti - establecer tiempo de reloj
            // Se maneja en el sistema principal
            log_operacion("TTI", inst.valor, 0, 0);
            break;
            
        case 18: // chmod
            if (cpu->PSW.modo == MODO_KERNEL) {
                cpu->PSW.modo = MODO_USUARIO;
            }
            log_operacion("CHMOD", cpu->PSW.modo, 0, 0);
            break;
            
        case 19: // loadrb
            cpu->AC = cpu->RB;
            log_operacion("LOADRB", cpu->AC, cpu->RB, cpu->AC);
            break;
            
        case 20: // strrb
            cpu->RB = cpu->AC;
            log_operacion("STRRB", cpu->AC, cpu->RB, cpu->RB);
            break;
            
        case 21: // loadrl
            cpu->AC = cpu->RL;
            log_operacion("LOADRL", cpu->AC, cpu->RL, cpu->AC);
            break;
            
        case 22: // strrl
            cpu->RL = cpu->AC;
            log_operacion("STRRL", cpu->AC, cpu->RL, cpu->RL);
            break;
            
        case 23: // loadsp
            cpu->AC = cpu->SP;
            log_operacion("LOADSP", cpu->AC, cpu->SP, cpu->AC);
            break;
            
        case 24: // strsp
            cpu->SP = cpu->AC;
            log_operacion("STRSP", cpu->AC, cpu->SP, cpu->SP);
            break;
            
        case 25: // psh
            cpu->SP++;
            memoria[cpu->SP] = cpu->AC;
            log_operacion("PSH", cpu->AC, cpu->SP, memoria[cpu->SP]);
            break;
            
        case 26: // pop
            cpu->AC = memoria[cpu->SP];
            cpu->SP--;
            log_operacion("POP", cpu->AC, cpu->SP, cpu->AC);
            break;
            
        case 27: // j - salto incondicional
            cpu->PSW.pc = inst.valor;
            log_operacion("J", cpu->PSW.pc, inst.valor, cpu->PSW.pc);
            break;
            
        case 28: // sdmap - establecer pista
        case 29: // sdmac - establecer cilindro
        case 30: // sdmas - establecer sector
        case 31: // sdmaio - establecer operacion
        case 32: // sdmam - establecer direccion memoria
        case 33: // sdmaon - iniciar DMA
            // Se manejan en el modulo DMA
            break;
            
        default:
            lanzar_interrupcion(INT_INST_INVALID);
            log_error("Instruccion invalida", inst.codigo_op);
            break;
    }
}

palabra_t cpu_psw_a_palabra(PSW_t psw) {
    return psw.codigo_condicion * 10000000 +
           psw.modo * 1000000 +
           psw.interrupciones * 100000 +
           psw.pc;
}

PSW_t cpu_palabra_a_psw(palabra_t palabra) {
    PSW_t psw;
    psw.codigo_condicion = palabra / 10000000;
    psw.modo = (palabra / 1000000) % 10;
    psw.interrupciones = (palabra / 100000) % 10;
    psw.pc = palabra % 100000;
    return psw;
}

void cpu_salvar_contexto(CPU_t *cpu, palabra_t *memoria) {
    // Guardar registros importantes en la pila
    memoria[++cpu->SP] = cpu->AC;
    memoria[++cpu->SP] = cpu->RX;
    memoria[++cpu->SP] = cpu_psw_a_palabra(cpu->PSW);
}

void cpu_restaurar_contexto(CPU_t *cpu, palabra_t *memoria) {
    // Restaurar registros desde la pila
    cpu->PSW = cpu_palabra_a_psw(memoria[cpu->SP--]);
    cpu->RX = memoria[cpu->SP--];
    cpu->AC = memoria[cpu->SP--];
}

void cpu_ciclo_instruccion(CPU_t *cpu, palabra_t *memoria) {
    // Fase de busqueda
    cpu_fetch(cpu, memoria);
    
    // Fase de decodificacion
    Instruccion_t inst = cpu_decode(cpu->IR);
    
    // Fase de ejecucion
    cpu_execute(cpu, inst, memoria);
}