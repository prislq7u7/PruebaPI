#include "AtencionBodega.h"
#include "Validaciones.h"
#include <cstring>
#include <stdexcept>

// un valor recibido con / o | romperia la respuesta de error que lo devuelve
static std::string limpiarValor(std::string valor) {
    for (char& c : valor) {
        if (c == '/' || c == '|') c = '_';
    }
    return valor;
}

AtencionBodega::AtencionBodega(Inventario& inventario, const std::string& miId)
    : inventario_(inventario), miId_(miId) {}

std::string AtencionBodega::responder(const std::string& texto) {
    Mensaje msg;
    try {
        msg = parsearMensaje(texto);
    } catch (const std::exception&) {
        return construirMensaje(miId_, ID_INTERMEDIARIO, TIPO_ERR_FORMATO, {"00", "mensaje", "invalido"});
    }

    // la respuesta va a quien envio el mensaje, si su id es valido
    std::string destino = validarOrigenDestino(msg.origen) ? msg.origen : ID_INTERMEDIARIO;
    std::string tipoOriginal = std::to_string(msg.tipo);

    // solo un intermediario puede hablar con el servidor de productos
    if (!validarOrigenDestino(msg.origen) || msg.origen.rfind("INT_", 0) != 0) {
        return errorFormato(destino, tipoOriginal, "origen", msg.origen);
    }
    if (msg.destino != miId_) {
        return errorFormato(destino, tipoOriginal, "destino", msg.destino);
    }

    try {
        switch (msg.tipo) {
            case TIPO_REQUEST_CATEGORIES:
                return listaCategorias(destino);
            case TIPO_REQUEST_PRODUCTS:
                return listaProductos(msg, destino);
            case TIPO_RESERVE_STOCK:
                return reservar(msg, destino);
            case TIPO_RELEASE_STOCK:
                return devolver(msg, destino);
            default:
                return errorFormato(destino, tipoOriginal, "tipo_mensaje", tipoOriginal);
        }
    } catch (const std::exception& e) {
        // fallo del disco (lleno, danado): se avisa como falla de comunicacion con la bodega
        return construirMensaje(miId_, destino, TIPO_ERR_COMM, {"disco", tipoOriginal, limpiarValor(e.what())});
    }
}

// lista con el formato categoria,categoria,categoria (escenario 1 del protocolo)
std::string AtencionBodega::listaCategorias(const std::string& destino) {
    std::string lista;
    for (const auto& categoria : inventario_.categorias()) {
        if (!lista.empty()) lista += ",";
        lista += categoria;
    }
    return construirMensaje(miId_, destino, TIPO_CATEGORY_LIST, {lista});
}

std::string AtencionBodega::listaProductos(const Mensaje& msg, const std::string& destino) {
    if (msg.campos.empty() || !validarCategoria(msg.campos[0])) {
        return errorFormato(destino, std::to_string(msg.tipo), "categoria", msg.campos.empty() ? "" : msg.campos[0]);
    }

    // una categoria que no existe se responde igual que una vacia: 22/0 (escenario 3)
    std::vector<Producto> productos = inventario_.productos(msg.campos[0]);
    if (productos.empty()) {
        return construirMensaje(miId_, destino, TIPO_PRODUCT_LIST, {"0"});
    }

    // lista con el formato nombre,precio,stock;nombre,precio,stock
    std::string lista;
    for (const auto& p : productos) {
        if (!lista.empty()) lista += ";";
        lista += p.nombre + "," + precioATexto(p.precioCentavos) + "," + std::to_string(p.cantidad);
    }
    return construirMensaje(miId_, destino, TIPO_PRODUCT_LIST, {std::to_string(productos.size()), lista});
}

// 51/<producto>/<precio>/<reservado>: reservado puede ser menor al pedido (escenario 5),
// o 0 si no habia stock. Si el producto no existe el precio va en 0.00
std::string AtencionBodega::reservar(const Mensaje& msg, const std::string& destino) {
    std::string tipoOriginal = std::to_string(msg.tipo);
    if (msg.campos.size() < 2 || !validarProducto(msg.campos[0])) {
        return errorFormato(destino, tipoOriginal, "producto", msg.campos.empty() ? "" : msg.campos[0]);
    }
    if (!validarCount(msg.campos[1])) {
        return errorFormato(destino, tipoOriginal, "cantidad", msg.campos[1]);
    }

    Producto encontrado;
    int reservado = inventario_.reservar(msg.campos[0], std::stoi(msg.campos[1]), &encontrado);
    bool existe = !encontrado.nombre.empty();

    return construirMensaje(miId_, destino, TIPO_RESERVED,
                            {existe ? encontrado.nombre : msg.campos[0],
                             precioATexto(existe ? encontrado.precioCentavos : 0),
                             std::to_string(reservado)});
}

// 52/<producto>/<cantidad>: el intermediario regresa lo que reservo para una sesion
// que se cerro sin facturar. Responde 53 con lo que se devolvio (0 si el producto ya no existe)
std::string AtencionBodega::devolver(const Mensaje& msg, const std::string& destino) {
    std::string tipoOriginal = std::to_string(msg.tipo);
    if (msg.campos.size() < 2 || !validarProducto(msg.campos[0])) {
        return errorFormato(destino, tipoOriginal, "producto", msg.campos.empty() ? "" : msg.campos[0]);
    }
    if (!validarStock(msg.campos[1])) {
        return errorFormato(destino, tipoOriginal, "cantidad", msg.campos[1]);
    }

    int cantidad = std::stoi(msg.campos[1]);
    bool devuelto = inventario_.devolver(msg.campos[0], cantidad);
    return construirMensaje(miId_, destino, TIPO_RELEASED,
                            {msg.campos[0], std::to_string(devuelto ? cantidad : 0)});
}

// mensaje 90: tipo original, campo invalido y valor recibido
std::string AtencionBodega::errorFormato(const std::string& destino, const std::string& tipoOriginal,
                                         const std::string& campo, const std::string& valor) {
    return construirMensaje(miId_, destino, TIPO_ERR_FORMATO, {tipoOriginal, campo, limpiarValor(valor)});
}
