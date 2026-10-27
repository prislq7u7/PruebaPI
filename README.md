# TicAmazon — Tercera etapa

```
cliente / navegador --HTTPS--> intermediario --TLS, una linea por mensaje--> servidorProductos --> fsN.dat.ci0123
                                     ^                                       (uno por disco)
                                     '------ broadcast UDP (80-83): se encuentran solos ------'
```

| Carpeta | Contenido |
|---|---|
| `almacenamiento/` | Sistema de archivos con i-nodos (`FileSystem`), capa de productos (`Inventario`), herramientas independientes, bodegas de ejemplo y pruebas |
| `Servidores&Cliente/` | `servidorProductos`, `intermediario` y `cliente`, cada uno es un programa aparte y pueden correr en computadoras distintas |
| `simulacion/` | Simulación con hilos y buzón System V del protocolo de la etapa 3: cliente, intermediario y dos servidores de productos, uno de ellos incorporándose con el sistema en marcha |
| `Protocolos/` | Protocolo interno de la isla 4 y protocolo mancomunado |

---

## 1. Sistema de archivos (`almacenamiento/`)

Cada servidor de productos tiene **un solo archivo en Linux** que hace de disco
(`fs1.dat.ci0123`, `fs2.dat.ci0123`). Adentro hay varias bodegas; cada bodega es
un **archivo de texto con una línea por producto**.

### 1.1 Distribución del disco

**1024 bloques de 256 bytes** (256 KB), siempre del mismo tamaño:

| Bloques | Contenido |
|---|---|
| 0 | Superbloque: firma `CI0123F`, versión, isla, servidor, tamaño y cantidad de bloques, dónde empieza cada zona, bloques e i-nodos libres, contador de ids de bodega |
| 1 | Bitmap de bloques: 1 bit por bloque |
| 2 – 17 | Tabla de i-nodos: 64 i-nodos de 64 bytes, 4 por bloque |
| 18 – 1023 | Datos y bloques de punteros |

```
 directorio (i-nodo 0)            i-nodo 1                     bloques 19..26
┌──────────────┬────────────┬───┐ ┌───────────────────────┐   ┌──────────────────────────────────────────┐
│ bodega-1.txt │ I04-S1-B01 │ 1 ├─► directos: 19 20 .. 26 ├──►│I04-S1-B01,postres,Flan-caramelo,20,320\n │
│ bodega-2.txt │ I04-S1-B02 │ 2 │ │ indirecto: -          │   │I04-S1-B01,postres,Tres-leches,15,380\n...│
└──────────────┴────────────┴───┘ │ doble indirecto: -    │   └──────────────────────────────────────────┘
                                  └───────────────────────┘
```

**I-nodo (64 B):** tipo, número, tamaño en bytes, **10 punteros directos, 1 indirecto
y 1 indirecto doble**, cantidad de bloques usados y fechas. Cuando se llenan los 10
directos, el i-nodo apunta a un bloque indirecto que continúa la lista de bloques
(128 punteros más), y cuando ese se llena, al indirecto doble. Así una bodega puede
crecer hasta llenar el disco.

**Directorio:** es el contenido del i-nodo 0, una lista de entradas de 32 B
(`nombre[18]`, `id[12]`, `inodo`). Crece con los mismos punteros que cualquier
archivo y, al borrar, recorta las entradas vacías del final y devuelve sus bloques.

**Bodega:** texto continuo, sin relleno, que pasa de un bloque al siguiente:

```
<id bodega>,<categoria>,<producto>,<cantidad>,<precio en centavos>
I04-S1-B01,postres,Flan-caramelo,20,320
```

### 1.2 Identificadores

| Id | Ejemplo | Significado |
|---|---|---|
| Disco | `I04-S1` | Isla 04, servidor de productos 1. Se fija al formatear y queda en el superbloque |
| Bodega | `I04-S1-B02` | Segunda bodega creada en el disco del servidor 1 de la isla 4 |
| I-nodo | `2` | Posición en la tabla de i-nodos; es interno y nunca sale del disco |

El id de bodega lo asigna el sistema de archivos con un contador del superbloque,
nunca se repite dentro del disco y, como incluye isla y servidor, tampoco entre
servidores. Va al inicio de cada línea, así un producto siempre dice de qué bodega salió.

### 1.3 Clases

| Archivo | Qué hace |
|---|---|
| `Formatos.h` | Constantes y structs empaquetados (`Superbloque`, `Inodo`, `EntradaDir`), con `static_assert` de sus tamaños |
| `FileSystem.*` | Formatear, abrir (valida firma y tamaño), `crear`, `leer`, `escribir`, `anexar`, `borrar`, `listar`; bitmap, i-nodos y punteros directos/indirectos. Revisa el espacio antes de escribir, así un disco lleno no deja archivos a medias. Todos los errores son `std::runtime_error`. Cada operación toma el disco con `flock`, así el servidor y las herramientas pueden usar el mismo disco a la vez |
| `Inventario.*` | Productos encima del FS: categorías, productos por categoría, `reservar` (parcial, como dice el protocolo), `devolver`, importar y exportar bodegas validando cada línea con las regex del protocolo |

### 1.4 Programas

```bash
cd Etapa3/almacenamiento
make
make discos      # fs1 (servidor 1: bodegas 1 y 2) y fs2 (servidor 2: bodegas 3 y 4), 100 productos cada uno
```

| Programa | Uso |
|---|---|
| `mkfs` | `./mkfs <disco> <isla> <servidor>` — formatea |
| `agregar_bodega` | `./agregar_bodega <disco> bodegas/bodega-1.txt` — mete una bodega de texto al disco |
| `extraer_bodega` | `./extraer_bodega <disco> bodega-1.txt [salida.txt]` — la saca a un archivo de Linux |
| `borrar_bodega` | `./borrar_bodega <disco> bodega-1.txt` — la borra y devuelve sus bloques |
| `listar_fs` | `./listar_fs <disco> [categoria]` — bodegas y productos por categoría, o solo una categoría |
| `dump_fs` | `./dump_fs <disco>` — superbloque, bitmap, i-nodos con sus bloques y directorio; `./dump_fs <disco> bloque N` — un bloque en hexadecimal |

Las bodegas de `bodegas/*.txt` tienen 4 campos por línea
(`categoria,producto,cantidad,precio`); si traen el id adelante (porque se
extrajeron de otro disco) se ignora y se asigna uno nuevo. Los dos discos tienen
categorías distintas y los nombres de producto no se repiten entre discos.

Las herramientas se pueden usar con el servidor de productos corriendo: una
bodega agregada aparece en la siguiente consulta sin reiniciar nada.

### 1.5 Pruebas

```bash
./test_fs.sh
```

Ids, listados, extraer y volver a agregar con programas independientes, borrar
(devuelve todos los bloques), discos inexistentes o dañados, líneas inválidas,
productos repetidos, disco lleno sin dejar basura, una bodega de 3000 productos que
usa el indirecto doble y 6 procesos agregando bodegas al mismo disco a la vez.

---

## 2. Servidores y cliente (`Servidores&Cliente/`)

### 2.1 Correrlo

En una sola computadora, todo de una vez:

```bash
cd Etapa3/Servidores&Cliente
./iniciar.sh            # o ./iniciar.sh 18081 si el 8080 esta ocupado
./cliente               # en otra terminal
./detener.sh            # al terminar
```

`iniciar.sh` compila, crea el certificado y los discos si no existen y levanta el intermediario
y los dos servidores de productos en segundo plano (logs en `intermediario.log`, `servidor1.log`,
`servidor2.log`). Cada programa detecta solo la ip de la máquina; **no aceptan `127.0.0.1`**.
Desde el navegador: `https://<ip>:8080/` (el certificado es autofirmado, hay que aceptar el aviso).

A mano, o en computadoras distintas (todas las opciones son opcionales y van en cualquier orden;
`--ayuda` muestra las de cada programa):

| Programa | Uso |
|---|---|
| `intermediario` | `./intermediario [--ip-publica IP] [--ip-privada IP] [--puerto 8080] [--puerto-registro 9040] [--broadcast IP]` |
| `servidorProductos` | `./servidorProductos fs1.dat.ci0123 [--intermediario IP] [--ip IP] [--puerto N] [--broadcast IP]` — el disco del servidor N es `SERV_0N` y escucha en 9050 + N (fs1 → `SERV_01` en 9051, fs2 → `SERV_02` en 9052); el disco se busca también en `../almacenamiento`. Sin `--intermediario` lo busca por broadcast |
| `cliente` | `./cliente [--intermediario IP] [--puerto 8080]` — comandos `CATEGORIAS`, `<categoria>`, `AGREGAR <producto> <cantidad>`, `FACTURA`, `SERVIDORES`, `RUTAS`, `BITACORA`, `salir` |
| `consultaIntermediario` | `./consultaIntermediario servidores \| rutas \| bitacora [n]` (73, 75, 77), `mensaje '<mensaje>'` para mandar cualquier mensaje del protocolo al intermediario, y `enviar <ip:puerto> '<mensaje>'` para hablar directo (por TLS) con un servidor de productos o con el puerto de registro |

`make certificado` crea `certificados/ci0123.pem` (certificado y llave). Lo necesitan el
intermediario y los servidores de productos, que son los que escuchan por TLS; se crea en cada
computadora y no se sube al repositorio.

Ejemplo en el laboratorio (los servidores de productos encuentran al intermediario solos):

```bash
./intermediario --ip-publica 10.1.35.20 --ip-privada 172.16.123.67
./servidorProductos fs1.dat.ci0123
./servidorProductos fs2.dat.ci0123        # en otra computadora de la isla
./cliente --intermediario 10.1.35.20
```

### 2.2 Qué hace cada programa

Sigue el protocolo interno de la isla 4 (borrador de la etapa 3).

- **`servidorProductos`**: `SERV_NN`, único programa que abre el disco. Atiende 10, 20, 50 y 52, un hilo por conexión. Si el disco no existe o no tiene la firma, no arranca (nunca lo reescribe). Al arrancar **se registra** en el intermediario (70) con su nombre (el id del disco, `I04-S1`), ip, puerto y categorías, y lo repite cada 10 s: así lo encuentra aunque el intermediario arranque después, publica las categorías de una bodega agregada en caliente y se reactiva si lo habían deshabilitado. Con Ctrl+C o `kill` **avisa que se cierra** (72).
- **`intermediario`**: atiende por HTTPS en el **8080** de dos formas:
  - **POST con un mensaje del protocolo en el cuerpo** (`CLI_04|INT_04/...`, respuesta en el cuerpo): 10, 20, 30, 40 y las consultas 73, 75 y 77. Es lo que puede usar cualquier cliente, `curl` u otra isla.
  - **GET** con páginas para el cliente de terminal y el navegador (`/`, `/sesion`, `/categorias`, `/productos`, `/agregar`, `/factura`, `/cancelar`, `/servidores`, `/rutas`, `/bitacora`).

  Las dos usan las mismas reglas de enrutamiento (`Enrutador`): el 10 junta las categorías de todos los servidores sin repetir; el 20 junta los productos de todos los servidores que atienden la categoría (23 si ninguno la atiende); el 30 manda el 50 primero al servidor donde se vio el producto y después a los demás, hasta que uno reserve (32 con `0.00/0` si ninguno lo tiene); el 40 factura con el carrito sin consultar a nadie. Un servidor que no responde después de 2 reintentos queda `sin-respuesta`, y uno que pasa 30 s sin registrarse queda `vencido`; en los dos casos sale de las rutas hasta su próximo 70. Si en un 10 o un 20 contestan otros servidores se responde con ellos; si no contesta ninguno, 92. Las sesiones sin uso por 10 minutos se cierran y su stock vuelve al servidor de donde salió (52).
- **Bitácora**: cada programa anota los mensajes que envía y recibe (`HH:MM:SS,origen,destino,tipo,estado`, con un detalle que solo queda en el archivo) y sus eventos internos, en `bitacora-intermediario.log` o `bitacora-SERV_01.log` y en la terminal. La del intermediario se consulta con el 77 (los *n* más recientes, recortando los más viejos si no caben en el buffer) y completa en `/bitacora`.

### 2.3 Cifrado (TLS)

Todas las conexiones TCP van por **TLS** con `SSLSocket` (OpenSSL):

| Tramo | Puerto | Quién escucha |
|---|---|---|
| cliente / navegador → intermediario (HTTPS) | 8080 | intermediario |
| servidor de productos → intermediario, registro (70/72) | 9040 | intermediario |
| intermediario → servidor de productos (10, 20, 50, 52) | 9050 + NN | servidor de productos |

- El intermediario y los servidores de productos cargan `certificados/ci0123.pem`; si falta, no
  arrancan y explican cómo crearlo.
- El saludo TLS se hace en el hilo de cada conexión (`WaitForConnection` + `CompletarConexion`),
  así una conexión que nunca lo completa no frena a las demás y se descarta a los 10 s.
- Una conexión sin TLS (texto plano) se rechaza y queda en la bitácora.
- El certificado es autofirmado: el canal va cifrado, pero los extremos no validan la identidad
  del otro.

Para ver que el canal es TLS: `echo | openssl s_client -connect <ip>:9051`.

### 2.4 Descubrimiento por broadcast

El servidor de productos no necesita saber dónde está el intermediario. Como no se conoce la
dirección del otro, no hay conexión TCP: se mandan datagramas UDP a la dirección de broadcast de
la red (en la isla 4, `172.16.123.79`; por defecto se calcula de la ip de la máquina) y cada
lado tiene un hilo esperando con `recvFrom` las solicitudes de su contraparte.

| Mensaje | Puerto UDP | Qué hace |
|---|---|---|
| `SERV_01\|BROADCAST/80` | 9091 | el servidor de productos busca al intermediario (3 intentos al arrancar, y otro cada vez que un registro falla) |
| `INT_04\|SERV_01/81/<puerto de registro>` | al que preguntó | el intermediario contesta; la ip sale de quien mandó el datagrama |
| `INT_04\|BROADCAST/82/<puerto de registro>` | 9092 | el intermediario, al arrancar, llama a los servidores que ya estaban corriendo |
| `SERV_01\|INT_04/83/<puerto>` | al que preguntó | el servidor contesta y se registra (70) de una vez |

Así da igual qué se levante primero, y si el intermediario se reinicia los servidores se
vuelven a registrar solos. El descubrimiento no se cifra: solo anuncia puertos, no lleva datos
de productos ni de clientes. Con `--intermediario IP` el servidor de productos se registra en esa
dirección sin buscar.

### 2.5 Protocolo interno

Una línea por mensaje terminada en `\n`, `ORIGEN|DESTINO/TIPO/campos`, sin tamaño máximo (91 si
una respuesta no cabe en el buffer de 8 KB del intermediario). Puertos: 8080 (clientes), 9040
(registro en el intermediario), 9050 + NN (servidor de productos `SERV_NN`), 9091 y 9092 (UDP,
descubrimiento).

| Mensaje | Respuesta |
|---|---|
| `INT_04\|SERV_01/10` | `SERV_01\|INT_04/11/postres,caramelos,...` |
| `INT_04\|SERV_01/20/<categoria>` | `SERV_01\|INT_04/22/<count>/<nombre,precio,stock;...>` o `22/0` |
| `INT_04\|SERV_01/50/<producto>/<cantidad>` | `51/<producto>/<precio>/<reservado>`: puede ser menos de lo pedido; si no existe, `0.00` y `0` |
| `INT_04\|SERV_01/52/<producto>/<cantidad>` | `53/<producto>/<devuelto>` |
| `SERV_01\|INT_04/70/<nombre>/<ip>/<puerto>/<cat,cat,...>` | `INT_04\|SERV_01/71/<count>` (o 90 si un campo no cumple su regex) |
| `SERV_01\|INT_04/72` | `INT_04\|SERV_01/71/0` |
| `CLI_04\|INT_04/73` | `74/<count>/<SERV_NN,nombre,ip,puerto;...>` |
| `CLI_04\|INT_04/75` | `76/<count>/<categoria,SERV_NN;...>` |
| `CLI_04\|INT_04/77/<n>` | `78/<count>/<timestamp,origen,destino,tipo,estado;...>` |

**72, 52, 53 y 80-83 todavía hay que agregarlos al documento del protocolo**: 72 para que un
servidor avise que se cierra, 52/53 para devolver al stock lo que una sesión reservó y nunca
facturó, y 80-83 para el descubrimiento. También falta que el documento diga que los tramos TCP
van por TLS y que el puerto de los clientes es el 8080.

### 2.6 Pruebas

```bash
./test_flujo.sh                 # PUERTO=18081 ./test_flujo.sh si el 8080 esta ocupado
```

Crea el certificado, levanta el intermediario y dos servidores de productos y prueba el
descubrimiento por broadcast (80-83, con los servidores arrancando antes que el intermediario),
TLS en todos los tramos (y que se rechaza el texto plano), el protocolo directo con los servidores
de productos, el registro (70/71/72), las consultas 73-78, los mensajes del protocolo por POST, el
flujo del cliente con productos de los dos servidores, HTTPS desde `curl`, sesiones abandonadas,
bodegas agregadas en caliente, servidores vencidos, caídos y cerrados con aviso, y clientes que
cortan la conexión.

---

## 3. Simulación (`simulacion/`)

```bash
cd Etapa3/simulacion
make
./simulacion
```

Un solo programa con hilos que se hablan por una cola de mensajes System V: el cliente, el
intermediario y dos servidores de productos (`SERV_01` con postres y caramelos, `SERV_02` con
bebidas y repostería). `SERV_02` se registra (70/71) con el sistema ya en marcha. El guion del
cliente recorre categorías, errores de formato (90), categorías que nadie atiende (23), carrito
vacío (42), reservas (incluida una parcial), las consultas 73-78, la reserva en el servidor que
tiene el producto, un servidor que no responde (92) y la factura.
