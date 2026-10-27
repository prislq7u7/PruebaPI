#ifndef VISTAS_H
#define VISTAS_H

#include <string>
#include <vector>

// copia temporal de lo que se va a mostrar: se arma en cada peticion y se descarta.
// los campos son texto para poder llenarlos desde el filesystem o desde un mensaje del protocolo
struct ProductoVista {
    std::string nombre;
    std::string precio;
    std::string stock;
};

struct CategoriaVista {
    std::string id;
    std::string nombre;
    std::vector<ProductoVista> productos;
};

struct BodegaVista {
    std::string id;
    std::string nombre;
    std::vector<CategoriaVista> categorias;
};

#endif