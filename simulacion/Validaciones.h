#ifndef VALIDACIONES_H
#define VALIDACIONES_H

#include <string>

bool validarOrigenDestino( const std::string & valor ); // ^(CLI|INT|SERV)_[0-9]{2}$
bool validarTipoMensaje( const std::string & valor );   // ^[0-9]{2}$
bool validarCategoria( const std::string & valor );     // ^[a-zA-Z]{3,20}$
bool validarProducto( const std::string & valor );      // ^[a-zA-Z0-9\-]{1,50}$
bool validarStock( const std::string & valor );         // ^[0-9]{1,5}$
bool validarPrecio( const std::string & valor );        // ^[0-9]{1,6}(\.[0-9]{1,2})?$
bool validarCount( const std::string & valor );         // ^[0-9]{1,3}$

// Campos del registro de un servidor (REGISTER_SERVER, 70)
bool validarNombre( const std::string & valor );        // ^[a-zA-Z0-9\-]{1,20}$
bool validarIp( const std::string & valor );            // ^([0-9]{1,3}\.){3}[0-9]{1,3}$
bool validarPuerto( const std::string & valor );        // ^[0-9]{1,5}$
bool validarListaCategorias( const std::string & valor ); // categorías separadas por coma, cada una con validarCategoria

#endif
