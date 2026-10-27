#include "AtencionRegistro.h"
#include "Validaciones.h"
#include <sstream>
#include <stdexcept>

AtencionRegistro::AtencionRegistro(TablaRutas& tabla, Bitacora& bitacora) : tabla_(tabla), bitacora_(bitacora) {}

std::string AtencionRegistro::responder(const std::string& texto) {
    Mensaje msg;
    std::string respuesta;
    try {
        msg = parsearMensaje(texto);
        respuesta = procesar(msg);
    } catch (const std::exception&) {
        respuesta = construirMensaje(ID_INTERMEDIARIO, idServidor(0), TIPO_ERR_FORMATO, {"00", "mensaje", "invalido"});
    }
    bitacora_.mensaje(texto);
    bitacora_.mensaje(respuesta);
    return respuesta;
}

std::string AtencionRegistro::procesar(const Mensaje& msg) {
    if (!validarIdServidor(msg.origen)) return errorFormato(msg, "origen", msg.origen);
    if (msg.destino != ID_INTERMEDIARIO) return errorFormato(msg, "destino", msg.destino);

    // 72: el servidor se cierra y sale de la tabla de rutas
    if (msg.tipo == TIPO_UNREGISTER_SERVER) {
        if (tabla_.quitar(msg.origen)) bitacora_.registrar("BAJA", msg.origen + " se cerro, sale de la tabla de rutas");
        return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_REGISTER_ACK, {"0"});
    }
    if (msg.tipo != TIPO_REGISTER_SERVER) return errorFormato(msg, "tipo_mensaje", std::to_string(msg.tipo));

    // 70/<nombre>/<ip>/<puerto>/<lista_categorias>; si un campo falla se responde 90 y no se registra nada
    if (msg.campos.size() < 3) return errorFormato(msg, "campos", std::to_string(msg.campos.size()));
    const std::string& nombre = msg.campos[0];
    const std::string& ip = msg.campos[1];
    std::string categoriasTexto = msg.campos.size() > 3 ? msg.campos[3] : "";
    if (!validarNombre(nombre)) return errorFormato(msg, "nombre", nombre);
    if (!validarIp(ip)) return errorFormato(msg, "ip", ip);
    if (!validarPuerto(msg.campos[2])) return errorFormato(msg, "puerto", msg.campos[2]);
    if (!validarListaCategorias(categoriasTexto)) {
        // se senala la primera categoria que no cumple, como en el escenario 15
        std::stringstream ss(categoriasTexto);
        std::string c;
        while (std::getline(ss, c, ',')) {
            if (!validarCategoria(c)) return errorFormato(msg, "categoria", c);
        }
        return errorFormato(msg, "lista_categorias", std::to_string(categoriasTexto.size()));
    }

    std::vector<std::string> categorias;
    std::stringstream ss(categoriasTexto);
    std::string categoria;
    while (std::getline(ss, categoria, ',')) categorias.push_back(categoria);

    bool cambio = false;
    bool nuevo = tabla_.registrar(msg.origen, nombre, ip, std::stoi(msg.campos[2]), categorias, cambio);
    if (cambio) {
        bitacora_.registrar("REGISTRO", std::string(nuevo ? "servidor registrado: " : "servidor reemplazado: ") +
                            msg.origen + " (" + nombre + ") en " + ip + ":" + msg.campos[2] + " con " +
                            (categoriasTexto.empty() ? "(sin categorias)" : categoriasTexto));
    }
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_REGISTER_ACK, {std::to_string(categorias.size())});
}

std::string AtencionRegistro::errorFormato(const Mensaje& msg, const std::string& campo, const std::string& valor) {
    std::string limpio = valor;
    for (char& c : limpio) if (c == '/' || c == '|') c = '_';
    std::string destino = validarIdServidor(msg.origen) ? msg.origen : idServidor(0);
    return construirMensaje(ID_INTERMEDIARIO, destino, TIPO_ERR_FORMATO, {std::to_string(msg.tipo), campo, limpio});
}
