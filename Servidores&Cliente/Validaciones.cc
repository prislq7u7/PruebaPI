#include "Validaciones.h"
#include "Protocolo.h"
#include <cctype>
#include <string>
#include <arpa/inet.h>

static bool esDigitos(const std::string& s, size_t desde, size_t hasta) {
    for (size_t i = desde; i < hasta; i++) {
        if (!isdigit((unsigned char)s[i])) return false;
    }
    return true;
}

// ^(CLI|INT|SERV)_[0-9]{2}$
bool validarOrigenDestino(const std::string& s) {
    size_t guion = s.find('_');
    if (guion == std::string::npos || s.size() != guion + 3) return false;
    std::string prefijo = s.substr(0, guion);
    if (prefijo != "CLI" && prefijo != "INT" && prefijo != "SERV") return false;
    return esDigitos(s, guion + 1, s.size());
}

bool validarTipoMensaje(const std::string& s) {
    return s.size() == 2 && esDigitos(s, 0, 2);
}

bool validarCategoria(const std::string& s) {
    if (s.size() < 3 || s.size() > 20) return false;
    for (char c : s) if (!isalpha((unsigned char)c)) return false;
    return true;
}

bool validarProducto(const std::string& s) {
    if (s.empty() || s.size() > 50) return false;
    for (char c : s) if (!isalnum((unsigned char)c) && c != '-') return false;
    return true;
}

bool validarPrecio(const std::string& s) {
    if (s.empty() || s.size() > 8) return false;
    size_t punto = s.find('.');
    std::string parteEntera = (punto == std::string::npos) ? s : s.substr(0, punto);
    if (parteEntera.empty() || parteEntera.size() > 6 || !esDigitos(parteEntera, 0, parteEntera.size())) return false;
    if (punto != std::string::npos) {
        std::string parteDecimal = s.substr(punto + 1);
        if (parteDecimal.empty() || parteDecimal.size() > 2 || !esDigitos(parteDecimal, 0, parteDecimal.size())) return false;
    }
    return true;
}

bool validarStock(const std::string& s) {
    return !s.empty() && s.size() <= 5 && esDigitos(s, 0, s.size());
}

bool validarDescripcion(const std::string& s) {
    return s.size() <= 100;
}

bool validarCount(const std::string& s) {
    return !s.empty() && s.size() <= 3 && esDigitos(s, 0, s.size());
}

bool validarIdServidor(const std::string& s) {
    return s.rfind("SERV_", 0) == 0 && validarOrigenDestino(s);
}

bool validarNombre(const std::string& s) {
    if (s.empty() || s.size() > 20) return false;
    for (char c : s) if (!isalnum((unsigned char)c) && c != '-') return false;
    return true;
}

bool validarIp(const std::string& s) {
    struct in_addr direccion;
    return inet_pton(AF_INET, s.c_str(), &direccion) == 1;
}

bool validarPuerto(const std::string& s) {
    if (s.empty() || s.size() > 5 || !esDigitos(s, 0, s.size())) return false;
    int puerto = std::stoi(s);
    return puerto >= 1 && puerto <= 65535;
}

bool validarListaCategorias(const std::string& s) {
    if (s.empty()) return true;
    if (s.size() > 195) return false;
    size_t inicio = 0;
    while (inicio <= s.size()) {
        size_t coma = s.find(',', inicio);
        if (coma == std::string::npos) coma = s.size();
        if (!validarCategoria(s.substr(inicio, coma - inicio))) return false;
        inicio = coma + 1;
    }
    return true;
}
