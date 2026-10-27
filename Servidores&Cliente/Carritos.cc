#include "Carritos.h"

int Carritos::nuevaSesion() {
    std::lock_guard<std::mutex> lock(mutex_);
    int sesion = siguienteSesion_++;
    carritos_[sesion] = {{}, time(nullptr)};
    return sesion;
}

int Carritos::sesionDe(const std::string& clave) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = claves_.find(clave);
    if (it != claves_.end() && carritos_.count(it->second)) {
        carritos_[it->second].ultimoUso = time(nullptr);
        return it->second;
    }
    int sesion = siguienteSesion_++;
    carritos_[sesion] = {{}, time(nullptr)};
    claves_[clave] = sesion;
    return sesion;
}

bool Carritos::existe(int sesion) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = carritos_.find(sesion);
    if (it == carritos_.end()) return false;
    it->second.ultimoUso = time(nullptr);
    return true;
}

bool Carritos::agregar(int sesion, const ItemReservado& item) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = carritos_.find(sesion);
    if (it == carritos_.end()) return false;
    it->second.ultimoUso = time(nullptr);

    for (auto& existente : it->second.items) {
        if (existente.producto == item.producto && existente.precioCentavos == item.precioCentavos &&
            existente.servidor == item.servidor) {
            existente.cantidad += item.cantidad;
            return true;
        }
    }
    it->second.items.push_back(item);
    return true;
}

bool Carritos::vaciar(int sesion, std::vector<ItemReservado>& items) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = carritos_.find(sesion);
    if (it == carritos_.end()) return false;

    items = it->second.items;
    it->second.items.clear();
    it->second.ultimoUso = time(nullptr);
    return true;
}

bool Carritos::cerrar(int sesion, std::vector<ItemReservado>& items) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = carritos_.find(sesion);
    if (it == carritos_.end()) return false;

    items = it->second.items;
    carritos_.erase(it);
    return true;
}

std::vector<ItemReservado> Carritos::cerrarVencidas(int segundos, std::vector<int>& cerradas) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ItemReservado> pendientes;
    time_t ahora = time(nullptr);

    for (auto it = carritos_.begin(); it != carritos_.end();) {
        if (ahora - it->second.ultimoUso < segundos) { ++it; continue; }
        for (auto& item : it->second.items) pendientes.push_back(item);
        cerradas.push_back(it->first);
        it = carritos_.erase(it);
    }
    return pendientes;
}
