#include "sistema.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_modo_debug = 0;

void sistema_init(Sistema_t *sys) {
    // Inicializar mutex
    pthread_mutex_init(&sys->mutex_bus, NULL);
    pthread_mutex_init(&sys->mutex_memoria, NULL);
    
    // Inicializar componentes
    cpu_init(&sys->cpu);
    memoria_init(&sys->memoria);
    dma_init(&sys->dma, sys->memoria.datos, &sys->mutex_bus);
    interrupciones_init(&sys->vector_int);
    
    sys->ejecutando = 0;
    sys->ciclos_reloj = 0;
    sys->periodo_reloj = 0;
    
    log_mensaje("Sistema completo inicializado");
}

void sistema_ejecutar_programa(Sistema_t *sys, const char *archivo, int modo_debug) {
    g_modo_debug = modo_debug;
    
    // Cargar programa en memoria
    int dir_inicio = memoria_cargar_programa(&sys->memoria, archivo, MEM_SO);
    if (dir_inicio < 0) {
        printf("Error al cargar programa\n");
        return;
    }
    
    // Configurar CPU para ejecutar desde esa direccion
    sys->cpu.PSW.pc = dir_inicio;
    sys->cpu.RB = dir_inicio;
    sys->cpu.RL = TAM_MEMORIA - 1;
    sys->ejecutando = 1;
    
    char msg[200];
    sprintf(msg, "Iniciando ejecucion desde direccion %d en modo %s", 
            dir_inicio, modo_debug ? "DEBUG" : "NORMAL");
    log_mensaje(msg);
    printf("\n%s\n\n", msg);
    
    // Ejecutar
    if (modo_debug) {
        sistema_debugger(sys);
    } else {
        while (sys->ejecutando) {
            sistema_ciclo(sys);
        }
        printf("\nPrograma finalizado\n\n");
    }
}

void sistema_ciclo(Sistema_t *sys) {
    // Verificar si hay interrupciones pendientes
    if (g_interrupcion_pendiente) {
        procesar_interrupcion(&sys->cpu, sys->memoria.datos, &sys->vector_int);
    }
    
    // Arbitraje del bus para CPU
    pthread_mutex_lock(&sys->mutex_bus);
    
    // Ejecutar ciclo de instruccion
    cpu_ciclo_instruccion(&sys->cpu, sys->memoria.datos);
    
    pthread_mutex_unlock(&sys->mutex_bus);
    
    // Incrementar contador de ciclos
    sys->ciclos_reloj++;
    
    // Verificar interrupcion de reloj
    if (sys->periodo_reloj > 0 && sys->ciclos_reloj >= sys->periodo_reloj) {
        lanzar_interrupcion(INT_RELOJ);
        sys->ciclos_reloj = 0;
    }
    
    // Condicion de parada: instruccion invalida o fuera de limites
    if (sys->cpu.PSW.pc >= TAM_MEMORIA || sys->cpu.PSW.pc < 0) {
        sys->ejecutando = 0;
    }
}

void sistema_debugger(Sistema_t *sys) {
    char comando[100];
    
    while (sys->ejecutando) {
        printf("\n--- DEBUGGER ---\n");
        printf("PC: %05d | AC: %08d | SP: %05d\n", 
               sys->cpu.PSW.pc, sys->cpu.AC, sys->cpu.SP);
        printf("Modo: %s | CC: %d | INT: %s\n",
               sys->cpu.PSW.modo == MODO_KERNEL ? "KERNEL" : "USUARIO",
               sys->cpu.PSW.codigo_condicion,
               sys->cpu.PSW.interrupciones == INT_HABILITADAS ? "ON" : "OFF");
        
        // Mostrar siguiente instruccion
        if (sys->cpu.PSW.pc < TAM_MEMORIA) {
            printf("Siguiente instruccion [%05d]: %08d\n", 
                   sys->cpu.PSW.pc, sys->memoria.datos[sys->cpu.PSW.pc]);
        }
        
        printf("\nComandos: (s)iguiente, (r)egistro, (m)emoria, (c)ontinuar, (q)uit\n");
        printf("> ");
        
        if (!fgets(comando, sizeof(comando), stdin)) {
            break;
        }
        
        comando[strcspn(comando, "\n")] = 0;
        
        if (strcmp(comando, "s") == 0) {
            // Ejecutar una instruccion
            sistema_ciclo(sys);
            
            printf("\nInstruccion ejecutada:\n");
            printf("Resultado - AC: %08d | PC: %05d | SP: %05d\n",
                   sys->cpu.AC, sys->cpu.PSW.pc, sys->cpu.SP);
                   
        } else if (strcmp(comando, "r") == 0) {
            // Mostrar todos los registros
            printf("\n=== REGISTROS ===\n");
            printf("AC  : %08d\n", sys->cpu.AC);
            printf("MAR : %08d\n", sys->cpu.MAR);
            printf("MDR : %08d\n", sys->cpu.MDR);
            printf("IR  : %08d\n", sys->cpu.IR);
            printf("RB  : %08d\n", sys->cpu.RB);
            printf("RL  : %08d\n", sys->cpu.RL);
            printf("RX  : %08d\n", sys->cpu.RX);
            printf("SP  : %08d\n", sys->cpu.SP);
            printf("PC  : %05d\n", sys->cpu.PSW.pc);
            
        } else if (strcmp(comando, "m") == 0) {
            // Ver memoria
            int dir;
            printf("Direccion de memoria: ");
            scanf("%d", &dir);
            getchar(); // Limpiar buffer
            
            if (dir >= 0 && dir < TAM_MEMORIA) {
                printf("Memoria[%d] = %08d\n", dir, sys->memoria.datos[dir]);
            } else {
                printf("Direccion invalida\n");
            }
            
        } else if (strcmp(comando, "c") == 0) {
            // Continuar hasta el final
            g_modo_debug = 0;
            while (sys->ejecutando) {
                sistema_ciclo(sys);
            }
            printf("\nPrograma finalizado\n");
            break;
            
        } else if (strcmp(comando, "q") == 0) {
            sys->ejecutando = 0;
            break;
        }
        
        // Verificar si termino el programa
        if (sys->cpu.PSW.pc >= TAM_MEMORIA || sys->cpu.PSW.pc < 0) {
            printf("\nPrograma finalizado (PC fuera de rango)\n");
            sys->ejecutando = 0;
        }
    }
}

void sistema_consola(Sistema_t *sys) {
    char comando[256];
    char archivo[200];
    char modo[20];
    
    printf("\n");
    printf("=============================================\n");
    printf("  ARQUITECTURA VIRTUAL - Sistema Operativo\n");
    printf("=============================================\n");
    printf("\n");
    
    while (1) {
        printf("sistema> ");
        
        if (!fgets(comando, sizeof(comando), stdin)) {
            break;
        }
        
        comando[strcspn(comando, "\n")] = 0;
        
        if (strlen(comando) == 0) {
            continue;
        }
        
        if (strcmp(comando, "salir") == 0 || strcmp(comando, "exit") == 0) {
            printf("Saliendo del sistema...\n");
            break;
        }
        
        if (strcmp(comando, "ayuda") == 0 || strcmp(comando, "help") == 0) {
            printf("\nComandos disponibles:\n");
            printf("  ejecutar <archivo> <modo>  - Ejecuta un programa\n");
            printf("                               modo: normal | debug\n");
            printf("  ayuda                       - Muestra esta ayuda\n");
            printf("  salir                       - Sale del sistema\n\n");
            continue;
        }
        
        if (sscanf(comando, "ejecutar %s %s", archivo, modo) == 2) {
            int modo_debug = (strcmp(modo, "debug") == 0) ? 1 : 0;
            sistema_ejecutar_programa(sys, archivo, modo_debug);
            
        } else if (sscanf(comando, "ejecutar %s", archivo) == 1) {
            // Por defecto modo normal
            sistema_ejecutar_programa(sys, archivo, 0);
            
        } else {
            printf("Comando no reconocido. Escribe 'ayuda' para ver comandos.\n");
        }
    }
}

void sistema_cleanup(Sistema_t *sys) {
    dma_cleanup(&sys->dma);
    pthread_mutex_destroy(&sys->mutex_bus);
    pthread_mutex_destroy(&sys->mutex_memoria);
    log_mensaje("Sistema finalizado correctamente");
}