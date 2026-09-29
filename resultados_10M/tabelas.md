### Tabela 1 - Strong scaling (tempo, speedup e eficiência)

Arquivo fixo com 10,000,000 requisições. Médias de 5 execuções. T_seq = 7.2354 s (± 0.0901).

| Threads | Versão | Tempo médio (s) | Desvio (s) | Speedup | Eficiência |
|---:|---|---:|---:|---:|---:|
| 1 | Mutex global | 7.6273 | 0.0621 | 0.95 | 94.9% |
| 2 | Mutex global | 4.9230 | 0.0846 | 1.47 | 73.5% |
| 4 | Mutex global | 4.8172 | 0.0212 | 1.50 | 37.5% |
| 6 | Mutex global | 5.1469 | 0.0202 | 1.41 | 23.4% |
| 8 | Mutex global | 5.7396 | 0.0164 | 1.26 | 15.8% |
| 12 | Mutex global | 6.4848 | 0.0693 | 1.12 | 9.3% |
| 16 | Mutex global | 6.6776 | 0.0774 | 1.08 | 6.8% |
| 1 | Redução local | 7.1149 | 0.1151 | 1.02 | 101.7% |
| 2 | Redução local | 3.5792 | 0.0259 | 2.02 | 101.1% |
| 4 | Redução local | 1.8151 | 0.0414 | 3.99 | 99.7% |
| 6 | Redução local | 1.2744 | 0.0056 | 5.68 | 94.6% |
| 8 | Redução local | 0.9717 | 0.0021 | 7.45 | 93.1% |
| 12 | Redução local | 0.7351 | 0.0151 | 9.84 | 82.0% |
| 16 | Redução local | 0.8151 | 0.0174 | 8.88 | 55.5% |

### Tabela 2 - Weak scaling

Ideal: tempo constante. Eficiência fraca = T(1 thread) / T(N threads).

| Threads | Requisições | Versão | Tempo médio (s) | Desvio (s) | Eficiência fraca |
|---:|---:|---|---:|---:|---:|
| 1 | 1,250,000 | Mutex global | 0.9692 | 0.0096 | 100.0% |
| 2 | 2,500,000 | Mutex global | 1.2263 | 0.0149 | 79.0% |
| 4 | 5,000,000 | Mutex global | 2.4596 | 0.0374 | 39.4% |
| 8 | 10,000,000 | Mutex global | 5.9195 | 0.0432 | 16.4% |
| 12 | 15,000,000 | Mutex global | 10.2406 | 0.1261 | 9.5% |
| 1 | 1,250,000 | Redução local | 0.8983 | 0.0207 | 100.0% |
| 2 | 2,500,000 | Redução local | 0.8970 | 0.0080 | 100.1% |
| 4 | 5,000,000 | Redução local | 0.9348 | 0.0325 | 96.1% |
| 8 | 10,000,000 | Redução local | 0.9910 | 0.0038 | 90.7% |
| 12 | 15,000,000 | Redução local | 1.2159 | 0.0277 | 73.9% |

### Tabela 3 - Granularidade (12 threads, redução local)

| Bloco | Tempo médio (s) | Desvio (s) | vs. estático |
|---:|---:|---:|---:|
| estático (1/thread) | 0.7771 | 0.0375 | - |
| 1 KB | 1.1869 | 0.0263 | +52.7% |
| 2 KB | 0.9601 | 0.0047 | +23.5% |
| 4 KB | 0.8493 | 0.0034 | +9.3% |
| 8 KB | 0.7745 | 0.0078 | -0.3% |
| 16 KB | 0.7306 | 0.0020 | -6.0% |
| 32 KB | 0.7032 | 0.0028 | -9.5% |
| 64 KB | 0.6964 | 0.0059 | -10.4% |
| 128 KB | 0.6938 | 0.0062 | -10.7% |
| 256 KB | 0.6942 | 0.0027 | -10.7% |
| 512 KB **(melhor dinâmico)** | 0.6884 | 0.0013 | -11.4% |
| 1 MB | 0.6900 | 0.0018 | -11.2% |
| 2 MB | 0.6897 | 0.0019 | -11.3% |
| 4 MB | 0.7332 | 0.0275 | -5.7% |
| 8 MB | 0.7814 | 0.0159 | +0.6% |
| 16 MB | 0.7957 | 0.0186 | +2.4% |

### Tabela 4 - Mutex global vs. redução local

Overhead de sincronização = (T_mutex − T_redução) / T_mutex.

| Threads | Mutex global (s) | Redução local (s) | Redução é X vezes mais rápida | Overhead do mutex |
|---:|---:|---:|---:|---:|
| 1 | 7.6273 | 7.1149 | 1.07x | 6.7% |
| 2 | 4.9230 | 3.5792 | 1.38x | 27.3% |
| 4 | 4.8172 | 1.8151 | 2.65x | 62.3% |
| 6 | 5.1469 | 1.2744 | 4.04x | 75.2% |
| 8 | 5.7396 | 0.9717 | 5.91x | 83.1% |
| 12 | 6.4848 | 0.7351 | 8.82x | 88.7% |
| 16 | 6.6776 | 0.8151 | 8.19x | 87.8% |
