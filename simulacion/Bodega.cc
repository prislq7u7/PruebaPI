#include <sstream>
#include <algorithm>
#include <iomanip>
#include "Bodega.h"
#include "Bitacora.h"

Bodega::Bodega( Buzon & buzon, int miId, const std::string & nombre,
                 const std::string & ip, int puerto,
                 std::vector<std::string> categorias, std::vector<Producto> productos )
    : buzon_( buzon ), miId_( miId ), nombre_( nombre ), ip_( ip ), puerto_( puerto ),
      categorias_( std::move( categorias ) ), productos_( std::move( productos ) ) {
}

// Se registra ante el intermediario con REGISTER_SERVER (70): publica su nombre,
// ip, puerto y las categorías que atiende. Espera el REGISTER_ACK (71).
// Devuelve false si el intermediario rechazó el registro.

bool Bodega::registrarse() {
    std::string registro = armarMensaje( idTexto( miId_ ), idTextoIntermediario(), REGISTER_SERVER,
                                          { nombre_, ip_, std::to_string( puerto_ ), unir( categorias_, ',' ) } );
    rotular( miId_, ID_INTERMEDIARIO, registro, nombre_ + " se registra y publica sus categorias" );
    buzon_.Enviar( registro.c_str(), CANAL_REGISTRO );

    struct { long mtype; char texto[ TAM_MAX_MENSAJE ]; } resp;
    buzon_.Recibir( &resp, sizeof( resp.texto ), canal( ID_INTERMEDIARIO, miId_ ) );
    std::string respuesta( resp.texto );

    if ( parsearMensaje( respuesta ).tipoMensaje != REGISTER_ACK ) {
        rotular( ID_INTERMEDIARIO, miId_, respuesta, nombre_ + ": registro rechazado, termina su hilo" );
        return false;
    }
    return true;
}

// REQUEST_CATEGORIES (10): responde con CATEGORY_LIST (11), un campo con
// las categorías separadas por coma.
void Bodega::manejarRequestCategories() {
    std::string resp = armarMensaje( idTexto( miId_ ), idTextoIntermediario(), CATEGORY_LIST, { unir( categorias_, ',' ) } );
    buzon_.Enviar( resp.c_str(), canal( miId_, ID_INTERMEDIARIO ) );
    rotular( miId_, ID_INTERMEDIARIO, resp, "categorias enviadas" );
}

// REQUEST_PRODUCTS (20): responde con PRODUCT_LIST (22): <count>/<lista>,
// o con campo "0" si la categoria no la atiende este servidor.

void Bodega::manejarRequestProducts( const std::string & categoria ) {
    bool laAtiendo = std::find( categorias_.begin(), categorias_.end(), categoria ) != categorias_.end();

    if ( !laAtiendo ) {
        std::string resp = armarMensaje( idTexto( miId_ ), idTextoIntermediario(), PRODUCT_LIST, { "0" } );
        buzon_.Enviar( resp.c_str(), canal( miId_, ID_INTERMEDIARIO ) );
        rotular( miId_, ID_INTERMEDIARIO, resp, "categoria que no me corresponde" );
        return;
    }

    // Cada entrada es producto,precio,stock y van separadas por ';'
    std::vector<std::string> entradas;
    for ( const auto & p : productos_ ) {
        if ( p.categoria != categoria ) continue;
        std::ostringstream entrada;
        entrada << p.nombre << "," << p.precio << "," << p.stock;
        entradas.push_back( entrada.str() );
    }
    std::string resp = armarMensaje( idTexto( miId_ ), idTextoIntermediario(), PRODUCT_LIST,
                                      { std::to_string( entradas.size() ), unir( entradas, ';' ) } );
    buzon_.Enviar( resp.c_str(), canal( miId_, ID_INTERMEDIARIO ) );
    rotular( miId_, ID_INTERMEDIARIO, resp, "listado de la categoria " + categoria );
}

void Bodega::manejarReserveStock( const std::string & producto, const std::string & cantidadTexto ) {
    if ( producto == "silenciar" ) {
        rotular( ID_INTERMEDIARIO, miId_, "RESERVE_STOCK " + producto,
                 "(" + nombre_ + " no va a responder: simulando falla)" );
        return;
    }

    int cantidadPedida = std::stoi( cantidadTexto );
    auto it = std::find_if( productos_.begin(), productos_.end(),
                             [ & ]( const Producto & p ) { return p.nombre == producto; } );

    float precio = 0.0f;
    int cantidadReservada = 0;
    if ( it != productos_.end() ) {
        precio = it->precio;
        cantidadReservada = std::min( cantidadPedida, it->stock );
        it->stock -= cantidadReservada;
    }

    std::ostringstream precioTexto;
    precioTexto << std::fixed << std::setprecision( 2 ) << precio;

    std::string resp = armarMensaje( idTexto( miId_ ), idTextoIntermediario(), RESERVED,
                                      { producto, precioTexto.str(), std::to_string( cantidadReservada ) } );
    buzon_.Enviar( resp.c_str(), canal( miId_, ID_INTERMEDIARIO ) );
    rotular( miId_, ID_INTERMEDIARIO, resp, "reserva de stock" );
}

void Bodega::ejecutar() {
    try {
        if ( !registrarse() ) return;
    } catch ( const std::runtime_error & ) {
        return; // el buzon fue destruido: salida de respaldo
    }

    while ( true ) {
        struct { long mtype; char texto[ TAM_MAX_MENSAJE ]; } msg;

        try {
            buzon_.Recibir( &msg, sizeof( msg.texto ), canal( ID_INTERMEDIARIO, miId_ ) );
        } catch ( const std::runtime_error & ) {
            return; // el buzon fue destruido: salida de respaldo
        }

        std::string linea( msg.texto );

        // EXIT es señal de control interna, no forma parte del protocolo.
        if ( linea == "EXIT" ) {
            return;
        }

        MensajeProtocolo mensaje = parsearMensaje( linea );

        switch ( mensaje.tipoMensaje ) {
            case REQUEST_CATEGORIES:
                manejarRequestCategories();
                break;
            case REQUEST_PRODUCTS:
                if ( !mensaje.campos.empty() ) {
                    manejarRequestProducts( mensaje.campos[ 0 ] );
                }
                break;
            case RESERVE_STOCK:
                if ( mensaje.campos.size() >= 2 ) {
                    manejarReserveStock( mensaje.campos[ 0 ], mensaje.campos[ 1 ] );
                }
                break;
            default:

                rotular( ID_INTERMEDIARIO, miId_, linea, "tipo de mensaje no reconocido (todavia)" );
                break;
        }
    }
}
