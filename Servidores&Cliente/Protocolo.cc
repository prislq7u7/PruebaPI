#include "Protocolo.h"
#include <sstream>
#include <stdexcept>
#include <cstdio>

std::string construirMensaje(const std::string& origen, const std::string& destino,
                              int tipo, const std::vector<std::string>& campos) {
    std::ostringstream oss;
    oss << origen << "|" << destino << "/" << tipo;
    for (const auto& campo : campos) oss << "/" << campo;
    return oss.str();
}

Mensaje parsearMensaje(const std::string& texto) {
    Mensaje msg;

    size_t pipe = texto.find('|');
    if (pipe == std::string::npos) {
        throw std::runtime_error("parsearMensaje: falta '|' entre origen y destino");
    }
    msg.origen = texto.substr(0, pipe);

    std::string resto = texto.substr(pipe + 1);
    size_t slash1 = resto.find('/');
    if (slash1 == std::string::npos) {
        throw std::runtime_error("parsearMensaje: falta '/' despues del destino");
    }
    msg.destino = resto.substr(0, slash1);

    resto = resto.substr(slash1 + 1);
    size_t slash2 = resto.find('/');
    std::string tipoStr = (slash2 == std::string::npos) ? resto : resto.substr(0, slash2);
    try {
        msg.tipo = std::stoi(tipoStr);
    } catch (...) {
        throw std::runtime_error("parsearMensaje: tipo_mensaje invalido");
    }

    if (slash2 != std::string::npos) {
        std::string campos = resto.substr(slash2 + 1);
        std::stringstream ss(campos);
        std::string campo;
        while (std::getline(ss, campo, '/')) msg.campos.push_back(campo);
    }

    return msg;
}

std::string idServidor(int numero) {
    char id[16];
    snprintf(id, sizeof(id), "SERV_%02d", numero);
    return id;
}

std::string precioATexto(uint32_t centavos) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%02u", centavos / 100, centavos % 100);
    return buf;
}

std::string codificarCampo(const std::string& texto) {
    std::string salida;
    for (char c : texto) {
        switch (c) {
            case '%': salida += "%25"; break;
            case '/': salida += "%2F"; break;
            case '|': salida += "%7C"; break;
            case ';': salida += "%3B"; break;
            default:  salida += c;
        }
    }
    return salida;
}

std::string decodificarCampo(const std::string& texto) {
    std::string salida;
    for (size_t i = 0; i < texto.size(); i++) {
        if (texto[i] == '%' && i + 2 < texto.size()) {
            std::string codigo = texto.substr(i + 1, 2);
            if (codigo == "25") { salida += '%'; i += 2; continue; }
            if (codigo == "2F") { salida += '/'; i += 2; continue; }
            if (codigo == "7C") { salida += '|'; i += 2; continue; }
            if (codigo == "3B") { salida += ';'; i += 2; continue; }
        }
        salida += texto[i];
    }
    return salida;
}
