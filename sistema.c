#include "sistema.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int g_modo_debug = 0;

void sistema_inicializar(Sistema_t *sys) {
    // Inicializar mutex
    pthread_mutex_init(&sys->mutex_bus, NULL);    //Controla quien puede usar el bus de datos. (Mutex)
    pthread_mutex_init(&sys->mutex_memoria, NULL); //Protege el acceso al arreglo de datos de la RAM.
    
    // Inicializar componentes
    cpu_inicializar(&sys->cpu);    //Llama a cpu_inicializar para poner los registros de la CPU en cero
    memoria_inicializar(&sys->memoria);  //Inicializa la memoria
    dma_inicializar(&sys->dma, sys->memoria.datos, &sys->mutex_bus);
    interrupciones_inicializar(&sys->vector_int);
    
    sys->ejecutando = 0;     //Indica si la maquina esta corriendo 
    sys->ciclos_reloj = 0;
    sys->periodo_reloj = 0;
    
    log_mensaje("Sistema completo inicializado");
}

//Recibe el sistema, el nombre del archivo a ejecutar y una bandera que indica si se activara el modo debugger.
void sistema_ejecutar_programa(Sistema_t *sys, const char *archivo, int modo_debug) {
    g_modo_debug = modo_debug;
    // Resetear estado de interrupciones antes de empezar
    interrupciones_inicializar(&sys->vector_int);
    int cant_palabras = 0; // Variable para recibir el tamaño
    // Cargar programa en memoria
    int dir_inicio = memoria_cargar_programa(&sys->memoria, archivo, MEM_SO, &cant_palabras);
    if (dir_inicio < 0) {
        printf("Error al cargar programa\n");
        return;
    }
    
    // Validar que exista espacio suficiente para el codigo y la pila
    if (dir_inicio + cant_palabras + TAM_PILA > TAM_MEMORIA) {
        printf("ERROR CRITICO: Memoria insuficiente para cargar el programa y reservar la pila.\n");
        log_error("Memoria insuficiente para programa + pila", 0);
        return;
    }

    // Configurar CPU para ejecutar desde esa direccion
    sys->cpu.PSW.pc = dir_inicio;     //Apunta a la PC a la primera instruccion del programa.
    sys->cpu.RB = dir_inicio;        //Establece el Registro Base (RB) al inicio de donde se cargo el programa.
    
    // Configurar RX como la base de la pila (inicia justo donde termina el codigo del programa)
    sys->cpu.RX = sys->cpu.RB + cant_palabras;

    //Establece el Registro Limite (RL), 
    // Para que el programa tenga acceso hasta el final de su espacio de memoria incluyendo la pila
    // Por ejemplo si cant_palabras es 7, el tamanio de la pila es 2 y RB esta en 300 
    // Entonces RL es RB  + cant_palabras + TAM_PILA - 1 por lo que RL = 308.
    sys->cpu.RL = sys->cpu.RX + TAM_PILA - 1;

    // Inicializar SP (Puntero de pila relativo a RX)
    sys->cpu.SP = 0;
     
    sys->ejecutando = 1;
    
    //Registra un mensaje 
    char msg[200];
    sprintf(msg, "Iniciando ejecucion desde direccion %d en modo %s", 
            dir_inicio, modo_debug ? "DEBUG" : "NORMAL");
    log_mensaje(msg);
    printf("\n%s\n\n", msg);
    
    
    if (modo_debug) {
        sistema_debugger(sys);   // Ejecuta el modo debugger
    } else {
        while (sys->ejecutando) {  //Modo normal
            sistema_ciclo(sys);
        }
        printf("\nPrograma finalizado\n\n");
    }
}

//Esta funcion encapsula lo que pasa en un ciclo de reloj.

void sistema_ciclo(Sistema_t *sys) {
    // Verificar si hay interrupciones pendientes
    if (interrupcion_pendiente) {
        // Si ocurre una interrupcion y no hay un manejador cargado en el vector
        if (sys->vector_int.manejadores[codigo_interrupcion] == 0) {
            
            // Caso SVC (Codigo 2): El programa hace una llamada al sistema operativo.
            if (codigo_interrupcion == INT_SYSCALL) {
                log_mensaje("Llamada al sistema no reconocida");
            }
            
            // Caso Direccionamiento Invalido (Codigo 6): El PC se salio de RL.
            if (codigo_interrupcion == INT_DIR_INVALIDA) {
                log_mensaje("ERROR: Violacion de limites de memoria (PC > RL)");
                printf("\nERROR: Direccionamiento invalido. \n");
                sys->ejecutando = 0;
                return;
            }
        }
        
        // Si hay manejador o no es critica, se procesa normalmente
        procesar_interrupcion(&sys->cpu, sys->memoria.datos, &sys->vector_int);
    }

    if (!sys->ejecutando) return;
    
    // Arbitraje del bus para CPU
    pthread_mutex_lock(&sys->mutex_bus);  // La CPU pide permiso exclusivo para usar el bus
    
    // Ejecutar ciclo de instruccion
    cpu_ciclo_instruccion(&sys->cpu, sys->memoria.datos, &sys->dma);// Una vez tiene el bus, realiza la busqueda y ejecucion
    
    pthread_mutex_unlock(&sys->mutex_bus); // Libera el bus para que el DMA pueda usarlo.
    
    // Incrementar contador de ciclos
    sys->ciclos_reloj++;
    
    // Verificar interrupcion de reloj
    if (sys->periodo_reloj > 0 && sys->ciclos_reloj >= sys->periodo_reloj) {
        lanzar_interrupcion(INT_RELOJ);
        sys->ciclos_reloj = 0;
    }
    
    // Verifica si el registro PC apunta a una direccion que no existe en la memoria del sistema
    // Para evitar un Segmentation Fault
    if (sys->cpu.PSW.pc >= TAM_MEMORIA || sys->cpu.PSW.pc < 0) {
        sys->ejecutando = 0;
    }
}

void sistema_debugger(Sistema_t *sys) {
    char comando[100];
    
    while (sys->ejecutando) {        //Se mantendra en la consola mientras este en el modo debugger 
        printf("\n--- DEBUGGER ---\n");
        printf("PC: %05d | AC: %08d | SP: %05d\n", 
               sys->cpu.PSW.pc, sys->cpu.AC, sys->cpu.SP);
        printf("MODO: %s | COD_CON: %d | INT_HAB: %s\n",
               sys->cpu.PSW.modo == MODO_KERNEL ? "KERNEL" : "USUARIO",     //Si esta en Usuario (0) o Kernel (1)
               sys->cpu.PSW.codigo_condicion,
               sys->cpu.PSW.interrupciones == INT_HABILITADAS ? "ON" : "OFF");
        
        // Verifica que el PC apunte a una direccion valida y luego imprime el contenido de la memoria en esa direccion
        if (sys->cpu.PSW.pc < TAM_MEMORIA) {  
            printf("Siguiente instruccion [%05d]: %08d\n", 
                   sys->cpu.PSW.pc, sys->memoria.datos[sys->cpu.PSW.pc]);
        }
        
        //Muestra el menu de opciones
        printf("\nComandos: (s)iguiente, (r)egistro, (m)emoria, (c)ontinuar, (q)uit\n");
        printf("> ");
        
        //Espera a que el usuario escriba algo y presione Enter. Si hay error o fin de archivo, rompe el bucle
        if (!fgets(comando, sizeof(comando), stdin)) {
            break;
        }
         //Elimina el salto de linea (\n) que fgets captura al presionar Enter.
        comando[strcspn(comando, "\n")] = 0;
        
        if (strcmp(comando, "s") == 0) {
            // Ejecutar una instruccion
            sistema_ciclo(sys);
            
            // vuelve a imprimir los registros despues de haber ejecutado la instruccion.
            printf("\nInstruccion ejecutada:\n");
            printf("res - AC: %08d | PC: %05d | SP: %05d\n",
                   sys->cpu.AC, sys->cpu.PSW.pc, sys->cpu.SP);
                   
        } else if (strcmp(comando, "r") == 0) {   //Imprime la lista completa de registros internos de la CPU.
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
            printf("COD_CON  : %05d\n", sys->cpu.PSW.codigo_condicion);
            printf("MODO  : %05d\n", sys->cpu.PSW.modo);
            printf("INT_HAB  : %05d\n", sys->cpu.PSW.interrupciones);
            printf("PC  : %05d\n", sys->cpu.PSW.pc);
            
        } else if (strcmp(comando, "m") == 0) {
            // Ver memoria
            int dir;
            printf("Direccion de memoria: ");   //Pide un numero entero al usuario (la direccion de RAM que quiere ver).
            scanf("%d", &dir);
            getchar(); // Limpiar buffer
            

            //Verifica que la direccion exista (sea menor a 2000) e imprime el valor guardado ahi
            if (dir >= 0 && dir < TAM_MEMORIA) {
                printf("Memoria[%d] = %08d\n", dir, sys->memoria.datos[dir]);
            } else {
                printf("Direccion invalida\n");
            }
            
        } else if (strcmp(comando, "c") == 0) {
            // Continuar hasta el final
            g_modo_debug = 0;
            //Ejecuta un bucle rapido que consume todas las instrucciones que quedan hasta que el programa termine.
            while (sys->ejecutando) {
                sistema_ciclo(sys);
            }
            printf("\nPrograma finalizado\n");
            break;
        
        //Termina la ejecucion
        } else if (strcmp(comando, "q") == 0) {
            sys->ejecutando = 0;
            break;
        }
        
        // revisa si el PC se salio de la memoria, eso quiere decir que debe terminar
        if (sys->cpu.PSW.pc >= TAM_MEMORIA || sys->cpu.PSW.pc < 0) {
            printf("\nPrograma finalizado (PC fuera de rango)\n");
            sys->ejecutando = 0;
        }
    }
}

void sistema_consola(Sistema_t *sys) {
    char comando[256];  //Almacenara la linea completa que el usuario escriba
    char archivo[256];  //Se usara para guardar el nombre del programa
    char modo[20];      //Se usara para guardar el modo si se especifica
        
    while (1) {
        printf("sistema> ");
        
        if (!fgets(comando, sizeof(comando), stdin)) {  //Lee lo que el usuario escribe en el teclado y lo guarda en la variable comando
            break;
        }
        
        comando[strcspn(comando, "\n")] = 0;            //ELimina el salto de linea 
        
        if (strlen(comando) == 0) {                    //Vuelve al ciclo si el usuario no presiono ninguna opcion
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
            printf("  ayuda                       - Solicitar ayuda\n");
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
            printf("Comando no reconocido. Escribe 'ayuda' para ver comandos disponibles.\n");
        }
    }
}

void sistema_limpiar(Sistema_t *sys) {
    dma_terminar(&sys->dma);
    pthread_mutex_destroy(&sys->mutex_bus);
    pthread_mutex_destroy(&sys->mutex_memoria);
    log_mensaje("Sistema finalizado correctamente");
}