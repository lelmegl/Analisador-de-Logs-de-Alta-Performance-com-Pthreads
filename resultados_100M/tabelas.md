### Tabela 1 - Strong scaling (tempo, speedup e eficiência)

Arquivo fixo com 100,000,000 requisições. Médias de 3 execuções. T_seq = 75.6941 s (± 0.0921).

| Threads | Versão | Tempo médio (s) | Desvio (s) | Speedup | Eficiência |
|---:|---|---:|---:|---:|---:|
| 1 | Mutex global | 80.0793 | 0.1316 | 0.95 | 94.5% |
| 2 | Mutex global | 52.7760 | 0.8687 | 1.43 | 71.7% |
| 4 | Mutex global | 49.3789 | 0.7679 | 1.53 | 38.3% |
| 8 | Mutex global | 57.2988 | 0.7510 | 1.32 | 16.5% |
| 16 | Mutex global | 67.4576 | 0.4277 | 1.12 | 7.0% |
| 1 | Redução local | 75.5464 | 1.2604 | 1.00 | 100.2% |
| 2 | Redução local | 39.3593 | 1.5352 | 1.92 | 96.2% |
| 4 | Redução local | 39.0563 | 5.9736 | 1.94 | 48.5% |
| 8 | Redução local | 37.1327 | 2.7723 | 2.04 | 25.5% |
| 16 | Redução local | 39.2067 | 2.7758 | 1.93 | 12.1% |

### Tabela 2 - Weak scaling

Ideal: tempo constante. Eficiência fraca = T(1 thread) / T(N threads).

| Threads | Requisições | Versão | Tempo médio (s) | Desvio (s) | Eficiência fraca |
|---:|---:|---|---:|---:|---:|
| 1 | 10,000,000 | Mutex global | 7.7092 | 0.0222 | 100.0% |
| 2 | 20,000,000 | Mutex global | 10.2085 | 0.1190 | 75.5% |
| 4 | 40,000,000 | Mutex global | 19.2087 | 0.1376 | 40.1% |
| 8 | 80,000,000 | Mutex global | 45.2999 | 0.4935 | 17.0% |
| 1 | 10,000,000 | Redução local | 7.1557 | 0.0321 | 100.0% |
| 2 | 20,000,000 | Redução local | 7.2720 | 0.0855 | 98.4% |
| 4 | 40,000,000 | Redução local | 7.4100 | 0.0953 | 96.6% |
| 8 | 80,000,000 | Redução local | 31.0517 | 5.8760 | 23.0% |

### Tabela 3 - Granularidade (8 threads, redução local)

| Bloco | Tempo médio (s) | Desvio (s) | vs. estático |
|---:|---:|---:|---:|
| estático (1/thread) | 31.7830 | 1.6867 | - |
| 1 KB | 43.4167 | 1.4398 | +36.6% |
| 2 KB | 44.0151 | 1.4172 | +38.5% |
| 4 KB **(melhor dinâmico)** | 40.9928 | 0.3914 | +29.0% |
| 8 KB | 41.8856 | 2.1517 | +31.8% |
| 16 KB | 43.9667 | 0.6986 | +38.3% |
| 32 KB | 45.9147 | 2.0351 | +44.5% |
| 64 KB | 45.6114 | 2.6299 | +43.5% |
| 128 KB | 45.8845 | 0.6511 | +44.4% |
| 256 KB | 45.5987 | 1.4010 | +43.5% |
| 512 KB | 44.9409 | 0.6902 | +41.4% |
| 1 MB | 45.1402 | 0.3849 | +42.0% |
| 2 MB | 57.1075 | 1.0300 | +79.7% |
| 4 MB | 56.6791 | 1.0462 | +78.3% |
| 8 MB | 52.8075 | 1.4078 | +66.2% |
| 16 MB | 52.9539 | 4.1359 | +66.6% |

### Tabela 4 - Mutex global vs. redução local

Overhead de sincronização = (T_mutex − T_redução) / T_mutex.

| Threads | Mutex global (s) | Redução local (s) | Redução é X vezes mais rápida | Overhead do mutex |
|---:|---:|---:|---:|---:|
| 1 | 80.0793 | 75.5464 | 1.06x | 5.7% |
| 2 | 52.7760 | 39.3593 | 1.34x | 25.4% |
| 4 | 49.3789 | 39.0563 | 1.26x | 20.9% |
| 8 | 57.2988 | 37.1327 | 1.54x | 35.2% |
| 16 | 67.4576 | 39.2067 | 1.72x | 41.9% |
