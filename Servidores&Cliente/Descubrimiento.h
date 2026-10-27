#ifndef DESCUBRIMIENTO_H
#define DESCUBRIMIENTO_H

#include <string>
#include <functional>

#include "Bitacora.h"

// descubrimiento por broadcast (mensajes 80 al 83). No se conoce la direccion del otro, asi que no
// hay conexion TCP: se manda un datagrama a la direccion de broadcast de la red y cada lado tiene un
// hilo esperando (recvFrom) las solicitudes de su contraparte. La respuesta va directo a quien pregunto

// intermediario: hilo que contesta cada 80 con un 81 que lleva su puerto de registro
void escucharServidores(int puertoRegistro, Bitacora* bitacora);

// intermediario: manda un 82 para que los servidores de productos que ya estaban corriendo se registren
void llamarServidores(const std::string& ipBroadcast, int puertoRegistro, Bitacora* bitacora);

// servidor de productos: manda el 80 y espera el 81; deja en ip la direccion de quien contesto
bool buscarIntermediario(const std::string& miId, const std::string& ipBroadcast, int intentos,
                         std::string& ip, int& puertoRegistro, Bitacora* bitacora);

// servidor de productos: hilo que espera el 82, contesta 83 y llama alSolicitar(ip, puertoRegistro)
void escucharIntermediario(const std::string& miId, int puerto, Bitacora* bitacora,
                           std::function<void(const std::string&, int)> alSolicitar);

// direccion de broadcast de la red de esa ip (172.16.123.67 -> 172.16.123.79); "" si no se encuentra
std::string broadcastDe(const std::string& ip);

#endif
