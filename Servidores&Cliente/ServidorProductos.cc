#include <iostream>
#include <thread>
#include <csignal>
#include <cstdlib>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <sys/stat.h>
#include <pthread.h>

#include "SSLSocket.h"
#include "FileSystem.h"
#include "Inventario.h"
#include "AtencionBodega.h"
#include "Protocolo.h"
#include "CanalInterno.h"
#include "Bitacora.h"
#include "Opciones.h"
#include "Certificado.h"
#include "Descubrimiento.h"

static const int SEGUNDOS_ESPERA = 10;   // un intermediario que no manda nada no deja el hilo pegado
static const int SEGUNDOS_REGISTRO = 10; // cada cuanto se vuelve a registrar en el intermediario

// el saludo TLS se hace aqui, en el hilo de la conexion
void atenderIntermediario(SSLSocket* servidor, int descriptor, Inventario* inventario, Bitacora* bitacora, std::string miId) {
    VSocket* conexion = nullptr;
    try {
        conexion = servidor->CompletarConexion(descriptor);
        if (conexion == nullptr) {
            bitacora->registrar("ERROR", "conexion sin TLS, se rechaza");
            return;
        }
        conexion->SetTimeout(SEGUNDOS_ESPERA);
        std::string mensaje, error;
        if (recibirMensaje(conexion, mensaje, error)) {
            bitacora->mensaje(mensaje);
            AtencionBodega atencion(*inventario, miId);
            std::string respuesta = atencion.responder(mensaje);
            bitacora->mensaje(respuesta);
            enviarMensaje(conexion, respuesta);
        } else if (error != "conexion cerrada sin mensaje") {
            bitacora->registrar("ERROR", "mensaje rechazado: " + error);
        }
        conexion->Close();
    } catch (const std::exception& e) {
        bitacora->registrar("ERROR", std::string("atendiendo al intermediario: ") + e.what());
    }
    delete conexion;
}

// a donde se registra este servidor. La ip y el puerto del intermediario pueden cambiar si se
// descubre por broadcast, por eso se leen y se cambian con el mutex
struct DatosRegistro {
    std::string id;              // SERV_NN
    std::string nombre;          // id del disco, I04-S1
    std::string ip;              // donde escucha este servidor
    int puerto;
    std::string broadcast;       // a donde se busca al intermediario (80); "" si se dio --intermediario
    std::mutex mutex;
    std::condition_variable despertar;   // el intermediario llamo (82): registrarse ya, sin esperar
    std::string ipIntermediario;
    int puertoIntermediario;
    bool llamado = false;
};

static void destino(DatosRegistro* datos, std::string& ip, int& puerto) {
    std::lock_guard<std::mutex> lock(datos->mutex);
    ip = datos->ipIntermediario;
    puerto = datos->puertoIntermediario;
}

static void cambiarDestino(DatosRegistro* datos, const std::string& ip, int puerto) {
    std::lock_guard<std::mutex> lock(datos->mutex);
    datos->ipIntermediario = ip;
    datos->puertoIntermediario = puerto;
}

// manda un mensaje al intermediario por el puerto de registro (TLS) y devuelve su respuesta ("" si no contesta)
static std::string enviarAlIntermediario(DatosRegistro* datos, const std::string& mensaje) {
    std::string ip;
    int puerto;
    destino(datos, ip, puerto);
    try {
        SSLSocket conexion;
        conexion.SetTimeout(3);
        conexion.Connect(ip.c_str(), puerto);
        enviarMensaje(&conexion, mensaje);
        std::string respuesta, error;
        return recibirMensaje(&conexion, respuesta, error) ? respuesta : "";
    } catch (const std::exception&) {
        return "";
    }
}

// se registra (70) al arrancar y lo repite cada cierto tiempo: asi el intermediario lo
// encuentra aunque arranque despues, se entera si se agrego una bodega con categorias
// nuevas, y lo vuelve a activar si lo habia deshabilitado o vencido.
// si el intermediario no contesta y no se dio --intermediario, lo vuelve a buscar por broadcast
static void registrarse(DatosRegistro* datos, Inventario* inventario, Bitacora* bitacora, int segundos) {
    bool registrado = false;
    std::string publicadas;
    for (;;) {
        std::string categorias;
        try {
            for (auto& c : inventario->categorias()) categorias += (categorias.empty() ? "" : ",") + c;
        } catch (const std::exception& e) {
            bitacora->registrar("ERROR", std::string("no se pudieron leer las categorias del disco: ") + e.what());
        }

        std::string mensaje = construirMensaje(datos->id, ID_INTERMEDIARIO, TIPO_REGISTER_SERVER,
                                               {datos->nombre, datos->ip, std::to_string(datos->puerto), categorias});
        std::string respuesta = enviarAlIntermediario(datos, mensaje);
        bool ok = respuesta.rfind(ID_INTERMEDIARIO + "|" + datos->id + "/" + std::to_string(TIPO_REGISTER_ACK) + "/", 0) == 0;
        std::string ipIntermediario;
        int puertoIntermediario;
        destino(datos, ipIntermediario, puertoIntermediario);

        if (ok && (!registrado || categorias != publicadas)) {
            bitacora->registrar("REGISTRO", "registrado en el intermediario " + ipIntermediario + ":" +
                                std::to_string(puertoIntermediario) + " con " +
                                (categorias.empty() ? "(sin categorias)" : categorias));
        } else if (!ok && registrado) {
            bitacora->registrar("ERROR", "el intermediario no responde, se reintenta cada " + std::to_string(segundos) + " s");
        } else if (!ok && !respuesta.empty()) {
            bitacora->registrar("ERROR", "el intermediario rechazo el registro: " + respuesta);
        }
        registrado = ok;
        if (ok) publicadas = categorias;

        if (!ok && !datos->broadcast.empty()) {
            std::string ip;
            int puerto;
            if (buscarIntermediario(datos->id, datos->broadcast, 1, ip, puerto, bitacora)) {
                cambiarDestino(datos, ip, puerto);
                continue;
            }
        }

        std::unique_lock<std::mutex> lock(datos->mutex);
        datos->despertar.wait_for(lock, std::chrono::seconds(segundos), [datos] { return datos->llamado; });
        datos->llamado = false;
    }
}

// el intermediario mando un 82: ahora se registra con el que llamo
static void llamadoDelIntermediario(DatosRegistro* datos, const std::string& ip, int puertoRegistro) {
    std::lock_guard<std::mutex> lock(datos->mutex);
    datos->ipIntermediario = ip;
    datos->puertoIntermediario = puertoRegistro;
    datos->llamado = true;
    datos->despertar.notify_one();
}

// espera Ctrl+C o kill, avisa al intermediario que este servidor se cierra (72) y termina
static void esperarCierre(DatosRegistro* datos, Bitacora* bitacora, sigset_t senales) {
    int senal;
    sigwait(&senales, &senal);
    bitacora->registrar("FIN", "cerrando, se avisa al intermediario");
    enviarAlIntermediario(datos, construirMensaje(datos->id, ID_INTERMEDIARIO, TIPO_UNREGISTER_SERVER, {}));
    std::exit(0);
}

static bool existe(const std::string& ruta) {
    struct stat info;
    return stat(ruta.c_str(), &info) == 0;
}

static void ayuda(const char* programa) {
    std::cout << "uso: " << programa << " <disco> [--intermediario IP] [--ip IP] [--puerto N] [--broadcast IP]\n\n"
              << "  <disco>             fs1.dat.ci0123, fs2.dat.ci0123... (se busca tambien en ../almacenamiento)\n"
              << "  --intermediario IP  donde corre el intermediario (por defecto se busca por broadcast)\n"
              << "  --ip IP             donde escucha este servidor (por defecto: la ip de esta computadora)\n"
              << "  --puerto N          por defecto " << PUERTO_BASE_SERVIDOR << " + numero de servidor del disco"
              << " (fs1 -> SERV_01 en " << PUERTO_BASE_SERVIDOR + 1 << ", fs2 -> SERV_02 en " << PUERTO_BASE_SERVIDOR + 2 << ")\n"
              << "  --broadcast IP      a donde se busca al intermediario (80), por defecto el broadcast de la red\n"
              << "                      de --ip (en la isla 4: 172.16.123.79)\n\n"
              << "necesita certificados/ci0123.pem (make certificado): el intermediario se le conecta por TLS\n\n"
              << "ejemplos:\n"
              << "  " << programa << " fs1.dat.ci0123\n"
              << "  " << programa << " fs2.dat.ci0123 --intermediario 172.16.123.67\n";
}

int main(int argc, char* argv[]) {
    std::string disco = argumentoSuelto(argc, argv);
    if (disco.empty() || tieneOpcion(argc, argv, "--ayuda")) {
        ayuda(argv[0]);
        return disco.empty() && !tieneOpcion(argc, argv, "--ayuda") ? 1 : 0;
    }
    if (!existe(disco) && existe("../almacenamiento/" + disco)) disco = "../almacenamiento/" + disco;

    std::string ipServidor = opcion(argc, argv, "--ip", ipLocal());
    std::string ipIntermediario = opcion(argc, argv, "--intermediario", "");
    std::string ipBroadcast = opcion(argc, argv, "--broadcast", broadcastDe(ipServidor));
    int puertoRegistro = std::atoi(opcion(argc, argv, "--puerto-registro", std::to_string(PUERTO_REGISTRO)).c_str());

    // para probar sin esperar: TIC_REGISTRO_SEGUNDOS=2 ./servidorProductos ...
    const char* variable = std::getenv("TIC_REGISTRO_SEGUNDOS");
    int segundosRegistro = (variable && std::atoi(variable) > 0) ? std::atoi(variable) : SEGUNDOS_REGISTRO;

    // si el intermediario cierra la conexion antes de tiempo, write no debe terminar todo el proceso
    signal(SIGPIPE, SIG_IGN);
    exigirCertificado();

    // Ctrl+C y kill los atiende un solo hilo (esperarCierre); se bloquean antes de crear los demas
    sigset_t senales;
    sigemptyset(&senales);
    sigaddset(&senales, SIGINT);
    sigaddset(&senales, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &senales, nullptr);

    // el disco se crea y se llena con las herramientas de almacenamiento (mkfs, agregar_bodega);
    // si no existe o esta danado el servidor no arranca, nunca lo reescribe
    FileSystem* fs;
    try {
        fs = new FileSystem(disco);
    } catch (const std::exception& e) {
        std::cerr << "no se pudo abrir el disco: " << e.what() << "\n";
        return 1;
    }
    Inventario inventario(*fs);
    std::string idDisco = fs->idDisco();
    std::string miId = idServidor(fs->superbloque().servidor);
    int puerto = std::atoi(opcion(argc, argv, "--puerto",
                                  std::to_string(PUERTO_BASE_SERVIDOR + fs->superbloque().servidor)).c_str());

    SSLSocket* servidor;
    try {
        servidor = new SSLSocket(ARCHIVO_CERTIFICADO, ARCHIVO_CERTIFICADO);
        servidor->Bind(ipServidor.c_str(), puerto);
        servidor->MarkPassive(10);
    } catch (const std::exception& e) {
        std::cerr << "no se pudo abrir el servidor de productos en " << ipServidor << ":" << puerto << ": " << e.what() << "\n";
        return 1;
    }

    Bitacora bitacora("bitacora-" + miId + ".log", miId);
    std::string categorias;
    for (auto& c : inventario.categorias()) categorias += (categorias.empty() ? "" : ",") + c;
    bitacora.registrar("INICIO", miId + " (" + idDisco + ") escuchando en " + ipServidor + ":" + std::to_string(puerto) + ", disco " + disco +
                       ", categorias " + categorias);

    DatosRegistro datos;
    datos.id = miId;
    datos.nombre = idDisco;
    datos.ip = ipServidor;
    datos.puerto = puerto;
    datos.ipIntermediario = ipIntermediario;
    datos.puertoIntermediario = puertoRegistro;

    // sin --intermediario se busca por broadcast (80 -> 81); si nadie contesta se prueba en esta
    // misma computadora y se sigue buscando en cada intento de registro
    if (ipIntermediario.empty()) {
        datos.broadcast = ipBroadcast;
        datos.ipIntermediario = ipServidor;
        std::string ip;
        int puertoEncontrado;
        if (!ipBroadcast.empty() && buscarIntermediario(miId, ipBroadcast, 3, ip, puertoEncontrado, &bitacora)) {
            datos.ipIntermediario = ip;
            datos.puertoIntermediario = puertoEncontrado;
        }
    }

    std::thread(registrarse, &datos, &inventario, &bitacora, segundosRegistro).detach();
    std::thread(esperarCierre, &datos, &bitacora, senales).detach();
    std::thread(escucharIntermediario, miId, puerto, &bitacora,
                [&datos](const std::string& ip, int puertoRegistro) { llamadoDelIntermediario(&datos, ip, puertoRegistro); }).detach();

    for (;;) {
        int descriptor = servidor->WaitForConnection();
        std::thread(atenderIntermediario, servidor, descriptor, &inventario, &bitacora, miId).detach();
    }

    return 0;
}
