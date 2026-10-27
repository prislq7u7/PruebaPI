CXX = g++
FS_DIR = ../almacenamiento
CXXFLAGS = -std=c++17 -Wall -pthread -I$(FS_DIR)
SSLFLAGS = -lssl -lcrypto

# todas las conexiones TCP van por TLS (SSLSocket); el descubrimiento por broadcast es UDP
RED = VSocket.cc Socket.cc SSLSocket.cc Opciones.cc
INTERNO = Protocolo.cc Validaciones.cc CanalInterno.cc Bitacora.cc
FS = $(FS_DIR)/FileSystem.cc $(FS_DIR)/Inventario.cc

all: servidorProductos intermediario cliente consultaIntermediario

# el servidor de productos es el unico que abre el disco
servidorProductos: ServidorProductos.cc AtencionBodega.cc Descubrimiento.cc $(INTERNO) $(RED) $(FS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(SSLFLAGS)

intermediario: Intermediario.cc AtencionCliente.cc AtencionProtocolo.cc AtencionRegistro.cc Enrutador.cc TablaRutas.cc ConexionBodega.cc Carritos.cc PaginaWeb.cc HttpUtil.cc Descubrimiento.cc $(INTERNO) $(RED)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(SSLFLAGS)

cliente: Cliente.cc HttpUtil.cc $(RED)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(SSLFLAGS)

consultaIntermediario: ConsultaIntermediario.cc HttpUtil.cc $(INTERNO) $(RED)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(SSLFLAGS)

# certificado autofirmado del intermediario y los servidores de productos (no se sube al repositorio)
certificado: certificados/ci0123.pem

certificados/ci0123.pem:
	mkdir -p certificados
	openssl req -x509 -newkey rsa:2048 -keyout certificados/key.pem -out certificados/cert.pem \
		-days 365 -nodes -subj "/C=CR/O=UCR/CN=ci0123" 2> /dev/null
	cat certificados/key.pem certificados/cert.pem > certificados/ci0123.pem

# discos fs1.dat.ci0123 y fs2.dat.ci0123, uno por servidor de productos
discos:
	$(MAKE) -C $(FS_DIR) discos

clean:
	rm -f servidorProductos intermediario cliente consultaIntermediario

.PHONY: all certificado discos clean
