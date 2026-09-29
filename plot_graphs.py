import csv
import os
import sys
import statistics as st
from collections import defaultdict
 
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
 
RAIZ = os.path.dirname(os.path.abspath(__file__))
# Uso: python3 plot_graphs.py [pasta_resultados] [pasta_graficos]
#   ex: python3 plot_graphs.py resultados_10M graficos_10M
DIR_RES = os.path.join(RAIZ, sys.argv[1] if len(sys.argv) > 1 else "resultados")
DIR_GRAF = os.path.join(RAIZ, sys.argv[2] if len(sys.argv) > 2 else "graficos")
 
# Paleta categórica (ordem fixa, validada para daltonismo) + tons neutros
COR = {"reducao": "#2a78d6", "mutex": "#eb6834", "seq": "#1baf7a"}
NOME = {"reducao": "Redução local", "mutex": "Mutex global", "seq": "Sequencial"}
COR_IDEAL = "#52514e"
TEXTO = "#0b0b0b"
TEXTO_2 = "#52514e"
GRADE = "#e4e3df"
 
plt.rcParams.update({
    "figure.figsize": (7.5, 4.6),
    "figure.dpi": 150,
    "savefig.dpi": 200,
    "savefig.bbox": "tight",
    "font.size": 10,
    "axes.titlesize": 12,
    "axes.titleweight": "bold",
    "axes.labelcolor": TEXTO,
    "axes.edgecolor": GRADE,
    "axes.spines.top": False,
    "axes.spines.right": False,
    "axes.grid": True,
    "grid.color": GRADE,
    "grid.linewidth": 0.8,
    "xtick.color": TEXTO_2,
    "ytick.color": TEXTO_2,
    "lines.linewidth": 2,
    "lines.markersize": 7,
    "legend.frameon": False,
})
 
def le_csv(nome):
    caminho = os.path.join(DIR_RES, nome)
    if not os.path.exists(caminho):
        print(f"(sem {nome}, pulando)")
        return None
    with open(caminho, encoding="utf-8") as f:
        return list(csv.DictReader(f))
 
 
def media_desvio(valores):
    m = st.mean(valores)
    d = st.stdev(valores) if len(valores) > 1 else 0.0
    return m, d
 
 
def agrupa(linhas, *chaves):
    g = defaultdict(list)
    for l in linhas:
        g[tuple(l[c] for c in chaves)].append(float(l["tempo_s"]))
    return g
 
 
def salva(fig, nome):
    caminho = os.path.join(DIR_GRAF, nome)
    fig.savefig(caminho)
    plt.close(fig)
    print(f"-> {os.path.relpath(caminho, RAIZ)}")
 
 
def rotulo_final(ax, x, y, texto, cor):
    """Rótulo direto no fim da série (a legenda continua existindo)."""
    ax.annotate(texto, (x, y), xytext=(6, 0), textcoords="offset points",
                va="center", fontsize=9, color=TEXTO)
    ax.plot([x], [y], "o", color=cor, markeredgecolor="white", markeredgewidth=2)

def rotulo_bloco(b):
    """1024 -> '1 KB', 2097152 -> '2 MB'"""
    return f"{b // 1024} KB" if b < 1024 * 1024 else f"{b // (1024 * 1024)} MB"

# Strong scaling: gráficos 1, 2, 3 e tabelas 1 e 4
def strong(tabelas):
    dados = le_csv("strong.csv")
    if not dados:
        return
    g = agrupa(dados, "versao", "threads")
    t_seq, d_seq = media_desvio(g[("seq", "1")])
    threads = sorted({int(l["threads"]) for l in dados if l["versao"] != "seq"})
    req = dados[0]["requisicoes"]
 
    res = {}  # versao -> lista de (threads, media, desvio, speedup, eficiencia)
    for v in ("mutex", "reducao"):
        res[v] = []
        for n in threads:
            m, d = media_desvio(g[(v, str(n))])
            s = t_seq / m
            res[v].append((n, m, d, s, s / n))
 
    # Gráfico 1: Speedup
    fig, ax = plt.subplots()
    ax.plot(threads, threads, "--", color=COR_IDEAL, linewidth=1.5, label="Ideal (speedup = N)")
    for v in ("reducao", "mutex"):
        xs = [r[0] for r in res[v]]
        ys = [r[3] for r in res[v]]
        ax.plot(xs, ys, "-o", color=COR[v], label=NOME[v],
                markeredgecolor="white", markeredgewidth=1.5)
        rotulo_final(ax, xs[-1], ys[-1], f"{ys[-1]:.2f}x", COR[v])
    ax.set_xscale("log", base=2)
    ax.set_xticks(threads, [str(n) for n in threads])
    ax.set_xlabel("Número de threads")
    ax.set_ylabel("Speedup (T_seq / T_par)")
    ax.set_title("Speedup vs. número de threads (strong scaling)", pad=20)
    ax.text(0, 1.015, f"Arquivo fixo: {int(req):,} requisições  |  T_seq = {t_seq:.3f} s",
            transform=ax.transAxes, fontsize=8.5, color=TEXTO_2)
    ax.set_ylim(bottom=0)
    ax.legend(loc="upper left")
    salva(fig, "1_speedup.png")
 
    # Gráfico 2: Eficiência
    fig, ax = plt.subplots()
    ax.axhline(100, linestyle="--", color=COR_IDEAL, linewidth=1.5, label="Ideal (100%)")
    for v in ("reducao", "mutex"):
        xs = [r[0] for r in res[v]]
        ys = [100 * r[4] for r in res[v]]
        ax.plot(xs, ys, "-o", color=COR[v], label=NOME[v],
                markeredgecolor="white", markeredgewidth=1.5)
        rotulo_final(ax, xs[-1], ys[-1], f"{ys[-1]:.0f}%", COR[v])
    ax.set_xscale("log", base=2)
    ax.set_xticks(threads, [str(n) for n in threads])
    ax.set_xlabel("Número de threads")
    ax.set_ylabel("Eficiência (Speedup / N) [%]")
    ax.set_title("Eficiência vs. número de threads")
    ax.set_ylim(0, max(110, max(100 * r[4] for v in res for r in res[v]) * 1.1))
    ax.legend(loc="lower left")
    salva(fig, "2_eficiencia.png")
 
    # Gráfico 3: Mutex global vs Redução local (tempo)
    fig, ax = plt.subplots()
    largura = 0.38
    pos = range(len(threads))
    for i, v in enumerate(("mutex", "reducao")):
        xs = [p + (i - 0.5) * largura for p in pos]
        ys = [r[1] for r in res[v]]
        err = [r[2] for r in res[v]]
        ax.bar(xs, ys, width=largura - 0.03, color=COR[v], label=NOME[v],
               yerr=err, capsize=3, error_kw={"ecolor": TEXTO_2, "elinewidth": 1})
    ax.axhline(t_seq, linestyle="--", color=COR_IDEAL, linewidth=1.5,
               label=f"Sequencial ({t_seq:.3f} s)")
    ax.set_xticks(list(pos), [str(n) for n in threads])
    ax.set_xlabel("Número de threads")
    ax.set_ylabel("Tempo de execução [s]")
    ax.set_title("Mutex global vs. redução local")
    ax.grid(axis="x", visible=False)
    ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.14), ncol=3)
    salva(fig, "3_mutex_vs_reducao.png")
 
    # Tabelas 1 e 4 
    t1 = ["### Tabela 1 - Strong scaling (tempo, speedup e eficiência)\n",
          f"Arquivo fixo com {int(req):,} requisições. Médias de "
          f"{len(g[('seq', '1')])} execuções. T_seq = {t_seq:.4f} s (± {d_seq:.4f}).\n",
          "| Threads | Versão | Tempo médio (s) | Desvio (s) | Speedup | Eficiência |",
          "|---:|---|---:|---:|---:|---:|"]
    for v in ("mutex", "reducao"):
        for n, m, d, s, e in res[v]:
            t1.append(f"| {n} | {NOME[v]} | {m:.4f} | {d:.4f} | {s:.2f} | {100 * e:.1f}% |")
    tabelas.append("\n".join(t1))
 
    t4 = ["### Tabela 4 - Mutex global vs. redução local\n",
          "Overhead de sincronização = (T_mutex − T_redução) / T_mutex.\n",
          "| Threads | Mutex global (s) | Redução local (s) | Redução é X vezes mais rápida | Overhead do mutex |",
          "|---:|---:|---:|---:|---:|"]
    for (n, mm, _, _, _), (_, mr, _, _, _) in zip(res["mutex"], res["reducao"]):
        t4.append(f"| {n} | {mm:.4f} | {mr:.4f} | {mm / mr:.2f}x | {100 * (mm - mr) / mm:.1f}% |")
    tabelas.append("\n".join(t4))
  
# Weak scaling: gráfico 5 e tabela 2
def weak(tabelas):
    dados = le_csv("weak.csv")
    if not dados:
        return
    g = agrupa(dados, "versao", "threads")
    req = {}
    for l in dados:
        req[int(l["threads"])] = int(l["requisicoes"])
    threads = sorted(req)
 
    fig, ax = plt.subplots()
    linhas_tab = []
    for v in ("reducao", "mutex"):
        pts = [(n, *media_desvio(g[(v, str(n))])) for n in threads]
        xs = [req[n] for n, _, _ in pts]
        ys = [m for _, m, _ in pts]
        ax.errorbar(xs, ys, yerr=[d for _, _, d in pts], fmt="-o", color=COR[v],
                    label=NOME[v], capsize=3, markeredgecolor="white", markeredgewidth=1.5)
        t1 = pts[0][1]
        for (n, m, d), x in zip(pts, xs):
            linhas_tab.append((n, x, NOME[v], m, d, t1 / m))
            if v == "reducao":
                ax.annotate(f"{n}T", (x, m), xytext=(0, -16), textcoords="offset points",
                            ha="center", fontsize=8.5, color=TEXTO_2)
    ax.set_xscale("log", base=2)
    ax.set_xticks([req[n] for n in threads], [f"{req[n] / 1e6:.1f}M" for n in threads])
    ax.minorticks_off()
    ax.set_xlabel("Tamanho do problema [requisições]  (threads crescem junto)")
    ax.set_ylabel("Tempo de execução [s]")
    ax.set_title("Weak scaling: carga fixa por thread")
    ax.set_ylim(bottom=0)
    ax.legend(loc="upper left")
    salva(fig, "5_weak_scaling.png")
 
    t2 = ["### Tabela 2 - Weak scaling\n",
          "Ideal: tempo constante. Eficiência fraca = T(1 thread) / T(N threads).\n",
          "| Threads | Requisições | Versão | Tempo médio (s) | Desvio (s) | Eficiência fraca |",
          "|---:|---:|---|---:|---:|---:|"]
    for n, x, nome, m, d, e in sorted(linhas_tab, key=lambda r: (r[2], r[0])):
        t2.append(f"| {n} | {x:,} | {nome} | {m:.4f} | {d:.4f} | {100 * e:.1f}% |")
    tabelas.append("\n".join(t2))

# Granularidade: gráfico 4 e tabela 3
def granularidade(tabelas):
    dados = le_csv("granularidade.csv")
    if not dados:
        return
    g = agrupa(dados, "bloco_bytes")
    th = dados[0]["threads"]
    est_m, est_d = media_desvio(g[("0",)])
    blocos = sorted(int(b[0]) for b in g if b[0] != "0")
    pts = [(b, *media_desvio(g[(str(b),)])) for b in blocos]
    melhor = min(pts, key=lambda p: p[1])
 
    fig, ax = plt.subplots()
    xs = [b / 1024 for b, _, _ in pts]
    ax.errorbar(xs, [m for _, m, _ in pts], yerr=[d for _, _, d in pts], fmt="-o",
                color=COR["reducao"], capsize=3, label="Distribuição dinâmica",
                markeredgecolor="white", markeredgewidth=1.5)
    ax.axhline(est_m, linestyle="--", color=COR_IDEAL, linewidth=1.5,
               label=f"Estático, 1 bloco/thread ({est_m:.3f} s)")
    ax.plot([melhor[0] / 1024], [melhor[1]], "o", markersize=13, markerfacecolor="none",
            markeredgecolor=TEXTO, markeredgewidth=1.5)
    ax.annotate(f"Melhor bloco: {rotulo_bloco(melhor[0])}\n{melhor[1]:.3f} s",
                (melhor[0] / 1024, melhor[1]), xytext=(0, -30), textcoords="offset points",
                ha="center", va="top", fontsize=9, color=TEXTO)
    ax.set_xscale("log", base=2)
    ax.set_xticks(xs, [rotulo_bloco(b) for b, _, _ in pts], rotation=45)
    ax.minorticks_off()
    ax.set_xlabel("Tamanho do bloco")
    ax.set_ylabel("Tempo de execução [s]")
    ax.set_title(f"Granularidade: tempo vs. tamanho do bloco ({th} threads)")
    lo = min(min(m for _, m, _ in pts), est_m)
    ax.set_ylim(bottom=lo * 0.8 - 0.02 * lo)
    ax.legend(loc="best")
    salva(fig, "4_granularidade.png")
 
    t3 = [f"### Tabela 3 - Granularidade ({th} threads, redução local)\n",
          "| Bloco | Tempo médio (s) | Desvio (s) | vs. estático |",
          "|---:|---:|---:|---:|",
          f"| estático (1/thread) | {est_m:.4f} | {est_d:.4f} | - |"]
    for b, m, d in pts:
        marca = " **(melhor dinâmico)**" if b == melhor[0] else ""
        t3.append(f"| {rotulo_bloco(b)}{marca} | {m:.4f} | {d:.4f} | {100 * (m - est_m) / est_m:+.1f}% |")
    tabelas.append("\n".join(t3))

def main():
    os.makedirs(DIR_GRAF, exist_ok=True)
    tabelas = []
    strong(tabelas)
    weak(tabelas)
    granularidade(tabelas)
    if tabelas:
        # ordena Tabela 1, 2, 3, 4
        tabelas.sort(key=lambda t: t.split(" - ")[0])
        caminho = os.path.join(DIR_RES, "tabelas.md")
        with open(caminho, "w", encoding="utf-8") as f:
            f.write("\n\n".join(tabelas) + "\n")
        print(f"-> {os.path.relpath(caminho, RAIZ)}")
 
 
if __name__ == "__main__":
    main()
