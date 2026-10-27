#include <cstdio>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>

#include "Bitacora.h"
#include "Protocolo.h"

static std::mutex mtxPantalla;

// Un evento guardado en la bitácora. "detalle" es texto libre, solo se ve localmente:
// no viaja en el mensaje 78.
struct Evento {
    std::string hora;      // HH:MM:SS
    std::string origen;
    std::string destino;
    int tipo;
    std::string estado;    // OK o código de error
    std::string detalle;
};

static std::mutex mtxEventos;
static std::vector<Evento> eventos;


void rotular( int origen, int destino, const std::string & linea, const std::string & explicacion ) {
    std::lock_guard<std::mutex> guard( mtxPantalla );
    printf( "[%-7s -> %-7s] %-45s %s\n",
            idTexto( origen ).c_str(), idTexto( destino ).c_str(), linea.c_str(), explicacion.c_str() );
}


void rotularAck( int receptor, int emisor, const std::string & explicacion ) {
    rotular( receptor, emisor, "ACK", explicacion );
}


void anotarEvento( int origen, int destino, const std::string & linea, const std::string & detalle ) {
    MensajeProtocolo mensaje = parsearMensaje( linea );
    if ( mensaje.tipoMensaje < 0 ) return; // no es un mensaje del protocolo

    std::time_t ahora = std::time( nullptr );
    std::tm local;
    localtime_r( &ahora, &local );
    char hora[ 9 ];
    std::strftime( hora, sizeof( hora ), "%H:%M:%S", &local );

    Evento evento;
    evento.hora = hora;
    evento.origen = idTexto( origen );
    evento.destino = idTexto( destino );
    evento.tipo = mensaje.tipoMensaje;
    bool esError = mensaje.tipoMensaje >= ERR_FORMAT && mensaje.tipoMensaje <= ERR_COMM;
    evento.estado = esError ? std::to_string( mensaje.tipoMensaje ) : "OK";
    evento.detalle = detalle;

    std::lock_guard<std::mutex> guard( mtxEventos );
    eventos.push_back( evento );
}


std::vector<std::string> ultimosEventos( int n ) {
    std::lock_guard<std::mutex> guard( mtxEventos );

    std::vector<std::string> resultado;
    for ( auto it = eventos.rbegin(); it != eventos.rend() && static_cast<int>( resultado.size() ) < n; ++it ) {
        std::ostringstream entrada;
        entrada << it->hora << "," << it->origen << "," << it->destino << ","
                << std::setw( 2 ) << std::setfill( '0' ) << it->tipo << "," << it->estado;
        resultado.push_back( entrada.str() );
    }
    return resultado;
}
