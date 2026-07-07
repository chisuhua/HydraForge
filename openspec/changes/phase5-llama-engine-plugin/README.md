# phase5-llama-engine-plugin

Day 2: First PDK engine plugin — pdk/llama_engine/. Implements B2.1/B2.2 (engine + model). Establishes PDK engine plugin pattern following ADR-0034 model_router precedent. Tool names use `inference/engine/*` and `inference/model/*` namespaces (per Adversarial Review D3 decision). SamplerStrategy deferred (per D1).
