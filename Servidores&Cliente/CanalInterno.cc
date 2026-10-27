#include "CanalInterno.h"
#include "Protocolo.h"

bool recibirMensaje(VSocket* conexion, std::string& mensaje, std::string& error) {
    char buf[512];
    std::string acumulado;
    while (acumulado.find('\n') == std::string::npos && acumulado.size() <= MAX_LINEA) {
        size_t leidos = conexion->Read(buf, sizeof(buf));
        if (leidos == 0) break;
        acumulado.append(buf, leidos);
    }

    size_t fin = acumulado.find('\n');
    if (fin == std::string::npos && acumulado.size() > MAX_LINEA) {
        error = "linea demasiado larga";
        return false;
    }
    if (fin != std::string::npos) acumulado.erase(fin);
    if (!acumulado.empty() && acumulado.back() == '\r') acumulado.pop_back();
    if (acumulado.empty()) {
        error = "conexion cerrada sin mensaje";
        return false;
    }

    mensaje = acumulado;
    return true;
}

void enviarMensaje(VSocket* conexion, const std::string& mensaje) {
    std::string linea = mensaje + "\n";
    conexion->Write(linea.data(), linea.size());
}
