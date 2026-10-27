/**
  *   C++ class to encapsulate Unix message passing intrinsic structures and system calls
  *
  *   UCR-ECCI
  *
  *   CI0122 Sistemas Operativos 2026-ii
  *
  *   Class implementation
  *
 **/

#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "Buzon.h"

// Estructura que exige msgsnd/msgrcv.
//char texto es la linea de protocolo que se envía/recibe.
struct MsgBuf {
    long mtype;
    char texto[ TAM_TEXTO ];
};

/**
  *  Class constructor
  *
 **/
Buzon::Buzon() {
    // IPC_CREAT: si la cola con esa llave ya existe se conecta a ella.
    // Si no existe, la crea. 0666 da permiso de lectura/escritura a cualquier usuario.
    int st = msgget( KEY, IPC_CREAT | 0666 );

    if ( -1 == st ) {
        throw std::runtime_error( "Buzon::Buzon( int )" );
    }

    id = st;
    owner = getpid();
}


/**
  * Class destructor
  *
 **/
Buzon::~Buzon() {
    //Se reporta el error si no se pudo eliminar la cola pero no se elimina aquí
    int st = msgctl( id, IPC_RMID, nullptr );

    if ( -1 == st && errno != EINVAL ) {
        // EINVAL significa "la cola ya no existe"
        fprintf( stderr, "Buzon::~Buzon(): no se pudo eliminar la cola (%s)\n", strerror( errno ) );
    }
}


/**
  *  Send method
  *
  *  @param     const char * mensaje: arreglo de caracteres a enviar
  *
 **/
int Buzon::Enviar( const char * mensaje, long tipo ) {
    MsgBuf msg;
    msg.mtype = tipo;

    // strncpy + terminador: evita desbordar el buffer si "mensaje"
    // llegara a ser más largo que TAM_TEXTO 
    strncpy( msg.texto, mensaje, TAM_TEXTO - 1 );
    msg.texto[ TAM_TEXTO - 1 ] = '\0';

    // msgsnd solo necesita el tamaño de los datos que se necesitan después de mtype,
    // no el struct completo.
    size_t tam = strlen( msg.texto ) + 1;

    int st = msgsnd( id, &msg, tam, 0 );

    if ( -1 == st ) {
        throw std::runtime_error( "Buzon::Enviar( const char * )" );
    }

    return st;
}


/**
  *  Send method
  *
  *  @param     const void * mensaje: estructura con el mensaje a enviar
  *  @param int cantidad: cantidad de bytes a enviar
  *
  *
 **/
int Buzon::Enviar( const void * mensaje, int cantidad, long tipo ) {

    char * buffer = new char[ sizeof( long ) + cantidad ];
    *reinterpret_cast<long *>( buffer ) = tipo;
    memcpy( buffer + sizeof( long ), mensaje, cantidad );

    int st = msgsnd( id, buffer, cantidad, 0 );
    delete [] buffer;

    if ( -1 == st ) {
        throw std::runtime_error( "Buzon::Enviar( const void *, int, long )" );
    }

    return st;
}


/**
  *  Receive method
  *
  *  @param     const void * mensaje: estructura con el mensaje a enviar
  *  @param int cantidad: cantidad de bytes a enviar
  *
  *
 **/
int Buzon::Recibir( void * mensaje, int cantidad, long tipo ) {
    // "mensaje" debe apuntar a un struct.
    // Al pasar "tipo" > 0, msgrcv devuelve el primer mensaje de ese tipo que haya en la cola.
    // Esto sirve para solo utilizar 1 buzon compartido entre los tres hilos, y que cada quien reciba solo lo que le corresponde.
    int st = msgrcv( id, mensaje, cantidad, tipo, 0 );

    if ( -1 == st ) {
        throw std::runtime_error( "Buzon::Recibir( void *, int, long )" );
    }

    return st;
}


/**
  *  Receive with retry/timeout
  *
  *  Sirve para el caso ERR_COMM del protocolo: no se bloquea si la bodega no contesta.
  *  Se usa IPC_NOWAIT para preguntar si ya llegó algo sin bloquear, y si no ha llegado se reintenta
  *  después de una espera corta.
  *
 **/
int Buzon::RecibirConEspera( void * mensaje, int cantidad, long tipo, int intentos, int esperaMs ) {
    for ( int i = 0; i < intentos; ++i ) {
        int st = msgrcv( id, mensaje, cantidad, tipo, IPC_NOWAIT );

        if ( st != -1 ) {
            return st;             // llegó el mensaje
        }

        if ( errno != ENOMSG ) {
            // Fue un error real (no "todavía no hay mensaje")
            throw std::runtime_error( "Buzon::RecibirConEspera( void *, int, long, int, int )" );
        }

        usleep( esperaMs * 1000 );  // todavía no llega: esperar y reintentar
    }

    return -1; // quien llama debe reportar ERR_COMM
}