#ifndef FORMATOS_H
#define FORMATOS_H

#include <cstdint>

// Disco del servidor de productos: un solo archivo en Linux (por ejemplo
// fs1.dat.ci0123) de 1024 bloques de 256 bytes.
//
//   bloque 0                      superbloque
//   bloque 1                      bitmap de bloques (1 bit por bloque)
//   bloques 2 .. 17               tabla de i-nodos (64 i-nodos, 4 por bloque)
//   bloques 18 .. 1023            datos y bloques de punteros
//
// El i-nodo 0 es el directorio raiz. Cada bodega es un archivo de texto con
// una linea por producto: <id bodega>,<categoria>,<producto>,<cantidad>,<precio en centavos>

const int TAM_BLOQUE = 256;
const uint16_t NUM_BLOQUES_DISCO = 1024;
const uint16_t NUM_INODOS_DISCO = 64;
const int BITS_POR_BLOQUE = TAM_BLOQUE * 8;                     // 2048 bloques por bloque de bitmap
const int TAM_INODO = 64;
const int INODOS_POR_BLOQUE = TAM_BLOQUE / TAM_INODO;           // 4
const int TAM_ENTRADA_DIR = 32;
const int ENTRADAS_POR_BLOQUE = TAM_BLOQUE / TAM_ENTRADA_DIR;   // 8
const int PUNTEROS_POR_BLOQUE = TAM_BLOQUE / 2;                 // 128 numeros de bloque de 16 bits
const int DIRECTOS = 10;

// tamano maximo de un archivo: 10 directos + 128 del indirecto + 128*128 del doble indirecto
const uint32_t MAX_BLOQUES_ARCHIVO = DIRECTOS + PUNTEROS_POR_BLOQUE + PUNTEROS_POR_BLOQUE * PUNTEROS_POR_BLOQUE;

// el bloque 0 es el superbloque y nunca es de datos, asi que 0 sirve como "sin bloque"
const uint16_t SIN_BLOQUE = 0;
const uint16_t INODO_RAIZ = 0;

const char FIRMA_FS[8] = "CI0123F";
const uint8_t VERSION_FS = 3;

const uint8_t INODO_LIBRE = 0;
const uint8_t INODO_ARCHIVO = 1;
const uint8_t INODO_DIRECTORIO = 2;

#pragma pack(push, 1)

// bloque 0
struct Superbloque {
    char firma[8];             // "CI0123F"
    uint8_t version;
    uint8_t isla;              // isla duena del disco, 4 para nosotros
    uint8_t servidor;          // numero de servidor de productos dentro de la isla (1-9)
    uint8_t relleno0;
    uint16_t tamBloque;
    uint16_t numBloques;
    uint16_t inicioBitmap;
    uint16_t bloquesBitmap;
    uint16_t inicioInodos;
    uint16_t bloquesInodos;
    uint16_t numInodos;
    uint16_t inicioDatos;
    uint16_t bloquesLibres;
    uint16_t inodosLibres;
    uint16_t siguienteBodega;  // contador para los ids I04-S1-B01, B02, ...
    uint32_t creado;
    char nombre[32];           // descripcion del disco
    char relleno[186];
};

// cada archivo (bodega) y el directorio raiz tienen un i-nodo
struct Inodo {
    uint8_t tipo;              // INODO_LIBRE, INODO_ARCHIVO o INODO_DIRECTORIO
    uint8_t relleno0;
    uint16_t numero;           // su propio numero, para leer el dump mas facil
    uint32_t tamano;           // bytes de contenido
    uint16_t directos[DIRECTOS];
    uint16_t indirecto;        // bloque con 128 numeros de bloques de datos
    uint16_t dobleIndirecto;   // bloque con 128 numeros de bloques indirectos
    uint16_t bloquesUsados;    // datos + punteros
    uint32_t creado;
    uint32_t modificado;
    char relleno[22];
};

// el contenido del directorio raiz es una lista de estas entradas
struct EntradaDir {
    char nombre[18];           // "bodega-1.txt", vacio si la entrada esta libre
    char id[12];               // "I04-S1-B01": isla 04, servidor 1, bodega 01
    uint16_t inodo;
};

#pragma pack(pop)

static_assert(sizeof(Superbloque) == TAM_BLOQUE, "el superbloque debe medir un bloque");
static_assert(sizeof(Inodo) == TAM_INODO, "el i-nodo debe medir 64 bytes");
static_assert(sizeof(EntradaDir) == TAM_ENTRADA_DIR, "la entrada de directorio debe medir 32 bytes");

#endif
