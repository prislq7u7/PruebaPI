#ifndef PROTOCOLO_H
#define PROTOCOLO_H

#include <string>
#include <vector>
#include <cstdint>

// el protocolo no tiene tamano maximo de mensaje; MAX_LINEA solo evita llenar la memoria
// si llega una linea que nunca termina en '\n'
const size_t MAX_LINEA = 65536;

const std::string ID_INTERMEDIARIO = "INT_04";
const std::string ID_CLIENTE = "CLI_04";

// cada servidor de productos es SERV_NN, con NN = numero de servidor de su disco (fs1 -> SERV_01)
std::string idServidor(int numero);

// puertos: clientes por HTTP, registro de servidores (70/72) y servidores de productos (9050 + NN)
const int PUERTO_CLIENTES         = 8080;
const int PUERTO_REGISTRO         = 9040;
const int PUERTO_BASE_SERVIDOR    = 9050;

// cada implementacion define su buffer: una respuesta que no cabe se contesta con 91 (ERR_SIZE)
const size_t MAX_RESPUESTA        = 8192;

// cliente <-> intermediario (HTTP)
const int TIPO_REQUEST_CATEGORIES = 10;
const int TIPO_CATEGORY_LIST      = 11;
const int TIPO_REQUEST_PRODUCTS   = 20;
const int TIPO_PRODUCT_LIST       = 22;
const int TIPO_PRODUCT_LIST_EMPTY = 23;
const int TIPO_ADD_TO_CART        = 30;
const int TIPO_CART_UPDATED       = 32;
const int TIPO_REQUEST_FACTURA    = 40;
const int TIPO_FACTURA            = 41;   // 41/<total>/<count>/<producto,cantidad,subtotal;...>
const int TIPO_CART_EMPTY         = 42;

// intermediario <-> servidor de productos (10, 11, 20 y 22 tambien se usan aqui)
const int TIPO_RESERVE_STOCK      = 50;
const int TIPO_RESERVED           = 51;   // si el producto no existe: 51/<producto>/0.00/0
const int TIPO_RELEASE_STOCK      = 52;   // INT_04|SERV_NN/52/<producto>/<cantidad>: devolver lo reservado
const int TIPO_RELEASED           = 53;   // SERV_NN|INT_04/53/<producto>/<cantidad devuelta>

// registro dinamico: servidor de productos -> intermediario, puerto PUERTO_REGISTRO
const int TIPO_REGISTER_SERVER    = 70;   // SERV_NN|INT_04/70/<nombre>/<ip>/<puerto>/<cat,cat,...>
const int TIPO_REGISTER_ACK       = 71;   // INT_04|SERV_NN/71/<count>
const int TIPO_UNREGISTER_SERVER  = 72;   // SERV_NN|INT_04/72: el servidor se cierra (responde 71/0)

// consultas del cliente al intermediario (HTTP)
const int TIPO_REQUEST_SERVERS    = 73;   // CLI_04|INT_04/73
const int TIPO_SERVER_LIST        = 74;   // 74/<count>/<SERV_NN,nombre,ip,puerto;...>
const int TIPO_REQUEST_ROUTES     = 75;   // CLI_04|INT_04/75
const int TIPO_ROUTE_TABLE        = 76;   // 76/<count>/<categoria,SERV_NN;...>
const int TIPO_REQUEST_LOG        = 77;   // CLI_04|INT_04/77/<n>
const int TIPO_LOG_LIST           = 78;   // 78/<count>/<timestamp,origen,destino,tipo,estado;...>

// descubrimiento por broadcast (UDP, sin conexion): el servidor de productos encuentra al intermediario
// sin que le pasen la ip, y el intermediario llama a los servidores que ya estaban corriendo
const int PUERTO_UDP_INTERMEDIARIO = 9091;  // escucha el intermediario (80)
const int PUERTO_UDP_SERVIDOR      = 9092;  // escuchan los servidores de productos (82)
const std::string ID_BROADCAST     = "BROADCAST";
const int TIPO_BUSCO_INTERMEDIARIO = 80;    // SERV_NN|BROADCAST/80
const int TIPO_SOY_INTERMEDIARIO   = 81;    // INT_04|SERV_NN/81/<puerto de registro>
const int TIPO_BUSCO_SERVIDORES    = 82;    // INT_04|BROADCAST/82/<puerto de registro>
const int TIPO_SOY_SERVIDOR        = 83;    // SERV_NN|INT_04/83/<puerto>

const int TIPO_ERR_FORMATO        = 90;
const int TIPO_ERR_TAMANO         = 91;
const int TIPO_ERR_COMM           = 92;

struct Mensaje {
    std::string origen;
    std::string destino;
    int tipo;
    std::vector<std::string> campos;
};

struct ItemCarrito {
    std::string producto;
    int cantidad;
    double precio;
};

// arma ORIGEN|DESTINO/TIPO/campo1/campo2/...
std::string construirMensaje(const std::string& origen, const std::string& destino,
                              int tipo, const std::vector<std::string>& campos);

// parsea texto crudo a Mensaje. lanza runtime_error si el formato basico esta roto
Mensaje parsearMensaje(const std::string& texto);

// las lineas de bitacora llevan '/', '|' y ';', que romperian el mensaje 75.
// se codifican como en las direcciones web: % -> %25, / -> %2F, | -> %7C, ; -> %3B
std::string codificarCampo(const std::string& texto);
std::string decodificarCampo(const std::string& texto);

// precio en centavos a texto con dos decimales, por ejemplo 150000 a "1500.00"
std::string precioATexto(uint32_t centavos);

#endif