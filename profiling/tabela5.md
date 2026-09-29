### Tabela 5 - Métricas de profiling (perf stat, média de 3 execuções)

| Versão | Ciclos | Instruções | CPI | Cache misses | Taxa de cache miss | Branch misses | Taxa de branch miss | Trocas de contexto |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| Sequencial | 32,692,333,292 | 59,484,955,991 | 0.55 | 14,318,099 | 4.20% | 89,556,068 | 0.71% | 36 |
| Mutex global (8 threads) | 125,378,992,599 | 76,106,808,477 | 1.65 | 432,399,449 | 13.32% | 548,650,620 | 3.24% | 213,604 |
| Redução local (8 threads) | 34,114,832,695 | 59,621,554,858 | 0.57 | 18,784,998 | 4.28% | 97,686,581 | 0.77% | 20 |
