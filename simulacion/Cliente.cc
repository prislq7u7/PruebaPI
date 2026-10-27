#include <thread>
#include <chrono>
#include <cstdio>
#include <vector>
#include <sstream>

#include "Cliente.h"
#include "Bitacora.h"
#include "Protocolo.h"

// Paso especial del guion: no es un mensaje del protocolo.
static const std::string INCORPORAR_SERVIDOR = "@incorporar-servidor";

Cliente::Cliente( Buzon & buzon ) : buzon_( buzon ) {
}

// pedir() se encarga de mandar mensaje, esperar la respuesta y rotular ambos.
void Cliente::pedir( const std::string & mensaje ) {
    rotular( ID_CLIENTE, ID_INTERMEDIARIO, mensaje, "solicitud enviada" );
    buzon_.Enviar( mensaje.c_str(), canal( ID_CLIENTE, ID_INTERMEDIARIO ) );

    struct { long mtype; char texto[ TAM_MAX_MENSAJE ]; } resp;
    buzon_.Recibir( &resp, sizeof( resp.texto ), canal( ID_INTERMEDIARIO, ID_CLIENTE ) );
    rotularAck( ID_CLIENTE, ID_INTERMEDIARIO, "respuesta recibida" );

    std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) ); // pausa para que el log sea legible
}

void Cliente::ejecutar() {
    // Pausa para que el primer servidor termine de registrarse antes de la primera petición
    std::this_thread::sleep_for( std::chrono::milliseconds( 150 ) );

    const std::string cli      = idTextoCliente();
    const std::string intermed = idTextoIntermediario();

    std::vector<std::string> peticiones = {
        // Al principio solo está registrado SERV_01 (Postres y Caramelos)

        // caso normal: pedir la lista de categorias disponibles
        armarMensaje( cli, intermed, REQUEST_CATEGORIES ),
        // ERR_FORMAT: categoria muy corta (regex exige 3-20 letras)
        armarMensaje( cli, intermed, REQUEST_PRODUCTS, { "co" } ),
        // formato valido, pero ningun servidor atiende esta categoria
        armarMensaje( cli, intermed, REQUEST_PRODUCTS, { "Licores" } ),
        // caso normal: categoria que si atiende SERV_01
        armarMensaje( cli, intermed, REQUEST_PRODUCTS, { "Caramelos" } ),
        // Bebidas todavia no la atiende nadie
        armarMensaje( cli, intermed, REQUEST_PRODUCTS, { "Bebidas" } ),
        // consultas al intermediario: servidores registrados y tabla de rutas
        armarMensaje( cli, intermed, REQUEST_SERVERS ),
        armarMensaje( cli, intermed, REQUEST_ROUTES ),
        //caso en el que el carrito esta vacio
        armarMensaje( cli, intermed, REQUEST_FACTURA ),
        //ERR_FORMAT
        armarMensaje( cli, intermed, ADD_TO_CART, { "torta#3", "2" } ),
        //caso correcto
        armarMensaje( cli, intermed, ADD_TO_CART, { "menta", "2" } ),
        //caso correcto (stock insuficiente: reserva lo que hay)
        armarMensaje( cli, intermed, ADD_TO_CART, { "fresa", "500" } ),

        // Incorporacion dinamica: SERV_02 (Bebidas y Reposteria) se registra con el sistema en marcha
        INCORPORAR_SERVIDOR,

        // ahora aparecen las categorias, los servidores y las rutas del servidor nuevo
        armarMensaje( cli, intermed, REQUEST_CATEGORIES ),
        armarMensaje( cli, intermed, REQUEST_SERVERS ),
        armarMensaje( cli, intermed, REQUEST_ROUTES ),
        // la categoria se compara sin distinguir mayusculas
        armarMensaje( cli, intermed, REQUEST_PRODUCTS, { "bebidas" } ),
        // producto de SERV_02: el intermediario sabe a quien reservarle
        armarMensaje( cli, intermed, ADD_TO_CART, { "cafe", "2" } ),
        //ERR_COMM
        armarMensaje( cli, intermed, ADD_TO_CART, { "silenciar", "1" } ),
        //Factura final
        armarMensaje( cli, intermed, REQUEST_FACTURA ),
        // bitacora: los ultimos 25 eventos
        armarMensaje( cli, intermed, REQUEST_LOG, { "25" } )
    };

    for ( const auto & peticion : peticiones ) {
        if ( peticion == INCORPORAR_SERVIDOR ) {
            printf( "\n--- se incorpora un segundo servidor de productos con el sistema en marcha ---\n\n" );
            if ( incorporarServidor ) incorporarServidor();
            // tiempo para que el servidor nuevo se registre antes de la siguiente peticion
            std::this_thread::sleep_for( std::chrono::milliseconds( 400 ) );
            continue;
        }
        pedir( peticion );
    }

    // Fin del guion de pruebas
    rotular( ID_CLIENTE, ID_INTERMEDIARIO, "EXIT", "cliente termino su guion, avisa cierre" );
    buzon_.Enviar( "EXIT", canal( ID_CLIENTE, ID_INTERMEDIARIO ) );
}
