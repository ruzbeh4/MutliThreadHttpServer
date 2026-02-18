# Multithreaded HTTP Server

A simple POSIX-threaded web server that serves static files, uses a fixed thread pool with a shared work queue, includes a thread-safe LRU cache, and records lightweight request statistics.

## Build

```bash
make
```

## Run

```bash
./webserver -p 8080 -t 8 -r www -c 5242880
```

Disable the LRU cache:

```bash
./webserver --noLRU
```

- `-p` port (default `8080`)
- `-t` worker threads (default `8`)
- `-r` document root (default `www`)
- `-c` cache size in bytes (default `5MB`)
- `--noLRU` disable in-memory cache (default is enabled)

Stop with `Ctrl+C` for graceful shutdown.

## Architecture

- `src/server.cpp` — creates socket, binds/listens, accepts clients, enqueues to thread pool.
- `src/thread_pool.cpp` — pthread-based worker pool with mutex/condvar protected queue.
- `src/http_handler.cpp` — parses GET requests, checks path safety, uses cache, serves files, returns 200/400/404 with proper `Content-Type`.
- `src/cache.cpp` — thread-safe LRU cache sized by bytes.
- `src/stats.cpp` — tracks total requests, avg latency, cache hit rate, simple RPS.

## Load Testing Note

Use `scripts/run_bench.sh` to benchmark with ApacheBench:

```bash
bash scripts/run_bench.sh [N] [C] [URL] [--auto|--noLRU|--cache-only]
```

- `N` total requests (default `1000`), `C` concurrency (default `50`), `URL` defaults to `http://127.0.0.1:$PORT/`.
- Flags: `--auto` (default, run with cache on then off), `--noLRU`/`--no-cache` (cache off only), `--cache-only` (cache on only).
- what is does: runs for `THREADS="1 2 4 8 16"`, `PORT=8080`, `ROOT=www`, `BINARY=./webserver`, `OUT_DIR=bench_out` (logs/results in `bench_out/logs`).
- Outputs: `results.tsv` + logs in `bench_out/logs`; plots/JSON in `bench_out/`.