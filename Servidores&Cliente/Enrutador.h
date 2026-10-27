#ifndef ENRUTADOR_H
#define ENRUTADOR_H

#include <string>
#include <vector>
#include "Protocolo.h"
#include "TablaRutas.h"
#include "Carritos.h"
#include "Bitacora.h"

// el servidor de productos que no contesto, para armar el 92: <componente>/<tipo pendiente>/<intentos>
struct FallaServidor {
    std::string servidor;
    int tipo = 0;
    int intentos = 0;
};

struct ProductoRuta {
    std::string nombre;
    std::string precio;
    std::string stock;
    std::string servidor;
};

struct Reserva {
    std::string nombre;       // como esta guardado en el servidor
    std::string precio;
    int reservado = 0;
    std::string servidor;
};

// reglas de enrutamiento del protocolo con varios servidores de productos. Lo usan las dos
// formas de hablar con el intermediario: mensajes del protocolo por POST y las paginas por GET
class Enrutador {
public:
    Enrutador(TablaRutas& tabla, Bitacora& bitacora);

    // 10: manda el 10 a cada servidor (en orden de SERV_NN) y junta las categorias sin repetir.
    // false solo si habia servidores y ninguno contesto
    bool categorias(std::vector<std::string>& lista, FallaServidor& falla);

    // 20: manda el 20 a cada servidor que publico la categoria y junta sus productos.
    // atendida queda en false si ningun servidor la publico (23 sin consultar a nadie).
    // false si ninguno de los que la atienden contesto
    bool productos(const std::string& categoria, std::vector<ProductoRuta>& lista, bool& atendida,
                   FallaServidor& falla);

    // 30: manda el 50 primero al servidor donde se vio el producto en un 22 y despues a los demas,
    // hasta que uno reserve mas de 0. false si ninguno contesto (92); si contestaron pero ninguno
    // lo tenia, devuelve true con reservado 0
    bool reservar(const std::string& producto, const std::string& cantidad, Reserva& reserva, FallaServidor& falla);

    // 52: regresa a cada servidor lo que se reservo y no se facturo
    void devolver(const std::vector<ItemReservado>& items);

    // un mensaje a un servidor; si no contesta lo deshabilita en la tabla de rutas
    bool consultar(const ServidorRegistrado& servidor, int tipo, const std::vector<std::string>& campos,
                   Mensaje& respuesta, FallaServidor& falla);

private:
    TablaRutas& tabla_;
    Bitacora& bitacora_;
};

#endif
