#!/bin/bash
# Uso: ./test_fs.sh
#
# compila las herramientas, crea los dos discos y prueba el sistema de
# archivos: ids, listados, extraer/agregar, borrar, errores, disco lleno y
# archivos grandes que usan el indirecto doble.

set -u
cd "$(dirname "$0")"

PASARON=0
FALLARON=0
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

verde() { printf "\033[32m%s\033[0m\n" "$1"; }
rojo()  { printf "\033[31m%s\033[0m\n" "$1"; }

verificar() {
    # verifica "nombre del caso" "texto que debe aparecer" "$salida"
    local nombre="$1" esperado="$2" salida="$3"
    if echo "$salida" | grep -qF -- "$esperado"; then
        verde "  OK   $nombre"
        PASARON=$((PASARON+1))
    else
        rojo  "  FAIL $nombre  (no se encontro: \"$esperado\")"
        echo "$salida" | head -5 | sed 's/^/         /'
        FALLARON=$((FALLARON+1))
    fi
}

libres() { ./dump_fs "$1" | grep -o "libres [0-9]*" | head -1 | cut -d' ' -f2; }

echo "--- 1. Compilando ---"
make clean > /dev/null
if ! make > "$TMP/build.log" 2>&1; then
    rojo "FALLO LA COMPILACION"; cat "$TMP/build.log"; exit 1
fi
grep -qi warning "$TMP/build.log" && { rojo "hay warnings:"; grep -i warning "$TMP/build.log"; }
verde "compilo"

echo ""
echo "--- 2. Discos de la isla 4 ---"
rm -f fs1.dat.ci0123 fs2.dat.ci0123
SALIDA=$(./crear_discos.sh 2>&1)
verificar "bodega-1 entra al disco 1 con id I04-S1-B01" "bodega I04-S1-B01 (50 productos)" "$SALIDA"
verificar "bodega-2 entra al disco 1 con id I04-S1-B02" "bodega I04-S1-B02 (50 productos)" "$SALIDA"
verificar "bodega-3 entra al disco 2 con id I04-S2-B01" "bodega I04-S2-B01 (50 productos)" "$SALIDA"

SALIDA=$(./listar_fs fs1.dat.ci0123)
verificar "disco 1: 2 bodegas, 4 categorias, 100 productos" "2 bodega(s), 4 categoria(s), 100 producto(s)" "$SALIDA"
SALIDA=$(./listar_fs fs2.dat.ci0123)
verificar "disco 2: 2 bodegas, 4 categorias, 100 productos" "2 bodega(s), 4 categoria(s), 100 producto(s)" "$SALIDA"

SALIDA=$(./listar_fs fs1.dat.ci0123 POSTRES)
verificar "extraer una categoria (sin importar mayusculas)" "POSTRES en I04-S1: 25 producto(s)" "$SALIDA"

SALIDA=$(./dump_fs fs1.dat.ci0123 bloque 0)
verificar "el superbloque tiene la firma CI0123F" "|CI0123F" "$SALIDA"

echo ""
echo "--- 3. Extraer y volver a agregar con programas independientes ---"
./extraer_bodega fs1.dat.ci0123 bodega-1.txt "$TMP/b1.txt" > /dev/null
./mkfs "$TMP/otro.dat" 4 3 > /dev/null
SALIDA=$(./agregar_bodega "$TMP/otro.dat" "$TMP/b1.txt" 2>&1)
verificar "la bodega extraida entra en otro disco con id nuevo" "bodega I04-S3-B01" "$SALIDA"
./extraer_bodega "$TMP/otro.dat" b1.txt "$TMP/b1-otra.txt" > /dev/null
if diff <(cut -d, -f2- "$TMP/b1.txt") <(cut -d, -f2- "$TMP/b1-otra.txt") > /dev/null; then
    verificar "el contenido sale igual (solo cambia el id)" "igual" "igual"
else
    verificar "el contenido sale igual (solo cambia el id)" "igual" "distinto"
fi

ANTES=$(libres "$TMP/otro.dat")
./mkfs "$TMP/vacio.dat" 4 3 > /dev/null
VACIO=$(libres "$TMP/vacio.dat")
SALIDA=$(./borrar_bodega "$TMP/otro.dat" b1.txt)
DESPUES=$(libres "$TMP/otro.dat")
verificar "borrar devuelve todos los bloques ($ANTES -> $DESPUES de $VACIO)" "$VACIO" "$DESPUES"

echo ""
echo "--- 4. Errores ---"
SALIDA=$(timeout 5 ./listar_fs no-existe.dat 2>&1); CODIGO=$?
verificar "disco inexistente: error claro, sin colgarse" "no existe" "$SALIDA (codigo $CODIGO)"
SALIDA=$(timeout 5 ./listar_fs bodegas/bodega-1.txt 2>&1)
verificar "archivo que no es un disco: firma incorrecta" "firma incorrecta" "$SALIDA"
SALIDA=$(./agregar_bodega fs1.dat.ci0123 bodegas/bodega-1.txt copia.txt 2>&1)
verificar "producto repetido en el disco: se rechaza" "ya existe en el disco" "$SALIDA"
printf "postres,Flan,10,300\npostres,Mal nombre,1,100\n" > "$TMP/mala.txt"
SALIDA=$(./agregar_bodega "$TMP/vacio.dat" "$TMP/mala.txt" 2>&1)
verificar "linea invalida: se rechaza indicando la linea" "linea 2: producto invalido" "$SALIDA"
SALIDA=$(./agregar_bodega fs1.dat.ci0123 bodegas/bodega-1.txt bodega-2.txt 2>&1)
verificar "nombre de bodega repetido: se rechaza" "ya hay una bodega llamada bodega-2.txt" "$SALIDA"

echo ""
echo "--- 5. Capacidad maxima (1024 bloques) ---"
for i in $(seq 1 10000); do echo "pruebas,Producto-$i,10,100"; done > "$TMP/enorme.txt"
./mkfs "$TMP/lleno.dat" 4 4 > /dev/null
ANTES=$(libres "$TMP/lleno.dat")
SALIDA=$(./agregar_bodega "$TMP/lleno.dat" "$TMP/enorme.txt" 2>&1)
verificar "bodega mas grande que el disco: disco lleno" "disco lleno" "$SALIDA"
DESPUES=$(libres "$TMP/lleno.dat")
verificar "despues del error no queda espacio perdido ($ANTES -> $DESPUES)" "$ANTES" "$DESPUES"
SALIDA=$(./listar_fs "$TMP/lleno.dat")
verificar "el disco sigue sano despues del error" "0 bodega(s)" "$SALIDA"

head -3000 "$TMP/enorme.txt" > "$TMP/grande.txt"
./mkfs "$TMP/grande.dat" 4 5 > /dev/null
INICIAL=$(libres "$TMP/grande.dat")
SALIDA=$(./agregar_bodega "$TMP/grande.dat" "$TMP/grande.txt" 2>&1)
verificar "3000 productos en una bodega (pasa al indirecto doble)" "(3000 productos)" "$SALIDA"
SALIDA=$(./dump_fs "$TMP/grande.dat")
verificar "el i-nodo muestra sus bloques de punteros" "punteros:" "$SALIDA"
SALIDA=$(./listar_fs "$TMP/grande.dat" pruebas | head -1)
verificar "se leen los 3000 de vuelta" "3000 producto(s)" "$SALIDA"
./borrar_bodega "$TMP/grande.dat" grande.txt > /dev/null
verificar "borrar la bodega grande devuelve todo ($INICIAL)" "$INICIAL" "$(libres "$TMP/grande.dat")"

echo ""
echo "--- 6. Varios procesos sobre el mismo disco (candado) ---"
./mkfs "$TMP/compartido.dat" 4 6 > /dev/null
for n in 1 2 3 4 5 6; do
    for i in $(seq 1 40); do echo "proceso,P$n-$i,5,100"; done > "$TMP/p$n.txt"
done
for n in 1 2 3 4 5 6; do ./agregar_bodega "$TMP/compartido.dat" "$TMP/p$n.txt" > /dev/null 2>&1 & done
wait
SALIDA=$(./listar_fs "$TMP/compartido.dat")
verificar "6 procesos agregando a la vez: no se pierde ninguna bodega" "6 bodega(s), 1 categoria(s), 240 producto(s)" "$SALIDA"
SALIDA=$(./dump_fs "$TMP/compartido.dat")
verificar "los ids siguen siendo unicos (B01..B06)" "siguiente id de bodega: I04-S6-B07" "$SALIDA"

echo ""
echo "------------------------------------"
echo "  Pasaron: $PASARON   Fallaron: $FALLARON"
echo "------------------------------------"
[ $FALLARON -eq 0 ]
