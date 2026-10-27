#include "TablaRutas.h"
#include <algorithm>
#include <cctype>

static std::string aMinusculas(std::string texto) {
    for (char& c : texto) c = (char)tolower((unsigned char)c);
    return texto;
}

TablaRutas::TablaRutas(int segundosVigencia) : segundosVigencia_(segundosVigencia) {}

std::string TablaRutas::estado(const ServidorRegistrado& s) {
    // vencido pesa mas: ademas de no responder, ya ni siquiera se esta registrando
    if (time(nullptr) - s.ultimoRegistro > segundosVigencia_) return "vencido";
    if (!s.activo) return "sin-respuesta";
    return "activo";
}

bool TablaRutas::registrar(const std::string& id, const std::string& nombre, const std::string& ip, int puerto,
                           const std::vector<std::string>& categorias, bool& cambio) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : servidores_) {
        if (s.id != id) continue;
        cambio = estado(s) != "activo" || s.nombre != nombre || s.ip != ip || s.puerto != puerto ||
                 s.categorias != categorias;
        s.nombre = nombre;
        s.ip = ip;
        s.puerto = puerto;
        s.categorias = categorias;
        s.activo = true;
        s.ultimoRegistro = time(nullptr);
        return false;
    }
    servidores_.push_back({id, nombre, ip, puerto, categorias, true, time(nullptr), "activo"});
    std::sort(servidores_.begin(), servidores_.end(),
              [](const ServidorRegistrado& a, const ServidorRegistrado& b) { return a.id < b.id; });
    cambio = true;
    return true;
}

bool TablaRutas::quitar(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = servidores_.begin(); it != servidores_.end(); ++it) {
        if (it->id == id) {
            servidores_.erase(it);
            return true;
        }
    }
    return false;
}

void TablaRutas::deshabilitar(const std::string& id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : servidores_) {
        if (s.id == id) s.activo = false;
    }
}

bool TablaRutas::buscar(const std::string& id, ServidorRegistrado& resultado) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& s : servidores_) {
        if (s.id == id) { resultado = s; resultado.estado = estado(s); return true; }
    }
    return false;
}

std::vector<ServidorRegistrado> TablaRutas::servidores() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServidorRegistrado> lista = servidores_;
    for (auto& s : lista) s.estado = estado(s);
    return lista;
}

std::vector<ServidorRegistrado> TablaRutas::activos() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServidorRegistrado> lista;
    for (auto& s : servidores_) {
        if (estado(s) == "activo") { lista.push_back(s); lista.back().estado = "activo"; }
    }
    return lista;
}

std::vector<ServidorRegistrado> TablaRutas::servidoresDe(const std::string& categoria) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServidorRegistrado> lista;
    std::string buscada = aMinusculas(categoria);
    for (auto& s : servidores_) {
        if (estado(s) != "activo") continue;
        for (auto& c : s.categorias) {
            if (aMinusculas(c) == buscada) { lista.push_back(s); lista.back().estado = "activo"; break; }
        }
    }
    return lista;
}

std::vector<Ruta> TablaRutas::rutas() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Ruta> filas;
    for (auto& s : servidores_) {
        for (auto& c : s.categorias) {
            filas.push_back({c, s.id, s.ip + ":" + std::to_string(s.puerto), estado(s)});
        }
    }
    return filas;
}

void TablaRutas::recordarProducto(const std::string& producto, const std::string& servidor) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string clave = aMinusculas(producto);
    auto it = productos_.find(clave);
    if (it == productos_.end() || servidor < it->second) productos_[clave] = servidor;
}

std::string TablaRutas::servidorDeProducto(const std::string& producto) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = productos_.find(aMinusculas(producto));
    return it == productos_.end() ? "" : it->second;
}
