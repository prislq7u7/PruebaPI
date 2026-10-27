#!/bin/bash
# Crea los discos de la isla 4, uno por servidor de productos.
# Cada disco tiene dos bodegas y 100 productos, con categorias distintas
# entre servidores (asi el intermediario sabe a quien preguntar).

set -e
cd "$(dirname "$0")"

./mkfs fs1.dat.ci0123 4 1
./agregar_bodega fs1.dat.ci0123 bodegas/bodega-1.txt
./agregar_bodega fs1.dat.ci0123 bodegas/bodega-2.txt

./mkfs fs2.dat.ci0123 4 2
./agregar_bodega fs2.dat.ci0123 bodegas/bodega-3.txt
./agregar_bodega fs2.dat.ci0123 bodegas/bodega-4.txt
