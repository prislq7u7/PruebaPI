#ifndef INVENTARIO_H
#define INVENTARIO_H

#include <mutex>
#include <string>
#include <vector>
#include "FileSystem.h"

// una linea de una bodega:  I04-S1-B01,postres,Flan-caramelo,20,320
struct Producto {
    std::string bodega;         // id de la bodega donde esta guardado
    std::string categoria;
    std::string nombre;
    int cantidad = 0;
    uint32_t precioCentavos = 0;
};

// Capa de productos encima del FileSystem. Cada bodega es un archivo de texto
// del disco; esta clase sabe leer y escribir esas lineas. Es lo que usa el
// servidor de productos para responder 10, 20 y 50.
class Inventario {
public:
    explicit Inventario(FileSystem& fs);

    std::vector<std::string> bodegas();                         // nombres de archivo
    std::vector<Producto> productosDeBodega(const std::string& archivo);
    std::vector<std::string> categorias();                      // sin repetir, en orden de aparicion
    std::vector<Producto> productos(const std::string& categoria);
    bool buscar(const std::string& producto, Producto& resultado);

    // reserva hasta "cantidad" unidades y devuelve cuantas pudo reservar
    // (puede ser menos que lo pedido, o 0 si no existe o no hay stock)
    int reservar(const std::string& producto, int cantidad, Producto* encontrado = nullptr);

    // regresa al stock unidades que se habian reservado y no se compraron
    bool devolver(const std::string& producto, int cantidad);

    // guarda una bodega nueva a partir de un texto con lineas
    // "categoria,producto,cantidad,precio" (o con el id de bodega adelante);
    // devuelve el id asignado y deja en "cantidadProductos" cuantas lineas se cargaron
    std::string importarBodega(const std::string& archivo, const std::string& texto, int& cantidadProductos);
    std::string exportarBodega(const std::string& archivo);

    static bool parsearLinea(const std::string& linea, Producto& p);
    static std::string armarLinea(const Producto& p);
    static bool categoriaValida(const std::string& categoria);
    static bool productoValido(const std::string& producto);

private:
    FileSystem& fs_;
    std::mutex mtx_;   // reservar es leer + modificar + escribir, no se puede intercalar

    bool modificarCantidad(const std::string& producto, int delta, int& aplicado, Producto* encontrado);
};

#endif
