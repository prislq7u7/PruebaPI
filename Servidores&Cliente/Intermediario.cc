#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cctype>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "SSLSocket.h"
#include "HttpUtil.h"
#include "TablaRutas.h"
#include "Carritos.h"
#include "AtencionCliente.h"
#include "AtencionRegistro.h"
#include "AtencionProtocolo.h"
#include "CanalInterno.h"
#include "Bitacora.h"
#include "Opciones.h"
#include "Certificado.h"
#include "Descubrimiento.h"

static const int MAXLINE = 4096;
static const size_t MAX_PETICION = 8192;

// una sesion sin peticiones por este tiempo se cierra y lo reservado vuelve al stock
// (el cliente se cerro con Ctrl+C o se cayo sin llegar a /cancelar)
static const int SEGUNDOS_SESION = 600;
static const int SEGUNDOS_REVISION = 30;

// un servidor que pasa este tiempo sin registrarse queda "vencido" (se registran cada 10 s)
static const int SEGUNDOS_VIGENCIA = 30;

// lee los encabezados y, si es un POST, el cuerpo completo segun Content-Length
std::string leerPeticion(VSocket* cliente) {
    char buf[MAXLINE];
    std::string acumulado;
    while (acumulado.find("\r\n\r\n") == std::string::npos && acumulado.size() <= MAX_PETICION) {
        size_t leidos = cliente->Read(buf, MAXLINE - 1);
        if (leidos == 0) return acumulado;
        acumulado.append(buf, leidos);
    }

    size_t finEncabezados = acumulado.find("\r\n\r\n");
    if (finEncabezados == std::string::npos) return acumulado;
    size_t largo = 0;
    std::string encabezados = acumulado.substr(0, finEncabezados);
    for (auto& c : encabezados) c = (char)tolower((unsigned char)c);
    size_t pos = encabezados.find("content-length:");
    if (pos != std::string::npos) largo = std::strtoul(encabezados.c_str() + pos + 15, nullptr, 10);
    if (largo > MAX_LINEA) return acumulado;

    while (acumulado.size() < finEncabezados + 4 + largo) {
        size_t leidos = cliente->Read(buf, MAXLINE - 1);
        if (leidos == 0) break;
        acumulado.append(buf, leidos);
    }
    return acumulado;
}

static std::string ipDe(VSocket* conexion) {
    struct sockaddr_in direccion;
    socklen_t largo = sizeof(direccion);
    if (getpeername(conexion->Descriptor(), (struct sockaddr*)&direccion, &largo) != 0) return "?";
    char texto[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &direccion.sin_addr, texto, sizeof(texto));
    return texto;
}

// GET: paginas para el cliente de terminal y el navegador.
// POST: un mensaje del protocolo en el cuerpo (CLI_04|INT_04/...), la respuesta va en el cuerpo.
// el saludo TLS se hace aqui, en el hilo de la conexion, para que un cliente lento no frene a los demas
void atenderCliente(SSLSocket* servidor, int descriptor, TablaRutas* tabla, Carritos* carritos, Bitacora* bitacora) {
    VSocket* cliente = nullptr;
    try {
        cliente = servidor->CompletarConexion(descriptor);
        if (cliente == nullptr) {
            bitacora->registrar("ERROR", "conexion de un cliente sin TLS, se rechaza");
            return;
        }
        std::string peticion = leerPeticion(cliente);

        // algunos navegadores abren conexiones de reserva y las cierran sin enviar nada
        if (!peticion.empty()) {
            std::string primeraLinea = peticion.substr(0, peticion.find("\r\n"));
            std::string salida;
            if (peticion.rfind("POST ", 0) == 0) {
                std::string cuerpo = extraerCuerpo(peticion);
                while (!cuerpo.empty() && (cuerpo.back() == '\n' || cuerpo.back() == '\r')) cuerpo.pop_back();
                AtencionProtocolo atencion(*tabla, *carritos, *bitacora, ipDe(cliente));
                salida = construirRespuestaHttp(200, atencion.responder(cuerpo), "text/plain; charset=utf-8");
            } else if (peticion.rfind("GET ", 0) == 0) {
                AtencionCliente atencion(*tabla, *carritos, *bitacora);
                RespuestaHttp respuesta = atencion.responder(peticion);
                // la bitacora no se anota a si misma, si no cada consulta de /bitacora agregaria una linea
                if (primeraLinea.find(" /bitacora") == std::string::npos) {
                    bitacora->registrar("HTTP", primeraLinea + " -> " + std::to_string(respuesta.codigo));
                }
                salida = construirRespuestaHttp(respuesta.codigo, respuesta.cuerpo);
            } else {
                salida = construirRespuestaHttp(405, "<p>Solo GET o POST.</p>");
            }
            cliente->Write(salida.data(), salida.size());
        }
        cliente->Close();
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("atendiendo cliente: ") + e.what());
    }
    delete cliente;
}

// mensajes del puerto de registro (por TLS): 70 y 72 de los servidores de productos
void atenderRegistro(SSLSocket* servidor, int descriptor, TablaRutas* tabla, Bitacora* bitacora) {
    VSocket* conexion = nullptr;
    try {
        conexion = servidor->CompletarConexion(descriptor);
        if (conexion == nullptr) {
            bitacora->registrar("ERROR", "registro sin TLS, se rechaza");
            return;
        }
        conexion->SetTimeout(5);
        std::string mensaje, error;
        if (recibirMensaje(conexion, mensaje, error)) {
            AtencionRegistro atencion(*tabla, *bitacora);
            enviarMensaje(conexion, atencion.responder(mensaje));
        } else if (error != "conexion cerrada sin mensaje") {
            bitacora->registrar("ERROR", "registro rechazado: " + error);
        }
        conexion->Close();
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("atendiendo un registro: ") + e.what());
    }
    delete conexion;
}

void escucharRegistros(SSLSocket* servidor, TablaRutas* tabla, Bitacora* bitacora) {
    for (;;) {
        int descriptor = servidor->WaitForConnection();
        std::thread(atenderRegistro, servidor, descriptor, tabla, bitacora).detach();
    }
}

// cada cierto tiempo cierra las sesiones abandonadas y devuelve su stock
void revisarSesiones(TablaRutas* tabla, Carritos* carritos, Bitacora* bitacora, int segundosSesion) {
    int espera = std::min(SEGUNDOS_REVISION, segundosSesion);
    for (;;) {
        std::this_thread::sleep_for(std::chrono::seconds(espera));
        std::vector<int> cerradas;
        std::vector<ItemReservado> pendientes = carritos->cerrarVencidas(segundosSesion, cerradas);
        if (cerradas.empty()) continue;

        AtencionCliente atencion(*tabla, *carritos, *bitacora);
        atencion.devolverReservas(pendientes);
        bitacora->registrar("SESION", std::to_string(cerradas.size()) + " sesion(es) vencida(s), " +
                            std::to_string(pendientes.size()) + " producto(s) devueltos al stock");
    }
}

static void ayuda(const char* programa) {
    std::cout << "uso: " << programa << " [--ip-publica IP] [--ip-privada IP] [--puerto N] [--puerto-registro N] [--broadcast IP]\n\n"
              << "  --ip-publica IP       donde lo buscan los clientes (por defecto: la ip de esta computadora)\n"
              << "  --ip-privada IP       donde se registran los servidores de productos (por defecto: la misma)\n"
              << "  --puerto N            puerto de los clientes, por defecto " << PUERTO_CLIENTES << "\n"
              << "  --puerto-registro N   puerto donde se registran los servidores de productos, por defecto " << PUERTO_REGISTRO << "\n"
              << "  --broadcast IP        a donde se llama a los servidores al arrancar (82), por defecto el broadcast\n"
              << "                        de la red de --ip-privada (en la isla 4: 172.16.123.79)\n\n"
              << "necesita certificados/ci0123.pem (make certificado): clientes y servidores se le conectan por TLS\n\n"
              << "ejemplos:\n"
              << "  " << programa << "\n"
              << "  " << programa << " --ip-publica 10.1.35.20 --ip-privada 172.16.123.67\n";
}

int main(int argc, char* argv[]) {
    if (tieneOpcion(argc, argv, "--ayuda")) { ayuda(argv[0]); return 0; }

    std::string ipClientes = opcion(argc, argv, "--ip-publica", ipLocal());
    std::string ipServidores = opcion(argc, argv, "--ip-privada", ipClientes);
    int puertoClientes = std::atoi(opcion(argc, argv, "--puerto", std::to_string(PUERTO_CLIENTES)).c_str());
    int puertoRegistro = std::atoi(opcion(argc, argv, "--puerto-registro", std::to_string(PUERTO_REGISTRO)).c_str());
    std::string ipBroadcast = opcion(argc, argv, "--broadcast", broadcastDe(ipServidores));
    exigirCertificado();

    // para las pruebas: TIC_SESION_SEGUNDOS=5 y TIC_VIGENCIA_SEGUNDOS=5 ./intermediario
    const char* variable = std::getenv("TIC_SESION_SEGUNDOS");
    int segundosSesion = (variable && std::atoi(variable) > 0) ? std::atoi(variable) : SEGUNDOS_SESION;
    variable = std::getenv("TIC_VIGENCIA_SEGUNDOS");
    int segundosVigencia = (variable && std::atoi(variable) > 0) ? std::atoi(variable) : SEGUNDOS_VIGENCIA;

    // si un cliente cierra la conexion antes de tiempo, write no debe terminar todo el proceso
    signal(SIGPIPE, SIG_IGN);

    // se comparten entre todos los hilos; cada una protege sus datos con su propio mutex
    TablaRutas tabla(segundosVigencia);
    Carritos carritos;
    Bitacora bitacora("bitacora-intermediario.log", ID_INTERMEDIARIO);

    SSLSocket* servidor;
    SSLSocket* registro;
    try {
        servidor = new SSLSocket(ARCHIVO_CERTIFICADO, ARCHIVO_CERTIFICADO);
        registro = new SSLSocket(ARCHIVO_CERTIFICADO, ARCHIVO_CERTIFICADO);
        servidor->Bind(ipClientes.c_str(), puertoClientes);
        servidor->MarkPassive(10);
        registro->Bind(ipServidores.c_str(), puertoRegistro);
        registro->MarkPassive(10);
    } catch (const std::exception& e) {
        std::cerr << "no se pudo abrir el intermediario: " << e.what() << "\n";
        return 1;
    }

    bitacora.registrar("INICIO", "clientes en https://" + ipClientes + ":" + std::to_string(puertoClientes) +
                       "/, registro de servidores en " + ipServidores + ":" + std::to_string(puertoRegistro) +
                       ", sesiones vencen a los " + std::to_string(segundosSesion) + " s");

    std::thread(escucharRegistros, registro, &tabla, &bitacora).detach();
    std::thread(revisarSesiones, &tabla, &carritos, &bitacora, segundosSesion).detach();

    // los servidores de productos que arrancan despues lo buscan con un 80; a los que ya estaban
    // corriendo se les avisa con un 82 para que se registren de una vez
    std::thread(escucharServidores, puertoRegistro, &bitacora).detach();
    if (!ipBroadcast.empty()) llamarServidores(ipBroadcast, puertoRegistro, &bitacora);

    for (;;) {
        int descriptor = servidor->WaitForConnection();
        std::thread(atenderCliente, servidor, descriptor, &tabla, &carritos, &bitacora).detach();
    }

    return 0;
}
