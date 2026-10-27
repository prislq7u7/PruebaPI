#include <sstream>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <thread>

#include "Intermediario.h"
#include "Bitacora.h"
#include "Validaciones.h"

namespace {
    // Cuando un servidor no contesta se reportan 2 intentos en el ERR_COMM (92).
    // Cada intento espera 15 sondeos de 20 ms = 300 ms.
    const int REINTENTOS = 2;
    const int SONDEOS_POR_INTENTO = 15;
    const int ESPERA_SONDEO_MS = 20;
}

Intermediario::Intermediario( Buzon & buzon ) : buzon_( buzon ) {
}


// UTILIDADES

std::vector<InfoServidor> Intermediario::copiaServidores() {
    std::lock_guard<std::mutex> guard( mtxTablas_ );
    std::vector<InfoServidor> copia;
    for ( const auto & par : servidores_ ) {
        copia.push_back( par.second );
    }
    return copia;
}

// Manda un mensaje por el canal Intermediario -> destino, lo rotula en pantalla y lo anota en la bitácora.
void Intermediario::enviarA( int destinoId, const std::string & mensaje, const std::string & explicacion, bool anotar ) {
    // El protocolo no fija un tamaño máximo; el buffer del buzón sí es finito. Si el mensaje no cabe,
    // Buzon::Enviar lo cortaría en silencio, así que se responde ERR_SIZE (91) en su lugar.
    if ( static_cast<int>( mensaje.size() ) >= TAM_MAX_MENSAJE ) {
        int tipo = parsearMensaje( mensaje ).tipoMensaje;
        std::string err = armarMensaje( idTextoIntermediario(), idTexto( destinoId ), ERR_SIZE,
                                         { std::to_string( tipo < 0 ? 0 : tipo ), "mensaje",
                                           std::to_string( mensaje.size() ), std::to_string( TAM_MAX_MENSAJE - 1 ) } );
        rotular( ID_INTERMEDIARIO, destinoId, err, "respuesta demasiado grande para el buffer" );
        buzon_.Enviar( err.c_str(), canal( ID_INTERMEDIARIO, destinoId ) );
        if ( anotar ) {
            anotarEvento( ID_INTERMEDIARIO, destinoId, err, "respuesta demasiado grande para el buffer" );
        }
        return;
    }
    // Se rotula antes de enviar para que en pantalla el mensaje salga antes que el ACK del receptor
    rotular( ID_INTERMEDIARIO, destinoId, mensaje, explicacion );
    buzon_.Enviar( mensaje.c_str(), canal( ID_INTERMEDIARIO, destinoId ) );
    if ( anotar ) {
        anotarEvento( ID_INTERMEDIARIO, destinoId, mensaje, explicacion );
    }
}

void Intermediario::enviarErrFormat( int destinoId, int tipoOriginal, const std::string & campo,
                                      const std::string & valor, const std::string & explicacion ) {
    std::string err = armarMensaje( idTextoIntermediario(), idTexto( destinoId ), ERR_FORMAT,
                                     { std::to_string( tipoOriginal ), campo, valor } );
    enviarA( destinoId, err, explicacion );
}

void Intermediario::enviarErrComm( const std::string & componente, int tipoPendiente ) {
    std::string err = armarMensaje( idTextoIntermediario(), idTextoCliente(), ERR_COMM,
                                     { componente, std::to_string( tipoPendiente ), std::to_string( REINTENTOS ) } );
    enviarA( ID_CLIENTE, err, componente + " no respondio" );
}

// Le manda un mensaje a un servidor de productos y espera su respuesta.
// Devuelve false si no contestó (el que llama decide si reporta ERR_COMM).
bool Intermediario::consultarServidor( const InfoServidor & servidor, int tipo, const std::vector<std::string> & campos,
                                        const std::string & explicacion, MensajeProtocolo & respuesta ) {
    std::string mensaje = armarMensaje( idTextoIntermediario(), idTexto( servidor.id ), tipo, campos );
    enviarA( servidor.id, mensaje, explicacion );

    struct { long mtype; char texto[ TAM_MAX_MENSAJE ]; } resp;
    int st = buzon_.RecibirConEspera( &resp, sizeof( resp.texto ), canal( servidor.id, ID_INTERMEDIARIO ),
                                       REINTENTOS * SONDEOS_POR_INTENTO, ESPERA_SONDEO_MS );
    if ( st == -1 ) {
        return false;
    }

    std::string linea( resp.texto );
    anotarEvento( servidor.id, ID_INTERMEDIARIO, linea, "respuesta de " + servidor.nombre );
    respuesta = parsearMensaje( linea );
    return true;
}


// REGISTRO DINAMICO DE SERVIDORES DE PRODUCTOS (hilo de registro)

// Escucha CANAL_REGISTRO mientras el intermediario esté corriendo. Un servidor puede
// mandar su REGISTER_SERVER (70) en cualquier momento.
void Intermediario::escucharRegistros() {
    try {
        while ( true ) {
            struct { long mtype; char texto[ TAM_MAX_MENSAJE ]; } msg;
            buzon_.Recibir( &msg, sizeof( msg.texto ), CANAL_REGISTRO );
            std::string linea( msg.texto );

            if ( linea == "EXIT" ) {
                return;
            }
            procesarRegistro( linea );
        }
    } catch ( const std::runtime_error & ) {
        return; // el buzon fue destruido: salida de respaldo
    }
}

// REGISTER_SERVER (70): SERV_NN|INT_04/70/<nombre>/<ip>/<puerto>/<lista_categorias>
// Valida los campos; si están bien guarda el servidor y responde REGISTER_ACK (71),
// si no responde ERR_FORMAT (90) y no registra nada.
void Intermediario::procesarRegistro( const std::string & linea ) {
    MensajeProtocolo m = parsearMensaje( linea );
    int id = idInternoServidor( m.origen );

    if ( id < 0 || m.tipoMensaje != REGISTER_SERVER || m.destino != idTextoIntermediario() ) {
        // Sin un SERV_NN válido no hay a quién responder.
        rotular( ID_INTERMEDIARIO, ID_INTERMEDIARIO, linea, "mensaje de registro invalido: se ignora" );
        return;
    }

    anotarEvento( id, ID_INTERMEDIARIO, linea, "registro recibido" );

    std::string campoMalo, valorMalo;
    if ( m.campos.size() != 4 ) {
        campoMalo = "campos";
        valorMalo = std::to_string( m.campos.size() );
    } else if ( !validarNombre( m.campos[ 0 ] ) ) {
        campoMalo = "nombre";
        valorMalo = m.campos[ 0 ];
    } else if ( !validarIp( m.campos[ 1 ] ) ) {
        campoMalo = "ip";
        valorMalo = m.campos[ 1 ];
    } else if ( !validarPuerto( m.campos[ 2 ] ) ) {
        campoMalo = "puerto";
        valorMalo = m.campos[ 2 ];
    } else if ( !validarListaCategorias( m.campos[ 3 ] ) ) {
        campoMalo = "categorias";
        valorMalo = m.campos[ 3 ];
    }

    if ( !campoMalo.empty() ) {
        enviarErrFormat( id, REGISTER_SERVER, campoMalo, valorMalo, "registro rechazado: " + campoMalo + " no cumple regex" );
        return;
    }

    InfoServidor info;
    info.id = id;
    info.nombre = m.campos[ 0 ];
    info.ip = m.campos[ 1 ];
    info.puerto = std::stoi( m.campos[ 2 ] );
    info.categorias = separar( m.campos[ 3 ], ',' );

    // El ACK se manda con la tabla bloqueada: el hilo principal no puede ver al servidor
    // nuevo (ni escribirle) antes de que el servidor haya recibido su REGISTER_ACK.
    std::lock_guard<std::mutex> guard( mtxTablas_ );
    servidores_[ id ] = info;
    std::string ack = armarMensaje( idTextoIntermediario(), idTexto( id ), REGISTER_ACK,
                                     { std::to_string( info.categorias.size() ) } );
    enviarA( id, ack, "registro confirmado a " + info.nombre );
}


// PETICIONES DEL CLIENTE

// REQUEST_CATEGORIES (10): no lleva campos. Se le pide la lista a cada servidor
// registrado, se juntan sin repetir y se devuelven al cliente como un solo CATEGORY_LIST (11).
void Intermediario::manejarRequestCategories() {
    std::vector<InfoServidor> servidores = copiaServidores();

    std::vector<std::string> categorias;
    std::set<std::string> vistas;
    bool alguno = false;
    std::string primerFallo;

    for ( const auto & s : servidores ) {
        MensajeProtocolo respuesta;
        if ( !consultarServidor( s, REQUEST_CATEGORIES, {}, "pidiendo categorias a " + s.nombre, respuesta ) ) {
            if ( primerFallo.empty() ) primerFallo = idTexto( s.id );
            continue;
        }
        alguno = true;
        if ( !respuesta.campos.empty() ) {
            for ( const auto & c : separar( respuesta.campos[ 0 ], ',' ) ) {
                if ( vistas.insert( aMinusculas( c ) ).second ) categorias.push_back( c );
            }
        }
    }

    if ( !servidores.empty() && !alguno ) {
        enviarErrComm( primerFallo, REQUEST_CATEGORIES );
        return;
    }

    std::string salida = armarMensaje( idTextoIntermediario(), idTextoCliente(), CATEGORY_LIST, { unir( categorias, ',' ) } );
    enviarA( ID_CLIENTE, salida, "categorias reenviadas al cliente" );
}

// REQUEST_PRODUCTS (20): un campo, la categoria. Se busca en la tabla de rutas cuáles
// servidores la atienden (sin distinguir mayúsculas) y se les pregunta a todos.
void Intermediario::manejarRequestProducts( const std::string & categoria ) {
    if ( !validarCategoria( categoria ) ) {
        enviarErrFormat( ID_CLIENTE, REQUEST_PRODUCTS, "categoria", categoria, "categoria no cumple regex" );
        return;
    }

    struct Destino {
        InfoServidor servidor;
        std::string categoriaRegistrada;   // como la publicó ese servidor, es lo que entiende
    };
    std::vector<Destino> destinos;
    const std::string buscada = aMinusculas( categoria );
    for ( const auto & s : copiaServidores() ) {
        for ( const auto & c : s.categorias ) {
            if ( aMinusculas( c ) == buscada ) {
                destinos.push_back( { s, c } );
                break;
            }
        }
    }

    std::ostringstream textoVacio;
    textoVacio << "No hay productos disponibles en la categoria \"" << categoria << "\"";

    if ( destinos.empty() ) {
        // Ningun servidor registrado la atiende; no hace falta ni preguntarle a nadie.
        std::string vacio = armarMensaje( idTextoIntermediario(), idTextoCliente(), PRODUCT_LIST_EMPTY, { textoVacio.str() } );
        enviarA( ID_CLIENTE, vacio, "ningun servidor atiende esa categoria" );
        return;
    }

    std::vector<std::string> entradas;
    bool alguno = false;
    std::string primerFallo;

    for ( const auto & d : destinos ) {
        MensajeProtocolo respuesta;
        if ( !consultarServidor( d.servidor, REQUEST_PRODUCTS, { d.categoriaRegistrada },
                                  "reenviando a " + d.servidor.nombre, respuesta ) ) {
            if ( primerFallo.empty() ) primerFallo = idTexto( d.servidor.id );
            continue;
        }
        alguno = true;

        bool vacio = respuesta.campos.size() < 2 || respuesta.campos[ 0 ] == "0";
        if ( vacio ) continue;

        for ( const auto & entrada : separar( respuesta.campos[ 1 ], ';' ) ) {
            entradas.push_back( entrada );
            // Se recuerda en que servidor está cada producto (si está en varios, queda el de menor SERV_NN).
            std::string nombreProducto = entrada.substr( 0, entrada.find( ',' ) );
            productoServidor_.emplace( nombreProducto, d.servidor.id );
        }
    }

    if ( !alguno ) {
        enviarErrComm( primerFallo, REQUEST_PRODUCTS );
        return;
    }

    if ( entradas.empty() ) {
        std::string salidaVacia = armarMensaje( idTextoIntermediario(), idTextoCliente(), PRODUCT_LIST_EMPTY, { textoVacio.str() } );
        enviarA( ID_CLIENTE, salidaVacia, "categoria vacia" );
    } else {
        std::string salida = armarMensaje( idTextoIntermediario(), idTextoCliente(), PRODUCT_LIST,
                                            { std::to_string( entradas.size() ), unir( entradas, ';' ) } );
        enviarA( ID_CLIENTE, salida, "listado reenviado al cliente" );
    }
}

// ADD_TO_CART (30): se reserva en el servidor donde se vio el producto. Si el producto
// no se ha visto en ningún listado, se prueba con los servidores en orden y se queda
// con el primero que reserve algo.
void Intermediario::manejarAddToCart( const std::string & producto, const std::string & cantidadTexto ) {
    if ( !validarProducto( producto ) ) {
        enviarErrFormat( ID_CLIENTE, ADD_TO_CART, "producto", producto, "producto no cumple regex" );
        return;
    }
    if ( !validarCount( cantidadTexto ) ) {
        enviarErrFormat( ID_CLIENTE, ADD_TO_CART, "cantidad", cantidadTexto, "cantidad no cumple regex" );
        return;
    }

    std::vector<InfoServidor> servidores = copiaServidores();
    std::vector<InfoServidor> orden;
    auto conocido = productoServidor_.find( producto );
    if ( conocido != productoServidor_.end() ) {
        for ( const auto & s : servidores ) {
            if ( s.id == conocido->second ) orden.push_back( s );
        }
    }
    if ( orden.empty() ) {
        orden = servidores;
    }

    // Lo que se le responde al cliente si ninguno reserva: producto inexistente (0.00/0)
    // o existente pero sin stock (precio real/0).
    std::vector<std::string> resultado = { producto, "0.00", "0" };

    for ( const auto & s : orden ) {
        MensajeProtocolo respuesta;
        if ( !consultarServidor( s, RESERVE_STOCK, { producto, cantidadTexto }, "reservando en " + s.nombre, respuesta ) ) {
            enviarErrComm( idTexto( s.id ), RESERVE_STOCK );
            return;
        }

        if ( respuesta.tipoMensaje != RESERVED || respuesta.campos.size() < 3 ) continue;

        int cantidadReservada = std::stoi( respuesta.campos[ 2 ] );
        if ( cantidadReservada > 0 ) {
            carrito_.push_back( { respuesta.campos[ 0 ], std::stod( respuesta.campos[ 1 ] ), cantidadReservada } );
            productoServidor_[ producto ] = s.id;
            resultado = respuesta.campos;
            break;
        }
        if ( respuesta.campos[ 1 ] != "0.00" ) {
            resultado = respuesta.campos;
        }
    }

    std::string salida = armarMensaje( idTextoIntermediario(), idTextoCliente(), CART_UPDATED, resultado );
    enviarA( ID_CLIENTE, salida, "carrito actualizado" );
}

void Intermediario::manejarRequestFactura() {
    if ( carrito_.empty() ) {
        std::string vacio = armarMensaje( idTextoIntermediario(), idTextoCliente(), CART_EMPTY, { "El carrito esta vacio" } );
        enviarA( ID_CLIENTE, vacio, "carrito vacio" );
        return;
    }

    std::ostringstream detalle;
    double total = 0.0;
    for ( const auto & item : carrito_ ) {
        double subtotal = item.precio * item.cantidad;
        total += subtotal;
        detalle << item.nombre << "," << item.cantidad << ","
                 << std::fixed << std::setprecision( 2 ) << subtotal << ";";
    }
    std::ostringstream totalTexto;
    totalTexto << std::fixed << std::setprecision( 2 ) << total;

    std::string factura = armarMensaje( idTextoIntermediario(), idTextoCliente(), FACTURA,
                                          { detalle.str(), totalTexto.str() } );
    enviarA( ID_CLIENTE, factura, "factura emitida" );

    carrito_.clear();
}

// REQUEST_SERVERS (73): responde SERVER_LIST (74): <count>/<SERV_NN,nombre,ip,puerto;...>
void Intermediario::manejarRequestServers() {
    std::vector<std::string> entradas;
    for ( const auto & s : copiaServidores() ) {
        entradas.push_back( idTexto( s.id ) + "," + s.nombre + "," + s.ip + "," + std::to_string( s.puerto ) );
    }
    std::string salida = armarMensaje( idTextoIntermediario(), idTextoCliente(), SERVER_LIST,
                                        { std::to_string( entradas.size() ), unir( entradas, ';' ) } );
    enviarA( ID_CLIENTE, salida, "servidores registrados" );
}

// REQUEST_ROUTES (75): responde ROUTE_TABLE (76): <count>/<categoria,SERV_NN;...>
// La tabla de rutas se arma con las categorías que publicó cada servidor al registrarse.
void Intermediario::manejarRequestRoutes() {
    std::vector<std::string> entradas;
    for ( const auto & s : copiaServidores() ) {
        for ( const auto & c : s.categorias ) {
            entradas.push_back( c + "," + idTexto( s.id ) );
        }
    }
    std::string salida = armarMensaje( idTextoIntermediario(), idTextoCliente(), ROUTE_TABLE,
                                        { std::to_string( entradas.size() ), unir( entradas, ';' ) } );
    enviarA( ID_CLIENTE, salida, "tabla de rutas" );
}

// REQUEST_LOG (77): un campo, n. Responde LOG_LIST (78) con los últimos n eventos,
// del más reciente al más antiguo. Si no caben todos en un mensaje se quitan los más antiguos.
// La consulta se anota en la bitácora después de responderla.
void Intermediario::manejarRequestLog( const std::string & linea, const std::vector<std::string> & campos ) {
    if ( campos.size() != 1 || !validarCount( campos[ 0 ] ) ) {
        anotarEvento( ID_CLIENTE, ID_INTERMEDIARIO, linea, "peticion recibida" );
        enviarErrFormat( ID_CLIENTE, REQUEST_LOG, "n", campos.empty() ? "" : campos[ 0 ], "n no cumple regex" );
        return;
    }

    std::vector<std::string> entradas = ultimosEventos( std::stoi( campos[ 0 ] ) );
    std::string salida;
    while ( true ) {
        salida = armarMensaje( idTextoIntermediario(), idTextoCliente(), LOG_LIST,
                                { std::to_string( entradas.size() ), unir( entradas, ';' ) } );
        if ( static_cast<int>( salida.size() ) < TAM_MAX_MENSAJE || entradas.empty() ) break;
        entradas.pop_back();
    }

    enviarA( ID_CLIENTE, salida, "bitacora entregada", false );
    anotarEvento( ID_CLIENTE, ID_INTERMEDIARIO, linea, "consulta de bitacora" );
    anotarEvento( ID_INTERMEDIARIO, ID_CLIENTE, salida, "bitacora entregada" );
}


// Atiende las peticiones del cliente hasta que llegue EXIT.
// Devuelve true si el cierre fue por EXIT, false si el buzon fue destruido.
bool Intermediario::atenderClientes() {
    while ( true ) {
        struct { long mtype; char texto[ TAM_MAX_MENSAJE ]; } msg;

        try {
            buzon_.Recibir( &msg, sizeof( msg.texto ), canal( ID_CLIENTE, ID_INTERMEDIARIO ) );
        } catch ( const std::runtime_error & ) {
            return false; // el buzon fue destruido: salida de respaldo
        }

        std::string linea( msg.texto );
        if ( linea == "EXIT" ) {
            rotularAck( ID_INTERMEDIARIO, ID_CLIENTE, "orden de cierre recibida del cliente" );
            return true;
        }
        rotularAck( ID_INTERMEDIARIO, ID_CLIENTE, "peticion recibida" );

        MensajeProtocolo mensaje = parsearMensaje( linea );

        // La consulta de bitácora se anota al responderla (ver manejarRequestLog).
        if ( mensaje.tipoMensaje != REQUEST_LOG ) {
            anotarEvento( ID_CLIENTE, ID_INTERMEDIARIO, linea, "peticion recibida" );
        }

        switch ( mensaje.tipoMensaje ) {
            case REQUEST_CATEGORIES:
                manejarRequestCategories();
                break;
            case REQUEST_PRODUCTS:
                if ( !mensaje.campos.empty() ) {
                    manejarRequestProducts( mensaje.campos[ 0 ] );
                }
                break;
            case ADD_TO_CART:
                if ( mensaje.campos.size() >= 2 ) {
                    manejarAddToCart( mensaje.campos[ 0 ], mensaje.campos[ 1 ] );
                }
                break;
            case REQUEST_FACTURA:
                manejarRequestFactura();
                break;
            case REQUEST_SERVERS:
                manejarRequestServers();
                break;
            case REQUEST_ROUTES:
                manejarRequestRoutes();
                break;
            case REQUEST_LOG:
                manejarRequestLog( linea, mensaje.campos );
                break;
            default:

                rotularAck( ID_INTERMEDIARIO, ID_CLIENTE, "tipo de mensaje no reconocido (todavia)" );
                break;
        }
    }
}

void Intermediario::ejecutar() {
    std::thread hRegistro( &Intermediario::escucharRegistros, this );

    bool cierreNormal = atenderClientes();

    if ( cierreNormal ) {
        // EXIT es señal de control interna: se reenvia a todos los servidores registrados
        // y al hilo de registro, para que ninguno se quede esperando.
        for ( const auto & s : copiaServidores() ) {
            rotular( ID_INTERMEDIARIO, s.id, "EXIT", "orden de cierre: " + s.nombre + " termina su hilo" );
            buzon_.Enviar( "EXIT", canal( ID_INTERMEDIARIO, s.id ) );
        }
        buzon_.Enviar( "EXIT", CANAL_REGISTRO );
    }

    hRegistro.join();
}