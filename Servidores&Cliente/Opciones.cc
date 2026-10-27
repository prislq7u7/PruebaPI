#include "Opciones.h"
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

std::string opcion(int argc, char* argv[], const std::string& nombre, const std::string& porDefecto) {
    for (int i = 1; i + 1 < argc; i++) {
        if (nombre == argv[i]) return argv[i + 1];
    }
    return porDefecto;
}

bool tieneOpcion(int argc, char* argv[], const std::string& nombre) {
    for (int i = 1; i < argc; i++) {
        if (nombre == argv[i]) return true;
    }
    return false;
}

std::string argumentoSuelto(int argc, char* argv[], int cual) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg.rfind("--", 0) == 0) {
            // las opciones con valor se saltan junto con su valor; --ayuda no lleva valor
            if (arg != "--ayuda") i++;
            continue;
        }
        if (cual-- == 0) return arg;
    }
    return "";
}

std::string ipLocal() {
    struct ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) == -1) return "";

    std::string ip;
    for (struct ifaddrs* i = interfaces; i != nullptr && ip.empty(); i = i->ifa_next) {
        if (i->ifa_addr == nullptr || i->ifa_addr->sa_family != AF_INET) continue;
        if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
        char texto[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &((struct sockaddr_in*)i->ifa_addr)->sin_addr, texto, sizeof(texto));
        ip = texto;
    }
    freeifaddrs(interfaces);
    return ip;
}
