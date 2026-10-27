#include <iostream>
#include <string>
#include "FileSystem.h"

// Borra una bodega del disco y devuelve sus bloques e i-nodo.
// Uso: borrar_bodega <disco> <nombre en el disco>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <disco> <nombre en el disco>\n";
        return 1;
    }

    try {
        FileSystem fs(argv[1]);
        std::string id = fs.idDe(argv[2]);
        fs.borrar(argv[2]);
        Superbloque sb = fs.superbloque();
        std::cout << "OK: " << argv[2] << " (" << id << ") borrada. Quedan "
                  << sb.bloquesLibres << " bloques y " << sb.inodosLibres << " i-nodos libres\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
