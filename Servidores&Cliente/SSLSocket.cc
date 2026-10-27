/**
  *  Universidad de Costa Rica
  *  ECCI
  *  CI0123 Proyecto integrador de redes y sistemas operativos
  *  2026-ii
  *  Grupos: 2 y 5
  *
  *  SSL Socket class implementation
  *
  * (Fedora version)
  *
 **/

// SSL includes
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>

#include "SSLSocket.h"


/**
  *  Class constructor
  *     use base class
  *
  *  @param     bool IPv6: if we need a IPv6 socket
  *
  *  Client side: the TLS handshake happens in Connect
  *
 **/
SSLSocket::SSLSocket( bool IPv6 ) {
   this->Init( 's', IPv6 );
   this->Context = nullptr;
   this->BIO = nullptr;
   this->InitSSL();					// Initializes to client context
}


/**
  *  Class constructor
  *     use base class
  *
  *  @param     const char * certFileName: file with the server certificate
  *  @param     const char * keyFileName: file with the private key
  *  @param     bool IPv6: if we need a IPv6 socket
  *
  *  Server side: the certificate is loaded once and shared by every accepted connection
  *
 **/
SSLSocket::SSLSocket( const char * certFileName, const char * keyFileName, bool IPv6 ) {
   this->Init( 's', IPv6 );
   this->Context = nullptr;
   this->BIO = nullptr;
   this->InitSSL( true );
   this->LoadCertificates( certFileName, keyFileName );
}


/**
  *  Class constructor
  *
  *  @param     int id: socket descriptor of an accepted connection
  *
 **/
SSLSocket::SSLSocket( int id ) {
   this->Init( id );
   this->Context = nullptr;
   this->BIO = nullptr;
}


/**
  * Class destructor
  *
 **/
SSLSocket::~SSLSocket() {
   try {
      this->Close();
   } catch ( const std::exception & ) {
   }
   if ( nullptr != this->BIO ) {
      SSL_free( reinterpret_cast<SSL *>( this->BIO ) );
   }
   if ( nullptr != this->Context ) {
      SSL_CTX_free( reinterpret_cast<SSL_CTX *>( this->Context ) );
   }
}


/**
  *  InitSSL
  *     use SSL_new with a defined context
  *
  *  Create a SSL object
  *
 **/
void SSLSocket::InitSSL( bool serverContext ) {
   this->InitContext( serverContext );
   SSL * ssl = SSL_new( reinterpret_cast<SSL_CTX *>( this->Context ) );
   if ( nullptr == ssl ) {
      throw std::runtime_error( "SSLSocket::InitSSL( SSL_new failed )" );
   }
   this->BIO = reinterpret_cast<void *>( ssl );
}


/**
  *  InitContext
  *     use TLS_server_method or TLS_client_method, SSL_CTX_new
  *
  *  Creates a new SSL context to start encrypted comunications, this context is stored in class instance
  *
 **/
void SSLSocket::InitContext( bool serverContext ) {
   const SSL_METHOD * method = serverContext ? TLS_server_method() : TLS_client_method();
   if ( nullptr == method ) {
      throw std::runtime_error( "SSLSocket::InitContext( bool ): method is null" );
   }
   SSL_CTX * context = SSL_CTX_new( method );
   if ( nullptr == context ) {
      throw std::runtime_error( "SSLSocket::InitContext( bool ): SSL_CTX_new failed" );
   }
#ifdef SSL_OP_IGNORE_UNEXPECTED_EOF
   // si el otro lado cierra sin close_notify, Read devuelve 0 como un socket normal en vez de fallar
   SSL_CTX_set_options( context, SSL_OP_IGNORE_UNEXPECTED_EOF );
#endif
   this->Context = reinterpret_cast<void *>( context );
}


/**
 *  Load certificates
 *    verify and load certificates
 *
 *  @param	const char * certFileName, file containing certificate
 *  @param	const char * keyFileName, file containing keys
 *
 *  The certificate is self signed: the channel is encrypted, but the peers do not validate each other
 *
 **/
void SSLSocket::LoadCertificates( const char * certFileName, const char * keyFileName ) {
   SSL_CTX * ctx = reinterpret_cast<SSL_CTX *>( this->Context );
   if ( SSL_CTX_use_certificate_file( ctx, certFileName, SSL_FILETYPE_PEM ) <= 0 ) {
      throw std::runtime_error( std::string( "SSLSocket::LoadCertificates(): no se pudo cargar el certificado " ) + certFileName );
   }
   if ( SSL_CTX_use_PrivateKey_file( ctx, keyFileName, SSL_FILETYPE_PEM ) <= 0 ) {
      throw std::runtime_error( std::string( "SSLSocket::LoadCertificates(): no se pudo cargar la llave " ) + keyFileName );
   }
   if ( !SSL_CTX_check_private_key( ctx ) ) {
      throw std::runtime_error( "SSLSocket::LoadCertificates(): la llave no corresponde al certificado" );
   }
}


/**
 *  Connect
 *     use SSL_connect to establish a secure conection
 *
 *  @param	char * hostName, host name
 *  @param	int port, service number
 *
 **/
int SSLSocket::Connect( const char * hostName, int port ) {
   int st = this->TryToConnect( hostName, port );		// Establish a non ssl connection first
   if ( st < 0 ) {
      return st;
   }
   SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
   SSL_set_fd( ssl, this->sockId );
   st = SSL_connect( ssl );
   if ( st != 1 ) {
      throw std::runtime_error( "SSLSocket::Connect: SSL_connect fallo (error " + std::to_string( SSL_get_error( ssl, st ) ) + ")" );
   }
   return 0;
}


/**
 *  Connect
 *     use SSL_connect to establish a secure conection
 *
 *  @param	char * hostName, host name
 *  @param	char * service, service name
 *
 **/
int SSLSocket::Connect( const char * host, const char * service ) {
   int st = this->TryToConnect( host, service );
   if ( st < 0 ) {
      return st;
   }
   SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
   SSL_set_fd( ssl, this->sockId );
   st = SSL_connect( ssl );
   if ( st != 1 ) {
      throw std::runtime_error( "SSLSocket::Connect: SSL_connect fallo (error " + std::to_string( SSL_get_error( ssl, st ) ) + ")" );
   }
   return 0;
}


/**
  *  Read
  *     use SSL_read to read data from an encrypted channel
  *
  *  @param	void * buffer to store data read
  *  @param	size_t size, buffer's capacity
  *
  *  @return	size_t byte quantity read, 0 when the other side closed the connection
  *
 **/
size_t SSLSocket::Read( void * buffer, size_t size ) {
   SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
   int st = SSL_read( ssl, buffer, static_cast<int>( size ) );
   if ( st <= 0 ) {
      int err = SSL_get_error( ssl, st );
      if ( SSL_ERROR_ZERO_RETURN == err ) {
         return 0;
      }
      throw std::runtime_error( "SSLSocket::Read(): SSL_read fallo (error " + std::to_string( err ) + ")" );
   }
   return static_cast<size_t>( st );
}


/**
  *  Write
  *     use SSL_write to write data to an encrypted channel
  *
  *  @param	const char * string to write
  *
  *  @return	size_t byte quantity written
  *
 **/
size_t SSLSocket::Write( const char * string ) {
   return this->Write( static_cast<const void *>( string ), strlen( string ) );
}


/**
  *  Write
  *     use SSL_write to write data to an encrypted channel
  *
  *  @param	const void * buffer with the data to write
  *  @param	size_t size, data size
  *
  *  @return	size_t byte quantity written
  *
 **/
size_t SSLSocket::Write( const void * buffer, size_t size ) {
   SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
   int st = SSL_write( ssl, buffer, static_cast<int>( size ) );
   if ( st <= 0 ) {
      throw std::runtime_error( "SSLSocket::Write(): SSL_write fallo (error " + std::to_string( SSL_get_error( ssl, st ) ) + ")" );
   }
   return static_cast<size_t>( st );
}


/**
  *  Close
  *     use SSL_shutdown to send close_notify, then close the descriptor
  *
 **/
void SSLSocket::Close() {
   if ( -1 != this->sockId && nullptr != this->BIO ) {
      SSL * ssl = reinterpret_cast<SSL *>( this->BIO );
      if ( SSL_is_init_finished( ssl ) ) {
         SSL_shutdown( ssl );
      }
   }
   VSocket::Close();
}


/**
 *   Show SSL certificates
 *
 **/
void SSLSocket::ShowCerts() {
   X509 * cert;
   char * line;

   cert = SSL_get_peer_certificate( (SSL *) this->BIO );	// Get certificates (if available)
   if ( nullptr != cert ) {
      printf( "Server certificates:\n" );
      line = X509_NAME_oneline( X509_get_subject_name( cert ), 0, 0 );
      printf( "Subject: %s\n", line );
      free( line );
      line = X509_NAME_oneline( X509_get_issuer_name( cert ), 0, 0 );
      printf( "Issuer: %s\n", line );
      free( line );
      X509_free( cert );
   } else {
      printf( "No certificates.\n" );
   }
}


/**
 *   Return the name of the currently used cipher
 *
 **/
const char * SSLSocket::GetCipher() {
   return SSL_get_cipher( reinterpret_cast<SSL *>( this->BIO ) );
}


/**
  *   SSLSocket::AcceptConnection
  *
  *  waits for a TLS/SSL client to initiate the TLS/SSL handshake (server side)
  *
  *  @returns	a new SSLSocket instance representing the accepted, already
  *  		SSL-handshaked connection (or nullptr if the handshake fails)
  *
 **/
VSocket * SSLSocket::AcceptConnection() {
   int newId = this->WaitForConnection();
   return this->CompletarConexion( newId );
}


/**
  *   SSLSocket::CompletarConexion
  *
  *  Completes the TLS/SSL handshake with a client that has already opened a TCP connection.
  *  It runs in the thread of each connection, so a client that never finishes the
  *  handshake does not block the others (it is dropped after 10 s)
  *
  *  @param	int newId: descriptor returned by WaitForConnection
  *
  *  @returns	the connection, or nullptr if the other side does not speak TLS
  *
 **/
VSocket * SSLSocket::CompletarConexion( int newId ) {
   struct timeval limiteTiempo;
   limiteTiempo.tv_sec = 10;
   limiteTiempo.tv_usec = 0;
   setsockopt( newId, SOL_SOCKET, SO_RCVTIMEO, &limiteTiempo, sizeof( limiteTiempo ) );
   setsockopt( newId, SOL_SOCKET, SO_SNDTIMEO, &limiteTiempo, sizeof( limiteTiempo ) );

   SSLSocket * peer = new SSLSocket( newId );

   // la conexion usa el mismo contexto (certificado) del socket que escucha
   SSL_CTX_up_ref( reinterpret_cast<SSL_CTX *>( this->Context ) );
   peer->Context = this->Context;
   SSL * ssl = SSL_new( reinterpret_cast<SSL_CTX *>( peer->Context ) );
   if ( nullptr == ssl ) {
      delete peer;
      throw std::runtime_error( "SSLSocket::CompletarConexion(): SSL_new failed" );
   }
   SSL_set_fd( ssl, peer->sockId );
   peer->BIO = reinterpret_cast<void *>( ssl );

   if ( SSL_accept( ssl ) != 1 ) {
      ERR_clear_error();
      delete peer;
      return nullptr;
   }
   return peer;
}
