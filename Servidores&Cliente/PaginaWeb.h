#ifndef PAGINAWEB_H
#define PAGINAWEB_H

#include <string>
#include <vector>
#include "Vistas.h"

// arma el html de las paginas. no sabe de donde vienen los datos: recibe las vistas ya llenas
class PaginaWeb {
public:
    std::string inventario(const std::vector<BodegaVista>& bodegas);
    std::string error(int codigo, const std::string& mensaje);

    // pagina completa con encabezado alrededor de un contenido ya armado
    std::string envolver(const std::string& titulo, const std::string& contenido);

    // evita que un texto se interprete como etiquetas dentro de la pagina
    std::string escapar(const std::string& texto);

private:
    std::string seccionBodega(const BodegaVista& bodega);
    std::string tablaProductos(const CategoriaVista& categoria);
};

#endif