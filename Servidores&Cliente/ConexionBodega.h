#ifndef CONEXIONBODEGA_H
#define CONEXIONBODEGA_H

#include <string>

// envia mensajes del protocolo interno al servidor de productos por tcp.
// cada mensaje usa su propia conexion: se envia una linea terminada en '\n' y se lee una linea de respuesta
class ConexionBodega {
public:
    ConexionBodega(const std::string& ip, int puerto);

    // devuelve false si despues de todos los reintentos no hubo respuesta;
    // reintentos queda con la cantidad de reintentos que se hicieron
    bool enviar(const std::string& mensaje, std::string& respuesta, int& reintentos);

private:
    std::string ip_;
    int puerto_;

    bool intentarUnaVez(const std::string& mensaje, std::string& respuesta);
};

#endif