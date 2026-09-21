#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
GERADOR DE LOGS DE SERVIDOR WEB - ANALISADOR DE LOGS PARALELO
================================================================

DESCRIÇÃO:
    Este script gera arquivos de log de servidor web no formato Common Log Format
    com dados realistas para o projeto "Analisador de Logs de Alta Performance com Pthreads".
    
    O gerador produz logs com características realistas incluindo:
    - Distribuição variada de IPs (com pesos realistas)
    - URLs hierárquicas (páginas, produtos, APIs, recursos estáticos)
    - Múltiplos métodos HTTP (GET, POST, PUT, DELETE, HEAD)
    - Distribuição realista de status codes (200, 404, 500, etc.)
    - User-Agents variados (Chrome, Firefox, Safari, bots, mobile)
    - Padrões temporais (horários de pico e vales)
    - Tamanhos de bytes baseados no tipo de recurso

USO:
    # Gerar um arquivo com 1 milhão de linhas (padrão)
    python3 generate_log_advanced.py
    
    # Gerar com tamanho personalizado
    python3 generate_log_advanced.py --lines 5000000
    
    # Gerar com nome de arquivo personalizado
    python3 generate_log_advanced.py --output meu_log.txt
    
    # Gerar com semente específica (para reprodutibilidade)
    python3 generate_log_advanced.py --seed 12345
    
    # Gerar múltiplos arquivos de teste
    python3 generate_log_advanced.py --test

EXEMPLOS:
    # Gerar arquivo pequeno para testes rápidos
    python3 generate_log_advanced.py --lines 10000 --output test.log
    
    # Gerar arquivo para teste de escalabilidade (10M linhas)
    python3 generate_log_advanced.py --lines 10000000 --output access_log_10M.txt
    
    # Gerar todos os arquivos de teste (1k, 10k, 100k, 1M, 10M)
    python3 generate_log_advanced.py --test


Disciplina: Computação Paralela - FCI Mackenzie

DATA:
    Setembro 2025

VERSÃO:
    2.0 - Gerador Avançado com Dados Realistas

LICENÇA:
    Uso acadêmico - Projeto de Computação Paralela
"""

import random
import datetime
import time
import argparse
import sys
import os
from collections import defaultdict

# ============================================================================
# CONFIGURAÇÕES PADRÃO
# ============================================================================

# Configurações que podem ser sobrescritas pela linha de comando
DEFAULT_LINES = 1000000           # 1 milhão de linhas
DEFAULT_OUTPUT = "access_log_large.txt"
DEFAULT_SEED = None               # None = aleatório, ou use um número fixo

# ============================================================================
# DADOS PARA GERAÇÃO REALISTA
# ============================================================================

# IPs com distribuição realista (alguns mais frequentes que outros)
# Formato: (IP, peso) onde peso = probabilidade relativa
IP_ADDRESSES = [
    # IPs comuns (alta frequência) - simulam usuários regulares
    ("192.168.1.10", 0.15),      # 15% das requisições
    ("10.0.0.5", 0.12),
    ("172.16.31.40", 0.10),
    ("192.168.1.100", 0.08),
    ("10.0.0.15", 0.07),
    
    # IPs médios - simulam visitantes ocasionais
    ("203.0.113.78", 0.05),
    ("198.51.100.22", 0.05),
    ("8.8.8.8", 0.04),
    ("1.1.1.1", 0.04),
    ("54.12.34.56", 0.03),
    
    # IPs raros - simulam visitantes esporádicos
    ("98.76.54.32", 0.02),
    ("11.22.33.44", 0.02),
    ("66.55.44.33", 0.02),
    ("192.168.1.200", 0.01),
    ("10.0.0.200", 0.01),
    ("172.16.0.10", 0.01),
    ("192.168.1.50", 0.01),
    
    # IPs esporádicos - simulam visitantes únicos
    ("192.168.1.150", 0.005),
    ("10.0.0.100", 0.005),
    ("172.16.0.200", 0.005),
    ("192.168.1.250", 0.005),
]

# URLs com estrutura hierárquica realista
# Formato: (URL, peso)
RESOURCES = {
    # Páginas principais - alta frequência
    "/index.html": 0.18,
    "/about.html": 0.05,
    "/contact.html": 0.04,
    "/products.html": 0.06,
    "/services.html": 0.04,
    "/blog.html": 0.03,
    
    # Produtos - média frequência
    "/products/iphone": 0.04,
    "/products/android": 0.035,
    "/products/laptop": 0.03,
    "/products/headphones": 0.025,
    "/products/smartwatch": 0.02,
    "/products/tablet": 0.02,
    
    # APIs - alta frequência
    "/api/health": 0.06,
    "/api/users": 0.04,
    "/api/data": 0.035,
    "/api/login": 0.03,
    "/api/logout": 0.02,
    "/api/search": 0.025,
    
    # Recursos estáticos - média/alta frequência
    "/css/style.css": 0.06,
    "/js/app.js": 0.05,
    "/images/logo.png": 0.045,
    "/images/banner.jpg": 0.03,
    "/assets/font.woff2": 0.025,
    "/js/vendor.js": 0.02,
    
    # Admin - baixa frequência
    "/admin/": 0.01,
    "/admin/dashboard": 0.005,
    "/admin/users": 0.005,
    "/admin/settings": 0.005,
    
    # Outros
    "/downloads/software.zip": 0.01,
    "/docs/manual.pdf": 0.01,
    "/privacy.html": 0.015,
    "/terms.html": 0.015,
}

# URLs de erro (geram 404)
ERROR_RESOURCES = {
    "/old_page.html": 0.15,
    "/images/nonexistent.gif": 0.12,
    "/wp-admin.php": 0.10,
    "/private/secret.txt": 0.08,
    "/downloads/removed_file.zip": 0.07,
    "/old-blog/post.html": 0.06,
    "/images/missing.png": 0.05,
    "/css/old-style.css": 0.05,
    "/js/deprecated.js": 0.04,
    "/api/v1/old-endpoint": 0.04,
    "/products/discontinued": 0.03,
    "/about/old-team.html": 0.03,
    "/contact-old.html": 0.02,
    "/temp/file-not-found.txt": 0.02,
    "/backup/old-backup.zip": 0.01,
}

# User-Agents com distribuição realista
# Formato: (User-Agent, peso)
USER_AGENTS = [
    # Chrome - navegador mais popular
    ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36", 0.25),
    ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36", 0.10),
    ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36", 0.08),
    
    # Firefox - segundo mais popular
    ("Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101 Firefox/121.0", 0.10),
    ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15; rv:109.0) Gecko/20100101 Firefox/120.0", 0.05),
    ("Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:109.0) Gecko/20100101 Firefox/120.0", 0.03),
    
    # Safari - popular no ecossistema Apple
    ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.1 Safari/605.1.15", 0.08),
    ("Mozilla/5.0 (iPhone; CPU iPhone OS 17_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.1 Mobile/15E148 Safari/604.1", 0.06),
    
    # Edge - crescente no mercado
    ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36 Edg/120.0.0.0", 0.06),
    
    # Bots - importantes para SEO
    ("Googlebot/2.1 (+http://www.google.com/bot.html)", 0.03),
    ("Bingbot/2.0 (+http://www.bing.com/bingbot.htm)", 0.02),
    ("Mozilla/5.0 (compatible; AhrefsBot/7.0; +http://ahrefs.com/robot/)", 0.01),
    
    # Mobile - Android
    ("Mozilla/5.0 (Linux; Android 14; SM-G998B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Mobile Safari/537.36", 0.06),
    ("Mozilla/5.0 (Linux; Android 13; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/119.0.0.0 Mobile Safari/537.36", 0.04),
]

# Métodos HTTP com distribuição realista
METHODS = [
    ("GET", 0.82),     # GET é o mais comum
    ("POST", 0.12),    # POST para formulários e APIs
    ("PUT", 0.03),     # PUT para atualizações
    ("DELETE", 0.02),  # DELETE para remoções
    ("HEAD", 0.01),    # HEAD para verificação
]

# Status Codes com distribuição realista
STATUS_CODES = [
    # 2xx - Sucesso (80%)
    (200, 0.80),  # OK
    (201, 0.02),  # Created
    (204, 0.01),  # No Content
    
    # 3xx - Redirecionamento (5%)
    (301, 0.03),  # Moved Permanently
    (302, 0.015), # Found
    (304, 0.005), # Not Modified
    
    # 4xx - Erro do Cliente (12%)
    (400, 0.02),  # Bad Request
    (401, 0.01),  # Unauthorized
    (403, 0.015), # Forbidden
    (404, 0.07),  # Not Found - o erro mais comum
    (405, 0.005), # Method Not Allowed
    
    # 5xx - Erro do Servidor (3%)
    (500, 0.015),  # Internal Server Error
    (502, 0.005),  # Bad Gateway
    (503, 0.005),  # Service Unavailable
    (504, 0.005),  # Gateway Timeout
]

# ============================================================================
# FUNÇÕES AUXILIARES
# ============================================================================

def weighted_choice(choices):
    """
    Escolhe um item de uma lista de tuplas (item, peso).
    
    Args:
        choices: Lista de tuplas (item, peso)
    
    Returns:
        O item escolhido baseado nos pesos
    
    Exemplo:
        escolha = weighted_choice([("A", 0.8), ("B", 0.2)])
        # 80% de chance de retornar "A", 20% de chance de retornar "B"
    """
    if not choices:
        return None
    
    total_weight = sum(weight for _, weight in choices)
    r = random.random() * total_weight
    
    for item, weight in choices:
        r -= weight
        if r <= 0:
            return item
    
    return choices[-1][0]

def weighted_choice_dict(choices_dict):
    """
    Versão para dicionários.
    
    Args:
        choices_dict: Dicionário {item: peso}
    
    Returns:
        O item escolhido baseado nos pesos
    """
    items = list(choices_dict.items())
    return weighted_choice([(item, weight) for item, weight in items])

def get_bytes_for_resource(resource, status):
    """
    Determina o número de bytes baseado no tipo de recurso e status.
    
    Args:
        resource: URL do recurso
        status: Código de status HTTP
    
    Returns:
        Número de bytes (inteiro)
    """
    if status == 200:
        # Recursos estáticos (imagens, CSS, JS) - maiores
        if resource.startswith(("/images/", "/assets/", "/css/", "/js/")):
            return random.randint(1000, 50000)
        # APIs e admin - menores
        elif resource.startswith(("/api/", "/admin/")):
            return random.randint(50, 5000)
        # Páginas HTML - médios
        else:
            return random.randint(500, 15000)
    elif status == 404:
        # Páginas de erro - pequenas
        return random.randint(200, 800)
    elif status in [301, 302]:
        # Redirecionamentos - muito pequenos
        return random.randint(100, 500)
    else:
        # Outros erros
        return random.randint(50, 1000)

def generate_timestamp(base_date, hour, minute, second):
    """
    Gera um timestamp no formato do Apache.
    
    Formato: [dd/MMM/yyyy:HH:MM:SS timezone]
    
    Args:
        base_date: Data base (datetime)
        hour: Hora (0-23)
        minute: Minuto (0-59)
        second: Segundo (0-59)
    
    Returns:
        String formatada do timestamp
    """
    return base_date.strftime(f'[%d/%b/%Y:{hour:02d}:{minute:02d}:{second:02d} %z-0300]')

def generate_ip_with_bursts():
    """
    Gera IPs com padrões de explosão realistas.
    
    Em cenários reais, alguns IPs aparecem mais em certos momentos
    (ex: ataques, testes de carga, usuários ativos).
    
    Returns:
        Endereço IP como string
    """
    # 70% das vezes usa a distribuição normal
    if random.random() < 0.7:
        return weighted_choice(IP_ADDRESSES)
    else:
        # 30% das vezes concentra em alguns IPs (simula ataques ou picos)
        burst_ips = ["192.168.1.10", "192.168.1.100", "10.0.0.5", "172.16.31.40"]
        return random.choice(burst_ips)

def generate_hour_pattern(index):
    """
    Gera horário com padrões de pico realistas.
    
    Horários de pico: 10-12h e 14-17h (típico de sistemas empresariais)
    Horários baixos: 0-6h (madrugada)
    
    Args:
        index: Índice da linha (para distribuir ao longo do dia)
    
    Returns:
        Tupla (hora, minuto, segundo)
    """
    # Distribui ao longo de 24 horas (~1000 requisições/hora para 1M linhas)
    hour = (index // 1000) % 24
    minute = random.randint(0, 59)
    second = random.randint(0, 59)
    
    # Horários de pico - mais requisições
    if hour in [10, 11, 12, 14, 15, 16, 17]:
        # Aumenta a densidade de requisições (pula menos)
        minute = random.randint(0, 59)
        second = random.randint(0, 59)
    elif hour in [0, 1, 2, 3, 4, 5]:
        # Menos requisições de madrugada
        # Retorna None para indicar que deve pular esta linha
        if random.random() < 0.4:
            return None
    
    return hour, minute, second

# ============================================================================
# FUNÇÃO PRINCIPAL
# ============================================================================

def generate_log_file(num_lines, output_file, seed=None, show_progress=True):
    """
    Gera um arquivo de log com características realistas.
    
    Args:
        num_lines: Número de linhas a gerar
        output_file: Nome do arquivo de saída
        seed: Semente para o gerador aleatório (None para aleatório)
        show_progress: Exibir barra de progresso
    
    Returns:
        Dicionário com estatísticas geradas
    """
    
    # Configura a semente para reprodutibilidade
    if seed is not None:
        random.seed(seed)
        print(f"🔒 Usando semente fixa: {seed}")
    else:
        seed = random.randint(0, 2**32 - 1)
        print(f"🔓 Usando semente aleatória: {seed}")
    
    print(f"📊 Gerando {num_lines:,} linhas de log...")
    print(f"📁 Arquivo: {output_file}")
    print("⏳ Processando...")
    
    start_time = time.time()
    
    # Data base (15 de Setembro de 2025, UTC-3)
    base_date = datetime.datetime(2025, 9, 15, 0, 0, 0)
    base_date = base_date.replace(tzinfo=datetime.timezone(datetime.timedelta(hours=-3)))
    
    # Estatísticas para relatório final
    stats = {
        'total': 0,
        'errors': 0,
        'status_dist': defaultdict(int),
        'method_dist': defaultdict(int),
        'hour_dist': defaultdict(int),
        'ua_dist': defaultdict(int),
    }
    
    # Abre o arquivo para escrita
    with open(output_file, "w", encoding='utf-8') as f:
        for i in range(num_lines):
            # --- 1. Gera IP com distribuição realista ---
            ip = generate_ip_with_bursts()
            
            # --- 2. Gera timestamp com padrões temporais ---
            hour_pattern = generate_hour_pattern(i)
            if hour_pattern is None:
                continue  # Pula esta linha (menos tráfego de madrugada)
            
            hour, minute, second = hour_pattern
            timestamp = generate_timestamp(base_date, hour, minute, second)
            
            # --- 3. Gera método HTTP ---
            method = weighted_choice(METHODS)
            stats['method_dist'][method] += 1
            
            # --- 4. Gera status code ---
            status = weighted_choice(STATUS_CODES)
            stats['status_dist'][status] += 1
            
            # --- 5. Gera URL baseado no status ---
            if status == 404:
                resource = weighted_choice_dict(ERROR_RESOURCES)
            elif status in [500, 502, 503, 504]:
                # Erros de servidor geralmente em URLs dinâmicas
                resource = random.choice([
                    "/api/health", "/api/users", "/api/data", 
                    "/admin/dashboard", "/products.html"
                ])
            else:
                resource = weighted_choice_dict(RESOURCES)
            
            # --- 6. Gera bytes baseado no recurso e status ---
            bytes_sent = get_bytes_for_resource(resource, status)
            
            # --- 7. Gera User-Agent ---
            user_agent = weighted_choice(USER_AGENTS)
            ua_short = user_agent[:50]  # Para estatísticas
            stats['ua_dist'][ua_short] += 1
            
            # --- 8. Monta a linha de log ---
            log_line = f'{ip} - - {timestamp} "{method} {resource} HTTP/1.1" {status} {bytes_sent} "{user_agent}"\n'
            f.write(log_line)
            
            # --- 9. Atualiza estatísticas ---
            stats['total'] += 1
            stats['hour_dist'][hour] += 1
            if status >= 400:
                stats['errors'] += 1
            
            # --- 10. Barra de progresso ---
            if show_progress and (i + 1) % max(1, num_lines // 100) == 0:
                progress = (i + 1) / num_lines * 100
                print(f"\rProgresso: {progress:.0f}%", end="")
    
    # --- Relatório Final ---
    end_time = time.time()
    elapsed = end_time - start_time
    
    print(f"\rProgresso: 100%")
    print("\n" + "=" * 60)
    print("✅ RELATÓRIO DE GERAÇÃO")
    print("=" * 60)
    print(f"📄 Arquivo gerado: {output_file}")
    print(f"📊 Total de linhas: {stats['total']:,}")
    print(f"⏱️  Tempo de geração: {elapsed:.2f} segundos")
    print(f"📈 Taxa de erro: {stats['errors'] / stats['total'] * 100:.2f}%")
    
    print("\n📊 Distribuição de Status Codes:")
    for status in sorted(stats['status_dist'].keys()):
        count = stats['status_dist'][status]
        pct = count / stats['total'] * 100
        bar = "█" * int(pct / 2)  # Barra visual
        print(f"  {status}: {count:>8,} ({pct:>5.2f}%) {bar}")
    
    print("\n📊 Distribuição de Métodos:")
    for method in sorted(stats['method_dist'].keys()):
        count = stats['method_dist'][method]
        pct = count / stats['total'] * 100
        print(f"  {method}: {count:>8,} ({pct:>5.2f}%)")
    
    print("\n📊 Top 5 User-Agents:")
    sorted_ua = sorted(stats['ua_dist'].items(), key=lambda x: x[1], reverse=True)
    for ua, count in sorted_ua[:5]:
        pct = count / stats['total'] * 100
        print(f"  {ua[:40]}: {count:>8,} ({pct:>5.2f}%)")
    
    print("\n📊 Distribuição por Hora (Top 5):")
    sorted_hours = sorted(stats['hour_dist'].items(), key=lambda x: x[1], reverse=True)
    for hour, count in sorted_hours[:5]:
        pct = count / stats['total'] * 100
        print(f"  {hour:02d}:00: {count:>8,} ({pct:>5.2f}%)")
    
    print("=" * 60)
    
    return stats

# ============================================================================
# FUNÇÕES PARA TESTES
# ============================================================================

def generate_test_files():
    """Gera múltiplos arquivos de log para testes de escalabilidade."""
    print("\n" + "=" * 60)
    print("🧪 GERANDO ARQUIVOS DE TESTE")
    print("=" * 60)
    
    test_sizes = [
        ("tiny", 1000, "test_tiny.log"),
        ("small", 10000, "test_small.log"),
        ("medium", 100000, "test_medium.log"),
        ("large", 1000000, "test_large.log"),
        ("xl", 10000000, "test_xl.log"),
    ]
    
    for name, size, output in test_sizes:
        print(f"\n▶️  Gerando {name} ({size:,} linhas)...")
        generate_log_file(size, output, show_progress=False)
        print(f"✅ {name} concluído!\n")

# ============================================================================
# INTERFACE DE LINHA DE COMANDO
# ============================================================================

def parse_arguments():
    """Configura e processa os argumentos da linha de comando."""
    parser = argparse.ArgumentParser(
        description="Gerador de Logs para o Projeto de Computação Paralela",
        epilog="Exemplos:\n"
               "  python3 generate_log.py --lines 100000\n"
               "  python3 generate_log.py --seed 42\n"
               "  python3 generate_log.py --test",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        '--lines', '-l',
        type=int,
        default=DEFAULT_LINES,
        help=f'Número de linhas a gerar (padrão: {DEFAULT_LINES:,})'
    )
    
    parser.add_argument(
        '--output', '-o',
        type=str,
        default=DEFAULT_OUTPUT,
        help=f'Arquivo de saída (padrão: {DEFAULT_OUTPUT})'
    )
    
    parser.add_argument(
        '--seed', '-s',
        type=int,
        default=None,
        help='Semente para reprodutibilidade (padrão: aleatório)'
    )
    
    parser.add_argument(
        '--test', '-t',
        action='store_true',
        help='Gerar múltiplos arquivos de teste (1k, 10k, 100k, 1M, 10M)'
    )
    
    parser.add_argument(
        '--quiet', '-q',
        action='store_true',
        help='Executar em modo silencioso (menos saída)'
    )
    
    return parser.parse_args()

# ============================================================================
# EXECUÇÃO PRINCIPAL
# ============================================================================

if __name__ == "__main__":
    """
    Ponto de entrada principal do script.
    
    O script pode ser executado de várias formas:
    1. Modo padrão: python3 generate_log.py
    2. Com parâmetros: python3 generate_log.py --lines 5000000 --output meu_log.log
    3. Modo teste: python3 generate_log.py --test
    4. Com semente fixa: python3 generate_log.py --seed 12345
    """
    
    try:
        # Processa argumentos
        args = parse_arguments()
        
        if args.test:
            # Modo de teste - gera múltiplos arquivos
            generate_test_files()
        else:
            # Modo normal - gera um único arquivo
            generate_log_file(
                num_lines=args.lines,
                output_file=args.output,
                seed=args.seed,
                show_progress=not args.quiet
            )
        
        print("\n🎯 Script concluído com sucesso!")
        
    except KeyboardInterrupt:
        print("\n\n⚠️  Execução interrompida pelo usuário.")
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ Erro durante a execução: {e}")
        sys.exit(1)