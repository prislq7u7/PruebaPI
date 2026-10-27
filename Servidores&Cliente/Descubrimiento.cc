#include "Descubrimiento.h"
#include "Socket.h"
#include "Protocolo.h"

#include <cstdlib>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static const int SEGUNDOS_POR_INTENTO = 1;

static std::string ipDe(const struct sockaddr_in& direccion) {
    char texto[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &direccion.sin_addr, texto, sizeof(texto));
    return texto;
}

// espera un datagrama; "" si se vence el tiempo
static std::string recibir(Socket& udp, struct sockaddr_in& origen) {
    char buffer[512];
    size_t leidos = udp.recvFrom(buffer, sizeof(buffer) - 1, &origen);
    return std::string(buffer, leidos);
}

static void enviar(Socket& udp, const std::string& mensaje, struct sockaddr_in& destino) {
    udp.sendTo(mensaje.data(), mensaje.size(), &destino);
}

void escucharServidores(int puertoRegistro, Bitacora* bitacora) {
    try {
        Socket udp('d');
        udp.Bind(PUERTO_UDP_INTERMEDIARIO);
        for (;;) {
            struct sockaddr_in origen;
            std::string datagrama = recibir(udp, origen);
            if (datagrama.empty()) continue;

            Mensaje msg;
            try { msg = parsearMensaje(datagrama); } catch (const std::exception&) { continue; }
            if (msg.tipo != TIPO_BUSCO_INTERMEDIARIO) continue;

            bitacora->mensaje(datagrama, "broadcast desde " + ipDe(origen));
            std::string respuesta = construirMensaje(ID_INTERMEDIARIO, msg.origen, TIPO_SOY_INTERMEDIARIO,
                                                     {std::to_string(puertoRegistro)});
            enviar(udp, respuesta, origen);
            bitacora->mensaje(respuesta, "a " + ipDe(origen));
        }
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("descubrimiento: no se pudo escuchar el puerto UDP ") +
                            std::to_string(PUERTO_UDP_INTERMEDIARIO) + ": " + e.what());
    }
}

void llamarServidores(const std::string& ipBroadcast, int puertoRegistro, Bitacora* bitacora) {
    try {
        Socket udp('d');
        udp.HabilitarBroadcast();
        struct sockaddr_in destino;
        VSocket::ArmarDireccion(ipBroadcast.c_str(), PUERTO_UDP_SERVIDOR, &destino);
        std::string mensaje = construirMensaje(ID_INTERMEDIARIO, ID_BROADCAST, TIPO_BUSCO_SERVIDORES,
                                               {std::to_string(puertoRegistro)});
        enviar(udp, mensaje, destino);
        bitacora->mensaje(mensaje, "broadcast a " + ipBroadcast);
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("descubrimiento: no se pudo mandar el broadcast a ") + ipBroadcast + ": " + e.what());
    }
}

bool buscarIntermediario(const std::string& miId, const std::string& ipBroadcast, int intentos,
                         std::string& ip, int& puertoRegistro, Bitacora* bitacora) {
    try {
        Socket udp('d');
        udp.HabilitarBroadcast();
        udp.SetTimeout(SEGUNDOS_POR_INTENTO);
        struct sockaddr_in destino;
        VSocket::ArmarDireccion(ipBroadcast.c_str(), PUERTO_UDP_INTERMEDIARIO, &destino);

        std::string mensaje = construirMensaje(miId, ID_BROADCAST, TIPO_BUSCO_INTERMEDIARIO, {});
        for (int intento = 1; intento <= intentos; intento++) {
            enviar(udp, mensaje, destino);
            bitacora->mensaje(mensaje, "broadcast a " + ipBroadcast + ", intento " + std::to_string(intento));

            struct sockaddr_in origen;
            std::string datagrama = recibir(udp, origen);
            if (datagrama.empty()) continue;

            Mensaje msg;
            try { msg = parsearMensaje(datagrama); } catch (const std::exception&) { continue; }
            if (msg.tipo != TIPO_SOY_INTERMEDIARIO || msg.campos.empty()) continue;

            bitacora->mensaje(datagrama, "desde " + ipDe(origen));
            ip = ipDe(origen);
            puertoRegistro = std::atoi(msg.campos[0].c_str());
            return puertoRegistro > 0;
        }
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("descubrimiento: ") + e.what());
    }
    return false;
}

void escucharIntermediario(const std::string& miId, int puerto, Bitacora* bitacora,
                           std::function<void(const std::string&, int)> alSolicitar) {
    try {
        Socket udp('d');
        udp.Bind(PUERTO_UDP_SERVIDOR);
        for (;;) {
            struct sockaddr_in origen;
            std::string datagrama = recibir(udp, origen);
            if (datagrama.empty()) continue;

            Mensaje msg;
            try { msg = parsearMensaje(datagrama); } catch (const std::exception&) { continue; }
            if (msg.tipo != TIPO_BUSCO_SERVIDORES) continue;

            bitacora->mensaje(datagrama, "broadcast desde " + ipDe(origen));
            std::string respuesta = construirMensaje(miId, msg.origen, TIPO_SOY_SERVIDOR, {std::to_string(puerto)});
            enviar(udp, respuesta, origen);
            bitacora->mensaje(respuesta, "a " + ipDe(origen));

            // el 82 trae el puerto de registro: el servidor se registra de una vez en ese intermediario
            if (!msg.campos.empty() && std::atoi(msg.campos[0].c_str()) > 0) {
                alSolicitar(ipDe(origen), std::atoi(msg.campos[0].c_str()));
            }
        }
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("descubrimiento: no se pudo escuchar el puerto UDP ") +
                            std::to_string(PUERTO_UDP_SERVIDOR) + ": " + e.what());
    }
}

std::string broadcastDe(const std::string& ip) {
    struct ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == -1) return "";

    std::string broadcast;
    for (struct ifaddrs* i = interfaces; i != nullptr && broadcast.empty(); i = i->ifa_next) {
        if (i->ifa_addr == nullptr || i->ifa_addr->sa_family != AF_INET) continue;
        if (!(i->ifa_flags & IFF_BROADCAST) || i->ifa_broadaddr == nullptr) continue;
        if (ipDe(*(struct sockaddr_in*)i->ifa_addr) == ip) broadcast = ipDe(*(struct sockaddr_in*)i->ifa_broadaddr);
    }
    freeifaddrs(interfaces);
    return broadcast;
}
