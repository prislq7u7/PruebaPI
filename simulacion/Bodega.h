#ifndef BODEGA_H
#define BODEGA_H

#include <string>
#include <vector>

#include "Buzon.h"
#include "Protocolo.h"

// Representa un "servidor de productos". No sabe nada de clientes, solo recibe mensajes del intermediario
//y responde con la información de productos que tiene en su inventario.
// Puede haber varios (SERV_01, SERV_02...), cada uno con su id interno, y se pueden
// incorporar en cualquier momento: al arrancar se registra ante el intermediario.
class Bodega {
public:
    // ip y puerto son los que se publican en el registro (en la simulación no se usan para
    // conectar, porque la comunicación va por el Buzon).
    Bodega( Buzon & buzon, int miId, const std::string & nombre,
            const std::string & ip, int puerto,
            std::vector<std::string> categorias, std::vector<Producto> productos );

    // Ciclo principal del hilo: se registra una vez y luego atiende mensajes hasta recibir EXIT.
    void ejecutar();

private:
    Buzon & buzon_;
    int miId_;
    std::string nombre_;
    std::string ip_;
    int puerto_;
    std::vector<std::string> categorias_;
    std::vector<Producto> productos_;

    bool registrarse();
    void manejarRequestCategories();
    void manejarRequestProducts( const std::string & categoria );
    void manejarReserveStock( const std::string & producto, const std::string & cantidadTexto );
};

#endif
