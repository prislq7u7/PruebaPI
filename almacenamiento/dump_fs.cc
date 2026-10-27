#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "FileSystem.h"

// Muestra la estructura interna del disco: superbloque, bitmap, i-nodos con
// sus bloques y el directorio. Con "bloque N" muestra ese bloque en hexadecimal.
// Uso: dump_fs <disco> [bloque N]

// "19-26, 30, 41-43" en lugar de la lista completa
static std::string rangos(const std::vector<uint16_t>& bloques) {
    std::string texto;
    size_t i = 0;
    while (i < bloques.size()) {
        size_t j = i;
        while (j + 1 < bloques.size() && bloques[j + 1] == bloques[j] + 1) j++;
        if (!texto.empty()) texto += ", ";
        texto += std::to_string(bloques[i]);
        if (j > i) texto += "-" + std::to_string(bloques[j]);
        i = j + 1;
    }
    return texto.empty() ? "-" : texto;
}

static std::string fecha(uint32_t segundos) {
    time_t t = segundos;
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&t));
    return buf;
}

static void volcarBloque(FileSystem& fs, uint16_t numero) {
    unsigned char buf[TAM_BLOQUE];
    fs.bloque(numero, buf);
    std::cout << "Bloque " << numero << " (offset 0x" << std::hex << numero * TAM_BLOQUE << std::dec
              << ", " << (fs.bloqueOcupado(numero) ? "ocupado" : "libre") << ")\n";
    for (int fila = 0; fila < TAM_BLOQUE; fila += 16) {
        printf("%08x  ", numero * TAM_BLOQUE + fila);
        for (int i = 0; i < 16; i++) printf("%02x%s", buf[fila + i], i == 7 ? "  " : " ");
        printf(" |");
        for (int i = 0; i < 16; i++) putchar(isprint(buf[fila + i]) ? buf[fila + i] : '.');
        printf("|\n");
    }
}

int main(int argc, char** argv) {
    if (argc != 2 && !(argc == 4 && std::string(argv[2]) == "bloque")) {
        std::cerr << "Uso: " << argv[0] << " <disco> [bloque N]\n";
        return 1;
    }

    try {
        FileSystem fs(argv[1]);

        if (argc == 4) {
            volcarBloque(fs, (uint16_t)std::stoi(argv[3]));
            return 0;
        }

        Superbloque sb = fs.superbloque();
        std::cout << "== Superbloque (bloque 0) ==\n"
                  << "  firma " << std::string(sb.firma, strnlen(sb.firma, sizeof(sb.firma)))
                  << " v" << (int)sb.version << ", disco " << fs.idDisco()
                  << ", \"" << std::string(sb.nombre, strnlen(sb.nombre, sizeof(sb.nombre))) << "\"\n"
                  << "  creado " << fecha(sb.creado) << "\n"
                  << "  " << sb.numBloques << " bloques x " << sb.tamBloque << " B, libres " << sb.bloquesLibres << "\n"
                  << "  bitmap en bloque(s) " << sb.inicioBitmap << "-" << sb.inicioBitmap + sb.bloquesBitmap - 1 << "\n"
                  << "  i-nodos en bloques " << sb.inicioInodos << "-" << sb.inicioInodos + sb.bloquesInodos - 1
                  << " (" << sb.numInodos << " i-nodos, libres " << sb.inodosLibres << ")\n"
                  << "  datos desde el bloque " << sb.inicioDatos << "\n"
                  << "  siguiente id de bodega: "
                  << FileSystem::armarIdBodega(sb.isla, sb.servidor, sb.siguienteBodega) << "\n";

        std::vector<uint16_t> ocupados;
        for (uint16_t b = 0; b < sb.numBloques; b++) if (fs.bloqueOcupado(b)) ocupados.push_back(b);
        std::cout << "\n== Bitmap ==\n  ocupados (" << ocupados.size() << "): " << rangos(ocupados) << "\n";

        std::cout << "\n== I-nodos en uso ==\n";
        for (uint16_t n = 0; n < sb.numInodos; n++) {
            Inodo ino = fs.inodo(n);
            if (ino.tipo == INODO_LIBRE) continue;
            std::cout << "  i-nodo " << std::setw(3) << n << "  "
                      << (ino.tipo == INODO_DIRECTORIO ? "directorio" : "archivo   ")
                      << "  " << std::setw(6) << ino.tamano << " B  " << std::setw(3) << ino.bloquesUsados << " bloques"
                      << "  modificado " << fecha(ino.modificado) << "\n"
                      << "      datos:    " << rangos(fs.bloquesDatos(ino)) << "\n";
            if (ino.indirecto != SIN_BLOQUE || ino.dobleIndirecto != SIN_BLOQUE) {
                std::cout << "      punteros: " << rangos(fs.bloquesPunteros(ino))
                          << " (indirecto " << ino.indirecto << ", doble " << ino.dobleIndirecto << ")\n";
            }
        }

        std::cout << "\n== Directorio (i-nodo 0) ==\n"
                  << "  " << std::left << std::setw(18) << "nombre" << std::setw(12) << "id" << "i-nodo\n";
        for (auto& e : fs.listar()) {
            std::cout << "  " << std::setw(18) << std::string(e.nombre, strnlen(e.nombre, sizeof(e.nombre)))
                      << std::setw(12) << std::string(e.id, strnlen(e.id, sizeof(e.id))) << e.inodo << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
