#include <iostream>
#include <string>
#include <vector>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <stdexcept>

#include "SSLSocket.h"
#include "HttpUtil.h"
#include "Opciones.h"

static int PUERTO_INTERMEDIARIO = 8080;    // se cambia con --puerto
static const int MAXLINE = 4096;

static std::string hostIntermediario;   // se recibe por argumento
static std::string sesion;              // numero que reparte el intermediario en /sesion

std::string leerTodo(VSocket* s) {
    char buf[MAXLINE];
    std::string acumulado;
    try {
        size_t leidos;
        while ((leidos = s->Read(buf, MAXLINE - 1)) > 0) acumulado.append(buf, leidos);
    } catch (const std::exception&) {}
    return acumulado;
}

// hace un get al intermediario (HTTPS) y devuelve el cuerpo; codigo queda en -1 si no hubo conexion
std::string pedir(const std::string& ruta, int& codigo) {
    std::cout << "[Cliente -> Intermediario] GET " << ruta << "\n";
    VSocket* conexion = nullptr;
    try {
        conexion = new SSLSocket();
        conexion->Connect(hostIntermediario.c_str(), PUERTO_INTERMEDIARIO);
        conexion->Write(buildGET(ruta, hostIntermediario).c_str());

        std::string respuesta = leerTodo(conexion);
        delete conexion;

        codigo = getCodigoEstado(respuesta);
        return extraerCuerpo(respuesta);

    } catch (const std::exception& e) {
        std::cerr << "[Cliente] error de comunicacion: " << e.what() << "\n";
        delete conexion;
        codigo = -1;
        return "";
    }
}

// el intermediario escapa los textos para el html; aqui se devuelven a su forma original
std::string decodificarEntidades(const std::string& texto) {
    static const std::pair<const char*, char> entidades[] = {
        {"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&#39;", '\''}
    };
    std::string resultado;
    for (size_t i = 0; i < texto.size(); i++) {
        bool reemplazado = false;
        if (texto[i] == '&') {
            for (const auto& entidad : entidades) {
                size_t largo = strlen(entidad.first);
                if (texto.compare(i, largo, entidad.first) == 0) {
                    resultado += entidad.second;
                    i += largo - 1;
                    reemplazado = true;
                    break;
                }
            }
        }
        if (!reemplazado) resultado += texto[i];
    }
    return resultado;
}

std::string extraerAtributo(const std::string& tag, const std::string& nombre) {
    // se busca con un espacio antes para que data-nombre no coincida dentro de otro atributo
    std::string buscado = " " + nombre + "=\"";
    size_t pos = tag.find(buscado);
    if (pos == std::string::npos) return "";
    pos += buscado.size();
    size_t fin = tag.find("\"", pos);
    if (fin == std::string::npos) return "";
    return decodificarEntidades(tag.substr(pos, fin - pos));
}

// texto visible de una pagina: quita style y script, las etiquetas y los espacios de mas.
// se usa cuando la respuesta no trae los atributos data- que espera el cliente
std::string textoSinEtiquetas(const std::string& html) {
    std::string limpio = html;
    for (const char* bloque : {"style", "script", "title"}) {
        std::string abre = std::string("<") + bloque;
        std::string cierra = std::string("</") + bloque + ">";
        size_t inicio;
        while ((inicio = limpio.find(abre)) != std::string::npos) {
            size_t fin = limpio.find(cierra, inicio);
            limpio.erase(inicio, (fin == std::string::npos) ? std::string::npos : fin + cierra.size() - inicio);
        }
    }

    std::string texto;
    bool dentroEtiqueta = false;
    for (char c : limpio) {
        if (c == '<') { dentroEtiqueta = true; texto += ' '; continue; }
        if (c == '>') { dentroEtiqueta = false; continue; }
        if (!dentroEtiqueta) texto += c;
    }

    std::string compacto;
    for (char c : decodificarEntidades(texto)) {
        bool espacio = isspace((unsigned char)c);
        if (espacio && (compacto.empty() || compacto.back() == ' ')) continue;
        compacto += espacio ? ' ' : c;
    }
    while (!compacto.empty() && compacto.back() == ' ') compacto.pop_back();
    return compacto;
}

std::vector<std::string> extraerElementos(const std::string& html, const std::string& tag) {
    std::vector<std::string> elementos;
    std::string abre = "<" + tag + " ";
    size_t pos = 0;
    while ((pos = html.find(abre, pos)) != std::string::npos) {
        size_t fin = html.find(">", pos);
        if (fin == std::string::npos) break;
        elementos.push_back(html.substr(pos, fin - pos + 1));
        pos = fin + 1;
    }
    return elementos;
}

// muestra los errores que no son propios de cada comando
void mostrarError(const std::string& html, int codigo) {
    if (codigo == -1) {
        std::cout << "  no se pudo conectar con el intermediario\n";
        return;
    }
    std::string estado = extraerAtributo(html, "data-estado");
    if (estado == "sin-comunicacion") {
        std::cout << "  el servidor de productos no responde (" << extraerAtributo(html, "data-intentos")
                  << " reintentos)\n";
        return;
    }
    std::string detalle = extraerAtributo(html, "data-detalle");
    if (detalle.empty()) detalle = estado;
    // si la respuesta no viene del intermediario se muestra su texto sin las etiquetas
    if (detalle.empty()) detalle = textoSinEtiquetas(html);
    std::cout << "  error " << codigo << ": " << detalle << "\n";
}

bool obtenerSesion() {
    int codigo;
    std::string html = pedir("/sesion", codigo);
    sesion = (codigo == 200) ? extraerAtributo(html, "data-sesion") : "";
    if (sesion.empty()) {
        mostrarError(html, codigo);
        return false;
    }
    std::cout << "  sesion " << sesion << "\n";
    return true;
}

// pide la ruta y, si el intermediario ya no reconoce la sesion, pide una nueva y repite una vez
std::string pedirConSesion(const std::string& rutaSinSesion, int& codigo) {
    std::string separador = (rutaSinSesion.find('?') == std::string::npos) ? "?" : "&";
    std::string html = pedir(rutaSinSesion + separador + "sesion=" + sesion, codigo);

    if (extraerAtributo(html, "data-estado") == "sesion-invalida") {
        std::cout << "  la sesion ya no existe en el intermediario, se pide una nueva\n";
        if (!obtenerSesion()) return html;
        html = pedir(rutaSinSesion + separador + "sesion=" + sesion, codigo);
    }
    return html;
}

void comandoCategorias() {
    int codigo;
    std::string html = pedir("/categorias", codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    // el id es lo que se escribe para pedir los productos de la categoria;
    // el nombre solo se muestra si es distinto del id
    for (auto& li : extraerElementos(html, "li")) {
        std::string id = extraerAtributo(li, "data-id");
        std::string nombre = extraerAtributo(li, "data-nombre");
        std::cout << "  " << id;
        if (!nombre.empty() && nombre != id) std::cout << "  " << nombre;
        std::cout << "\n";
    }
}

void comandoProductos(const std::string& idCategoria) {
    int codigo;
    std::string html = pedir("/productos?categoria=" + urlEncode(idCategoria), codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    if (extraerAtributo(html, "data-estado") == "sin-productos") {
        std::cout << "  no hay productos disponibles en la categoria \"" << idCategoria << "\"\n";
        return;
    }
    for (auto& fila : extraerElementos(html, "tr")) {
        std::cout << "  " << extraerAtributo(fila, "data-nombre")
                  << " | precio: " << extraerAtributo(fila, "data-precio")
                  << " | stock: " << extraerAtributo(fila, "data-stock") << "\n";
    }
}

void comandoAgregar(const std::string& args) {
    size_t espacio = args.find_last_of(' ');
    if (espacio == std::string::npos) { std::cout << "  uso: AGREGAR <nombre_producto> <cantidad>\n"; return; }
    std::string nombre = args.substr(0, espacio);
    std::string cantidad = args.substr(espacio + 1);

    int codigo;
    std::string html = pedirConSesion("/agregar?producto=" + urlEncode(nombre) + "&cantidad=" + urlEncode(cantidad), codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    std::string estado = extraerAtributo(html, "data-estado");
    if (estado == "agregado") {
        std::string reservado = extraerAtributo(html, "data-cantidad");
        std::string pedido = extraerAtributo(html, "data-pedido");
        std::cout << "  agregado " << reservado << " x " << nombre << " @ " << extraerAtributo(html, "data-precio") << "\n";
        if (reservado != pedido) {
            std::cout << "  solo habia " << reservado << " disponibles de los " << pedido << " pedidos\n";
        }
    } else if (estado == "sin-stock") {
        std::cout << "  no hay stock de " << nombre << "\n";
    } else if (estado == "no-existe") {
        std::cout << "  " << nombre << " no existe\n";
    } else {
        mostrarError(html, codigo);
    }
}

void comandoFactura() {
    int codigo;
    std::string html = pedirConSesion("/factura", codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    if (extraerAtributo(html, "data-estado") == "vacio") {
        std::cout << "  el carrito esta vacio\n";
        return;
    }

    std::cout << "\n--- Factura TicAmazon ---\n";
    for (auto& fila : extraerElementos(html, "tr")) {
        std::cout << "  " << extraerAtributo(fila, "data-producto")
                  << " x" << extraerAtributo(fila, "data-cantidad")
                  << " = " << extraerAtributo(fila, "data-subtotal") << "\n";
    }
    std::cout << "TOTAL: " << extraerAtributo(html, "data-total") << "\n";
}

// servidores de productos registrados en el intermediario
void comandoServidores() {
    int codigo;
    std::string html = pedir("/servidores", codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    auto filas = extraerElementos(html, "tr");
    if (filas.empty()) std::cout << "  no hay servidores de productos registrados\n";
    for (auto& fila : filas) {
        std::cout << "  " << extraerAtributo(fila, "data-id") << " | " << extraerAtributo(fila, "data-direccion")
                  << " | " << extraerAtributo(fila, "data-estado-servidor")
                  << " | " << extraerAtributo(fila, "data-categorias") << "\n";
    }
}

// tabla de rutas: que servidor atiende cada categoria
void comandoRutas() {
    int codigo;
    std::string html = pedir("/rutas", codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    auto filas = extraerElementos(html, "tr");
    if (filas.empty()) std::cout << "  la tabla de rutas esta vacia\n";
    for (auto& fila : filas) {
        std::cout << "  " << extraerAtributo(fila, "data-categoria") << " -> " << extraerAtributo(fila, "data-servidor")
                  << " (" << extraerAtributo(fila, "data-direccion") << ", "
                  << extraerAtributo(fila, "data-estado-servidor") << ")\n";
    }
}

// ultimos eventos de la bitacora del intermediario
void comandoBitacora() {
    int codigo;
    std::string html = pedir("/bitacora", codigo);
    if (codigo != 200) { mostrarError(html, codigo); return; }

    size_t inicio = html.find("<pre class=\"bitacora\">");
    size_t fin = html.find("</pre>", inicio);
    if (inicio == std::string::npos || fin == std::string::npos) return;
    inicio = html.find('\n', inicio) + 1;
    std::string texto = decodificarEntidades(html.substr(inicio, fin - inicio));
    // solo las ultimas 20 lineas, la bitacora completa se ve en el navegador
    std::vector<std::string> lineas;
    size_t pos = 0, salto;
    while ((salto = texto.find('\n', pos)) != std::string::npos) { lineas.push_back(texto.substr(pos, salto - pos)); pos = salto + 1; }
    for (size_t i = lineas.size() > 20 ? lineas.size() - 20 : 0; i < lineas.size(); i++) std::cout << "  " << lineas[i] << "\n";
}

int main(int argc, char* argv[]) {
    if (tieneOpcion(argc, argv, "--ayuda")) {
        std::cout << "uso: " << argv[0] << " [--intermediario IP] [--puerto N]\n\n"
                  << "  --intermediario IP  por defecto: esta misma computadora (tambien se puede poner la ip sola)\n"
                  << "  --puerto N          por defecto 8080\n\n"
                  << "ejemplos:\n  " << argv[0] << "\n  " << argv[0] << " --intermediario 10.1.35.20\n";
        return 0;
    }
    hostIntermediario = opcion(argc, argv, "--intermediario", argumentoSuelto(argc, argv));
    if (hostIntermediario.empty()) hostIntermediario = ipLocal();
    PUERTO_INTERMEDIARIO = std::atoi(opcion(argc, argv, "--puerto", "8080").c_str());
    std::cout << "intermediario: " << hostIntermediario << ":" << PUERTO_INTERMEDIARIO << "\n";

    std::cout << "---TicAmazon Cliente---\n";
    if (!obtenerSesion()) {
        std::cerr << "no se pudo obtener una sesion del intermediario\n";
        return 1;
    }
    std::cout << "Comandos: CATEGORIAS | <idCategoria> | AGREGAR <nombre> <cantidad> | FACTURA | SERVIDORES | RUTAS | BITACORA | salir\n";

    std::string linea;
    while (std::cout << "\n> " && std::getline(std::cin, linea)) {
        std::string comando = linea;
        for (char& c : comando) c = (char)tolower((unsigned char)c);

        if (comando == "salir") break;
        if (comando.empty()) continue;
        if (comando == "categorias") { comandoCategorias(); continue; }
        if (comando.rfind("agregar ", 0) == 0) { comandoAgregar(linea.substr(8)); continue; }
        if (comando == "factura") { comandoFactura(); continue; }
        if (comando == "servidores") { comandoServidores(); continue; }
        if (comando == "rutas") { comandoRutas(); continue; }
        if (comando == "bitacora") { comandoBitacora(); continue; }

        comandoProductos(linea); // cualquier otra cosa se trata como id de categoria
    }

    // al salir sin facturar, lo reservado tiene que volver al inventario
    int codigo;
    pedir("/cancelar?sesion=" + sesion, codigo);

    std::cout << "\n---Fin---\n";
    return 0;
}