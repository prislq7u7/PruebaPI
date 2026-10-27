#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "FileSystem.h"
#include "Inventario.h"

// Copia una bodega (archivo de texto de Linux) adentro del disco.
// Uso: agregar_bodega <disco> <bodega.txt> [nombre dentro del disco]

static std::string sinRuta(const std::string& ruta) {
    size_t barra = ruta.find_last_of('/');
    return barra == std::string::npos ? ruta : ruta.substr(barra + 1);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <disco> <bodega.txt> [nombre dentro del disco]\n";
        return 1;
    }

    try {
        std::string origen = argv[2];
        std::string nombre = argc > 3 ? argv[3] : sinRuta(origen);

        std::ifstream entrada(origen);
        if (!entrada) throw std::runtime_error("no se pudo abrir " + origen);
        std::stringstream texto;
        texto << entrada.rdbuf();

        FileSystem fs(argv[1]);
        Inventario inventario(fs);
        int productos = 0;
        std::string id = inventario.importarBodega(nombre, texto.str(), productos);

        Superbloque sb = fs.superbloque();
        std::cout << "OK: " << nombre << " guardada como bodega " << id << " (" << productos
                  << " productos). Quedan " << sb.bloquesLibres << " bloques libres\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
