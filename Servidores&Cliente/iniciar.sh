#!/bin/bash
# Levanta en esta computadora el intermediario y los dos servidores de productos (fs1 y fs2),
# cada uno en segundo plano con su propio .log. Los servidores encuentran al intermediario por
# broadcast y todo va por TLS (crea el certificado si falta). Para apagarlos: ./detener.sh
#
# Uso: ./iniciar.sh            clientes en el puerto 8080
#      ./iniciar.sh 18081      si el 8080 esta ocupado

cd "$(dirname "$0")"
PUERTO=${1:-8080}

if [ -f .pids ]; then
    echo "ya hay servidores corriendo; primero ./detener.sh"
    exit 1
fi

make -s all || exit 1
make -s certificado || exit 1
if [ ! -f ../almacenamiento/fs1.dat.ci0123 ] || [ ! -f ../almacenamiento/fs2.dat.ci0123 ]; then
    make -s discos > /dev/null || exit 1
fi

./intermediario --puerto "$PUERTO" > intermediario.log 2>&1 &
echo $! > .pids
sleep 0.5
./servidorProductos fs1.dat.ci0123 > servidor1.log 2>&1 &
echo $! >> .pids
./servidorProductos fs2.dat.ci0123 > servidor2.log 2>&1 &
echo $! >> .pids
sleep 1.5

for pid in $(cat .pids); do
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "algo no arranco; revisa intermediario.log, servidor1.log y servidor2.log"
        ./detener.sh > /dev/null
        exit 1
    fi
done

IP=$(hostname -I 2>/dev/null | awk '{print $1}')
echo "intermediario y dos servidores de productos corriendo"
./consultaIntermediario rutas --puerto "$PUERTO"
echo ""
echo "  cliente:     ./cliente --puerto $PUERTO"
echo "  navegador:   https://$IP:$PUERTO/   (certificado autofirmado: aceptar el aviso)"
echo "  consultas:   ./consultaIntermediario servidores | rutas | bitacora --puerto $PUERTO"
echo "  en vivo:     tail -f intermediario.log servidor1.log servidor2.log"
echo "  apagar:      ./detener.sh"
