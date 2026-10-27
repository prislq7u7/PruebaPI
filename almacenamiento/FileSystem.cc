#include "FileSystem.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

static uint32_t ahora() {
    return (uint32_t)std::time(nullptr);
}

static std::string nombreDeEntrada(const EntradaDir& e) {
    return std::string(e.nombre, strnlen(e.nombre, sizeof(e.nombre)));
}

// los nombres de archivo van en el directorio y en mensajes de las herramientas,
// asi que se limitan a letras, numeros, punto, guion y guion bajo
static bool nombreValido(const std::string& nombre) {
    if (nombre.empty() || nombre.size() >= sizeof(EntradaDir::nombre)) return false;
    for (char c : nombre) {
        if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_')) return false;
    }
    return true;
}

std::string FileSystem::armarIdBodega(uint8_t isla, uint8_t servidor, uint16_t numero) {
    char id[32];
    int largo = snprintf(id, sizeof(id), "I%02u-S%u-B%02u", (unsigned)isla, (unsigned)servidor, (unsigned)numero);
    if (largo >= (int)sizeof(EntradaDir::id)) {
        throw std::runtime_error(std::string("el id ") + id + " no cabe en el directorio");
    }
    return id;
}

// ---------------------------------------------------------------------------
// Formateo y apertura
// ---------------------------------------------------------------------------

void FileSystem::formatear(const std::string& ruta, uint8_t isla, uint8_t servidor, const std::string& nombre) {
    const uint16_t numBloques = NUM_BLOQUES_DISCO;
    const uint16_t numInodos = NUM_INODOS_DISCO;
    if (isla > 99) throw std::runtime_error("la isla va de 0 a 99");
    if (servidor < 1 || servidor > 9) throw std::runtime_error("el servidor va de 1 a 9");

    uint16_t bloquesBitmap = (numBloques + BITS_POR_BLOQUE - 1) / BITS_POR_BLOQUE;
    uint16_t bloquesInodos = (numInodos + INODOS_POR_BLOQUE - 1) / INODOS_POR_BLOQUE;
    uint32_t inicioDatos = 1 + bloquesBitmap + bloquesInodos;
    if (inicioDatos + 2 > numBloques) {
        throw std::runtime_error("el disco es muy pequeno para esa cantidad de i-nodos");
    }

    Superbloque sb{};
    std::memcpy(sb.firma, FIRMA_FS, sizeof(sb.firma));
    sb.version = VERSION_FS;
    sb.isla = isla;
    sb.servidor = servidor;
    sb.tamBloque = TAM_BLOQUE;
    sb.numBloques = numBloques;
    sb.inicioBitmap = 1;
    sb.bloquesBitmap = bloquesBitmap;
    sb.inicioInodos = 1 + bloquesBitmap;
    sb.bloquesInodos = bloquesInodos;
    sb.numInodos = numInodos;
    sb.inicioDatos = (uint16_t)inicioDatos;
    sb.bloquesLibres = (uint16_t)(numBloques - inicioDatos);
    sb.inodosLibres = numInodos - 1;   // el 0 es la raiz
    sb.siguienteBodega = 1;
    sb.creado = ahora();
    std::strncpy(sb.nombre, nombre.c_str(), sizeof(sb.nombre) - 1);

    std::ofstream nuevo(ruta, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!nuevo) throw std::runtime_error("no se pudo crear " + ruta);

    nuevo.write(reinterpret_cast<const char*>(&sb), TAM_BLOQUE);

    // bitmap: superbloque, bitmap e i-nodos quedan ocupados desde el inicio
    for (uint16_t b = 0; b < bloquesBitmap; b++) {
        uint8_t mapa[TAM_BLOQUE] = {0};
        for (int i = 0; i < BITS_POR_BLOQUE; i++) {
            uint32_t numero = (uint32_t)b * BITS_POR_BLOQUE + i;
            if (numero < inicioDatos) mapa[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
        }
        nuevo.write(reinterpret_cast<const char*>(mapa), TAM_BLOQUE);
    }

    // tabla de i-nodos: todos libres menos la raiz
    for (uint16_t b = 0; b < bloquesInodos; b++) {
        Inodo inodos[INODOS_POR_BLOQUE] = {};
        for (int i = 0; i < INODOS_POR_BLOQUE; i++) {
            inodos[i].numero = (uint16_t)(b * INODOS_POR_BLOQUE + i);
        }
        if (b == 0) {
            inodos[0].tipo = INODO_DIRECTORIO;
            inodos[0].creado = inodos[0].modificado = sb.creado;
        }
        nuevo.write(reinterpret_cast<const char*>(inodos), TAM_BLOQUE);
    }

    char vacio[TAM_BLOQUE] = {0};
    for (uint32_t b = inicioDatos; b < numBloques; b++) {
        nuevo.write(vacio, TAM_BLOQUE);
    }

    nuevo.close();
    if (!nuevo) throw std::runtime_error("fallo escribiendo " + ruta);
}

FileSystem::FileSystem(const std::string& ruta) : ruta_(ruta), fdCandado_(-1) {
    archivo_.open(ruta_, std::ios::in | std::ios::out | std::ios::binary);
    if (!archivo_.is_open()) {
        throw std::runtime_error("\"" + ruta_ + "\" no existe (formatealo primero con mkfs)");
    }

    archivo_.read(reinterpret_cast<char*>(&sb_), sizeof(sb_));
    if (!archivo_ || std::memcmp(sb_.firma, FIRMA_FS, sizeof(sb_.firma)) != 0) {
        throw std::runtime_error("\"" + ruta_ + "\" no es un disco CI0123F (firma incorrecta)");
    }
    if (sb_.version != VERSION_FS || sb_.tamBloque != TAM_BLOQUE) {
        throw std::runtime_error("\"" + ruta_ + "\" tiene una version o tamano de bloque distinto");
    }

    archivo_.seekg(0, std::ios::end);
    std::streamoff tamano = archivo_.tellg();
    if (tamano < (std::streamoff)sb_.numBloques * TAM_BLOQUE) {
        throw std::runtime_error("\"" + ruta_ + "\" esta truncado: mide menos de lo que dice el superbloque");
    }

    fdCandado_ = ::open(ruta_.c_str(), O_RDWR);
    if (fdCandado_ == -1) throw std::runtime_error("no se pudo abrir \"" + ruta_ + "\" para el candado");
}

FileSystem::~FileSystem() {
    if (fdCandado_ != -1) ::close(fdCandado_);
}

FileSystem::Acceso::Acceso(FileSystem& fs) : fs_(fs), lock_(fs.mtx_) {
    if (flock(fs_.fdCandado_, LOCK_EX) == -1) {
        throw std::runtime_error("no se pudo tomar el candado del disco");
    }
    try {
        fs_.cargarSuperbloque();
    } catch (...) {
        flock(fs_.fdCandado_, LOCK_UN);   // el destructor no corre si el constructor falla
        throw;
    }
}

FileSystem::Acceso::~Acceso() {
    flock(fs_.fdCandado_, LOCK_UN);
}

// ---------------------------------------------------------------------------
// Bloques
// ---------------------------------------------------------------------------

void FileSystem::leerBloque(uint16_t numero, void* destino) {
    if (numero >= sb_.numBloques) {
        throw std::runtime_error("bloque " + std::to_string(numero) + " fuera del disco");
    }
    archivo_.clear();
    archivo_.seekg((std::streamoff)numero * TAM_BLOQUE);
    archivo_.read(reinterpret_cast<char*>(destino), TAM_BLOQUE);
    if (!archivo_) {
        archivo_.clear();
        throw std::runtime_error("no se pudo leer el bloque " + std::to_string(numero));
    }
}

void FileSystem::escribirBloque(uint16_t numero, const void* origen) {
    if (numero >= sb_.numBloques) {
        throw std::runtime_error("bloque " + std::to_string(numero) + " fuera del disco");
    }
    archivo_.clear();
    archivo_.seekp((std::streamoff)numero * TAM_BLOQUE);
    archivo_.write(reinterpret_cast<const char*>(origen), TAM_BLOQUE);
    archivo_.flush();
    if (!archivo_) {
        archivo_.clear();
        throw std::runtime_error("no se pudo escribir el bloque " + std::to_string(numero));
    }
}

// otro proceso (una herramienta o el servidor) pudo haber cambiado los contadores
void FileSystem::cargarSuperbloque() {
    Superbloque leido;
    leerBloque(0, &leido);
    if (std::memcmp(leido.firma, FIRMA_FS, sizeof(leido.firma)) != 0) {
        throw std::runtime_error("\"" + ruta_ + "\" ya no tiene la firma del disco (lo reformatearon o se dano)");
    }
    sb_ = leido;
}

void FileSystem::guardarSuperbloque() {
    escribirBloque(0, &sb_);
}

// ---------------------------------------------------------------------------
// Bitmap
// ---------------------------------------------------------------------------

bool FileSystem::ocupado(uint16_t numero) {
    uint8_t mapa[TAM_BLOQUE];
    leerBloque(sb_.inicioBitmap + numero / BITS_POR_BLOQUE, mapa);
    int bit = numero % BITS_POR_BLOQUE;
    return (mapa[bit / 8] & (1 << (7 - (bit % 8)))) != 0;
}

// busca el primer bloque libre, lo marca, lo deja en ceros y devuelve su numero
uint16_t FileSystem::pedirBloque() {
    for (uint16_t b = 0; b < sb_.bloquesBitmap; b++) {
        uint8_t mapa[TAM_BLOQUE];
        leerBloque(sb_.inicioBitmap + b, mapa);
        for (int i = 0; i < BITS_POR_BLOQUE; i++) {
            uint32_t numero = (uint32_t)b * BITS_POR_BLOQUE + i;
            if (numero >= sb_.numBloques) break;
            if (mapa[i / 8] & (1 << (7 - (i % 8)))) continue;

            mapa[i / 8] |= (uint8_t)(1 << (7 - (i % 8)));
            escribirBloque(sb_.inicioBitmap + b, mapa);

            char vacio[TAM_BLOQUE] = {0};
            escribirBloque((uint16_t)numero, vacio);

            sb_.bloquesLibres--;
            guardarSuperbloque();
            return (uint16_t)numero;
        }
    }
    throw std::runtime_error("disco lleno: no quedan bloques libres");
}

void FileSystem::liberarBloque(uint16_t numero) {
    if (numero < sb_.inicioDatos || numero >= sb_.numBloques) {
        throw std::runtime_error("se intento liberar el bloque reservado " + std::to_string(numero));
    }
    uint8_t mapa[TAM_BLOQUE];
    uint16_t bloqueMapa = sb_.inicioBitmap + numero / BITS_POR_BLOQUE;
    leerBloque(bloqueMapa, mapa);
    int bit = numero % BITS_POR_BLOQUE;
    if (!(mapa[bit / 8] & (1 << (7 - (bit % 8))))) return;   // ya estaba libre
    mapa[bit / 8] &= (uint8_t)~(1 << (7 - (bit % 8)));
    escribirBloque(bloqueMapa, mapa);
    sb_.bloquesLibres++;
    guardarSuperbloque();
}

// ---------------------------------------------------------------------------
// I-nodos
// ---------------------------------------------------------------------------

Inodo FileSystem::leerInodo(uint16_t numero) {
    if (numero >= sb_.numInodos) {
        throw std::runtime_error("i-nodo " + std::to_string(numero) + " fuera de la tabla");
    }
    Inodo inodos[INODOS_POR_BLOQUE];
    leerBloque(sb_.inicioInodos + numero / INODOS_POR_BLOQUE, inodos);
    return inodos[numero % INODOS_POR_BLOQUE];
}

void FileSystem::escribirInodo(const Inodo& ino) {
    Inodo inodos[INODOS_POR_BLOQUE];
    uint16_t bloqueTabla = sb_.inicioInodos + ino.numero / INODOS_POR_BLOQUE;
    leerBloque(bloqueTabla, inodos);
    inodos[ino.numero % INODOS_POR_BLOQUE] = ino;
    escribirBloque(bloqueTabla, inodos);
}

uint16_t FileSystem::pedirInodo(uint8_t tipo) {
    for (uint16_t n = 1; n < sb_.numInodos; n++) {
        Inodo ino = leerInodo(n);
        if (ino.tipo != INODO_LIBRE) continue;

        Inodo nuevo{};
        nuevo.tipo = tipo;
        nuevo.numero = n;
        nuevo.creado = nuevo.modificado = ahora();
        escribirInodo(nuevo);
        sb_.inodosLibres--;
        guardarSuperbloque();
        return n;
    }
    throw std::runtime_error("no quedan i-nodos libres");
}

void FileSystem::liberarInodo(Inodo& ino) {
    truncar(ino);
    Inodo vacio{};
    vacio.numero = ino.numero;
    escribirInodo(vacio);
    ino = vacio;
    sb_.inodosLibres++;
    guardarSuperbloque();
}

// ---------------------------------------------------------------------------
// Contenido de los archivos
// ---------------------------------------------------------------------------

// cuantos bloques (datos + punteros) ocupa un archivo de ese tamano
uint32_t FileSystem::bloquesNecesarios(uint32_t bytes) {
    uint32_t datos = (bytes + TAM_BLOQUE - 1) / TAM_BLOQUE;
    uint32_t total = datos;
    if (datos > (uint32_t)DIRECTOS) total += 1;   // bloque indirecto
    if (datos > (uint32_t)(DIRECTOS + PUNTEROS_POR_BLOQUE)) {
        uint32_t resto = datos - DIRECTOS - PUNTEROS_POR_BLOQUE;
        total += 1 + (resto + PUNTEROS_POR_BLOQUE - 1) / PUNTEROS_POR_BLOQUE;   // doble + sus indirectos
    }
    return total;
}

// traduce el bloque logico "indice" del archivo a un bloque del disco; con
// reservar=true pide los bloques (de datos o de punteros) que hagan falta
uint16_t FileSystem::bloqueDeDatos(Inodo& ino, uint32_t indice, bool reservar) {
    if (indice < (uint32_t)DIRECTOS) {
        if (ino.directos[indice] == SIN_BLOQUE && reservar) {
            ino.directos[indice] = pedirBloque();
            ino.bloquesUsados++;
        }
        return ino.directos[indice];
    }

    indice -= DIRECTOS;
    if (indice < (uint32_t)PUNTEROS_POR_BLOQUE) {
        if (ino.indirecto == SIN_BLOQUE) {
            if (!reservar) return SIN_BLOQUE;
            ino.indirecto = pedirBloque();
            ino.bloquesUsados++;
        }
        uint16_t punteros[PUNTEROS_POR_BLOQUE];
        leerBloque(ino.indirecto, punteros);
        if (punteros[indice] == SIN_BLOQUE && reservar) {
            punteros[indice] = pedirBloque();
            ino.bloquesUsados++;
            escribirBloque(ino.indirecto, punteros);
        }
        return punteros[indice];
    }

    indice -= PUNTEROS_POR_BLOQUE;
    if (indice < (uint32_t)(PUNTEROS_POR_BLOQUE * PUNTEROS_POR_BLOQUE)) {
        if (ino.dobleIndirecto == SIN_BLOQUE) {
            if (!reservar) return SIN_BLOQUE;
            ino.dobleIndirecto = pedirBloque();
            ino.bloquesUsados++;
        }
        uint16_t nivel1[PUNTEROS_POR_BLOQUE];
        leerBloque(ino.dobleIndirecto, nivel1);
        uint32_t i1 = indice / PUNTEROS_POR_BLOQUE;
        uint32_t i2 = indice % PUNTEROS_POR_BLOQUE;
        if (nivel1[i1] == SIN_BLOQUE) {
            if (!reservar) return SIN_BLOQUE;
            nivel1[i1] = pedirBloque();
            ino.bloquesUsados++;
            escribirBloque(ino.dobleIndirecto, nivel1);
        }
        uint16_t nivel2[PUNTEROS_POR_BLOQUE];
        leerBloque(nivel1[i1], nivel2);
        if (nivel2[i2] == SIN_BLOQUE && reservar) {
            nivel2[i2] = pedirBloque();
            ino.bloquesUsados++;
            escribirBloque(nivel1[i1], nivel2);
        }
        return nivel2[i2];
    }

    throw std::runtime_error("el archivo supera el tamano maximo de un i-nodo");
}

std::string FileSystem::leerContenido(const Inodo& ino) {
    std::string contenido;
    contenido.reserve(ino.tamano);
    Inodo copia = ino;
    uint32_t bloques = (ino.tamano + TAM_BLOQUE - 1) / TAM_BLOQUE;
    for (uint32_t i = 0; i < bloques; i++) {
        uint16_t b = bloqueDeDatos(copia, i, false);
        if (b == SIN_BLOQUE) {
            throw std::runtime_error("i-nodo " + std::to_string(ino.numero) + " corrupto: le falta el bloque logico " + std::to_string(i));
        }
        char buf[TAM_BLOQUE];
        leerBloque(b, buf);
        uint32_t cuantos = std::min<uint32_t>(TAM_BLOQUE, ino.tamano - i * TAM_BLOQUE);
        contenido.append(buf, cuantos);
    }
    return contenido;
}

// escribe "datos" a partir del byte "desplazamiento", creciendo el archivo si hace falta
void FileSystem::escribirDesde(Inodo& ino, uint32_t desplazamiento, const std::string& datos) {
    uint32_t fin = desplazamiento + (uint32_t)datos.size();
    if ((fin + TAM_BLOQUE - 1) / TAM_BLOQUE > MAX_BLOQUES_ARCHIVO) {
        throw std::runtime_error("el archivo supera el tamano maximo de un i-nodo");
    }
    // se revisa el espacio antes de tocar nada, para no dejar el archivo a medias
    uint32_t necesarios = bloquesNecesarios(std::max(fin, ino.tamano));
    if (necesarios > (uint32_t)ino.bloquesUsados + sb_.bloquesLibres) {
        throw std::runtime_error("disco lleno: hacen falta " + std::to_string(necesarios - ino.bloquesUsados) +
                                 " bloques y quedan " + std::to_string(sb_.bloquesLibres));
    }

    size_t escritos = 0;
    while (escritos < datos.size()) {
        uint32_t posicion = desplazamiento + (uint32_t)escritos;
        uint32_t indice = posicion / TAM_BLOQUE;
        uint32_t offset = posicion % TAM_BLOQUE;
        size_t cuantos = std::min<size_t>(TAM_BLOQUE - offset, datos.size() - escritos);

        uint16_t b = bloqueDeDatos(ino, indice, true);
        char buf[TAM_BLOQUE];
        leerBloque(b, buf);
        std::memcpy(buf + offset, datos.data() + escritos, cuantos);
        escribirBloque(b, buf);
        escritos += cuantos;
    }

    if (fin > ino.tamano) ino.tamano = fin;
    ino.modificado = ahora();
    escribirInodo(ino);
}

// devuelve todos los bloques del archivo al bitmap
void FileSystem::truncar(Inodo& ino) {
    for (int i = 0; i < DIRECTOS; i++) {
        if (ino.directos[i] != SIN_BLOQUE) liberarBloque(ino.directos[i]);
        ino.directos[i] = SIN_BLOQUE;
    }
    if (ino.indirecto != SIN_BLOQUE) {
        uint16_t punteros[PUNTEROS_POR_BLOQUE];
        leerBloque(ino.indirecto, punteros);
        for (uint16_t p : punteros) if (p != SIN_BLOQUE) liberarBloque(p);
        liberarBloque(ino.indirecto);
        ino.indirecto = SIN_BLOQUE;
    }
    if (ino.dobleIndirecto != SIN_BLOQUE) {
        uint16_t nivel1[PUNTEROS_POR_BLOQUE];
        leerBloque(ino.dobleIndirecto, nivel1);
        for (uint16_t p1 : nivel1) {
            if (p1 == SIN_BLOQUE) continue;
            uint16_t nivel2[PUNTEROS_POR_BLOQUE];
            leerBloque(p1, nivel2);
            for (uint16_t p2 : nivel2) if (p2 != SIN_BLOQUE) liberarBloque(p2);
            liberarBloque(p1);
        }
        liberarBloque(ino.dobleIndirecto);
        ino.dobleIndirecto = SIN_BLOQUE;
    }
    ino.tamano = 0;
    ino.bloquesUsados = 0;
    ino.modificado = ahora();
    escribirInodo(ino);
}

// reemplaza todo el contenido del archivo
void FileSystem::escribirContenido(Inodo& ino, const std::string& contenido) {
    uint32_t necesarios = bloquesNecesarios((uint32_t)contenido.size());
    if (necesarios > (uint32_t)ino.bloquesUsados + sb_.bloquesLibres) {
        throw std::runtime_error("disco lleno: hacen falta " + std::to_string(necesarios) +
                                 " bloques y hay " + std::to_string(ino.bloquesUsados + sb_.bloquesLibres));
    }
    truncar(ino);
    escribirDesde(ino, 0, contenido);
}

// ---------------------------------------------------------------------------
// Directorio raiz
// ---------------------------------------------------------------------------

std::vector<EntradaDir> FileSystem::leerDirectorio() {
    Inodo raiz = leerInodo(INODO_RAIZ);
    std::string contenido = leerContenido(raiz);
    std::vector<EntradaDir> entradas(contenido.size() / TAM_ENTRADA_DIR);
    if (!entradas.empty()) std::memcpy(entradas.data(), contenido.data(), entradas.size() * TAM_ENTRADA_DIR);
    return entradas;
}

void FileSystem::guardarDirectorio(const std::vector<EntradaDir>& entradas) {
    Inodo raiz = leerInodo(INODO_RAIZ);
    std::string contenido(reinterpret_cast<const char*>(entradas.data()), entradas.size() * TAM_ENTRADA_DIR);
    escribirContenido(raiz, contenido);
}

int FileSystem::buscarEntrada(const std::vector<EntradaDir>& entradas, const std::string& nombre) {
    for (size_t i = 0; i < entradas.size(); i++) {
        if (entradas[i].nombre[0] != '\0' && nombreDeEntrada(entradas[i]) == nombre) return (int)i;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Operaciones publicas
// ---------------------------------------------------------------------------

bool FileSystem::existe(const std::string& nombre) {
    Acceso acceso(*this);
    return buscarEntrada(leerDirectorio(), nombre) != -1;
}

std::string FileSystem::crear(const std::string& nombre) {
    Acceso acceso(*this);
    if (!nombreValido(nombre)) {
        throw std::runtime_error("nombre de archivo invalido: \"" + nombre + "\" (1-17 caracteres: letras, numeros, . - _)");
    }

    std::vector<EntradaDir> entradas = leerDirectorio();
    if (buscarEntrada(entradas, nombre) != -1) {
        throw std::runtime_error("ya existe un archivo llamado " + nombre);
    }

    uint16_t numInodo = pedirInodo(INODO_ARCHIVO);
    std::string id = armarIdBodega(sb_.isla, sb_.servidor, sb_.siguienteBodega);

    EntradaDir nueva{};
    std::strncpy(nueva.nombre, nombre.c_str(), sizeof(nueva.nombre) - 1);
    std::strncpy(nueva.id, id.c_str(), sizeof(nueva.id) - 1);
    nueva.inodo = numInodo;

    // se reutiliza una entrada borrada si hay alguna, si no el directorio crece
    int libre = -1;
    for (size_t i = 0; i < entradas.size(); i++) {
        if (entradas[i].nombre[0] == '\0') { libre = (int)i; break; }
    }
    if (libre == -1) entradas.push_back(nueva);
    else entradas[libre] = nueva;

    try {
        guardarDirectorio(entradas);
    } catch (...) {
        Inodo ino = leerInodo(numInodo);
        liberarInodo(ino);
        throw;
    }

    sb_.siguienteBodega++;
    guardarSuperbloque();
    return id;
}

void FileSystem::borrar(const std::string& nombre) {
    Acceso acceso(*this);
    std::vector<EntradaDir> entradas = leerDirectorio();
    int pos = buscarEntrada(entradas, nombre);
    if (pos == -1) throw std::runtime_error("no existe el archivo " + nombre);

    Inodo ino = leerInodo(entradas[pos].inodo);
    liberarInodo(ino);
    entradas[pos] = EntradaDir{};
    // las entradas vacias del final se recortan, asi el directorio tambien devuelve sus bloques
    while (!entradas.empty() && entradas.back().nombre[0] == '\0') entradas.pop_back();
    guardarDirectorio(entradas);
}

std::string FileSystem::leer(const std::string& nombre) {
    Acceso acceso(*this);
    std::vector<EntradaDir> entradas = leerDirectorio();
    int pos = buscarEntrada(entradas, nombre);
    if (pos == -1) throw std::runtime_error("no existe el archivo " + nombre);
    return leerContenido(leerInodo(entradas[pos].inodo));
}

void FileSystem::escribir(const std::string& nombre, const std::string& contenido) {
    Acceso acceso(*this);
    std::vector<EntradaDir> entradas = leerDirectorio();
    int pos = buscarEntrada(entradas, nombre);
    if (pos == -1) throw std::runtime_error("no existe el archivo " + nombre);
    Inodo ino = leerInodo(entradas[pos].inodo);
    escribirContenido(ino, contenido);
}

void FileSystem::anexar(const std::string& nombre, const std::string& texto) {
    Acceso acceso(*this);
    std::vector<EntradaDir> entradas = leerDirectorio();
    int pos = buscarEntrada(entradas, nombre);
    if (pos == -1) throw std::runtime_error("no existe el archivo " + nombre);
    Inodo ino = leerInodo(entradas[pos].inodo);
    escribirDesde(ino, ino.tamano, texto);
}

std::vector<EntradaDir> FileSystem::listar() {
    Acceso acceso(*this);
    std::vector<EntradaDir> ocupadas;
    for (auto& e : leerDirectorio()) {
        if (e.nombre[0] != '\0') ocupadas.push_back(e);
    }
    return ocupadas;
}

std::string FileSystem::idDe(const std::string& nombre) {
    Acceso acceso(*this);
    std::vector<EntradaDir> entradas = leerDirectorio();
    int pos = buscarEntrada(entradas, nombre);
    if (pos == -1) throw std::runtime_error("no existe el archivo " + nombre);
    return std::string(entradas[pos].id, strnlen(entradas[pos].id, sizeof(entradas[pos].id)));
}

std::string FileSystem::idDisco() {
    Acceso acceso(*this);
    char id[16];
    snprintf(id, sizeof(id), "I%02u-S%u", (unsigned)sb_.isla, (unsigned)sb_.servidor);
    return id;
}

Superbloque FileSystem::superbloque() {
    Acceso acceso(*this);
    return sb_;
}

Inodo FileSystem::inodo(uint16_t numero) {
    Acceso acceso(*this);
    return leerInodo(numero);
}

void FileSystem::bloque(uint16_t numero, void* destino) {
    Acceso acceso(*this);
    leerBloque(numero, destino);
}

bool FileSystem::bloqueOcupado(uint16_t numero) {
    Acceso acceso(*this);
    return ocupado(numero);
}

std::vector<uint16_t> FileSystem::bloquesDatos(const Inodo& ino) {
    Acceso acceso(*this);
    std::vector<uint16_t> bloques;
    Inodo copia = ino;
    uint32_t cuantos = (ino.tamano + TAM_BLOQUE - 1) / TAM_BLOQUE;
    for (uint32_t i = 0; i < cuantos; i++) bloques.push_back(bloqueDeDatos(copia, i, false));
    return bloques;
}

std::vector<uint16_t> FileSystem::bloquesPunteros(const Inodo& ino) {
    Acceso acceso(*this);
    std::vector<uint16_t> bloques;
    if (ino.indirecto != SIN_BLOQUE) bloques.push_back(ino.indirecto);
    if (ino.dobleIndirecto != SIN_BLOQUE) {
        bloques.push_back(ino.dobleIndirecto);
        uint16_t nivel1[PUNTEROS_POR_BLOQUE];
        leerBloque(ino.dobleIndirecto, nivel1);
        for (uint16_t p : nivel1) if (p != SIN_BLOQUE) bloques.push_back(p);
    }
    return bloques;
}
