#ifndef ATENCIONCLIENTE_H
#define ATENCIONCLIENTE_H

#include <string>
#include <vector>
#include "HttpUtil.h"
#include "Protocolo.h"
#include "Enrutador.h"
#include "PaginaWeb.h"

// paginas GET del intermediario, para el cliente de terminal y el navegador. Usa las mismas reglas
// de enrutamiento (Enrutador) que los mensajes del protocolo por POST, pero responde html
class AtencionCliente {
public:
    AtencionCliente(TablaRutas& tabla, Carritos& carritos, Bitacora& bitacora);

    RespuestaHttp responder(const std::string& peticion);

    // regresa a cada servidor de productos (mensaje 52) lo reservado que nunca se facturo
    void devolverReservas(const std::vector<ItemReservado>& items);

private:
    TablaRutas& tabla_;
    Carritos& carritos_;
    Bitacora& bitacora_;
    Enrutador enrutador_;
    PaginaWeb pagina_;

    RespuestaHttp inicio();
    RespuestaHttp sesion();
    RespuestaHttp categorias();
    RespuestaHttp productos(const std::string& categoria);
    RespuestaHttp agregar(const std::string& sesionTexto, const std::string& producto, const std::string& cantidadTexto);
    RespuestaHttp factura(const std::string& sesionTexto);
    RespuestaHttp cancelar(const std::string& sesionTexto);
    RespuestaHttp servidores();
    RespuestaHttp rutas();
    RespuestaHttp bitacora(long desde);

    bool leerSesion(const std::string& sesionTexto, int& sesion);

    RespuestaHttp sinComunicacion(const FallaServidor& falla);
    RespuestaHttp respuesta(int codigo, const std::string& titulo, const std::string& estado,
                            const std::string& atributos, const std::string& contenido);
    RespuestaHttp error(int codigo, const std::string& estado, const std::string& detalle);
};

#endif
