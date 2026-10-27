#!/bin/bash
# Apaga lo que levanto ./iniciar.sh. Los servidores de productos reciben Ctrl+C (SIGINT),
# asi avisan al intermediario que se cierran (72) antes de terminar.

cd "$(dirname "$0")"
if [ ! -f .pids ]; then
    echo "no hay nada corriendo (no existe .pids)"
    exit 0
fi

PIDS=($(cat .pids))
# primero los servidores de productos, para que el intermediario reciba su aviso
for pid in "${PIDS[@]:1}"; do kill -INT "$pid" 2>/dev/null; done
sleep 1
kill "${PIDS[0]}" 2>/dev/null
rm -f .pids
echo "apagados; los eventos quedaron en bitacora-*.log"
