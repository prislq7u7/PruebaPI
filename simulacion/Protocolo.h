#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

#include "Buzon.h"


// IDs INTERNOS (para el enrutamiento por Buzon / colas de mensajes)

const int ID_CLIENTE         = 1;
const int ID_INTERMEDIARIO   = 2;

// Los servidores de productos usan los ids 3, 4, 5... (SERV_01, SERV_02, ...).
// canal() calcula origen*10+destino, por eso el id mas alto es 9 (hasta 7 servidores).
const int ID_PRIMER_SERVIDOR = 3;
const int ID_MAX_SERVIDOR    = 9;

// El mtype identifica el CANAL, quién habla con quién, no solo
// el destinatario. Se calcula como origen*10 + destino. Así:
//   Cliente -> Intermediario        = 12
//   Intermediario -> Cliente        = 21
//   Intermediario -> SERV_01 (id 3) = 23
//   SERV_01 -> Intermediario        = 32
//   Intermediario -> SERV_02 (id 4) = 24
//   SERV_02 -> Intermediario        = 42

inline long canal( int origen, int destino ) {
    return origen * 10 + destino;
}

// Canal por el que los servidores de productos le mandan el REGISTER_SERVER (70)
// al intermediario, en cualquier momento de la ejecución.
const long CANAL_REGISTRO = 99;

// Tamaño del buffer de un mensaje en esta simulación (el mismo del Buzon). No es un límite del
// protocolo: un mensaje que no cabe se rechaza con ERR_SIZE (91).
const int TAM_MAX_MENSAJE = TAM_TEXTO;

// IDs DE TEXTO DEL PROTOCOLO TICAMAZON

const std::string NUM_ISLA = "04";

inline std::string idTextoCliente()       { return "CLI_" + NUM_ISLA; }
inline std::string idTextoIntermediario() { return "INT_" + NUM_ISLA; }

// ID interno de un servidor de productos -> "SERV_NN" (id 3 -> SERV_01, id 4 -> SERV_02, ...).
inline std::string idTextoServidor( int idInterno ) {
    std::ostringstream oss;
    oss << "SERV_" << std::setw( 2 ) << std::setfill( '0' ) << ( idInterno - ID_PRIMER_SERVIDOR + 1 );
    return oss.str();
}

// "SERV_NN" -> ID interno. Devuelve -1 si el texto no es un servidor válido.
inline int idInternoServidor( const std::string & texto ) {
    if ( texto.size() != 7 || texto.compare( 0, 5, "SERV_" ) != 0 ) return -1;
    if ( !std::isdigit( static_cast<unsigned char>( texto[ 5 ] ) ) ||
         !std::isdigit( static_cast<unsigned char>( texto[ 6 ] ) ) ) return -1;
    int nn = ( texto[ 5 ] - '0' ) * 10 + ( texto[ 6 ] - '0' );
    int id = nn + ID_PRIMER_SERVIDOR - 1;
    if ( nn < 1 || id > ID_MAX_SERVIDOR ) return -1;
    return id;
}

// Traduce un ID interno al identificador de texto que exige el protocolo
// (CLI_04, INT_04, SERV_01...). Útil para armar mensajes y para los rótulos en Bitacora.
inline std::string idTexto( int idInterno ) {
    if ( idInterno == ID_CLIENTE )       return idTextoCliente();
    if ( idInterno == ID_INTERMEDIARIO ) return idTextoIntermediario();
    if ( idInterno >= ID_PRIMER_SERVIDOR ) return idTextoServidor( idInterno );
    return "?";
}

// tabla del protocolo)
enum TipoMensaje {
    REQUEST_CATEGORIES = 10,
    CATEGORY_LIST      = 11,
    REQUEST_PRODUCTS   = 20,
    PRODUCT_LIST       = 22,

    PRODUCT_LIST_EMPTY = 23,
    ADD_TO_CART        = 30,

    CART_UPDATED       = 32,
    REQUEST_FACTURA    = 40,
    FACTURA            = 41,

    CART_EMPTY         = 42,
    RESERVE_STOCK      = 50,
    RESERVED           = 51,

    // Tercera etapa: registro dinámico de servidores de productos
    REGISTER_SERVER    = 70,   // SERV_NN -> INT: nombre, ip, puerto, categorias
    REGISTER_ACK       = 71,   // INT -> SERV_NN: cantidad de categorias registradas

    // Tercera etapa: consultas al intermediario
    REQUEST_SERVERS    = 73,
    SERVER_LIST        = 74,   // <count>/<SERV_NN,nombre,ip,puerto;...>
    REQUEST_ROUTES     = 75,
    ROUTE_TABLE        = 76,   // <count>/<categoria,SERV_NN;...>
    REQUEST_LOG        = 77,   // <n>: los ultimos n eventos
    LOG_LIST           = 78,   // <count>/<hora,origen,destino,tipo,estado;...>

    ERR_FORMAT         = 90,
    ERR_SIZE           = 91,
    ERR_COMM           = 92
};

// UTILIDADES DE TEXTO

// Parte "a,b,c" por el separador. Los campos vacíos se descartan.
inline std::vector<std::string> separar( const std::string & texto, char separador ) {
    std::vector<std::string> partes;
    std::istringstream iss( texto );
    std::string parte;
    while ( std::getline( iss, parte, separador ) ) {
        if ( !parte.empty() ) partes.push_back( parte );
    }
    return partes;
}

// Une las partes con el separador: { "a", "b" }, ',' -> "a,b".
inline std::string unir( const std::vector<std::string> & partes, char separador ) {
    std::string resultado;
    for ( size_t i = 0; i < partes.size(); ++i ) {
        if ( i > 0 ) resultado += separador;
        resultado += partes[ i ];
    }
    return resultado;
}

// Las categorías se comparan sin distinguir mayúsculas de minúsculas.
inline std::string aMinusculas( std::string texto ) {
    std::transform( texto.begin(), texto.end(), texto.begin(),
                    []( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
    return texto;
}

// ARMAR / PARSEAR mensajes con el formato ORIGEN|DESTINO/TIPO/campo1/...

// Construye la línea de protocolo a partir de sus partes.
// Ej: armarMensaje( idTextoCliente(), idTextoIntermediario(), REQUEST_PRODUCTS, { "postres" } )
//     -> "CLI_04|INT_04/20/postres"
inline std::string armarMensaje( const std::string & origen, const std::string & destino,
                                  int tipoMensaje, const std::vector<std::string> & campos = {} ) {
    std::ostringstream oss;
    oss << origen << "|" << destino << "/"
        << std::setw( 2 ) << std::setfill( '0' ) << tipoMensaje;
    for ( const auto & campo : campos ) {
        oss << "/" << campo;
    }
    return oss.str();
}

// Mensaje ya separado en sus componentes, para que cada clase no tenga que hacer su propio parseo
struct MensajeProtocolo {
    std::string origen;
    std::string destino;
    int tipoMensaje = -1;
    std::vector<std::string> campos;
};

// Descompone una línea "ORIGEN|DESTINO/TIPO/campo1/campo2/..." en sus partes.
// No valida regex/tamaños (eso lo sigue haciendo Validaciones.cc); si la
// línea viene mal formada, tipoMensaje queda en -1 para que quien llama lo
// detecte.
inline MensajeProtocolo parsearMensaje( const std::string & linea ) {
    MensajeProtocolo m;

    auto posPipe = linea.find( '|' );
    if ( posPipe == std::string::npos ) return m; // mal formado

    m.origen = linea.substr( 0, posPipe );
    std::string resto = linea.substr( posPipe + 1 ); // DESTINO/TIPO/campo1/...

    std::vector<std::string> partes;
    std::istringstream iss( resto );
    std::string parte;
    while ( std::getline( iss, parte, '/' ) ) {
        partes.push_back( parte );
    }
    if ( partes.size() < 2 ) return m; // falta destino o tipo

    m.destino = partes[ 0 ];
    try {
        m.tipoMensaje = std::stoi( partes[ 1 ] );
    } catch ( const std::exception & ) {
        m.tipoMensaje = -1;
        return m;
    }
    for ( size_t i = 2; i < partes.size(); ++i ) {
        m.campos.push_back( partes[ i ] );
    }
    return m;
}

// Un producto de un servidor de productos.

struct Producto {
    std::string nombre;
    float precio;
    int stock;
    std::string descripcion;
    std::string categoria;
};

#endif