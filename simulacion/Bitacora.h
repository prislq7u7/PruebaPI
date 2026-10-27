#ifndef BITACORA_H
#define BITACORA_H

#include <string>
#include <vector>

// Imprime una línea con el formato [origen -> destino] mensaje y explicación.
// origen/destino son los IDs internos (ID_CLIENTE, ID_INTERMEDIARIO, ID de servidor);
// rotular() los traduce a los IDs de texto del protocolo (CLI_04, INT_04, SERV_01)
// antes de imprimirlos. Internamente usa un mutex para que los rótulos de los
// distintos hilos no se mezclen.
void rotular( int origen, int destino, const std::string & linea, const std::string & explicacion );

// Imprime el ACK con el que el receptor confirma que le llegó un mensaje. En vez de repetir
// el mensaje completo en pantalla (ya lo rotuló el emisor), el receptor imprime "ACK" en la
// dirección contraria: [receptor -> emisor]. Es solo una confirmación en la bitácora de
// pantalla, no un mensaje del protocolo ni viaja por el buzón.
void rotularAck( int receptor, int emisor, const std::string & explicacion );

// Guarda un evento en la bitácora del intermediario (en memoria), con la hora actual.
// "linea" debe ser un mensaje del protocolo; si no se puede interpretar (por ejemplo
// la señal interna EXIT) no se guarda. El estado es "OK" o el código de error (90, 91, 92).
void anotarEvento( int origen, int destino, const std::string & linea, const std::string & detalle );

// Los últimos n eventos guardados, del más reciente al más antiguo. Cada entrada
// tiene el formato que viaja en el mensaje 78: hora,origen,destino,tipo,estado
std::vector<std::string> ultimosEventos( int n );

#endif
