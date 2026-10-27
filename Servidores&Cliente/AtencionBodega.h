#ifndef ATENCIONBODEGA_H
#define ATENCIONBODEGA_H

#include <string>
#include "Protocolo.h"
#include "Inventario.h"

// procesa un mensaje del intermediario y arma la respuesta del servidor de productos.
// no maneja sockets: recibe el texto del mensaje y devuelve el texto de la respuesta.
// los productos se leen del disco (Inventario) en cada mensaje
class AtencionBodega {
public:
    // miId es el SERV_NN de este servidor: con el se responde y se revisa el destino
    AtencionBodega(Inventario& inventario, const std::string& miId);

    std::string responder(const std::string& texto);

private:
    Inventario& inventario_;
    std::string miId_;

    std::string listaCategorias(const std::string& destino);
    std::string listaProductos(const Mensaje& msg, const std::string& destino);
    std::string reservar(const Mensaje& msg, const std::string& destino);
    std::string devolver(const Mensaje& msg, const std::string& destino);
    std::string errorFormato(const std::string& destino, const std::string& tipoOriginal,
                             const std::string& campo, const std::string& valor);
};

#endif
