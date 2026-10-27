#include "AtencionProtocolo.h"
#include "Validaciones.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <cstdio>
#include <cctype>

static std::string aMinusculas(std::string texto) {
    for (char& c : texto) c = (char)tolower((unsigned char)c);
    return texto;
}

static std::vector<std::string> separar(const std::string& texto, char separador) {
    std::vector<std::string> partes;
    std::stringstream ss(texto);
    std::string parte;
    while (std::getline(ss, parte, separador)) {
        if (!parte.empty()) partes.push_back(parte);
    }
    return partes;
}

static uint32_t textoACentavos(const std::string& precio) {
    size_t punto = precio.find('.');
    uint32_t entero = (uint32_t)std::stoul(precio.substr(0, punto));
    uint32_t decimales = 0;
    if (punto != std::string::npos) {
        std::string parte = precio.substr(punto + 1);
        if (parte.size() == 1) parte += "0";
        decimales = (uint32_t)std::stoul(parte);
    }
    return entero * 100 + decimales;
}

static std::string centavosATexto(uint64_t centavos) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu.%02llu", (unsigned long long)(centavos / 100), (unsigned long long)(centavos % 100));
    return buf;
}

AtencionProtocolo::AtencionProtocolo(TablaRutas& tabla, Carritos& carritos, Bitacora& bitacora, const std::string& ipCliente)
    : tabla_(tabla), carritos_(carritos), bitacora_(bitacora), enrutador_(tabla, bitacora), ipCliente_(ipCliente) {}

std::string AtencionProtocolo::responder(const std::string& linea) {
    Mensaje msg;
    std::string respuesta;
    try {
        msg = parsearMensaje(linea);
        respuesta = procesar(msg);
    } catch (const std::exception&) {
        respuesta = construirMensaje(ID_INTERMEDIARIO, ID_CLIENTE, TIPO_ERR_FORMATO, {"00", "mensaje", "invalido"});
    }

    // una respuesta que no cabe en el buffer se cambia por un 91 (el 78 ya recorta lo suyo)
    if (respuesta.size() > MAX_RESPUESTA) {
        Mensaje r = parsearMensaje(respuesta);
        respuesta = construirMensaje(ID_INTERMEDIARIO, r.destino, TIPO_ERR_TAMANO,
                                     {std::to_string(r.tipo), "mensaje", std::to_string(respuesta.size()),
                                      std::to_string(MAX_RESPUESTA)});
    }

    // se anota despues de responder: la respuesta a un 77 no incluye la consulta que la pidio
    bitacora_.mensaje(linea);
    bitacora_.mensaje(respuesta);
    return respuesta;
}

std::string AtencionProtocolo::procesar(const Mensaje& msg) {
    if (!validarOrigenDestino(msg.origen) || msg.origen.rfind("CLI_", 0) != 0) return error90(msg, "origen", msg.origen);
    if (msg.destino != ID_INTERMEDIARIO) return error90(msg, "destino", msg.destino);

    switch (msg.tipo) {
        case TIPO_REQUEST_CATEGORIES: return categorias(msg);
        case TIPO_REQUEST_PRODUCTS:   return productos(msg);
        case TIPO_ADD_TO_CART:        return agregar(msg);
        case TIPO_REQUEST_FACTURA:    return factura(msg);
        case TIPO_REQUEST_SERVERS:    return servidores(msg);
        case TIPO_REQUEST_ROUTES:     return rutas(msg);
        case TIPO_REQUEST_LOG:        return bitacora(msg);
        default:                      return error90(msg, "tipo_mensaje", std::to_string(msg.tipo));
    }
}

// 10 -> 11 con las categorias de todos los servidores, sin repetir
std::string AtencionProtocolo::categorias(const Mensaje& msg) {
    std::vector<std::string> lista;
    FallaServidor falla;
    if (!enrutador_.categorias(lista, falla)) return error92(msg, falla);

    std::string texto;
    for (const auto& c : lista) texto += (texto.empty() ? "" : ",") + c;
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_CATEGORY_LIST, {texto});
}

// 20 -> 22 con los productos de todos los servidores que la atienden, o 23
std::string AtencionProtocolo::productos(const Mensaje& msg) {
    std::string categoria = msg.campos.empty() ? "" : msg.campos[0];
    if (!validarCategoria(categoria)) return error90(msg, "categoria", categoria);

    std::vector<ProductoRuta> lista;
    bool atendida;
    FallaServidor falla;
    if (!enrutador_.productos(categoria, lista, atendida, falla)) return error92(msg, falla);
    if (lista.empty()) {
        return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_PRODUCT_LIST_EMPTY,
                                {"No hay productos disponibles en la categoria \"" + categoria + "\""});
    }

    std::string texto;
    for (const auto& p : lista) texto += (texto.empty() ? "" : ";") + p.nombre + "," + p.precio + "," + p.stock;
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_PRODUCT_LIST, {std::to_string(lista.size()), texto});
}

// 30 -> 32 con lo que realmente quedo en el carrito (0.00 y 0 si ningun servidor lo tiene)
std::string AtencionProtocolo::agregar(const Mensaje& msg) {
    if (msg.campos.size() < 2 || !validarProducto(msg.campos[0])) {
        return error90(msg, "producto", msg.campos.empty() ? "" : msg.campos[0]);
    }
    if (!validarCount(msg.campos[1]) || std::stoi(msg.campos[1]) == 0) return error90(msg, "cantidad", msg.campos[1]);

    Reserva reserva;
    FallaServidor falla;
    if (!enrutador_.reservar(msg.campos[0], msg.campos[1], reserva, falla)) return error92(msg, falla);
    if (reserva.reservado == 0) {
        return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_CART_UPDATED, {msg.campos[0], "0.00", "0"});
    }

    int sesion = carritos_.sesionDe(msg.origen + "@" + ipCliente_);
    carritos_.agregar(sesion, {reserva.nombre, reserva.reservado, textoACentavos(reserva.precio), reserva.servidor});
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_CART_UPDATED,
                            {reserva.nombre, reserva.precio, std::to_string(reserva.reservado)});
}

// 40/<count>/<producto,cantidad;...> -> 41/<total>/<count>/<producto,cantidad,subtotal;...> o 42.
// se cobra solo lo que el cliente trae en su lista; lo reservado que no trae (de una sesion
// anterior desde la misma computadora que nunca facturo) vuelve al stock
std::string AtencionProtocolo::factura(const Mensaje& msg) {
    std::map<std::string, int> pedidos;
    if (!msg.campos.empty() && msg.campos[0] != "0" && msg.campos.size() > 1) {
        for (const auto& item : separar(msg.campos[1], ';')) {
            auto partes = separar(item, ',');
            if (partes.size() == 2 && validarCount(partes[1])) pedidos[aMinusculas(partes[0])] += std::stoi(partes[1]);
        }
    }

    int sesion = carritos_.sesionDe(msg.origen + "@" + ipCliente_);
    std::vector<ItemReservado> items, sobrantes;
    carritos_.vaciar(sesion, items);

    std::vector<ItemReservado> facturados;
    for (auto& item : items) {
        int& pedido = pedidos[aMinusculas(item.producto)];
        int cobrar = std::min(pedido, item.cantidad);
        pedido -= cobrar;
        if (cobrar > 0) facturados.push_back({item.producto, cobrar, item.precioCentavos, item.servidor});
        if (item.cantidad > cobrar) sobrantes.push_back({item.producto, item.cantidad - cobrar, item.precioCentavos, item.servidor});
    }
    enrutador_.devolver(sobrantes);

    if (facturados.empty()) return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_CART_EMPTY, {"El carrito esta vacio"});

    uint64_t total = 0;
    std::string detalle;
    for (const auto& item : facturados) {
        uint64_t subtotal = (uint64_t)item.precioCentavos * item.cantidad;
        total += subtotal;
        detalle += (detalle.empty() ? "" : ";") + item.producto + "," + std::to_string(item.cantidad) + "," + centavosATexto(subtotal);
    }
    bitacora_.registrar("FACTURA", msg.origen + "@" + ipCliente_ + ": " + centavosATexto(total));
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_FACTURA,
                            {centavosATexto(total), std::to_string(facturados.size()), detalle});
}

// 73 -> 74/<count>/<SERV_NN,nombre,ip,puerto;...>
std::string AtencionProtocolo::servidores(const Mensaje& msg) {
    auto lista = tabla_.servidores();
    std::string texto;
    for (const auto& s : lista) {
        texto += (texto.empty() ? "" : ";") + s.id + "," + s.nombre + "," + s.ip + "," + std::to_string(s.puerto);
    }
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_SERVER_LIST, {std::to_string(lista.size()), texto});
}

// 75 -> 76/<count>/<categoria,SERV_NN;...>, solo las rutas que se estan usando (servidores activos)
std::string AtencionProtocolo::rutas(const Mensaje& msg) {
    std::string texto;
    int count = 0;
    for (const auto& r : tabla_.rutas()) {
        if (r.estado != "activo") continue;
        texto += (texto.empty() ? "" : ";") + r.categoria + "," + r.servidor;
        count++;
    }
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_ROUTE_TABLE, {std::to_string(count), texto});
}

// 77/<n> -> 78/<count>/<timestamp,origen,destino,tipo,estado;...>, del mas nuevo al mas viejo.
// si no cabe en el buffer se quitan los mas viejos
std::string AtencionProtocolo::bitacora(const Mensaje& msg) {
    std::string n = msg.campos.empty() ? "" : msg.campos[0];
    if (!validarCount(n)) return error90(msg, "n", n);

    std::vector<std::string> eventos = bitacora_.ultimos(std::stoi(n));
    for (;;) {
        std::string texto;
        for (const auto& e : eventos) texto += (texto.empty() ? "" : ";") + e;
        std::string respuesta = construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_LOG_LIST,
                                                 {std::to_string(eventos.size()), texto});
        if (respuesta.size() <= MAX_RESPUESTA || eventos.empty()) return respuesta;
        eventos.pop_back();
    }
}

std::string AtencionProtocolo::error90(const Mensaje& msg, const std::string& campo, const std::string& valor) {
    std::string limpio = valor;
    for (char& c : limpio) if (c == '/' || c == '|') c = '_';
    std::string destino = validarOrigenDestino(msg.origen) ? msg.origen : ID_CLIENTE;
    return construirMensaje(ID_INTERMEDIARIO, destino, TIPO_ERR_FORMATO, {std::to_string(msg.tipo), campo, limpio});
}

// 92/<componente que no responde>/<tipo pendiente>/<intentos>
std::string AtencionProtocolo::error92(const Mensaje& msg, const FallaServidor& falla) {
    return construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_ERR_COMM,
                            {falla.servidor, std::to_string(falla.tipo), std::to_string(falla.intentos)});
}
