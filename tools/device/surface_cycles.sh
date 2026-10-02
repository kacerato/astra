#!/usr/bin/env bash
# Ciclos de surface perdida/recriada (Home → reabrir) num app Astra, com verificação pelo log do The Forge.
# Uso: tools/device/surface_cycles.sh <serial> <pacote/atividade> <ciclos> <prefixo-de-log> <nome-do-log>
# Ex.:  tools/device/surface_cycles.sh "$SERIAL" dev.astra.spike.s02/com.google.androidgamesdk.GameActivity 100 S02 AstraSpikeS02
#
# Conta pelo arquivo de log do TF no aparelho (getExternalFilesDir/<nome>.log), não pelo logcat: o buffer
# circular do logcat descarta as linhas antigas em execuções longas.
# Aprova se: mesmo PID do início ao fim, uma surface perdida por ciclo, nenhuma linha ERR| do TF, nenhum
# erro da camada de validação Vulkan, e o app segue apresentando frames depois do último ciclo.
set -u
export MSYS_NO_PATHCONV=1 # Git Bash no Windows: não converter caminhos do Android
ADB="${ADB:-adb}"
SERIAL="$1"; COMPONENT="$2"; CYCLES="$3"; TAG="$4"; LOGNAME="$5"
PKG="${COMPONENT%%/*}"
DEVICE_LOG="/sdcard/Android/data/$PKG/files/$LOGNAME.log"
a() { "$ADB" -s "$SERIAL" "$@"; }
count() { a shell cat "$DEVICE_LOG" | grep -c "$1"; }

a shell am start -W -n "$COMPONENT" >/dev/null
sleep 3
PID0=$(a shell pidof "$PKG" | tr -d '\r')
LOST0=$(count "$TAG: TERM_WINDOW")
SWAP0=$(count "$TAG: swapchain #")
ERR0=$(count "ERR|")
VAL0=$(a shell cat "$DEVICE_LOG" | grep -ciE "VUID-|Validation Error")

for i in $(seq 1 "$CYCLES"); do
    a shell input keyevent KEYCODE_HOME
    sleep 1.0
    a shell am start -n "$COMPONENT" >/dev/null 2>&1
    sleep 1.5
done
sleep 11

PID1=$(a shell pidof "$PKG" | tr -d '\r')
LOST=$(( $(count "$TAG: TERM_WINDOW") - LOST0 ))
SWAP=$(( $(count "$TAG: swapchain #") - SWAP0 ))
ERRS=$(( $(count "ERR|") - ERR0 ))
VALID=$(( $(a shell cat "$DEVICE_LOG" | grep -ciE "VUID-|Validation Error") - VAL0 ))
ALIVE=$(a shell cat "$DEVICE_LOG" | grep "$TAG: .* frames em" | tail -1)

echo "ciclos=$CYCLES pid_inicial=$PID0 pid_final=$PID1 surfaces_perdidas=$LOST swapchains_criadas=$SWAP erros_forge=$ERRS erros_validacao=$VALID"
echo "último resumo: ${ALIVE:-nenhum}"
if [ "$PID0" = "$PID1" ] && [ "$LOST" -ge "$CYCLES" ] && [ "$SWAP" -ge "$CYCLES" ] && [ "$ERRS" = 0 ] && [ "$VALID" = 0 ] && [ -n "$ALIVE" ]; then
    echo "APROVADO"
else
    echo "REPROVADO"; exit 1
fi
