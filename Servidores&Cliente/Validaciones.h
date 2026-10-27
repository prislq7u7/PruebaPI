#ifndef VALIDACIONES_H
#define VALIDACIONES_H

#include <string>

// validaciones campo por campo hechas a mano, sin <regex> en tiempo de
// ejecucion; las expresiones del documento de protocolo son la referencia
// de diseno, no lo que corre en el programa
bool validarOrigenDestino(const std::string& s);    // (CLI|INT|SERV)_NN
bool validarTipoMensaje(const std::string& s);       // 2 digitos
bool validarCategoria(const std::string& s);         // 3-20 letras
bool validarProducto(const std::string& s);          // 1-50, letras/digitos/guion
bool validarPrecio(const std::string& s);            // NNNNNN(.NN)?
bool validarStock(const std::string& s);             // 1-5 digitos
bool validarDescripcion(const std::string& s);       // 0-100 chars
bool validarCount(const std::string& s);             // 1-3 digitos

// registro de servidores de productos
bool validarIdServidor(const std::string& s);        // SERV_NN
bool validarNombre(const std::string& s);            // 1-20 letras, digitos o guion
bool validarIp(const std::string& s);                // IPv4 en notacion de puntos
bool validarPuerto(const std::string& s);            // 1-65535
bool validarListaCategorias(const std::string& s);   // cat,cat,cat, hasta 195 caracteres (puede venir vacia)

#endif