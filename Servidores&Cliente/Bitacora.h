#ifndef BITACORA_H
#define BITACORA_H

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <fstream>

// registro de eventos de un programa. cada evento queda:
//   - en un archivo de texto (se puede seguir en vivo con tail -f)
//   - en la terminal
//   - en memoria (los ultimos MAX_EN_MEMORIA), para responder consultas en tiempo real
//
// hay dos clases de eventos:
//   - mensajes del protocolo: timestamp, origen, destino, tipo y estado (OK o 90-92), mas un detalle
//     que solo queda en el archivo. Son los que se consultan con el mensaje 77
//   - eventos internos (INICIO, SESION, ERROR...): solo texto, se ven en /bitacora y en el archivo
class Bitacora {
public:
    Bitacora(const std::string& ruta, const std::string& componente);

    // evento interno; tipo es una palabra corta: INICIO, SESION, FACTURA, ERROR, ...
    void registrar(const std::string& tipo, const std::string& detalle);

    // un mensaje del protocolo que se envio o se recibio; el estado sale del tipo
    // (90, 91 o 92 si el mensaje es un error, OK si no)
    void mensaje(const std::string& texto, const std::string& detalle = "");

    // un mensaje que no se pudo entregar: queda con estado 92
    void falloComunicacion(const std::string& origen, const std::string& destino, int tipo, const std::string& detalle);

    // los n eventos de mensajes mas recientes, del mas nuevo al mas viejo: "HH:MM:SS,origen,destino,tipo,estado"
    std::vector<std::string> ultimos(int n);

    // todas las lineas (mensajes e internos) con numero >= desde; siguiente queda con el que se pide despues
    std::vector<std::string> desde(long desde, long& siguiente);

private:
    static const size_t MAX_EN_MEMORIA = 1000;
    std::mutex mutex_;
    std::ofstream archivo_;
    std::string componente_;
    std::deque<std::string> lineas_;
    std::deque<std::string> eventos_;   // formato del 78
    long siguiente_ = 1;

    void anotar(const std::string& tipo, const std::string& detalle);
    void evento(const std::string& origen, const std::string& destino, int tipo, const std::string& estado,
                const std::string& detalle);
};

#endif
