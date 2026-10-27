/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-ii
  *  Grupos: 2 y 5
  *
  ****** VSocket base class implementation
  *
  * (Fedora version)
  *
 **/

#include <sys/socket.h>
#include <arpa/inet.h>		// ntohs, htons
#include <stdexcept>            // runtime_error
#include <cstring>		// memset
#include <netdb.h>		// getaddrinfo, freeaddrinfo
#include <unistd.h>		// close
#include <iostream>
#include <sys/time.h>	// timeval
#include <cerrno>		// EAGAIN
/*
#include <cstddef>
#include <cstdio>

//#include <sys/types.h>
*/
#include "VSocket.h"


/**
  *  Class creator (constructor)
  *     use Unix socket system call
  *
  *  @param     char t: socket type to define
  *     's' for stream
  *     'd' for datagram
  *  @param     bool ipv6: if we need a IPv6 socket
  *
 **/
void VSocket::Init( char t, bool IPv6 ){
   //guardar en el objeto que type de socket es y si es IPv6
   this->IPv6 = IPv6;
   this->port = 0; //sin puerto asociado
   this->type = t;   // TryToConnect(host, service) usa type para elegir SOCK_STREAM o SOCK_DGRAM

   //dominio para socket ipv4 o ipv6
   int domain = this->IPv6 ? AF_INET6 : AF_INET;

   //tipo de socket : stream/datagram
   int type = ( 's' == t ) ? SOCK_STREAM : SOCK_DGRAM;

   //socket para crear el descriptor que el SO va a manejar como archivo
   int st = socket( domain, type, 0 );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::Init, socket" );
   }

   this->sockId = st;

}

void VSocket::Init( int id  ){

   // si accept() fallo en algun punto anterior, no deberiamos ni llegar aca pero por si acaso se valida.
   if ( -1 == id ) {
      throw std::runtime_error( "VSocket::Init(int), descriptor invalido" );
   }

   this->sockId = id;

   // como el socket ya viene "hecho" de afuera, asumimos que es un socket stream (TCP) IPv4, que es el unico caso que maneja este proyecto.
   this->IPv6 = false;
   this->port = 0;
   this->type = 's';

}


/**
  * Class destructor
  *
 **/
VSocket::~VSocket() {

   this->Close();

}


/**
  * Close method
  *    use Unix close system call (once opened a socket is managed like a file in Unix)
  *
 **/
void VSocket::Close(){

   if ( -1 == this->sockId ) return;
   int st = -1;

   st = close( this->sockId );
   this->sockId = -1;

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::Close()" );
   }


}

/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dot notation, example "10.84.166.62"
  * @param      int port: process address, example 80
  *
 **/
int VSocket::TryToConnect( const char * hostip, int port ) {

   int st;

   std::cerr << "Trying to connect to: " << hostip << " on port " << port << std::endl;

   if (IPv6) {
      //para IPv6
      struct sockaddr_in6  host6;
      struct sockaddr * ha;

      memset( &host6, 0, sizeof( host6 ) );
      host6.sin6_family = AF_INET6;
      st = inet_pton( AF_INET6, hostip, &host6.sin6_addr );
      if ( st <= 0 ) {	// 0 = direc. invalida, -1 = error en direc.
         throw std::runtime_error( "Socket::Connect( const char *, int ) [inet_pton]" );
      }
      host6.sin6_port = htons( port );
      ha = (struct sockaddr *) &host6;
      st = connect( this->sockId, ha, sizeof( host6 ) );
      if ( -1 == st ) {
         throw std::runtime_error( "Socket::Connect( const char *, int ) [connect]" );
      }

   } else {
      //para IPv4
      struct sockaddr_in  host4;
      memset( (char *) &host4, 0, sizeof( host4 ) );
      host4.sin_family = AF_INET;
      st = inet_pton( AF_INET, hostip, &host4.sin_addr );
      if ( st <= 0 ) {
         throw std::runtime_error( "VSocket::DoConnect, inet_pton" );
      }
      host4.sin_port = htons( port );
      st = connect( this->sockId, (sockaddr *) &host4, sizeof( host4 ) );
      if ( -1 == st ) {
         throw std::runtime_error( "VSocket::DoConnect, connect" );
      }
   }

   return 0;
}


/**
  * TryToConnect method
  *   use "connect" Unix system call
  *
  * @param      char * host: host address in dns notation, example "os.ecci.ucr.ac.cr"
  * @param      char * service: process address, example "http"
  *
 **/
int VSocket::TryToConnect( const char *host, const char *service ) {
   int st;

   struct addrinfo hints, * result;

   //le dice a getaddrinfo que adress type busca
   memset( &hints, 0, sizeof( hints ) );
   hints.ai_family = this->IPv6 ? AF_INET6 : AF_INET;
   hints.ai_socktype = ( 's' == this->type ) ? SOCK_STREAM : SOCK_DGRAM;

   //getaddrinfo para resolver host name y el servicio
   //en una direccion usable
   st = getaddrinfo( host, service, &hints, &result );
   if ( 0 != st ) {
      throw std::runtime_error( "VSocket::TryToConnect, getaddrinfo" );
   }

   //connect usando la direccion que nos devolvio getaddrinfo
   st = connect( this->sockId, result->ai_addr, result->ai_addrlen );

   //freeaddrinfo para liberar la memoria que reservo getaddrinfo
   freeaddrinfo( result );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::TryToConnect, connect" );
   }  
   
   return st;

}


/**
  * Bind method
  *    use "bind" Unix system call (man 3 bind) (server mode)
  *
  * @param      int port: bind a unamed socket to a port defined in sockaddr structure
  *
  *  Links the calling process to a service at port
  *
 **/
int VSocket::Bind( int port ) {
   struct sockaddr_in host4; //crear estructura sockaddr_in para IPv4
   host4.sin_family = AF_INET; //dominio de direcciones
   host4.sin_addr.s_addr = htonl( INADDR_ANY ); //htonl turns el valor al orden de bytes de red
   host4.sin_port = htons(port); //establecer # de puerto y convertirlo a orden de bytes de red

   memset(host4.sin_zero, '\0', sizeof (host4.sin_zero)); //rellenar con ceros

   // varios servidores de productos en la misma maquina escuchan el mismo puerto UDP del
   // descubrimiento; con SO_REUSEADDR en todos, a cada uno le llega su copia del broadcast
   int reusar = 1;
   setsockopt( this->sockId, SOL_SOCKET, SO_REUSEADDR, &reusar, sizeof( reusar ) );

   // size-host4 le dice a bind el size del address structure
   if ( -1 == bind(this->sockId, (struct sockaddr *) &host4, sizeof( host4 ) ) ) {
      //if bind returns -1 = error
      throw std::runtime_error( "VSocket::Bind" );
   }

   return 0;
}

/**
  * Bind method
  *    bind a una direccion ip especifica en lugar de INADDR_ANY
  *
  * @param      const char * hostip: ip asignada en el laboratorio 3-5
  * @param      int port: puerto del servicio
  *
 **/
int VSocket::Bind( const char * hostip, int port ) {
   struct sockaddr_in host4;
   memset( &host4, 0, sizeof( host4 ) );
   host4.sin_family = AF_INET;
   host4.sin_port = htons( port );

   if ( inet_pton( AF_INET, hostip, &host4.sin_addr ) <= 0 ) {
      throw std::runtime_error( "VSocket::Bind, inet_pton" );
   }

   // se rechaza 127.x.x.x y 0.0.0.0 porque el servidor no puede atender por loopback
   uint32_t ip = ntohl( host4.sin_addr.s_addr );
   if ( ( ip >> 24 ) == 127 || ip == 0 ) {
      throw std::runtime_error( "VSocket::Bind, no se permite loopback ni INADDR_ANY" );
   }

   // permite reiniciar el servidor sin esperar a que el puerto quede libre
   int reusar = 1;
   setsockopt( this->sockId, SOL_SOCKET, SO_REUSEADDR, &reusar, sizeof( reusar ) );

   if ( -1 == bind( this->sockId, (struct sockaddr *) &host4, sizeof( host4 ) ) ) {
      throw std::runtime_error( "VSocket::Bind" );
   }

   this->port = port;
   return 0;
}

/**
  * MarkPassive method
  *    use "listen" Unix system call (man listen) (server mode)
  *
  * @param      int backlog: defines the maximum length to which the queue of pending connections for this socket may grow
  *
  *  Establish socket queue length
  *
 **/
int VSocket::MarkPassive( int backlog ) {
   int st = listen( this->sockId, backlog );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::MarkPassive" );
   }

   return st;

}

/**
  * WaitForConnection method
  *    use "accept" Unix system call (man 3 accept) (server mode)
  *
  *
  *  Waits for a peer connections, return a sockfd of the connecting peer
  *
 **/
int VSocket::WaitForConnection( void ) {
   // pasamos NULL, NULL porque en este proyecto no interesa guardar
   // la direccion/puerto del cliente que se conecto, solo el descriptor nuevo
   int st = accept( this->sockId, NULL, NULL );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::WaitForConnection" );
   }

   return st;

}

/**
  * Shutdown method
  *    use "shutdown" Unix system call (man 3 shutdown) (server mode)
  *
  *
  *  cause all or part of a full-duplex connection on the socket associated with the file descriptor socket to be shut down
  *
 **/
int VSocket::Shutdown( int mode ) {
   int st = shutdown( this->sockId, mode );

   if ( -1 == st ) {
      throw std::runtime_error( "VSocket::Shutdown" );
   }

   return st;

}



/**
  *  sendTo method
  *
  *  @param	const void * buffer: data to send
  *  @param	size_t size data size to send
  *  @param	void * addr address to send data
  *
  *  Send data to another network point (addr) without connection (Datagram)
  *
 **/
size_t VSocket::sendTo( const void * buffer, size_t size, void * addr ) {
   struct sockaddr_in *dest = (struct sockaddr_in *)addr; //turn addr generico a sockaddr_in para IPv4
   socklen_t dest_len = sizeof(struct sockaddr_in); //get struct size 

   ssize_t sent_bytes = sendto(this->sockId, buffer, size, 0, (struct sockaddr*)dest, dest_len);
   if (sent_bytes == -1) { //if sendto returns -1 = error
      perror("sendTo error");
      throw std::runtime_error("VSocket::sendTo");
   }

   return static_cast<size_t>(sent_bytes); //turn y return el # de bytes enviados

}


/**
  *  recvFrom method
  *
  *  @param	const void * buffer: data to send
  *  @param	size_t size data size to send
  *  @param	void * addr address to receive from data
  *
  *  @return	size_t bytes received
  *
  *  Receive data from another network point (addr) without connection (Datagram)
  *
 **/
size_t VSocket::recvFrom( void * buffer, size_t size, void * addr ) {
   struct sockaddr_in *source = (struct sockaddr_in *)addr; //turn addr generico a sockaddr_in para IPv4
   socklen_t len = sizeof(struct sockaddr_in); //get struct size
   //recvfrom recibe datos de un socket 
   ssize_t received_bytes = recvfrom(this->sockId, buffer, size, 0, (struct sockaddr*)source, &len);
   if (received_bytes == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      return 0; //se vencio el tiempo de SetTimeout sin que llegara nada
   }
   if (received_bytes == -1) { //if recvfrom returns -1 = error
      perror("RecvFrom error"); 
      throw std::runtime_error("VSocket::recvFrom"); 
   }

   return static_cast<size_t>(received_bytes); //turn y return el # de bytes enviados

}


/**
  * SetTimeout method
  *    use "setsockopt" Unix system call con SO_RCVTIMEO y SO_SNDTIMEO
  *
  * @param      int segundos: tiempo maximo que esperan connect, read y write antes de fallar
  *
 **/
int VSocket::SetTimeout( int segundos ) {
   struct timeval espera;
   espera.tv_sec = segundos;
   espera.tv_usec = 0;

   // sin esto un read se queda esperando para siempre si el otro lado no responde
   if ( -1 == setsockopt( this->sockId, SOL_SOCKET, SO_RCVTIMEO, &espera, sizeof( espera ) ) ||
        -1 == setsockopt( this->sockId, SOL_SOCKET, SO_SNDTIMEO, &espera, sizeof( espera ) ) ) {
      throw std::runtime_error( "VSocket::SetTimeout" );
   }

   return 0;
}

/**
  *  HabilitarBroadcast method
  *
  *  Permite que este socket UDP mande datagramas a una direccion broadcast, sin esto sendto da permission denied
  *
 **/
int VSocket::HabilitarBroadcast() {
   int activar = 1;
   if ( -1 == setsockopt( this->sockId, SOL_SOCKET, SO_BROADCAST, &activar, sizeof( activar ) ) ) {
      throw std::runtime_error( "VSocket::HabilitarBroadcast" );
   }
   return 0;
}


/**
  *  ArmarDireccion method
  *
  *  @param	const char * ip: direccion en notacion de puntos
  *  @param	int puerto
  *  @param	void * addr: sockaddr_in donde se deja la direccion, para usar en sendTo
  *
 **/
void VSocket::ArmarDireccion( const char * ip, int puerto, void * addr ) {
   struct sockaddr_in * dest = (struct sockaddr_in *) addr;
   memset( dest, 0, sizeof( struct sockaddr_in ) );
   dest->sin_family = AF_INET;
   dest->sin_port = htons( puerto );
   if ( inet_pton( AF_INET, ip, &dest->sin_addr ) != 1 ) {
      throw std::runtime_error( "VSocket::ArmarDireccion, ip invalida" );
   }
}
