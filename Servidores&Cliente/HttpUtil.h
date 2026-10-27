#ifndef HTTPUTIL_H
#define HTTPUTIL_H

#include <string>

// codigo http y cuerpo html de una respuesta ya armada
struct RespuestaHttp {
    int codigo;
    std::string cuerpo;
};

std::string urlEncode(const std::string& s);
std::string urlDecode(const std::string& s);
std::string buildGET(const std::string& path, const std::string& host);
int getCodigoEstado(const std::string& respuesta);
std::string extraerCuerpo(const std::string& respuesta);

// ruta pedida sin los parametros, por ejemplo "/" o "/productos"
std::string extraerRuta(const std::string& peticion);

// valor ya decodificado de un parametro de la direccion; vacio si no viene
std::string parametroUrl(const std::string& peticion, const std::string& nombre);

std::string construirRespuestaHttp(int codigo, const std::string& cuerpo,
                                   const std::string& tipo = "text/html; charset=utf-8");

#endif