
#ifndef CANALINTERNO_H
#define CANALINTERNO_H

#include <string>
#include "VSocket.h"

// lectura y escritura de un mensaje del protocolo interno sobre un socket ya conectado.
// cada mensaje es una linea terminada en '\n'; el cifrado lo pone el SSLSocket que va por debajo.
// lo usan el servidor de productos, el intermediario y consultaIntermediario

// devuelve false si la conexion se cerro sin mensaje o si la linea es demasiado larga;
// en ese caso error explica que paso
bool recibirMensaje(VSocket* conexion, std::string& mensaje, std::string& error);

void enviarMensaje(VSocket* conexion, const std::string& mensaje);

#endif
