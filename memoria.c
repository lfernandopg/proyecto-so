#include "memoria.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>

void memoria_inicializar(Memoria_t *mem) {
    int i;

    // Pone toda la memoria en 0
    for (i = 0; i < TAM_MEMORIA; i++) {
        mem->datos[i] = 0;
        mem->ocupado[i] = 0;
    }
    
    // Marca la zona como area reservada para el Sistema Operativo
    for (i = 0; i < MEM_SO; i++) {
        mem->ocupado[i] = 1;
    }
    
    log_mensaje("Memoria inicializada");
}

palabra_t memoria_leer(Memoria_t *mem, int direccion) {
    if (direccion < 0 || direccion >= TAM_MEMORIA) {
        log_error("Direccion de memoria invalida en lectura", direccion);
        return 0;
    }
    return mem->datos[direccion];
}

void memoria_escribir(Memoria_t *mem, int direccion, palabra_t dato) {
    if (direccion < 0 || direccion >= TAM_MEMORIA) {
        log_error("Direccion de memoria invalida en escritura", direccion);
        return;
    }
    mem->datos[direccion] = dato;
}

int memoria_cargar_programa(Memoria_t *mem, const char *archivo, int dir_inicio, int *cant_palabras) {
    FILE *fp;
    char linea[100];
    int num_palabras = 0;
    char nombre_prog[256];
    int posicion = dir_inicio;
    int en_codigo = 0;
    
    fp = fopen(archivo, "r");
    if (!fp) {
        log_error("No se pudo abrir archivo", 0);
        return -1;
    }
    
    while (fgets(linea, sizeof(linea), fp)) {
        // Eliminar salto de linea
        linea[strcspn(linea, "\n")] = 0;
        
        // Ignorar lineas vacias y comentarios
        if (strlen(linea) == 0 || linea[0] == '\n') {
            continue;
        }
        
        if (strncmp(linea, "_start", 6) == 0) {
            sscanf(linea, "_start %d", &dir_inicio);
            posicion = dir_inicio;
            continue;
        }
        
        if (strncmp(linea, ".NumeroPalabras", 15) == 0) {
            sscanf(linea, ".NumeroPalabras %d", &num_palabras);
            continue;
        }
        
        if (strncmp(linea, ".NombreProg", 11) == 0) {
            sscanf(linea, ".NombreProg %s", nombre_prog);
            en_codigo = 1;
            continue;
        }
        
        // Cargar instrucciones
        if (en_codigo && linea[0] >= '0' && linea[0] <= '9') {
            palabra_t instruccion;
            sscanf(linea, "%d", &instruccion);
            
            if (posicion >= TAM_MEMORIA) {
                log_error("Memoria insuficiente para cargar programa", posicion);
                fclose(fp);
                return -1;
            }
            
            // Guarda la palabra en la memoria
            mem->datos[posicion] = instruccion;
            
            // Marca la posicion de memoria como ocupada
            mem->ocupado[posicion] = 1;
            
            char msg[1000];
            sprintf(msg, "Cargado en memoria[%d]: %08d", posicion, instruccion);
            log_mensaje(msg);
            
            posicion++;
        }
    }
    
    fclose(fp);

    if (cant_palabras != NULL) {
        *cant_palabras = num_palabras;
    }
    
    char msg[1000];
    sprintf(msg, "Programa '%s' cargado: %d palabras desde posicion %d", 
            nombre_prog, num_palabras, dir_inicio);
    log_mensaje(msg);
    
    return dir_inicio;
}