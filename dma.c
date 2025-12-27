#include "dma.h"
#include "interrupciones.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void dma_init(ControladorDMA_t *ctrl, palabra_t *memoria, pthread_mutex_t *mutex_bus) {
    ctrl->dma.pista = 0;
    ctrl->dma.cilindro = 0;
    ctrl->dma.sector = 0;
    ctrl->dma.operacion = DMA_LEER;
    ctrl->dma.dir_memoria = 0;
    ctrl->dma.estado = DMA_EXITO;
    ctrl->dma.activo = 0;
    ctrl->memoria = memoria;
    ctrl->mutex_bus = mutex_bus;
    ctrl->ejecutando = 0;
    
    // Inicializar disco con datos vacios
    memset(&ctrl->disco, 0, sizeof(Disco_t));
    
    log_mensaje("DMA y Disco inicializados");
}

void dma_set_pista(ControladorDMA_t *ctrl, int pista) {
    ctrl->dma.pista = pista;
    char msg[100];
    sprintf(msg, "DMA: Pista establecida = %d", pista);
    log_mensaje(msg);
}

void dma_set_cilindro(ControladorDMA_t *ctrl, int cilindro) {
    ctrl->dma.cilindro = cilindro;
    char msg[100];
    sprintf(msg, "DMA: Cilindro establecido = %d", cilindro);
    log_mensaje(msg);
}

void dma_set_sector(ControladorDMA_t *ctrl, int sector) {
    ctrl->dma.sector = sector;
    char msg[100];
    sprintf(msg, "DMA: Sector establecido = %d", sector);
    log_mensaje(msg);
}

void dma_set_operacion(ControladorDMA_t *ctrl, int operacion) {
    ctrl->dma.operacion = operacion;
    char msg[100];
    sprintf(msg, "DMA: Operacion establecida = %s", 
            operacion == DMA_LEER ? "LEER" : "ESCRIBIR");
    log_mensaje(msg);
}

void dma_set_direccion(ControladorDMA_t *ctrl, int direccion) {
    ctrl->dma.dir_memoria = direccion;
    char msg[100];
    sprintf(msg, "DMA: Direccion memoria = %d", direccion);
    log_mensaje(msg);
}

void* dma_thread_func(void *arg) {
    ControladorDMA_t *ctrl = (ControladorDMA_t*)arg;
    
    log_mensaje("DMA: Iniciando operacion de E/S");
    
    // Validar parametros
    if (ctrl->dma.pista >= DISCO_PISTAS || 
        ctrl->dma.cilindro >= DISCO_CILINDROS ||
        ctrl->dma.sector >= DISCO_SECTORES) {
        ctrl->dma.estado = DMA_ERROR;
        log_error("DMA: Parametros de disco invalidos", 0);
        lanzar_interrupcion(INT_IO_FINISH);
        ctrl->dma.activo = 0;
        return NULL;
    }
    
    // Simular tiempo de acceso a disco
    usleep(100000); // 100ms
    
    // Arbitraje del bus
    pthread_mutex_lock(ctrl->mutex_bus);
    
    if (ctrl->dma.operacion == DMA_LEER) {
        // Leer del disco a memoria
        char *sector_data = ctrl->disco.datos[ctrl->dma.pista]
                                             [ctrl->dma.cilindro]
                                             [ctrl->dma.sector];
        
        palabra_t dato;
        sscanf(sector_data, "%d", &dato);
        ctrl->memoria[ctrl->dma.dir_memoria] = dato;
        
        log_mensaje("DMA: Lectura de disco completada");
    } else {
        // Escribir de memoria a disco
        palabra_t dato = ctrl->memoria[ctrl->dma.dir_memoria];
        sprintf(ctrl->disco.datos[ctrl->dma.pista]
                                 [ctrl->dma.cilindro]
                                 [ctrl->dma.sector], 
                "%08d", dato);
        
        log_mensaje("DMA: Escritura a disco completada");
    }
    
    pthread_mutex_unlock(ctrl->mutex_bus);
    
    // Operacion exitosa
    ctrl->dma.estado = DMA_EXITO;
    ctrl->dma.activo = 0;
    
    // Lanzar interrupcion de finalizacion
    lanzar_interrupcion(INT_IO_FINISH);
    
    return NULL;
}

void dma_iniciar(ControladorDMA_t *ctrl) {
    if (ctrl->dma.activo) {
        log_error("DMA ya esta en operacion", 0);
        return;
    }
    
    ctrl->dma.activo = 1;
    ctrl->ejecutando = 1;
    
    // Crear thread para la operacion DMA
    if (pthread_create(&ctrl->thread, NULL, dma_thread_func, ctrl) != 0) {
        log_error("Error al crear thread DMA", 0);
        ctrl->dma.activo = 0;
        ctrl->ejecutando = 0;
        return;
    }
    
    log_mensaje("DMA: Thread de E/S creado");
}

void dma_cleanup(ControladorDMA_t *ctrl) {
    if (ctrl->ejecutando) {
        pthread_join(ctrl->thread, NULL);
        ctrl->ejecutando = 0;
    }
}