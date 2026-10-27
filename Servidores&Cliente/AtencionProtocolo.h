#ifndef ATENCIONPROTOCOLO_H
#define ATENCIONPROTOCOLO_H

#include <string>
#include "Protocolo.h"
#include "Enrutador.h"

// mensajes del protocolo que llegan en el cuerpo de un POST: 10, 20, 30, 40 y las consultas 73, 75, 77.
// recibe la linea CLI_04|INT_04/... y devuelve la linea de respuesta. El carrito se guarda con la
// clave "origen@ip", porque estos clientes no piden numero de sesion
class AtencionProtocolo {
public:
    AtencionProtocolo(TablaRutas& tabla, Carritos& carritos, Bitacora& bitacora, const std::string& ipCliente);

    std::string responder(const std::string& linea);

private:
    TablaRutas& tabla_;
    Carritos& carritos_;
    Bitacora& bitacora_;
    Enrutador enrutador_;
    std::string ipCliente_;

    std::string procesar(const Mensaje& msg);
    std::string categorias(const Mensaje& msg);
    std::string productos(const Mensaje& msg);
    std::string agregar(const Mensaje& msg);
    std::string factura(const Mensaje& msg);
    std::string servidores(const Mensaje& msg);
    std::string rutas(const Mensaje& msg);
    std::string bitacora(const Mensaje& msg);

    std::string error90(const Mensaje& msg, const std::string& campo, const std::string& valor);
    std::string error92(const Mensaje& msg, const FallaServidor& falla);
};

#endif
