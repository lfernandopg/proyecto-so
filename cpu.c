#include "cpu.h"
#include "interrupciones.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

extern int g_modo_debug;

//Inicializa la CPU a 0 
void cpu_init(CPU_t *cpu) {
    cpu->AC = 0;
    cpu->MAR = 0;
    cpu->MDR = 0;
    cpu->IR = 0;
    cpu->RB = MEM_SO;  //Lo primero que se ejecuta al iniciar el sistema es el SO, el registro base se igual a el, ya que contiene la pos de mem del proceso en ejecucion
    cpu->RX = 0;
    cpu->SP = 0;
    cpu->PSW.codigo_condicion = 0;     //El codigo de condicion del PSW se inicializa a 0
    cpu->PSW.modo = MODO_USUARIO;       //La CPU siempre debe incializarse en modo kernel
    cpu->PSW.interrupciones = INT_HABILITADAS;  //La CPU reacciona a señales externas
    cpu->PSW.pc = MEM_SO;              //Comienza a leer donde se carga el SO
    
    log_mensaje("CPU inicializada");
}

//------------------------------------------------------CICLOS DE INSTRUCCIoN DE LA CPU----------------------------------------------------------------------------------


void cpu_fetch(CPU_t *cpu, palabra_t *memoria) {  //Indica lo primero que debe hacer la CPU
    // Verificar proteccion de memoria (si estamos en modo usuario)
    if (cpu->PSW.modo == MODO_USUARIO) {
        // Asumiendo que tu PC es absoluto (basado en tu codigo actual)
        if (cpu->PSW.pc > cpu->RL || cpu->PSW.pc < cpu->RB) {
            lanzar_interrupcion(INT_DIR_INVALID); // Codigo 6
            return; // No incrementar PC ni leer instrucciones basura
        }
    }

    // MAR obtiene PC
    cpu->MAR = cpu->PSW.pc;
    
    // MDR obtiene contenido de memoria[MAR]
    cpu->MDR = memoria[cpu->MAR];
    
    // IR obtiene MDR
    cpu->IR = cpu->MDR;
    
    // Incrementar PC
    cpu->PSW.pc++;
    
    //Imprime el estado actual de los registros 
    if (g_modo_debug) {
        printf("FETCH: MAR=%d, MDR=%08d, IR=%08d, PC=%d\n", 
               cpu->MAR, cpu->MDR, cpu->IR, cpu->PSW.pc);
    }
}

Instruccion_t cpu_decode(palabra_t instruccion_raw) {
    Instruccion_t inst;
    
    inst.codigo_op = instruccion_raw / 1000000;        //Al dividir entre 1000000 se obtiene el codigo de la instruccion
    inst.direccionamiento = (instruccion_raw / 100000) % 10;     //Obtiene el tipo de direccionamiento (0 o 1)
    inst.valor = instruccion_raw % 100000;             //Obtiene el dato con el que se trabaja 
    
    return inst;
}

int cpu_calcular_direccion(CPU_t *cpu, Instruccion_t inst) {     //Calcula la direccion de destino
    int direccion = 0;
    
    //Se decide como calcular basandose en el campo direccionamiento que extrajimos en la fase de decode:
    switch(inst.direccionamiento) {
        case DIR_DIRECTO:
            direccion = inst.valor;       //Toma la direccion de la instruccion
            break;
        case DIR_INMEDIATO:
            direccion = -1; // No aplica ya que la direccion es el dato directamente
            break;
        case DIR_INDEXADO:
            direccion = cpu->AC + inst.valor;   //Suma el contenido del registro AC + el valor de la instruccion.
            break;
    }
    
    return direccion;
}

palabra_t cpu_obtener_operando(CPU_t *cpu, Instruccion_t inst, palabra_t *memoria) {
    palabra_t operando = 0;
    
    switch(inst.direccionamiento) {  //Dependiento del tipo de direccionamiento actuara

        case DIR_DIRECTO:
            if (cpu->PSW.modo == MODO_USUARIO) {
                int dir_fisica = cpu->RB + inst.valor;
                if (!cpu_verificar_memoria(cpu, dir_fisica)) {  // Si el programa intenta acceder a una direccion fuera de limite
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

int cpu_verificar_memoria(CPU_t *cpu, int direccion) {   //Recibe el estado de la CPU y la direccion fisica que ya calculamos
    return (direccion >= cpu->RB && direccion <= cpu->RL);  //verifica que la direccion este entre RB y RL
}

void cpu_actualizar_cc(CPU_t *cpu, palabra_t resultado) {
    // Detectar overflow (mas de 7 digitos de magnitud)
    if (abs(resultado) > 9999999) {
        cpu->PSW.codigo_condicion = CC_OVERFLOW;   //Detecta un desbordamiento 
        lanzar_interrupcion(INT_OVERFLOW);
    } else if (resultado == 0) {              
        cpu->PSW.codigo_condicion = CC_IGUAL;     //El resultado de la operacion fue 0, el cod de condicion es 0
    } else if (resultado < 0) {
        cpu->PSW.codigo_condicion = CC_MENOR;     //El resultado de la operacion es negativo, el cod de condicion 1
    } else {
        cpu->PSW.codigo_condicion = CC_MAYOR;     //El resultado de la operacion es positivo, el cod de condicion 2
    }
}

void cpu_execute(CPU_t *cpu, Instruccion_t inst, palabra_t *memoria, ControladorDMA_t *dma) {
    palabra_t operando;
    palabra_t resultado;
    int direccion;
    
    if (g_modo_debug) {
        printf("EXECUTE: OP=%02d, DIR=%d, VAL=%05d\n", 
               inst.codigo_op, inst.direccionamiento, inst.valor);
    }
    
    switch(inst.codigo_op) {
        case 0: // sum
            operando = cpu_obtener_operando(cpu, inst, memoria); //Trae el dato (segun el direccionamiento)
            resultado = cpu->AC + operando;                      // Hace la suma 
            cpu_actualizar_cc(cpu, resultado);                   // Actualiza el codigo de condicion
            cpu->AC = resultado;                                 // Guarda el resultado en AC
            log_operacion("SUM", cpu->AC, operando, resultado);  // Registra la actividad en el log
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
            operando = cpu_obtener_operando(cpu, inst, memoria);  // Copia un dato de la RAM al registro AC.
            cpu->AC = operando;
            log_operacion("LOAD", cpu->AC, operando, cpu->AC);
            break;
            
        case 5: // str copia el valor de AC a la RAM.
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
            dma_set_pista(dma, inst.valor);
            log_operacion("SDMAP", 0, inst.valor, 0);
            break;
            
        case 29: // sdmac - establecer cilindro
            dma_set_cilindro(dma, inst.valor);
            log_operacion("SDMAC", 0, inst.valor, 0);
            break;
            
        case 30: // sdmas - establecer sector
            dma_set_sector(dma, inst.valor);
            log_operacion("SDMAS", 0, inst.valor, 0);
            break;
            
        case 31: // sdmaio - establecer operacion (0=Leer, 1=Escribir)
            dma_set_operacion(dma, inst.valor);
            log_operacion("SDMAIO", 0, inst.valor, 0);
            break;
            
        case 32: // sdmam - establecer direccion memoria
            dma_set_direccion(dma, inst.valor);
            log_operacion("SDMAM", 0, inst.valor, 0);
            break;
            
        case 33: // sdmaon - iniciar DMA
            dma_iniciar(dma);
            log_operacion("SDMAON", 0, 0, 0);
            break;
            
        default:
            lanzar_interrupcion(INT_INST_INVALID);
            log_error("Instruccion invalida", inst.codigo_op);
            break;
    }
}

 //Guarda los datos del PSW en la RAM
palabra_t cpu_psw_a_palabra(PSW_t psw) {
    return psw.codigo_condicion * 10000000 +     // Pone el CC en el 8vo digito
           psw.modo * 1000000 +                  // Pone el Modo en el 7mo digito
           psw.interrupciones * 100000 +         // Pone las Int en el 6to digito
           psw.pc;                               // Los ultimos 5 digitos son para el PC
}

PSW_t cpu_palabra_a_psw(palabra_t palabra) {
    PSW_t psw;
    psw.codigo_condicion = palabra / 10000000;    // Extrae el digito de la izquierda
    psw.modo = (palabra / 1000000) % 10;          // Aisla el digito del modo
    psw.interrupciones = (palabra / 100000) % 10;  // Aisla el digito de interrupciones
    psw.pc = palabra % 100000;                     // Se queda con los ultimos 5 digitos
    return psw;
}

 //Se usa cuando ocurre una interrupcion, guardamos todo para que el SO pueda retomar
void cpu_salvar_contexto(CPU_t *cpu, palabra_t *memoria) {
    // Guardar registros importantes en la pila
    memoria[++cpu->SP] = cpu->AC;       // Sube el puntero de pila y guarda el AC
    memoria[++cpu->SP] = cpu->RX;       // Sube el puntero y guarda el Registro X
    memoria[++cpu->SP] = cpu_psw_a_palabra(cpu->PSW);  // Guarda el estado completo (PSW) empaquetado
}

//saca los valores de la pila para que la CPU siga exactamente donde se quedo
void cpu_restaurar_contexto(CPU_t *cpu, palabra_t *memoria) {
    // Restaurar registros desde la pila
    cpu->PSW = cpu_palabra_a_psw(memoria[cpu->SP--]);  // Recupera y desglosa el PSW, luego baja la pila
    cpu->RX = memoria[cpu->SP--];                      // Recupera el RX y baja la pila
    cpu->AC = memoria[cpu->SP--];                      // Recupera el AC y baja la pila
}

void cpu_ciclo_instruccion(CPU_t *cpu, palabra_t *memoria, ControladorDMA_t *dma) {
    // Fase de busqueda
    cpu_fetch(cpu, memoria);

    // Si el fetch lanzó una interrupción (ej. fuera de límites), abortamos el ciclo
    if (g_interrupcion_pendiente) {
        return; 
    }
    
    // Fase de decodificacion
    Instruccion_t inst = cpu_decode(cpu->IR);
    
    // Fase de ejecucion
    cpu_execute(cpu, inst, memoria, dma);
}
