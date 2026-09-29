#!/usr/bin/env bash
# profile.sh - Coleta de métricas de profiling (Tabela 5 do relatório)
#
# Gera em profiling/:
#   stat_*.txt            perf stat: ciclos, instruções, cache, branches, trocas de contexto
#   tabela5.md            resumo com CPI, taxa de cache miss e taxa de branch miss
#   flame_*.svg           flame graphs (perf record + FlameGraph)
#   secao_critica_*.txt   % do tempo em funções de lock (mutex/futex) -> overhead de sincronização
#   cachegrind_*.txt      resumo do valgrind --tool=cachegrind (simulação de cache)
#
# Uso: ./profile.sh [arquivo_de_log] [threads]
#   padrão: dados/base_10000000.log e 8 threads

set -u
cd "$(dirname "$0")"

ARQ=$(realpath "${1:-dados/base_10000000.log}")
TH=${2:-8}
OUT=profiling
REPS=3
EVENTOS=cycles,instructions,cache-references,cache-misses,branches,branch-misses,context-switches,cpu-migrations

[ -f "$ARQ" ] || { echo "Arquivo não encontrado: $ARQ"; exit 1; }
mkdir -p "$OUT"

echo "==> Compilando"
make -s || exit 1
# Binários só para o perf record: mesmo -O2 dos binários medidos, mas com
# símbolos de depuração e frame pointers, para o perf montar a pilha de chamadas.
PROF_FLAGS="-O2 -g -fno-omit-frame-pointer -pthread"
gcc $PROF_FLAGS log_analyzer_par.c -o "$OUT/par_prof" || exit 1
gcc $PROF_FLAGS log_analyzer_par_optimized.c -o "$OUT/opt_prof" || exit 1

# O sequencial só lê ./access_log_large.txt
ln -sf "$ARQ" access_log_large.txt

# perf
# No WSL2 o /usr/bin/perf não bate com o kernel da Microsoft; o binário real
# do pacote linux-tools fica em /usr/lib/linux-tools/<versão>/perf
PERF=""
for p in perf /usr/lib/linux-tools/*/perf; do
    if "$p" stat -e task-clock true > /dev/null 2>&1; then PERF=$p; break; fi
done

if [ -z "$PERF" ]; then
    echo "!! perf não encontrado. Instale: sudo apt install -y linux-tools-generic"
else
    echo "==> perf: $PERF"
    # Libera o perf para usuário comum e mostra símbolos do kernel (futex etc.)
    if [ "$(cat /proc/sys/kernel/perf_event_paranoid)" -gt -1 ]; then
        sudo sysctl -q -w kernel.perf_event_paranoid=-1 kernel.kptr_restrict=0
    fi

    echo "==> perf stat ($REPS execuções cada, $TH threads nas versões paralelas)"
    "$PERF" stat -r $REPS -x ';' -e $EVENTOS -o "$OUT/stat_seq.txt" \
        ./log_analyzer_seq > /dev/null
    "$PERF" stat -r $REPS -x ';' -e $EVENTOS -o "$OUT/stat_mutex.txt" \
        ./log_analyzer_par "$TH" "$ARQ" > /dev/null
    "$PERF" stat -r $REPS -x ';' -e $EVENTOS -o "$OUT/stat_reducao.txt" \
        ./log_analyzer_par_optimized "$TH" "$ARQ" > /dev/null

    python3 - "$OUT" "$TH" << 'EOF'
import os, sys
out, th = sys.argv[1], sys.argv[2]

def le(nome):
    d = {}
    for l in open(os.path.join(out, nome), encoding="utf-8"):
        p = l.strip().split(";")
        if len(p) < 3 or l.startswith("#"):
            continue
        ev = p[2].split(":")[0]          # "cycles:u" -> "cycles"
        try:
            d[ev] = float(p[0])
        except ValueError:
            d[ev] = None                  # <not supported> / <not counted>
    return d

def f(v, fmt="{:,.0f}"):
    return "n/d" if v is None else fmt.format(v)

def div(a, b):
    return None if a is None or not b else a / b

linhas = [
    "### Tabela 5 - Métricas de profiling (perf stat, média de 3 execuções)", "",
    "| Versão | Ciclos | Instruções | CPI | Cache misses | Taxa de cache miss "
    "| Branch misses | Taxa de branch miss | Trocas de contexto |",
    "|---|---:|---:|---:|---:|---:|---:|---:|---:|",
]
for rot, arq in (("Sequencial", "stat_seq.txt"),
                 (f"Mutex global ({th} threads)", "stat_mutex.txt"),
                 (f"Redução local ({th} threads)", "stat_reducao.txt")):
    g = le(arq).get
    cpi = div(g("cycles"), g("instructions"))
    cm = div(g("cache-misses"), g("cache-references"))
    bm = div(g("branch-misses"), g("branches"))
    linhas.append(
        f"| {rot} | {f(g('cycles'))} | {f(g('instructions'))} | {f(cpi, '{:.2f}')} "
        f"| {f(g('cache-misses'))} | {f(None if cm is None else cm * 100, '{:.2f}%')} "
        f"| {f(g('branch-misses'))} | {f(None if bm is None else bm * 100, '{:.2f}%')} "
        f"| {f(g('context-switches'))} |")

open(os.path.join(out, "tabela5.md"), "w", encoding="utf-8").write("\n".join(linhas) + "\n")
print("\n".join(linhas))
EOF

    echo "==> perf record + flame graphs"
    [ -d FlameGraph ] || git clone -q --depth 1 https://github.com/brendangregg/FlameGraph.git

    perfil() {  # $1 = nome da versão, $2 = binário com símbolos
        "$PERF" record -q -F 999 -g -o "$OUT/perf_$1.data" "$2" "$TH" "$ARQ" > /dev/null
        "$PERF" script -i "$OUT/perf_$1.data" 2> /dev/null \
            | FlameGraph/stackcollapse-perf.pl \
            | FlameGraph/flamegraph.pl --title "$1 ($TH threads)" > "$OUT/flame_$1.svg"
        # Overhead de sincronização: % das amostras em funções de lock
        "$PERF" report -i "$OUT/perf_$1.data" --stdio --no-children --sort symbol 2> /dev/null \
            | grep -iE "mutex|futex|lll_lock|spin" > "$OUT/secao_critica_$1.txt" \
            || echo "(nenhuma função de lock entre as amostras)" > "$OUT/secao_critica_$1.txt"
        echo "    -> $OUT/flame_$1.svg, $OUT/secao_critica_$1.txt"
    }
    perfil mutex "$OUT/par_prof"
    perfil reducao "$OUT/opt_prof"
fi

# valgrind / cachegrind
# O valgrind é ~50x mais lento e executa as threads uma de cada vez,
# por isso usa uma amostra menor do arquivo.
if command -v valgrind > /dev/null; then
    echo "==> cachegrind (amostra de 500 mil linhas)"
    PEQ="$OUT/amostra_500k.log"
    head -n 500000 "$ARQ" > "$PEQ"
    ln -sf "$(realpath "$PEQ")" access_log_large.txt
    for cfg in "seq ./log_analyzer_seq" \
               "mutex ./log_analyzer_par $TH $PEQ" \
               "reducao ./log_analyzer_par_optimized $TH $PEQ"; do
        set -- $cfg; nome=$1; shift
        valgrind --tool=cachegrind --cache-sim=yes \
            --cachegrind-out-file="$OUT/cg_$nome.out" "$@" > /dev/null 2> "$OUT/cachegrind_$nome.txt"
        echo "    -> $OUT/cachegrind_$nome.txt"
    done
    rm -f "$PEQ"
else
    echo "!! valgrind não encontrado. Instale: sudo apt install -y valgrind"
fi

rm -f access_log_large.txt
echo "Pronto. Resultados em $OUT/"
