#include <fstream>
#include <iostream>
#include <string>
#include "FileSystem.h"
#include "Inventario.h"

// Saca una bodega del disco a un archivo de texto de Linux (o a la pantalla).
// Uso: extraer_bodega <disco> <nombre en el disco> [salida.txt]

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <disco> <nombre en el disco> [salida.txt]\n";
        return 1;
    }

    try {
        FileSystem fs(argv[1]);
        Inventario inventario(fs);
        std::string contenido = inventario.exportarBodega(argv[2]);

        if (argc < 4) {
            std::cout << contenido;
            return 0;
        }

        std::ofstream salida(argv[3], std::ios::binary);
        if (!salida) throw std::runtime_error(std::string("no se pudo crear ") + argv[3]);
        salida << contenido;
        std::cout << "OK: " << argv[2] << " (" << fs.idDe(argv[2]) << ") -> " << argv[3]
                  << " (" << contenido.size() << " bytes)\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
