#include "Enrutador.h"
#include "ConexionBodega.h"
#include "Validaciones.h"
#include <set>
#include <sstream>
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

static bool precioCero(const std::string& precio) {
    return precio.find_first_not_of("0.") == std::string::npos;
}

Enrutador::Enrutador(TablaRutas& tabla, Bitacora& bitacora) : tabla_(tabla), bitacora_(bitacora) {}

bool Enrutador::consultar(const ServidorRegistrado& servidor, int tipo, const std::vector<std::string>& campos,
                          Mensaje& respuesta, FallaServidor& falla) {
    std::string enviado = construirMensaje(ID_INTERMEDIARIO, servidor.id, tipo, campos);
    bitacora_.mensaje(enviado);

    ConexionBodega bodega(servidor.ip, servidor.puerto);
    std::string texto;
    int reintentos = 0;
    if (!bodega.enviar(enviado, texto, reintentos)) {
        // no responde: sale de las rutas hasta que se vuelva a registrar
        tabla_.deshabilitar(servidor.id);
        bitacora_.falloComunicacion(ID_INTERMEDIARIO, servidor.id, tipo,
                                    "sin respuesta despues de " + std::to_string(reintentos) + " reintentos, se deshabilita");
        if (falla.servidor.empty()) falla = {servidor.id, tipo, reintentos};
        return false;
    }
    bitacora_.mensaje(texto);

    try {
        respuesta = parsearMensaje(texto);
    } catch (const std::exception&) {
        if (falla.servidor.empty()) falla = {servidor.id, tipo, reintentos};
        return false;
    }
    if (respuesta.origen != servidor.id || respuesta.tipo == TIPO_ERR_FORMATO ||
        respuesta.tipo == TIPO_ERR_TAMANO || respuesta.tipo == TIPO_ERR_COMM) {
        if (falla.servidor.empty()) falla = {servidor.id, tipo, reintentos};
        return false;
    }
    return true;
}

bool Enrutador::categorias(std::vector<std::string>& lista, FallaServidor& falla) {
    std::vector<ServidorRegistrado> servidores = tabla_.activos();
    std::set<std::string> vistas;
    bool alguno = false;
    for (const auto& servidor : servidores) {
        Mensaje respuesta;
        if (!consultar(servidor, TIPO_REQUEST_CATEGORIES, {}, respuesta, falla)) continue;
        if (respuesta.tipo != TIPO_CATEGORY_LIST) continue;
        alguno = true;
        for (const auto& c : separar(respuesta.campos.empty() ? "" : respuesta.campos[0], ',')) {
            if (validarCategoria(c) && vistas.insert(aMinusculas(c)).second) lista.push_back(c);
        }
    }
    return alguno || servidores.empty();
}

bool Enrutador::productos(const std::string& categoria, std::vector<ProductoRuta>& lista, bool& atendida,
                          FallaServidor& falla) {
    std::vector<ServidorRegistrado> servidores = tabla_.servidoresDe(categoria);
    atendida = !servidores.empty();
    bool alguno = false;
    for (const auto& servidor : servidores) {
        Mensaje respuesta;
        if (!consultar(servidor, TIPO_REQUEST_PRODUCTS, {categoria}, respuesta, falla)) continue;
        if (respuesta.tipo != TIPO_PRODUCT_LIST) continue;
        alguno = true;
        if (respuesta.campos.size() < 2) continue;   // 22/0
        for (const auto& item : separar(respuesta.campos[1], ';')) {
            auto partes = separar(item, ',');
            if (partes.size() != 3 || !validarProducto(partes[0]) || !validarPrecio(partes[1]) || !validarStock(partes[2])) continue;
            lista.push_back({partes[0], partes[1], partes[2], servidor.id});
            tabla_.recordarProducto(partes[0], servidor.id);
        }
    }
    return alguno || !atendida;
}

bool Enrutador::reservar(const std::string& producto, const std::string& cantidad, Reserva& reserva,
                         FallaServidor& falla) {
    // primero el servidor donde se vio el producto, despues los demas en orden de SERV_NN
    std::vector<ServidorRegistrado> candidatos;
    std::string recordado = tabla_.servidorDeProducto(producto);
    for (const auto& s : tabla_.activos()) {
        if (s.id == recordado) candidatos.insert(candidatos.begin(), s);
        else candidatos.push_back(s);
    }

    bool alguno = false;
    reserva = {producto, "0.00", 0, ""};
    for (const auto& servidor : candidatos) {
        Mensaje respuesta;
        if (!consultar(servidor, TIPO_RESERVE_STOCK, {producto, cantidad}, respuesta, falla)) continue;
        alguno = true;

        // el servidor busca sin distinguir mayusculas y responde con el nombre como esta guardado
        if (respuesta.tipo != TIPO_RESERVED || respuesta.campos.size() < 3 ||
            aMinusculas(respuesta.campos[0]) != aMinusculas(producto) ||
            !validarPrecio(respuesta.campos[1]) || !validarCount(respuesta.campos[2])) {
            continue;
        }
        int reservado = std::stoi(respuesta.campos[2]);
        if (reservado == 0 || precioCero(respuesta.campos[1])) continue;   // aqui no hay: el siguiente

        reserva = {respuesta.campos[0], respuesta.campos[1], reservado, servidor.id};
        tabla_.recordarProducto(respuesta.campos[0], servidor.id);
        return true;
    }
    return alguno || candidatos.empty();
}

void Enrutador::devolver(const std::vector<ItemReservado>& items) {
    for (const auto& item : items) {
        ServidorRegistrado servidor;
        Mensaje respuesta;
        FallaServidor falla;
        if (!tabla_.buscar(item.servidor, servidor) ||
            !consultar(servidor, TIPO_RELEASE_STOCK, {item.producto, std::to_string(item.cantidad)}, respuesta, falla)) {
            bitacora_.registrar("ERROR", "no se pudo devolver " + std::to_string(item.cantidad) + " x " + item.producto +
                                " al servidor " + item.servidor);
        }
    }
}
