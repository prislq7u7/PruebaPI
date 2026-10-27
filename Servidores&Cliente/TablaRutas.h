#ifndef TABLARUTAS_H
#define TABLARUTAS_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <ctime>

// un servidor de productos que se registro ante el intermediario (mensaje 70)
struct ServidorRegistrado {
    std::string id;                        // SERV_NN
    std::string nombre;
    std::string ip;
    int puerto;
    std::vector<std::string> categorias;   // lo que publico en su ultimo registro
    bool activo;                           // false si fallo al consultarlo y no se ha vuelto a registrar
    time_t ultimoRegistro;
    std::string estado;                    // "activo", "sin-respuesta" o "vencido" (se calcula al consultar)
};

// una fila de la tabla de rutas: que servidor atiende cada categoria
struct Ruta {
    std::string categoria;
    std::string servidor;
    std::string direccion;                 // ip:puerto
    std::string estado;
};

// servidores de productos registrados, tabla de rutas categoria -> servidores y en que servidor
// se vio cada producto. Todo se entrega ordenado por SERV_NN, como pide el protocolo.
// la comparten todos los hilos del intermediario, por eso todo pasa por mutex_
class TablaRutas {
public:
    // un servidor que pasa esta cantidad de segundos sin registrarse queda "vencido" y fuera de las rutas
    explicit TablaRutas(int segundosVigencia = 30);

    // agrega el servidor o reemplaza sus datos y categorias; devuelve true si es nuevo.
    // cambio queda en true si es nuevo o si cambio algo (para no llenar la bitacora
    // con los registros periodicos que no traen nada distinto)
    bool registrar(const std::string& id, const std::string& nombre, const std::string& ip, int puerto,
                   const std::vector<std::string>& categorias, bool& cambio);

    // el servidor aviso que se cierra (mensaje 72)
    bool quitar(const std::string& id);

    // fallo al consultarlo: queda fuera de las rutas hasta que se vuelva a registrar
    void deshabilitar(const std::string& id);

    bool buscar(const std::string& id, ServidorRegistrado& resultado);

    std::vector<ServidorRegistrado> servidores();
    std::vector<ServidorRegistrado> activos();

    // servidores activos que publicaron esa categoria (sin distinguir mayusculas)
    std::vector<ServidorRegistrado> servidoresDe(const std::string& categoria);

    std::vector<Ruta> rutas();

    // en que servidor se vio un producto en un 22 (si esta en varios, el de menor SERV_NN)
    void recordarProducto(const std::string& producto, const std::string& servidor);
    std::string servidorDeProducto(const std::string& producto);

private:
    std::mutex mutex_;
    int segundosVigencia_;
    std::vector<ServidorRegistrado> servidores_;   // ordenados por SERV_NN
    std::map<std::string, std::string> productos_; // producto en minusculas -> SERV_NN

    std::string estado(const ServidorRegistrado& s);
};

#endif
