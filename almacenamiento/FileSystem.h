#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include "Formatos.h"

// Sistema de archivos con i-nodos guardado en un solo archivo de Linux.
// No sabe nada de productos: solo crea, lee, escribe y borra archivos por
// nombre. La capa de productos (Inventario) trabaja encima de esta.
//
// Todos los errores (disco lleno, archivo que no existe, archivo corrupto,
// fallo de lectura) se reportan con std::runtime_error.
//
// Cada operacion publica toma el disco completo con un candado de archivo
// (flock), asi el servidor de productos y las herramientas (agregar_bodega,
// borrar_bodega...) pueden usar el mismo disco al mismo tiempo sin pisarse.
class FileSystem {
public:
    // crea un disco nuevo de NUM_BLOQUES_DISCO bloques, borrando lo que hubiera en esa ruta
    static void formatear(const std::string& ruta, uint8_t isla, uint8_t servidor, const std::string& nombre);

    // abre un disco existente; falla si no existe o si la firma no calza
    explicit FileSystem(const std::string& ruta);
    ~FileSystem();

    bool existe(const std::string& nombre);
    std::string crear(const std::string& nombre);   // devuelve el id asignado (I04-S1-B01)
    void borrar(const std::string& nombre);
    std::string leer(const std::string& nombre);
    void escribir(const std::string& nombre, const std::string& contenido);
    void anexar(const std::string& nombre, const std::string& texto);
    std::vector<EntradaDir> listar();
    std::string idDe(const std::string& nombre);

    // "I04-S1": isla y servidor duenos de este disco
    std::string idDisco();

    // para las herramientas que muestran la estructura del disco
    Superbloque superbloque();
    Inodo inodo(uint16_t numero);
    void bloque(uint16_t numero, void* destino);
    bool bloqueOcupado(uint16_t numero);
    std::vector<uint16_t> bloquesDatos(const Inodo& ino);
    std::vector<uint16_t> bloquesPunteros(const Inodo& ino);

    static std::string armarIdBodega(uint8_t isla, uint8_t servidor, uint16_t numero);

private:
    std::string ruta_;
    std::fstream archivo_;
    int fdCandado_;     // descriptor aparte, solo para flock
    Superbloque sb_;
    std::mutex mtx_;

    // acceso exclusivo mientras dura el objeto: mutex entre hilos, flock entre
    // procesos, y relee el superbloque porque otro proceso pudo cambiarlo
    class Acceso {
    public:
        explicit Acceso(FileSystem& fs);
        ~Acceso();
    private:
        FileSystem& fs_;
        std::lock_guard<std::mutex> lock_;
    };

    // acceso crudo a bloques
    void leerBloque(uint16_t numero, void* destino);
    void escribirBloque(uint16_t numero, const void* origen);
    void cargarSuperbloque();
    void guardarSuperbloque();

    // bitmap
    uint16_t pedirBloque();
    void liberarBloque(uint16_t numero);
    bool ocupado(uint16_t numero);

    // i-nodos
    Inodo leerInodo(uint16_t numero);
    void escribirInodo(const Inodo& ino);
    uint16_t pedirInodo(uint8_t tipo);
    void liberarInodo(Inodo& ino);

    // contenido de un archivo
    uint16_t bloqueDeDatos(Inodo& ino, uint32_t indice, bool reservar);
    std::string leerContenido(const Inodo& ino);
    void escribirContenido(Inodo& ino, const std::string& contenido);
    void escribirDesde(Inodo& ino, uint32_t desplazamiento, const std::string& datos);
    void truncar(Inodo& ino);
    static uint32_t bloquesNecesarios(uint32_t bytes);

    // directorio raiz
    std::vector<EntradaDir> leerDirectorio();
    void guardarDirectorio(const std::vector<EntradaDir>& entradas);
    int buscarEntrada(const std::vector<EntradaDir>& entradas, const std::string& nombre);
};

#endif
