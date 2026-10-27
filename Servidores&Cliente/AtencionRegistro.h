#ifndef ATENCIONREGISTRO_H
#define ATENCIONREGISTRO_H

#include <string>
#include "Protocolo.h"
#include "TablaRutas.h"
#include "Bitacora.h"

// procesa los mensajes que los servidores de productos mandan al puerto de registro del
// intermediario: 70 (me registro y publico mis categorias) y 72 (me cierro). Responde 71.
// no maneja sockets: recibe el texto del mensaje y devuelve el texto de la respuesta
class AtencionRegistro {
public:
    AtencionRegistro(TablaRutas& tabla, Bitacora& bitacora);

    std::string responder(const std::string& texto);

private:
    TablaRutas& tabla_;
    Bitacora& bitacora_;

    std::string procesar(const Mensaje& msg);
    std::string errorFormato(const Mensaje& msg, const std::string& campo, const std::string& valor);
};

#endif
