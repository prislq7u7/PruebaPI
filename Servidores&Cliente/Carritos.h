#ifndef CARRITOS_H
#define CARRITOS_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <ctime>
#include <cstdint>

// un producto ya reservado en el servidor de productos, con el precio que este confirmo
struct ItemReservado {
    std::string producto;
    int cantidad;
    uint32_t precioCentavos;
    std::string servidor;   // id del servidor de productos donde quedo reservado
};

// guarda el carrito de cada sesion en la memoria del intermediario.
// varios hilos atienden clientes al mismo tiempo, por eso todo pasa por mutex_
class Carritos {
public:
    // reparte un numero de sesion nuevo con su carrito vacio
    int nuevaSesion();

    // sesion de un cliente que habla con mensajes del protocolo (no pide /sesion):
    // se identifica con "origen@ip"; si no tenia o ya vencio, se le crea una nueva
    int sesionDe(const std::string& clave);

    // ademas de revisar, marca la sesion como usada ahora (para que no venza)
    bool existe(int sesion);

    // si el producto ya estaba con el mismo precio se suman las cantidades
    bool agregar(int sesion, const ItemReservado& item);

    // entrega los productos del carrito y lo deja vacio
    bool vaciar(int sesion, std::vector<ItemReservado>& items);

    // borra la sesion y entrega lo que tenia reservado, para devolverlo al stock
    bool cerrar(int sesion, std::vector<ItemReservado>& items);

    // borra las sesiones sin uso por mas de "segundos" y entrega todo lo que tenian reservado
    std::vector<ItemReservado> cerrarVencidas(int segundos, std::vector<int>& cerradas);

private:
    struct Sesion {
        std::vector<ItemReservado> items;
        time_t ultimoUso;
    };

    std::mutex mutex_;
    std::map<int, Sesion> carritos_;
    std::map<std::string, int> claves_;
    int siguienteSesion_ = 1;
};

#endif
