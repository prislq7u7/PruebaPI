#include "AtencionCliente.h"
#include "Validaciones.h"
#include <map>
#include <sstream>
#include <cstdio>

static bool esNumero(const std::string& texto) {
    if (texto.empty() || texto.size() > 9) return false;
    for (char c : texto) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// precio con el formato que valida validarPrecio, por ejemplo "1500.00" o "3.5", a centavos
static uint32_t textoACentavos(const std::string& precio) {
    size_t punto = precio.find('.');
    uint32_t entero = (uint32_t)std::stoul(precio.substr(0, punto));
    uint32_t decimales = 0;
    if (punto != std::string::npos) {
        std::string parte = precio.substr(punto + 1);
        if (parte.size() == 1) parte += "0";
        decimales = (uint32_t)std::stoul(parte);
    }
    return entero * 100 + decimales;
}

// el total de una factura puede pasar de uint32_t, por eso este formato usa uint64_t
static std::string centavosATexto(uint64_t centavos) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llu.%02llu",
             (unsigned long long)(centavos / 100), (unsigned long long)(centavos % 100));
    return buf;
}

AtencionCliente::AtencionCliente(TablaRutas& tabla, Carritos& carritos, Bitacora& bitacora)
    : tabla_(tabla), carritos_(carritos), bitacora_(bitacora), enrutador_(tabla, bitacora) {}

RespuestaHttp AtencionCliente::responder(const std::string& peticion) {
    std::string ruta = extraerRuta(peticion);
    if (ruta == "/") return inicio();
    if (ruta == "/sesion") return sesion();
    if (ruta == "/categorias") return categorias();
    if (ruta == "/productos") return productos(parametroUrl(peticion, "categoria"));
    if (ruta == "/agregar") {
        return agregar(parametroUrl(peticion, "sesion"), parametroUrl(peticion, "producto"),
                       parametroUrl(peticion, "cantidad"));
    }
    if (ruta == "/factura") return factura(parametroUrl(peticion, "sesion"));
    if (ruta == "/cancelar") return cancelar(parametroUrl(peticion, "sesion"));
    if (ruta == "/servidores") return servidores();
    if (ruta == "/rutas") return rutas();
    if (ruta == "/bitacora") {
        std::string desde = parametroUrl(peticion, "desde");
        return bitacora(esNumero(desde) ? std::stol(desde) : 0);
    }
    return error(404, "ruta-inexistente", "La página " + ruta + " no existe.");
}

// pagina del navegador: una seccion por servidor de productos con sus categorias y productos
RespuestaHttp AtencionCliente::inicio() {
    std::vector<std::string> lista;
    FallaServidor falla;
    enrutador_.categorias(lista, falla);

    std::map<std::string, BodegaVista> secciones;   // ordenadas por SERV_NN
    for (const auto& categoria : lista) {
        std::vector<ProductoRuta> productos;
        bool atendida;
        enrutador_.productos(categoria, productos, atendida, falla);
        for (const auto& p : productos) {
            BodegaVista& seccion = secciones[p.servidor];
            seccion.id = p.servidor;
            seccion.nombre = "Servidor " + p.servidor;
            if (seccion.categorias.empty() || seccion.categorias.back().id != categoria) {
                seccion.categorias.push_back({categoria, categoria, {}});
            }
            seccion.categorias.back().productos.push_back({p.nombre, p.precio, p.stock});
        }
    }
    std::vector<BodegaVista> vista;
    for (auto& s : secciones) vista.push_back(s.second);
    return {200, pagina_.inventario(vista)};
}

RespuestaHttp AtencionCliente::sesion() {
    int nueva = carritos_.nuevaSesion();
    bitacora_.registrar("SESION", "sesion nueva " + std::to_string(nueva));
    return respuesta(200, "Sesión", "ok", "data-sesion=\"" + std::to_string(nueva) + "\"",
                     "<p>Su número de sesión es " + std::to_string(nueva) + ".</p>\n");
}

// las categorias de todos los servidores registrados, sin repetir
RespuestaHttp AtencionCliente::categorias() {
    std::vector<std::string> lista;
    FallaServidor falla;
    if (!enrutador_.categorias(lista, falla)) return sinComunicacion(falla);

    std::ostringstream oss;
    oss << "<ul class=\"categorias\">\n";
    for (const auto& c : lista) {
        oss << "<li data-id=\"" << pagina_.escapar(c) << "\" data-nombre=\"" << pagina_.escapar(c) << "\">"
            << pagina_.escapar(c) << "</li>\n";
    }
    oss << "</ul>\n";
    return respuesta(200, "Categorías", "ok", "", oss.str());
}

// los productos de la categoria, juntando todos los servidores que la atienden
RespuestaHttp AtencionCliente::productos(const std::string& categoria) {
    if (!validarCategoria(categoria)) {
        return error(400, "categoria-invalida", "La categoría debe tener entre 3 y 20 letras.");
    }

    std::vector<ProductoRuta> lista;
    bool atendida;
    FallaServidor falla;
    if (!enrutador_.productos(categoria, lista, atendida, falla)) return sinComunicacion(falla);
    if (lista.empty()) {
        return respuesta(200, "Productos", "sin-productos", "data-categoria=\"" + pagina_.escapar(categoria) + "\"",
                         "<p>No hay productos disponibles en la categoría \"" + pagina_.escapar(categoria) + "\".</p>\n");
    }

    std::ostringstream oss;
    oss << "<table class=\"productos\">\n<tr><th>Producto</th><th>Precio</th><th>Stock</th><th>Servidor</th></tr>\n";
    for (const auto& p : lista) {
        oss << "<tr data-nombre=\"" << pagina_.escapar(p.nombre) << "\" data-precio=\"" << p.precio
            << "\" data-stock=\"" << p.stock << "\"><td>" << pagina_.escapar(p.nombre) << "</td><td>" << p.precio
            << "</td><td>" << p.stock << "</td><td>" << p.servidor << "</td></tr>\n";
    }
    oss << "</table>\n";
    return respuesta(200, "Productos de " + categoria, "ok", "", oss.str());
}

RespuestaHttp AtencionCliente::agregar(const std::string& sesionTexto, const std::string& producto,
                                       const std::string& cantidadTexto) {
    int sesionId;
    if (!leerSesion(sesionTexto, sesionId)) {
        return error(400, "sesion-invalida", "La sesión no existe; pida una nueva en /sesion.");
    }
    if (!validarProducto(producto)) {
        return error(400, "producto-invalido", "El producto solo puede tener letras, dígitos o guion.");
    }
    if (!validarCount(cantidadTexto) || std::stoi(cantidadTexto) == 0) {
        return error(400, "cantidad-invalida", "La cantidad debe estar entre 1 y 999.");
    }

    Reserva reserva;
    FallaServidor falla;
    if (!enrutador_.reservar(producto, cantidadTexto, reserva, falla)) return sinComunicacion(falla);
    if (reserva.reservado == 0) {
        return respuesta(200, "Agregar", "no-existe", "data-producto=\"" + pagina_.escapar(producto) + "\"",
                         "<p>\"" + pagina_.escapar(producto) + "\" no está disponible en ningún servidor; "
                         "la solicitud no se puede procesar.</p>\n");
    }

    ItemReservado item{reserva.nombre, reserva.reservado, textoACentavos(reserva.precio), reserva.servidor};
    if (!carritos_.agregar(sesionId, item)) {
        // la sesion vencio mientras se reservaba: lo reservado no se puede quedar fuera del stock
        enrutador_.devolver({item});
        return error(400, "sesion-invalida", "La sesión no existe; pida una nueva en /sesion.");
    }

    // la cantidad reservada puede ser menor a la pedida si no habia suficiente stock
    std::string atributos = "data-producto=\"" + pagina_.escapar(producto) + "\" data-precio=\"" + reserva.precio +
                            "\" data-cantidad=\"" + std::to_string(reserva.reservado) + "\" data-pedido=\"" + cantidadTexto +
                            "\" data-servidor=\"" + reserva.servidor + "\"";
    return respuesta(200, "Agregar", "agregado", atributos,
                     "<p>Agregado " + std::to_string(reserva.reservado) + " x " + pagina_.escapar(producto) +
                     " @ " + reserva.precio + " (" + reserva.servidor + ")</p>\n");
}

// arma la factura con lo ya reservado en el carrito, sin volver a consultar a ningun servidor
RespuestaHttp AtencionCliente::factura(const std::string& sesionTexto) {
    int sesionId;
    if (!leerSesion(sesionTexto, sesionId)) {
        return error(400, "sesion-invalida", "La sesión no existe; pida una nueva en /sesion.");
    }

    std::vector<ItemReservado> items;
    carritos_.vaciar(sesionId, items);
    if (items.empty()) {
        return respuesta(200, "Factura", "vacio", "", "<p>El carrito está vacío.</p>\n");
    }

    uint64_t total = 0;
    std::ostringstream filas;
    filas << "<table class=\"factura\">\n<tr><th>Producto</th><th>Cantidad</th><th>Precio</th><th>Subtotal</th></tr>\n";
    for (const auto& item : items) {
        uint64_t subtotal = (uint64_t)item.precioCentavos * item.cantidad;
        total += subtotal;
        filas << "<tr data-producto=\"" << pagina_.escapar(item.producto) << "\" data-cantidad=\"" << item.cantidad
              << "\" data-subtotal=\"" << centavosATexto(subtotal) << "\"><td>" << pagina_.escapar(item.producto)
              << "</td><td>" << item.cantidad << "</td><td>" << centavosATexto(item.precioCentavos)
              << "</td><td>" << centavosATexto(subtotal) << "</td></tr>\n";
    }
    filas << "</table>\n<p>Total: " << centavosATexto(total) << "</p>\n";
    bitacora_.registrar("FACTURA", "sesion " + std::to_string(sesionId) + ": " + centavosATexto(total));

    return respuesta(200, "Factura proforma", "ok", "data-total=\"" + centavosATexto(total) + "\"", filas.str());
}

// el cliente se va sin facturar: se cierra la sesion y lo reservado vuelve al stock
RespuestaHttp AtencionCliente::cancelar(const std::string& sesionTexto) {
    int sesionId;
    std::vector<ItemReservado> items;
    if (!esNumero(sesionTexto) || !carritos_.cerrar(sesionId = std::stoi(sesionTexto), items)) {
        return error(400, "sesion-invalida", "La sesión no existe.");
    }
    enrutador_.devolver(items);
    bitacora_.registrar("SESION", "sesion " + std::to_string(sesionId) + " cancelada, " +
                        std::to_string(items.size()) + " producto(s) devueltos al stock");
    return respuesta(200, "Cancelar", "cancelada", "data-devueltos=\"" + std::to_string(items.size()) + "\"",
                     "<p>Sesión cerrada; lo reservado volvió al inventario.</p>\n");
}

void AtencionCliente::devolverReservas(const std::vector<ItemReservado>& items) {
    enrutador_.devolver(items);
}

// servidores de productos registrados, con su direccion, estado y categorias
RespuestaHttp AtencionCliente::servidores() {
    std::ostringstream oss;
    oss << "<table class=\"servidores\">\n<tr><th>Servidor</th><th>Nombre</th><th>Dirección</th><th>Estado</th><th>Categorías</th></tr>\n";
    for (const auto& s : tabla_.servidores()) {
        std::string categorias;
        for (const auto& c : s.categorias) categorias += (categorias.empty() ? "" : ",") + c;
        std::string direccion = s.ip + ":" + std::to_string(s.puerto);
        oss << "<tr data-id=\"" << s.id << "\" data-direccion=\"" << direccion << "\" data-estado-servidor=\"" << s.estado
            << "\" data-categorias=\"" << categorias << "\"><td>" << s.id << "</td><td>" << pagina_.escapar(s.nombre)
            << "</td><td>" << direccion << "</td><td>" << s.estado << "</td><td>" << categorias << "</td></tr>\n";
    }
    oss << "</table>\n";
    return respuesta(200, "Servidores de productos", "ok", "", oss.str());
}

// tabla de rutas: que servidor atiende cada categoria
RespuestaHttp AtencionCliente::rutas() {
    std::ostringstream oss;
    oss << "<table class=\"rutas\">\n<tr><th>Categoría</th><th>Servidor</th><th>Dirección</th><th>Estado</th></tr>\n";
    for (const auto& r : tabla_.rutas()) {
        oss << "<tr data-categoria=\"" << r.categoria << "\" data-servidor=\"" << r.servidor << "\" data-direccion=\""
            << r.direccion << "\" data-estado-servidor=\"" << r.estado << "\"><td>" << r.categoria << "</td><td>"
            << r.servidor << "</td><td>" << r.direccion << "</td><td>" << r.estado << "</td></tr>\n";
    }
    oss << "</table>\n";
    return respuesta(200, "Tabla de rutas", "ok", "", oss.str());
}

// eventos de la bitacora del intermediario (?desde=N para ver solo los nuevos)
RespuestaHttp AtencionCliente::bitacora(long desde) {
    long siguiente = 0;
    std::ostringstream oss;
    oss << "<pre class=\"bitacora\">\n";
    for (const auto& linea : bitacora_.desde(desde, siguiente)) oss << pagina_.escapar(linea) << "\n";
    oss << "</pre>\n";
    return respuesta(200, "Bitácora", "ok", "data-siguiente=\"" + std::to_string(siguiente) + "\"", oss.str());
}

bool AtencionCliente::leerSesion(const std::string& sesionTexto, int& sesionId) {
    if (!esNumero(sesionTexto)) return false;
    sesionId = std::stoi(sesionTexto);
    return carritos_.existe(sesionId);
}

// mismos datos que el mensaje 92: componente que no responde, tipo pendiente y reintentos
RespuestaHttp AtencionCliente::sinComunicacion(const FallaServidor& falla) {
    std::string atributos = "data-componente=\"" + falla.servidor + "\" data-tipo-pendiente=\"" +
                            std::to_string(falla.tipo) + "\" data-intentos=\"" + std::to_string(falla.intentos) + "\"";
    return respuesta(502, "Error de comunicación", "sin-comunicacion", atributos,
                     "<p>El servidor de productos " + falla.servidor + " no responde.</p>\n");
}

// el div con class respuesta y data-estado es lo que lee el cliente de terminal;
// el resto de la pagina es para que tambien se vea bien en un navegador
RespuestaHttp AtencionCliente::respuesta(int codigo, const std::string& titulo, const std::string& estado,
                                         const std::string& atributos, const std::string& contenido) {
    std::string div = "<div class=\"respuesta\" data-estado=\"" + estado + "\"" +
                      (atributos.empty() ? "" : " " + atributos) + ">\n" + contenido + "</div>\n";
    return {codigo, pagina_.envolver(titulo, div)};
}

RespuestaHttp AtencionCliente::error(int codigo, const std::string& estado, const std::string& detalle) {
    return respuesta(codigo, "Error " + std::to_string(codigo), estado,
                     "data-detalle=\"" + pagina_.escapar(detalle) + "\"",
                     "<p>" + pagina_.escapar(detalle) + "</p>\n");
}
