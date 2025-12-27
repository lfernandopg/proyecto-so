#include "sistema.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    Sistema_t sistema;
    
    // Inicializar logger
    log_init();
    
    // Inicializar sistema
    sistema_init(&sistema);
    
    // Lanzar consola interactiva
    sistema_consola(&sistema);
    
    // Limpiar recursos
    sistema_cleanup(&sistema);
    log_close();
    
    return 0;
}