#include "Bitacora.h"
#include "Protocolo.h"
#include <cstdio>
#include <ctime>
#include <iostream>

static std::string ahora(const char* formato) {
    char texto[32];
    time_t t = time(nullptr);
    strftime(texto, sizeof(texto), formato, localtime(&t));
    return texto;
}

Bitacora::Bitacora(const std::string& ruta, const std::string& componente)
    : archivo_(ruta, std::ios::app), componente_(componente) {}

// quien llama ya tiene el mutex
void Bitacora::anotar(const std::string& tipo, const std::string& detalle) {
    std::string linea = std::to_string(siguiente_) + " " + ahora("%Y-%m-%d %H:%M:%S") + " [" + componente_ + "] " +
                        tipo + " " + detalle;
    siguiente_++;

    archivo_ << linea << "\n";
    archivo_.flush();
    std::cout << linea << "\n";

    lineas_.push_back(linea);
    if (lineas_.size() > MAX_EN_MEMORIA) lineas_.pop_front();
}

void Bitacora::registrar(const std::string& tipo, const std::string& detalle) {
    std::lock_guard<std::mutex> lock(mutex_);
    anotar(tipo, detalle);
}

void Bitacora::evento(const std::string& origen, const std::string& destino, int tipo, const std::string& estado,
                      const std::string& detalle) {
    char tipoTexto[8];
    snprintf(tipoTexto, sizeof(tipoTexto), "%02d", tipo);
    std::string resumen = ahora("%H:%M:%S") + "," + origen + "," + destino + "," + tipoTexto + "," + estado;

    std::lock_guard<std::mutex> lock(mutex_);
    anotar("MENSAJE", origen + " -> " + destino + " " + tipoTexto + " " + estado + (detalle.empty() ? "" : " " + detalle));
    eventos_.push_back(resumen);
    if (eventos_.size() > MAX_EN_MEMORIA) eventos_.pop_front();
}

void Bitacora::mensaje(const std::string& texto, const std::string& detalle) {
    Mensaje msg;
    try {
        msg = parsearMensaje(texto);
    } catch (const std::exception&) {
        registrar("ERROR", "mensaje mal formado: " + texto);
        return;
    }
    std::string estado = (msg.tipo >= TIPO_ERR_FORMATO && msg.tipo <= TIPO_ERR_COMM) ? std::to_string(msg.tipo) : "OK";
    evento(msg.origen, msg.destino, msg.tipo, estado, detalle.empty() ? texto : detalle);
}

void Bitacora::falloComunicacion(const std::string& origen, const std::string& destino, int tipo,
                                 const std::string& detalle) {
    evento(origen, destino, tipo, std::to_string(TIPO_ERR_COMM), detalle);
}

std::vector<std::string> Bitacora::ultimos(int n) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> resultado;
    for (auto it = eventos_.rbegin(); it != eventos_.rend() && (int)resultado.size() < n; ++it) {
        resultado.push_back(*it);
    }
    return resultado;
}

std::vector<std::string> Bitacora::desde(long desde, long& siguiente) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> resultado;

    // la primera linea en memoria tiene el numero siguiente_ - lineas_.size()
    long primero = siguiente_ - (long)lineas_.size();
    for (long n = (desde > primero ? desde : primero); n < siguiente_; n++) {
        resultado.push_back(lineas_[n - primero]);
    }
    siguiente = siguiente_;
    return resultado;
}
