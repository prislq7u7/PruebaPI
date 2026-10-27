#include <iostream>
#include <string>
#include "FileSystem.h"

// Formatea un disco nuevo de 1024 bloques de 256 bytes.
// Uso: mkfs <disco> <isla> <servidor> [descripcion]

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Uso: " << argv[0] << " <disco> <isla> <servidor> [descripcion]\n"
                  << "  ej: " << argv[0] << " fs1.dat.ci0123 4 1\n";
        return 1;
    }

    try {
        std::string ruta = argv[1];
        int isla = std::stoi(argv[2]);
        int servidor = std::stoi(argv[3]);
        std::string descripcion = argc > 4 ? argv[4]
                                : "TicAmazon isla " + std::to_string(isla) + " servidor " + std::to_string(servidor);

        FileSystem::formatear(ruta, (uint8_t)isla, (uint8_t)servidor, descripcion);

        FileSystem fs(ruta);
        Superbloque sb = fs.superbloque();
        std::cout << "Disco " << fs.idDisco() << " creado en " << ruta << "\n"
                  << "  " << sb.numBloques << " bloques de " << sb.tamBloque << " bytes ("
                  << sb.numBloques * sb.tamBloque / 1024 << " KB)\n"
                  << "  bitmap: bloque " << sb.inicioBitmap << "\n"
                  << "  i-nodos: bloques " << sb.inicioInodos << "-" << sb.inicioInodos + sb.bloquesInodos - 1
                  << " (" << sb.numInodos << " i-nodos)\n"
                  << "  datos desde el bloque " << sb.inicioDatos << ", " << sb.bloquesLibres << " bloques libres\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
