#include "Inventario.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

static const int MAX_STOCK = 99999;   // 5 digitos, igual que la regex de stock del protocolo

static std::string aMinusculas(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = (char)tolower((unsigned char)c);
    return r;
}

static std::vector<std::string> separar(const std::string& s, char delim) {
    std::vector<std::string> partes;
    std::stringstream ss(s);
    std::string parte;
    while (std::getline(ss, parte, delim)) partes.push_back(parte);
    if (!s.empty() && s.back() == delim) partes.push_back("");
    return partes;
}

static bool soloDigitos(const std::string& s, size_t maximo) {
    if (s.empty() || s.size() > maximo) return false;
    for (char c : s) if (!isdigit((unsigned char)c)) return false;
    return true;
}

bool Inventario::categoriaValida(const std::string& categoria) {
    static const std::regex patron("^[a-zA-Z]{3,20}$");
    return std::regex_match(categoria, patron);
}

bool Inventario::productoValido(const std::string& producto) {
    static const std::regex patron("^[a-zA-Z0-9\\-]{1,50}$");
    return std::regex_match(producto, patron);
}

bool Inventario::parsearLinea(const std::string& linea, Producto& p) {
    auto campos = separar(linea, ',');
    if (campos.size() != 5) return false;
    if (!categoriaValida(campos[1]) || !productoValido(campos[2])) return false;
    if (!soloDigitos(campos[3], 5) || !soloDigitos(campos[4], 9)) return false;
    p.bodega = campos[0];
    p.categoria = campos[1];
    p.nombre = campos[2];
    p.cantidad = std::stoi(campos[3]);
    p.precioCentavos = (uint32_t)std::stoul(campos[4]);
    return true;
}

std::string Inventario::armarLinea(const Producto& p) {
    return p.bodega + "," + p.categoria + "," + p.nombre + "," +
           std::to_string(p.cantidad) + "," + std::to_string(p.precioCentavos) + "\n";
}

Inventario::Inventario(FileSystem& fs) : fs_(fs) {}

std::vector<std::string> Inventario::bodegas() {
    std::vector<std::string> nombres;
    for (auto& e : fs_.listar()) nombres.push_back(std::string(e.nombre, strnlen(e.nombre, sizeof(e.nombre))));
    return nombres;
}

std::vector<Producto> Inventario::productosDeBodega(const std::string& archivo) {
    std::vector<Producto> productos;
    std::istringstream contenido(fs_.leer(archivo));
    std::string linea;
    while (std::getline(contenido, linea)) {
        Producto p;
        if (parsearLinea(linea, p)) productos.push_back(p);
    }
    return productos;
}

std::vector<std::string> Inventario::categorias() {
    std::vector<std::string> categorias;
    std::set<std::string> vistas;
    for (auto& archivo : bodegas()) {
        for (auto& p : productosDeBodega(archivo)) {
            if (vistas.insert(aMinusculas(p.categoria)).second) categorias.push_back(p.categoria);
        }
    }
    return categorias;
}

std::vector<Producto> Inventario::productos(const std::string& categoria) {
    std::vector<Producto> resultado;
    std::string buscada = aMinusculas(categoria);
    for (auto& archivo : bodegas()) {
        for (auto& p : productosDeBodega(archivo)) {
            if (aMinusculas(p.categoria) == buscada) resultado.push_back(p);
        }
    }
    return resultado;
}

bool Inventario::buscar(const std::string& producto, Producto& resultado) {
    std::string buscado = aMinusculas(producto);
    for (auto& archivo : bodegas()) {
        for (auto& p : productosDeBodega(archivo)) {
            if (aMinusculas(p.nombre) == buscado) { resultado = p; return true; }
        }
    }
    return false;
}

// busca el producto en todas las bodegas, le suma "delta" (negativo para reservar)
// y reescribe solo el archivo de esa bodega
bool Inventario::modificarCantidad(const std::string& producto, int delta, int& aplicado, Producto* encontrado) {
    std::string buscado = aMinusculas(producto);
    for (auto& archivo : bodegas()) {
        std::vector<Producto> lista = productosDeBodega(archivo);
        for (auto& p : lista) {
            if (aMinusculas(p.nombre) != buscado) continue;

            if (delta < 0) aplicado = std::min(-delta, p.cantidad);      // reserva parcial
            else aplicado = std::min(delta, MAX_STOCK - p.cantidad);
            p.cantidad += (delta < 0) ? -aplicado : aplicado;
            if (encontrado) *encontrado = p;
            if (aplicado == 0) return true;

            std::string contenido;
            for (auto& q : lista) contenido += armarLinea(q);
            fs_.escribir(archivo, contenido);
            return true;
        }
    }
    aplicado = 0;
    return false;
}

int Inventario::reservar(const std::string& producto, int cantidad, Producto* encontrado) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (cantidad <= 0) {
        if (encontrado) buscar(producto, *encontrado);
        return 0;
    }
    int reservado = 0;
    modificarCantidad(producto, -cantidad, reservado, encontrado);
    return reservado;
}

bool Inventario::devolver(const std::string& producto, int cantidad) {
    if (cantidad <= 0) return false;
    std::lock_guard<std::mutex> lock(mtx_);
    int devuelto = 0;
    return modificarCantidad(producto, cantidad, devuelto, nullptr);
}

std::string Inventario::importarBodega(const std::string& archivo, const std::string& texto, int& cantidadProductos) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (fs_.existe(archivo)) throw std::runtime_error("ya hay una bodega llamada " + archivo + " en el disco");

    // los nombres de producto no se pueden repetir en el disco, porque el
    // mensaje 50 (RESERVE_STOCK) solo trae el nombre
    std::set<std::string> usados;
    for (auto& otra : bodegas()) {
        for (auto& p : productosDeBodega(otra)) usados.insert(aMinusculas(p.nombre));
    }

    std::vector<Producto> productos;
    std::istringstream entrada(texto);
    std::string linea;
    int numeroLinea = 0;
    while (std::getline(entrada, linea)) {
        numeroLinea++;
        if (!linea.empty() && linea.back() == '\r') linea.pop_back();
        if (linea.empty() || linea[0] == '#') continue;

        auto campos = separar(linea, ',');
        if (campos.size() == 5) campos.erase(campos.begin());   // traia el id de otra bodega
        std::string error;
        if (campos.size() != 4) error = "se esperaban 4 campos: categoria,producto,cantidad,precio";
        else if (!categoriaValida(campos[0])) error = "categoria invalida \"" + campos[0] + "\" (3-20 letras)";
        else if (!productoValido(campos[1])) error = "producto invalido \"" + campos[1] + "\" (1-50 letras, numeros o -)";
        else if (!soloDigitos(campos[2], 5)) error = "cantidad invalida \"" + campos[2] + "\" (0-99999)";
        else if (!soloDigitos(campos[3], 9)) error = "precio invalido \"" + campos[3] + "\" (en centavos, sin punto)";
        else if (!usados.insert(aMinusculas(campos[1])).second) error = "el producto \"" + campos[1] + "\" ya existe en el disco";
        if (!error.empty()) {
            throw std::runtime_error("linea " + std::to_string(numeroLinea) + ": " + error);
        }

        Producto p;
        p.categoria = campos[0];
        p.nombre = campos[1];
        p.cantidad = std::stoi(campos[2]);
        p.precioCentavos = (uint32_t)std::stoul(campos[3]);
        productos.push_back(p);
    }
    if (productos.empty()) throw std::runtime_error("el archivo no tiene productos");

    std::string id = fs_.crear(archivo);
    std::string contenido;
    for (auto& p : productos) {
        p.bodega = id;
        contenido += armarLinea(p);
    }
    try {
        fs_.escribir(archivo, contenido);
    } catch (...) {
        fs_.borrar(archivo);   // no dejar una bodega vacia si el disco se lleno
        throw;
    }
    cantidadProductos = (int)productos.size();
    return id;
}

std::string Inventario::exportarBodega(const std::string& archivo) {
    return fs_.leer(archivo);
}
