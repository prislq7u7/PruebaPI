#ifndef INTERMEDIARIO_H
#define INTERMEDIARIO_H

#include <map>
#include <mutex>
#include <string>
#include <vector>
#include "Buzon.h"
#include "Protocolo.h"


struct ItemCarrito {
    std::string nombre;
    double precio;
    int cantidad;
};

// Un servidor de productos registrado en el intermediario.
struct InfoServidor {
    int id;                                // id interno (ID_PRIMER_SERVIDOR, +1, ...)
    std::string nombre;                    // "Bodega1", para los rótulos
    std::string ip;
    int puerto;
    std::vector<std::string> categorias;   // categorías que publicó al registrarse
};

// Representa el "servidor intermediario". No tiene datos propios de productos.
// Comunica a los servidores de productos con el cliente.
//
// Atiende dos cosas a la vez, cada una en su hilo:
//  - el hilo principal (ejecutar) atiende las peticiones del cliente;
//  - un hilo de registro escucha CANAL_REGISTRO, de modo que un servidor de productos
//    se puede incorporar en cualquier momento, aunque el intermediario ya esté atendiendo.
class Intermediario {
public:
    explicit Intermediario( Buzon & buzon );

    void ejecutar();

private:
    Buzon & buzon_;

    // Tabla de servidores registrados, por id interno (queda en orden SERV_01, SERV_02...).
    // La escribe el hilo de registro y la lee el hilo principal: se protege con mtxTablas_.
    // La tabla de rutas (categoria -> servidores) se calcula a partir de esta.
    std::mutex mtxTablas_;
    std::map<int, InfoServidor> servidores_;

    // Producto -> id del servidor donde se vio listado. Solo lo usa el hilo principal.
    std::map<std::string, int> productoServidor_;

    std::vector<ItemCarrito> carrito_;           // lo que el cliente ha agregado y ya se reservo

    // Registro dinámico (hilo de registro)
    void escucharRegistros();
    void procesarRegistro( const std::string & linea );

    // Utilidades
    std::vector<InfoServidor> copiaServidores();
    void enviarA( int destinoId, const std::string & mensaje, const std::string & explicacion, bool anotar = true );
    void enviarErrFormat( int destinoId, int tipoOriginal, const std::string & campo,
                          const std::string & valor, const std::string & explicacion );
    void enviarErrComm( const std::string & componente, int tipoPendiente );
    bool consultarServidor( const InfoServidor & servidor, int tipo, const std::vector<std::string> & campos,
                            const std::string & explicacion, MensajeProtocolo & respuesta );

    // Peticiones del cliente
    bool atenderClientes();
    void manejarRequestCategories();
    void manejarRequestProducts( const std::string & categoria );
    void manejarAddToCart( const std::string & producto, const std::string & cantidadTexto );
    void manejarRequestFactura();
    void manejarRequestServers();
    void manejarRequestRoutes();
    void manejarRequestLog( const std::string & linea, const std::vector<std::string> & campos );
};

#endif
