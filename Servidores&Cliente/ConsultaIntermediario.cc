#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <csignal>

#include "SSLSocket.h"
#include "Protocolo.h"
#include "CanalInterno.h"
#include "HttpUtil.h"
#include "Opciones.h"

// Herramienta de consulta y prueba:
//   - servidores, rutas, bitacora y mensaje: le manda al intermediario un mensaje del protocolo
//     por HTTP (POST, como cualquier cliente) y muestra la respuesta
//   - enviar: manda un mensaje directo (por TLS) a un servidor de productos o al puerto de registro

static std::vector<std::string> separar(const std::string& texto, char separador) {
    std::vector<std::string> partes;
    std::stringstream ss(texto);
    std::string parte;
    while (std::getline(ss, parte, separador)) partes.push_back(parte);
    return partes;
}

// POST por HTTPS con el mensaje en el cuerpo; devuelve el cuerpo de la respuesta ("" si no hubo conexion)
static std::string postear(const std::string& ip, int puerto, const std::string& mensaje) {
    try {
        SSLSocket conexion;
        conexion.SetTimeout(10);
        conexion.Connect(ip.c_str(), puerto);
        std::string peticion = "POST / HTTP/1.1\r\nHost: " + ip + "\r\nContent-Type: text/plain\r\nContent-Length: " +
                               std::to_string(mensaje.size()) + "\r\nConnection: close\r\n\r\n" + mensaje;
        conexion.Write(peticion.data(), peticion.size());
        std::string respuesta;
        char buf[4096];
        size_t leidos;
        while ((leidos = conexion.Read(buf, sizeof(buf))) > 0) respuesta.append(buf, leidos);
        return extraerCuerpo(respuesta);
    } catch (const std::exception& e) {
        std::cerr << "no se pudo conectar con " << ip << ":" << puerto << " (" << e.what() << ")\n";
        return "";
    }
}

// un mensaje del protocolo interno por TLS y su respuesta ("" si nadie contesta)
static std::string enviarDirecto(const std::string& ip, int puerto, const std::string& mensaje) {
    try {
        SSLSocket conexion;
        conexion.SetTimeout(5);
        conexion.Connect(ip.c_str(), puerto);
        enviarMensaje(&conexion, mensaje);
        std::string respuesta, error;
        if (recibirMensaje(&conexion, respuesta, error)) return respuesta;
        std::cerr << "respuesta invalida: " << error << "\n";
    } catch (const std::exception& e) {
        std::cerr << "no se pudo conectar con " << ip << ":" << puerto << " (" << e.what() << ")\n";
    }
    return "";
}

static void ayuda(const char* programa) {
    std::cout << "uso: " << programa << " <consulta> [--intermediario IP] [--puerto N]\n\n"
              << "  servidores                  servidores de productos registrados (73 -> 74)\n"
              << "  rutas                       tabla de rutas categoria -> servidor (75 -> 76)\n"
              << "  bitacora [n]                los n eventos mas recientes, 20 por defecto (77 -> 78)\n"
              << "  mensaje '<mensaje>'         cualquier mensaje del protocolo al intermediario, por HTTP\n"
              << "  enviar <ip:puerto> '<msg>'  mensaje directo a un servidor de productos (TLS)\n\n"
              << "ejemplos:\n"
              << "  " << programa << " rutas\n"
              << "  " << programa << " mensaje 'CLI_04|INT_04/20/postres'\n"
              << "  " << programa << " enviar 172.16.123.67:9051 'INT_04|SERV_01/10'\n";
}

int main(int argc, char* argv[]) {
    std::string consulta = argumentoSuelto(argc, argv);
    if (consulta.empty() || tieneOpcion(argc, argv, "--ayuda")) { ayuda(argv[0]); return consulta.empty() ? 1 : 0; }
    signal(SIGPIPE, SIG_IGN);

    std::string ip = opcion(argc, argv, "--intermediario", ipLocal());
    int puerto = std::atoi(opcion(argc, argv, "--puerto", std::to_string(PUERTO_CLIENTES)).c_str());

    if (consulta == "enviar") {
        std::string destino = argumentoSuelto(argc, argv, 1);
        std::string mensaje = argumentoSuelto(argc, argv, 2);
        size_t dosPuntos = destino.find(':');
        if (dosPuntos == std::string::npos || mensaje.empty()) { ayuda(argv[0]); return 1; }
        std::string respuesta = enviarDirecto(destino.substr(0, dosPuntos), std::atoi(destino.substr(dosPuntos + 1).c_str()), mensaje);
        std::cout << respuesta << "\n";
        return respuesta.empty() ? 1 : 0;
    }

    std::string mensaje;
    if (consulta == "servidores") mensaje = construirMensaje(ID_CLIENTE, ID_INTERMEDIARIO, TIPO_REQUEST_SERVERS, {});
    else if (consulta == "rutas") mensaje = construirMensaje(ID_CLIENTE, ID_INTERMEDIARIO, TIPO_REQUEST_ROUTES, {});
    else if (consulta == "bitacora") {
        std::string n = argumentoSuelto(argc, argv, 1);
        mensaje = construirMensaje(ID_CLIENTE, ID_INTERMEDIARIO, TIPO_REQUEST_LOG, {n.empty() ? "20" : n});
    } else if (consulta == "mensaje") mensaje = argumentoSuelto(argc, argv, 1);
    if (mensaje.empty()) { ayuda(argv[0]); return 1; }

    std::string respuesta = postear(ip, puerto, mensaje);
    if (respuesta.empty()) return 1;

    Mensaje msg;
    try { msg = parsearMensaje(respuesta); } catch (const std::exception&) { std::cout << respuesta << "\n"; return 1; }
    std::cout << respuesta << "\n";
    if (consulta == "mensaje" || msg.campos.size() < 2) return 0;

    // ademas de la linea del protocolo, una version facil de leer
    for (auto& entrada : separar(msg.campos[1], ';')) {
        auto p = separar(entrada, ',');
        if (msg.tipo == TIPO_SERVER_LIST && p.size() == 4) std::cout << "  " << p[0] << "  " << p[1] << "  " << p[2] << ":" << p[3] << "\n";
        else if (msg.tipo == TIPO_ROUTE_TABLE && p.size() == 2) std::cout << "  " << p[0] << " -> " << p[1] << "\n";
        else if (msg.tipo == TIPO_LOG_LIST && p.size() == 5) std::cout << "  " << p[0] << "  " << p[1] << " -> " << p[2] << "  " << p[3] << "  " << p[4] << "\n";
    }
    return 0;
}
