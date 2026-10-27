#ifndef CLIENTE_H
#define CLIENTE_H

#include <functional>
#include <string>

#include "Buzon.h"

// Esta clase es solo un guion de pruebas para probar el protocolo
class Cliente {
public:
    explicit Cliente( Buzon & buzon );
    void ejecutar();

    // Lo llama el guion en el paso donde se incorpora un segundo servidor de productos
    // con el sistema ya en marcha. Lo define simulacion.cc (arranca el hilo del servidor).
    std::function<void()> incorporarServidor;

private:
    Buzon & buzon_;
    void pedir( const std::string & peticion );
};

#endif
