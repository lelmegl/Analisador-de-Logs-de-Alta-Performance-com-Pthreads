import argparse
import csv
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
 
RAIZ = os.path.dirname(os.path.abspath(__file__))
DIR_DADOS = os.path.join(RAIZ, "dados")
DIR_RES = os.path.join(RAIZ, "resultados")
GERADOR = os.path.join(RAIZ, "generate_log_advanced.py")
 
SEQ = os.path.join(RAIZ, "log_analyzer_seq")
PAR = os.path.join(RAIZ, "log_analyzer_par")
OPT = os.path.join(RAIZ, "log_analyzer_par_optimized")
 
SEED = 42  # semente fixa: mesmos arquivos em qualquer máquina
 
RE_TEMPO = re.compile(r"TEMPO DE EXECU\S*:\s*([\d.]+)")
RE_TOTAL = re.compile(r"Total de Requisi\S*:\s*([\d,]+)")

# Utilidades
def log(msg):
    print(msg, flush=True)
 
 
def compila():
    log("==> Compilando (make)")
    r = subprocess.run(["make"], cwd=RAIZ, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("Erro no make:\n" + r.stdout + r.stderr)
    for b in (SEQ, PAR, OPT):
        if not os.path.exists(b):
            sys.exit(f"Binário não encontrado: {b}")
 
 
def gera_arquivo(linhas, nome):
    """Gera o log com semente fixa, se ainda não existir."""
    caminho = os.path.join(DIR_DADOS, nome)
    if os.path.exists(caminho):
        log(f"    (reaproveitando {nome})")
        return caminho
    log(f"    gerando {nome} ({linhas:,} linhas pedidas)...")
    subprocess.run([sys.executable, GERADOR, "--lines", str(linhas),
                    "--output", caminho, "--seed", str(SEED), "--quiet"],
                   check=True, stdout=subprocess.DEVNULL)
    return caminho
 
 
def executa(cmd, cwd=None):
    """Roda um analisador e devolve (tempo, total_requisicoes)."""
    r = subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise RuntimeError(f"Falhou: {' '.join(cmd)}\n{r.stderr}")
    t = RE_TEMPO.search(r.stdout)
    n = RE_TOTAL.search(r.stdout)
    if not t or not n:
        raise RuntimeError(f"Não achei tempo/total na saída de {' '.join(cmd)}")
    return float(t.group(1)), int(n.group(1).replace(",", ""))
 
 
def roda_seq(arquivo):
    """O seq só lê ./access_log_large.txt: roda numa pasta temporária com um link."""
    with tempfile.TemporaryDirectory() as tmp:
        os.symlink(os.path.abspath(arquivo), os.path.join(tmp, "access_log_large.txt"))
        return executa([SEQ], cwd=tmp)
 
 
def aquece(arquivo):
    """Lê o arquivo uma vez para ele entrar no cache de páginas do SO,
    senão a 1ª execução mede o disco e as outras a memória."""
    with open(arquivo, "rb") as f:
        while f.read(1 << 24):
            pass
 
 
class Csv:
    def __init__(self, nome, cabecalho):
        self.caminho = os.path.join(DIR_RES, nome)
        self.f = open(self.caminho, "w", newline="", encoding="utf-8")
        self.w = csv.writer(self.f)
        self.w.writerow(cabecalho)
 
    def linha(self, *valores):
        self.w.writerow(valores)
        self.f.flush()
 
    def fecha(self):
        self.f.close()
        log(f"    -> {os.path.relpath(self.caminho, RAIZ)}")
 
 
def confere_total(esperado, obtido, rotulo):
    if esperado is not None and obtido != esperado:
        log(f"    !! ATENÇÃO: {rotulo} contou {obtido:,} requisições, "
            f"esperado {esperado:,} (resultado inconsistente)")
      
# Ambiente (seção 2 do relatório)
def salva_ambiente():
    caminho = os.path.join(DIR_RES, "ambiente.txt")
    cmds = [
        ("Sistema", ["uname", "-a"]),
        ("CPU", ["lscpu"]),
        ("Memória", ["free", "-h"]),
        ("Discos", ["lsblk", "-d", "-o", "NAME,ROTA,SIZE,MODEL"]),
        ("GCC", ["gcc", "--version"]),
    ]
    with open(caminho, "w", encoding="utf-8") as f:
        f.write(f"Python: {platform.python_version()}\n")
        f.write(f"Núcleos lógicos (os.cpu_count): {os.cpu_count()}\n\n")
        for titulo, c in cmds:
            f.write(f"===== {titulo}: {' '.join(c)} =====\n")
            try:
                f.write(subprocess.run(c, capture_output=True, text=True).stdout)
            except FileNotFoundError:
                f.write("(comando não disponível)\n")
            f.write("\n")
        f.write("Flags de compilação: veja o Makefile (CFLAGS)\n")
    log(f"    -> {os.path.relpath(caminho, RAIZ)}")

# Experimentos
def strong_scaling(args):
    log("\n==> 1. STRONG SCALING")
    arq = gera_arquivo(args.lines_strong, f"strong_{args.lines_strong}.log")
    aquece(arq)
    out = Csv("strong.csv", ["versao", "threads", "rep", "tempo_s", "requisicoes"])
 
    esperado = None
    for rep in range(1, args.reps + 1):
        t, n = roda_seq(arq)
        esperado = esperado or n
        out.linha("seq", 1, rep, t, n)
        log(f"    seq                rep {rep}: {t:.4f} s")
 
    for th in args.threads:
        for versao, binario in (("mutex", PAR), ("reducao", OPT)):
            for rep in range(1, args.reps + 1):
                t, n = executa([binario, str(th), arq])
                confere_total(esperado, n, f"{versao} {th}T")
                out.linha(versao, th, rep, t, n)
                log(f"    {versao:<8} {th:>2} threads rep {rep}: {t:.4f} s")
    out.fecha()
 
 
def weak_scaling(args):
    log("\n==> 2. WEAK SCALING")
    out = Csv("weak.csv", ["versao", "threads", "linhas_pedidas", "rep", "tempo_s", "requisicoes"])
    for th in args.threads_weak:
        linhas = args.lines_weak * th
        arq = gera_arquivo(linhas, f"weak_{linhas}.log")
        aquece(arq)
        for versao, binario in (("mutex", PAR), ("reducao", OPT)):
            for rep in range(1, args.reps + 1):
                t, n = executa([binario, str(th), arq])
                out.linha(versao, th, linhas, rep, t, n)
                log(f"    {versao:<8} {th:>2} threads ({n:,} req) rep {rep}: {t:.4f} s")
    out.fecha()
 
 
def granularidade(args):
    log("\n==> 3. GRANULARIDADE")
    arq = gera_arquivo(args.lines_strong, f"strong_{args.lines_strong}.log")
    aquece(arq)
    th = args.threads_gran
    out = Csv("granularidade.csv", ["threads", "bloco_bytes", "rep", "tempo_s", "requisicoes"])
    esperado = None
 
    # referência: divisão estática (1 bloco por thread)
    for rep in range(1, args.reps + 1):
        t, n = executa([OPT, str(th), arq])
        esperado = esperado or n
        out.linha(th, 0, rep, t, n)
        log(f"    estático             rep {rep}: {t:.4f} s")
 
    bloco = 1024
    while bloco <= 1024 * 1024:
        for rep in range(1, args.reps + 1):
            t, n = executa([OPT, str(th), arq, str(bloco)])
            confere_total(esperado, n, f"bloco {bloco}")
            out.linha(th, bloco, rep, t, n)
            log(f"    bloco {bloco // 1024:>5} KB      rep {rep}: {t:.4f} s")
        bloco *= 2
    out.fecha()

def main():
    p = argparse.ArgumentParser(description="Experimentos do LAB1 - Analisador de Logs")
    p.add_argument("--reps", type=int, default=3, help="repetições por configuração (padrão 3)")
    p.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8, 16],
                   help="threads do strong scaling (padrão 1 2 4 8 16)")
    p.add_argument("--threads-weak", type=int, nargs="+", default=[1, 2, 4, 8],
                   help="threads do weak scaling (padrão 1 2 4 8)")
    p.add_argument("--threads-gran", type=int, default=min(8, os.cpu_count() or 4),
                   help="threads do teste de granularidade (padrão: min(8, núcleos))")
    p.add_argument("--lines-strong", type=int, default=10_000_000,
                   help="linhas pedidas ao gerador no strong scaling (padrão 10M)")
    p.add_argument("--lines-weak", type=int, default=2_000_000,
                   help="linhas pedidas POR THREAD no weak scaling (padrão 2M)")
    p.add_argument("--only", choices=["strong", "weak", "gran"],
                   help="roda só um experimento")
    args = p.parse_args()
 
    os.makedirs(DIR_DADOS, exist_ok=True)
    os.makedirs(DIR_RES, exist_ok=True)
 
    compila()
    log("==> Salvando especificação do ambiente")
    salva_ambiente()
 
    if args.only in (None, "strong"):
        strong_scaling(args)
    if args.only in (None, "weak"):
        weak_scaling(args)
    if args.only in (None, "gran"):
        granularidade(args)
 
    log("\nPronto. Agora rode:  python3 plot_graphs.py")
 
 
if __name__ == "__main__":
    main()
 
