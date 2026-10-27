#!/bin/bash
# Uso: ./test_flujo.sh              (o PUERTO=18081 ./test_flujo.sh si el 8080 esta ocupado)
#
# compila todo, crea el certificado y los dos discos, levanta el intermediario y dos servidores
# de productos (SERV_01 y SERV_02) y prueba: descubrimiento por broadcast (80-83), TLS en todos
# los tramos, protocolo directo con los servidores de productos, registro dinamico (70/71/72),
# mensajes del protocolo por POST (10-40, 73-78), el flujo completo con el cliente, sesiones
# abandonadas, vencimiento y caidas de servidores.
# Los servidores no aceptan 127.0.0.1: se usa la ip de la maquina.

set -u
cd "$(dirname "$0")"

IP=$(hostname -I 2>/dev/null | awk '{print $1}')
PUERTO=${PUERTO:-8080}
P1=9051
P2=9052
FS_DIR=../almacenamiento
DISCO1=$FS_DIR/fs1.dat.ci0123
DISCO2=$FS_DIR/fs2.dat.ci0123
PASARON=0
FALLARON=0
PIDS=""

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
        echo "$salida" | head -4 | sed 's/^/         /'
        FALLARON=$((FALLARON+1))
    fi
}

# mensaje del protocolo, por TLS, directo a un servidor de productos
bodega()  { ./consultaIntermediario enviar "$IP:$1" "$2" 2>&1; }
consulta() { ./consultaIntermediario "$@" --puerto $PUERTO 2>&1; }
post()     { ./consultaIntermediario mensaje "$1" --puerto $PUERTO 2>&1; }
web()      { curl -sk --max-time 5 "https://$IP:$PUERTO$1"; }
rutasWeb() { web /rutas; }
cliente() { printf "$1" | ./cliente --puerto $PUERTO 2>/dev/null; }
stock()   { $FS_DIR/listar_fs $1 | grep " $2 " | grep -o "cantidad=[0-9]*"; }

esperarPuerto() {
    for i in $(seq 1 40); do
        (echo > /dev/tcp/$IP/$1) 2>/dev/null && return 0
        sleep 0.25
    done
    return 1
}

esperar() {   # esperar "<comando>" "<texto>": reintenta hasta 10 s
    for i in $(seq 1 20); do
        eval "$1" 2>/dev/null | grep -qF -- "$2" && return 0
        sleep 0.5
    done
    return 1
}

levantar() {
    # levantar "nombre" comando... ; deja el pid en PID_nombre
    local nombre="$1"; shift
    "$@" > "$nombre.log" 2>&1 &
    PIDS="$PIDS $!"
    eval "PID_$nombre=$!"
}
trap 'kill $PIDS 2>/dev/null' EXIT

echo "--- 1. Compilando y creando los discos (ip $IP) ---"
if ! make -C $FS_DIR > build.log 2>&1 || ! make >> build.log 2>&1 || ! make certificado >> build.log 2>&1; then
    rojo "FALLO LA COMPILACION -- revisa build.log"; cat build.log; exit 1
fi
grep -qi warning build.log && echo "  (hay warnings, revisa build.log)"
rm -f $FS_DIR/*.dat.ci0123 bitacora-*.log
$FS_DIR/crear_discos.sh > /dev/null || { rojo "no se pudieron crear los discos"; exit 1; }
verde "compilo, creo el certificado y los discos fs1 y fs2"

echo ""
echo "--- 2. Arranque ---"
verificar "disco que no existe: no arranca y no crea nada" "no se pudo abrir el disco" "$(timeout 3 ./servidorProductos no-existe.dat.ci0123 2>&1)"
verificar "archivo que no es un disco: firma incorrecta" "firma incorrecta" "$(timeout 3 ./servidorProductos $FS_DIR/bodegas/bodega-1.txt 2>&1)"
verificar "no acepta 127.0.0.1" "no se permite loopback" "$(timeout 3 ./servidorProductos fs1.dat.ci0123 --ip 127.0.0.1 2>&1)"

export TIC_SESION_SEGUNDOS=3    # sesiones abandonadas vencen a los 3 s
export TIC_REGISTRO_SEGUNDOS=1  # los servidores de productos se registran cada segundo
export TIC_VIGENCIA_SEGUNDOS=4  # un servidor sin registrarse 4 s queda vencido
# los servidores de productos arrancan antes que el intermediario y sin saber su ip: lo buscan
# por broadcast (80) sin respuesta, y cuando el intermediario arranca los llama (82)
levantar productos1 ./servidorProductos fs1.dat.ci0123
levantar productos2 ./servidorProductos fs2.dat.ci0123
sleep 4
levantar intermediario ./intermediario --puerto $PUERTO
if ! esperarPuerto $P1 || ! esperarPuerto $P2 || ! esperarPuerto $PUERTO; then
    rojo "algun servidor no arranco"; tail -3 *.log; exit 1
fi
esperar "consulta servidores" "SERV_01" && esperar "consulta servidores" "SERV_02"
verde "intermediario ($PUERTO) y dos servidores de productos ($P1, $P2) corriendo"
verificar "80: el servidor busca al intermediario por broadcast" "SERV_01 -> BROADCAST 80 OK" "$(cat bitacora-SERV_01.log)"
verificar "82: el intermediario llama a los servidores que ya corrian" "INT_04 -> BROADCAST 82 OK" "$(cat bitacora-SERV_02.log)"
verificar "83: el servidor contesta con su puerto" "SERV_02 -> INT_04 83 OK" "$(cat bitacora-SERV_02.log)"

echo ""
echo "--- 3. Registro (70/71) y consultas (73-78) ---"
verificar "73/74: los dos servidores se registraron solos" "INT_04|CLI_04/74/2/SERV_01,I04-S1,$IP,$P1;SERV_02,I04-S2,$IP,$P2" "$(consulta servidores)"
verificar "75/76: helados -> SERV_02" "helados,SERV_02" "$(consulta rutas)"
verificar "77/78: la bitacora tiene los registros" "SERV_01 -> INT_04  70  OK" "$(consulta bitacora 50)"
verificar "77/3: trae solo los 3 mas recientes" "INT_04|CLI_04/78/3/" "$(post 'CLI_04|INT_04/77/3')"
verificar "77 con n invalido -> 90" "INT_04|CLI_04/90/77/n/abc" "$(post 'CLI_04|INT_04/77/abc')"
verificar "el servidor de productos tambien lleva bitacora" "REGISTRO registrado en el intermediario" "$(cat bitacora-SERV_01.log)"
REG=$(./consultaIntermediario enviar "$IP:9040" "SERV_03|INT_04/70/Bodega3/$IP/9053/pos#res" 2>&1)
verificar "70 con una categoria invalida -> 90 y no se registra" "INT_04|SERV_03/90/70/categoria/pos#res" "$REG"

echo ""
echo "--- 4. Protocolo directo con los servidores de productos (TLS) ---"
verificar "servidor 1: 10 -> 11 con las categorias de su disco" "SERV_01|INT_04/11/postres,caramelos,bebidas,reposteria" "$(bodega $P1 'INT_04|SERV_01/10')"
verificar "servidor 2: 10 -> 11 con categorias distintas" "SERV_02|INT_04/11/helados,snacks,sandwiches,desayunos" "$(bodega $P2 'INT_04|SERV_02/10')"
verificar "20 postres -> 22 con 25 productos" "SERV_01|INT_04/22/25/Flan-caramelo,3.20,20;" "$(bodega $P1 'INT_04|SERV_01/20/postres')"
verificar "50 con mas de lo que hay -> 51 con reserva parcial" "SERV_01|INT_04/51/Tiramisu/4.50/10" "$(bodega $P1 'INT_04|SERV_01/50/Tiramisu/15')"
verificar "52 devuelve lo reservado -> 53" "SERV_01|INT_04/53/Tiramisu/10" "$(bodega $P1 'INT_04|SERV_01/52/Tiramisu/10')"
verificar "la devolucion quedo en el disco (Tiramisu 10)" "cantidad=10" "$(stock $DISCO1 Tiramisu)"
verificar "50 de un producto que no existe -> 51 con 0.00 y 0" "SERV_01|INT_04/51/NoExiste/0.00/0" "$(bodega $P1 'INT_04|SERV_01/50/NoExiste/1')"
verificar "origen que no es un intermediario -> 90" "/90/10/origen/CLI_04" "$(bodega $P1 'CLI_04|SERV_01/10')"
LARGO="INT_04|SERV_01/20/postres/$(head -c 400 /dev/zero | tr '\0' a)"
verificar "mensaje de mas de 256 bytes: se atiende (sin tamano maximo)" "SERV_01|INT_04/22/25/" "$(bodega $P1 "$LARGO")"
SALIDA=$( (printf 'INT_04|SERV_01/10\n'; sleep 1) | timeout 3 bash -c "cat > /dev/tcp/$IP/$P1" 2>&1; tail -2 bitacora-SERV_01.log)
verificar "un mensaje sin TLS se rechaza" "conexion sin TLS, se rechaza" "$SALIDA"
ANTES=$(grep -c "cliente sin TLS" bitacora-intermediario.log)
(printf 'GET / HTTP/1.1\r\nHost: x\r\n\r\n'; sleep 1) | timeout 3 bash -c "cat > /dev/tcp/$IP/$PUERTO" 2>/dev/null; sleep 0.5
verificar "HTTP sin TLS al intermediario se rechaza" "$((ANTES+1))" "$(grep -c "cliente sin TLS" bitacora-intermediario.log)"
verificar "el intermediario atiende con TLS" "New, TLS" "$(echo | timeout 5 openssl s_client -connect $IP:$PUERTO 2>/dev/null)"

echo ""
echo "--- 5. Cliente -> intermediario -> servidores de productos ---"
verificar "CATEGORIAS junta las de los dos servidores" "desayunos" "$(cliente 'CATEGORIAS\nsalir\n')"
verificar "productos de postres (servidor 1)" "Flan-caramelo | precio: 3.20 | stock: 20" "$(cliente 'postres\nsalir\n')"
verificar "productos de helados (servidor 2)" "Helado-fresa | precio: 1.50 | stock: 50" "$(cliente 'helados\nsalir\n')"
verificar "AGREGAR sin importar mayusculas, con reserva parcial" "solo habia 10 disponibles de los 999 pedidos" "$(cliente 'AGREGAR tiramisu 999\nsalir\n')"
verificar "al salir sin facturar el stock vuelve (Tiramisu 10)" "cantidad=10" "$(stock $DISCO1 Tiramisu)"
verificar "un producto que no esta en ningun servidor" "NoExiste no existe" "$(cliente 'AGREGAR NoExiste 1\nsalir\n')"
verificar "factura con productos de los dos servidores" "TOTAL: 6.20" "$(cliente 'AGREGAR Helado-fresa 2\nAGREGAR Flan-caramelo 1\nFACTURA\nsalir\n')"
verificar "lo facturado queda en el disco del servidor 2 (Helado-fresa 48)" "cantidad=48" "$(stock $DISCO2 Helado-fresa)"
verificar "lo facturado queda en el disco del servidor 1 (Flan-caramelo 19)" "cantidad=19" "$(stock $DISCO1 Flan-caramelo)"
verificar "SERVIDORES y RUTAS desde el cliente" "helados -> SERV_02" "$(cliente 'SERVIDORES\nRUTAS\nsalir\n')"
verificar "BITACORA desde el cliente" "FACTURA" "$(cliente 'BITACORA\nsalir\n')"

echo ""
echo "--- 6. Mensajes del protocolo por POST (cualquier cliente, curl u otra isla) ---"
verificar "10 -> 11 con las categorias de los dos servidores" "INT_04|CLI_04/11/postres,caramelos,bebidas,reposteria,helados,snacks,sandwiches,desayunos" "$(post 'CLI_04|INT_04/10')"
verificar "20 -> 22 con count y lista" "INT_04|CLI_04/22/25/Helado-vainilla,1.50,50;" "$(post 'CLI_04|INT_04/20/helados')"
verificar "20 de una categoria que nadie atiende -> 23" "INT_04|CLI_04/23/No hay productos disponibles en la categoria" "$(post 'CLI_04|INT_04/20/licores')"
verificar "30 -> 32 (el producto se busca en el servidor donde se vio)" "INT_04|CLI_04/32/Helado-mora/1.60/2" "$(post 'CLI_04|INT_04/30/helado-mora/2')"
verificar "30 de un producto que no existe -> 32 con 0.00 y 0" "INT_04|CLI_04/32/NoExiste/0.00/0" "$(post 'CLI_04|INT_04/30/NoExiste/1')"
verificar "40 -> 41/<total>/<count>/<detalle>" "INT_04|CLI_04/41/3.20/1/Helado-mora,2,3.20" "$(post 'CLI_04|INT_04/40/1/Helado-mora,2')"
verificar "40/0 -> 42" "INT_04|CLI_04/42/El carrito esta vacio" "$(post 'CLI_04|INT_04/40/0')"
verificar "origen invalido -> 90" "INT_04|CLI_04/90/10/origen/HACKER" "$(post 'HACKER|INT_04/10')"

S=$(web /sesion | grep -o 'data-sesion="[0-9]*"' | grep -o '[0-9]*')
web "/agregar?sesion=$S&producto=Flan-caramelo&cantidad=5" > /dev/null
verificar "sesion abandonada: reserva hecha (Flan-caramelo 14)" "cantidad=14" "$(stock $DISCO1 Flan-caramelo)"
esperar "stock $DISCO1 Flan-caramelo" "cantidad=19"
verificar "la sesion vence y el stock vuelve solo (Flan-caramelo 19)" "cantidad=19" "$(stock $DISCO1 Flan-caramelo)"

printf "galletas,Galleta-jengibre,30,150\ngalletas,Galleta-limon,30,150\n" > /tmp/bodega-nueva-$$.txt
$FS_DIR/agregar_bodega $DISCO1 /tmp/bodega-nueva-$$.txt bodega-5.txt > /dev/null
rm -f /tmp/bodega-nueva-$$.txt
esperar "consulta rutas" "galletas"
verificar "bodega agregada con el servidor corriendo: se publica sola" "galletas,SERV_01" "$(consulta rutas)"
$FS_DIR/borrar_bodega $DISCO1 bodega-5.txt > /dev/null

verificar "navegador (HTTPS): GET / con el inventario de los dos servidores" "Helado-fresa" "$(web /)"
verificar "navegador (HTTPS): GET /bitacora" "REGISTRO" "$(web /bitacora)"

echo ""
echo "--- 7. Fallas ---"
kill -9 $PID_productos1; sleep 0.5   # se cae sin avisar
SALIDA=$(cliente 'CATEGORIAS\nsalir\n')
verificar "servidor 1 caido: se siguen viendo las categorias del 2" "helados" "$SALIDA"
VENCIDO='data-servidor="SERV_01" data-direccion="'$IP:$P1'" data-estado-servidor="vencido"'
esperar "rutasWeb" "$VENCIDO"
verificar "servidor 1 sin registrarse: queda vencido en la tabla de rutas" "$VENCIDO" "$(rutasWeb)"
verificar "76 deja de anunciar las rutas del servidor vencido" "helados,SERV_02" "$(consulta rutas | grep -v SERV_01)"
levantar productos1 ./servidorProductos fs1.dat.ci0123
esperar "consulta rutas" "postres,SERV_01"
verificar "al volver a levantarlo se registra solo" "postres" "$(cliente 'CATEGORIAS\nsalir\n')"
verificar "81: esta vez encontro al intermediario por broadcast" "INT_04 -> SERV_01 81 OK" "$(cat bitacora-SERV_01.log)"
kill -INT $PID_productos2; sleep 1    # se cierra avisando (72)
verificar "servidor 2 cerrado con Ctrl+C (72): sale de la lista" "INT_04|CLI_04/74/1/SERV_01" "$(consulta servidores)"
verificar "la baja queda en la bitacora" "BAJA SERV_02 se cerro" "$(cat bitacora-intermediario.log)"

if command -v python3 > /dev/null; then
    python3 - "$IP" "$PUERTO" << 'EOF'
import socket, struct, sys, time
for _ in range(20):
    try:
        s = socket.create_connection((sys.argv[1], int(sys.argv[2])))
        s.sendall(b"GET / HTTP/1.1\r\nHost: x\r\n\r\n")
        s.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        s.close()
    except OSError:
        pass
    time.sleep(0.05)
EOF
    sleep 1
fi
VIVOS=0
for p in $PID_productos1 $PID_intermediario; do kill -0 $p 2>/dev/null && VIVOS=$((VIVOS+1)); done
verificar "clientes que cortan la conexion: el intermediario y el servidor 1 siguen vivos" "2" "$VIVOS"

echo ""
echo "------------------------------------"
echo "  Pasaron: $PASARON   Fallaron: $FALLARON"
echo "------------------------------------"
[ $FALLARON -eq 0 ]
