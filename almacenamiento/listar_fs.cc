#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include "FileSystem.h"
#include "Inventario.h"

// Lista las bodegas del disco y sus productos agrupados por categoria.
// Con una categoria como segundo argumento, extrae solo esa categoria.
// Uso: listar_fs <disco> [categoria]

static std::string precio(uint32_t centavos) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%02u", centavos / 100, centavos % 100);
    return buf;
}

static void imprimirProducto(const Producto& p) {
    std::cout << "    " << std::left << std::setw(28) << p.nombre
              << " cantidad=" << std::setw(6) << p.cantidad
              << " precio=" << std::setw(9) << precio(p.precioCentavos)
              << " [" << p.bodega << "]\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Uso: " << argv[0] << " <disco> [categoria]\n";
        return 1;
    }

    try {
        FileSystem fs(argv[1]);
        Inventario inventario(fs);
        Superbloque sb = fs.superbloque();

        if (argc > 2) {
            auto productos = inventario.productos(argv[2]);
            std::cout << argv[2] << " en " << fs.idDisco() << ": " << productos.size() << " producto(s)\n";
            for (auto& p : productos) imprimirProducto(p);
            return productos.empty() ? 1 : 0;
        }

        std::cout << "Disco " << fs.idDisco() << " \"" << std::string(sb.nombre, strnlen(sb.nombre, sizeof(sb.nombre)))
                  << "\"\n\nBodegas:\n";
        int totalProductos = 0;
        for (auto& e : fs.listar()) {
            std::string nombre(e.nombre, strnlen(e.nombre, sizeof(e.nombre)));
            Inodo ino = fs.inodo(e.inodo);
            size_t cantidad = inventario.productosDeBodega(nombre).size();
            totalProductos += (int)cantidad;
            std::cout << "  " << std::left << std::setw(18) << nombre << std::setw(12)
                      << std::string(e.id, strnlen(e.id, sizeof(e.id)))
                      << cantidad << " productos, i-nodo " << e.inodo << ", " << ino.tamano << " bytes\n";
        }

        auto categorias = inventario.categorias();
        std::cout << "\nCategorias:\n";
        for (auto& categoria : categorias) {
            auto productos = inventario.productos(categoria);
            std::cout << "  " << categoria << " (" << productos.size() << ")\n";
            for (auto& p : productos) imprimirProducto(p);
        }

        std::cout << "\ntotal: " << fs.listar().size() << " bodega(s), " << categorias.size()
                  << " categoria(s), " << totalProductos << " producto(s); "
                  << sb.bloquesLibres << " de " << sb.numBloques << " bloques libres\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
