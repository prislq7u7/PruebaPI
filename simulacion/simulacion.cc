// Simulación de la tercera etapa de TicAmazon (Equipo 4 - cafeteria).
// Hilos: Cliente, Intermediario y dos servidores de productos (Bodega1 y Bodega2).
// Bodega1 arranca con el sistema; Bodega2 se incorpora con el sistema ya en marcha.

#include <thread>
#include <chrono>
#include <vector>

#include "Buzon.h"
#include "Protocolo.h"
#include "Bodega.h"
#include "Intermediario.h"
#include "Cliente.h"

int main() {
    Buzon buzon; // un solo buzon compartido por todos los hilos

    // SERV_01: Postres y Caramelos
    std::vector<Producto> productosBodega1 = {
        { "tarta",     2200.0f, 10, "tarta de queso del dia",       "Postres" },
        { "flan",      1500.0f,  8, "flan de vainilla casero",      "Postres" },
        { "menta",      300.0f, 100, "caramelo de menta individual", "Caramelos" },
        { "fresa",      350.0f,  80, "caramelo de fresa individual", "Caramelos" }
    };

    // SERV_02: Bebidas y Reposteria
    std::vector<Producto> productosBodega2 = {
        { "cafe",      1000.0f, 50, "cafe negro",                   "Bebidas" },
        { "capuchino", 1200.0f, 40, "espresso con leche",           "Bebidas" },
        { "pan-dulce",  900.0f, 20, "pan dulce relleno de queso",   "Reposteria" },
        { "galleta",    250.0f, 60, "galleta de mantequilla",       "Reposteria" }
    };

    Bodega bodega1( buzon, ID_PRIMER_SERVIDOR, "Bodega1", "172.16.123.68", 9050,
                    { "Postres", "Caramelos" }, productosBodega1 );
    Bodega bodega2( buzon, ID_PRIMER_SERVIDOR + 1, "Bodega2", "172.16.123.69", 9050,
                    { "Bebidas", "Reposteria" }, productosBodega2 );
    Intermediario intermediario( buzon );
    Cliente cliente( buzon );

    // Bodega2 no arranca con el resto: el cliente lo pide en el paso del guion donde se
    // incorpora un servidor nuevo, y entonces se registra ante el intermediario.
    std::thread hBodega2;
    cliente.incorporarServidor = [ & ]() {
        hBodega2 = std::thread( &Bodega::ejecutar, &bodega2 );
    };

    std::thread hBodega1( &Bodega::ejecutar, &bodega1 );
    std::thread hIntermediario( &Intermediario::ejecutar, &intermediario );

    // Pausa para que Bodega1 se registre antes de que el cliente empiece
    std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );
    std::thread hCliente( &Cliente::ejecutar, &cliente );

    // El cliente manda EXIT a intermediario; este lo reenvía a los servidores registrados antes de terminar él mismo.
    // Por eso join() en todos los hilos no se queda bloqueado para siempre.
    hCliente.join();
    hIntermediario.join();
    hBodega1.join();
    if ( hBodega2.joinable() ) hBodega2.join();

    return 0;
}
