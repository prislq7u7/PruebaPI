#include "ConexionBodega.h"
#include "SSLSocket.h"
#include "Protocolo.h"
#include "CanalInterno.h"
#include <chrono>
#include <thread>
#include <stdexcept>

static const int REINTENTOS = 2;           // intentos extra despues del primero
static const int SEGUNDOS_ESPERA = 3;      // tiempo maximo de connect y de read
static const int MILISEGUNDOS_ENTRE_INTENTOS = 500;

ConexionBodega::ConexionBodega(const std::string& ip, int puerto) : ip_(ip), puerto_(puerto) {}

bool ConexionBodega::enviar(const std::string& mensaje, std::string& respuesta, int& reintentos) {
    for (reintentos = 0; reintentos <= REINTENTOS; reintentos++) {
        if (reintentos > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(MILISEGUNDOS_ENTRE_INTENTOS));
        }
        if (intentarUnaVez(mensaje, respuesta)) return true;
    }
    reintentos = REINTENTOS;
    return false;
}

bool ConexionBodega::intentarUnaVez(const std::string& mensaje, std::string& respuesta) {
    try {
        SSLSocket conexion;
        conexion.SetTimeout(SEGUNDOS_ESPERA);
        conexion.Connect(ip_.c_str(), puerto_);

        // la conexion va por TLS: una linea por mensaje (CanalInterno)
        enviarMensaje(&conexion, mensaje);
        std::string error;
        return recibirMensaje(&conexion, respuesta, error);

    } catch (const std::exception&) {
        // connect rechazado, tiempo de espera agotado o conexion cortada: cuenta como intento fallido
        return false;
    }
}