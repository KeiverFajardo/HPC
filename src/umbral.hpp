#pragma once

#include <cstdint>
#include <unordered_map>
#include <utility>

// Para hash de pair como clave en unordered_map
#include <boost/functional/hash.hpp>

struct UmbralPorPar {
    uint8_t municipio_id;
    uint8_t franja_horaria;
    float umbral;
};

using ClaveUmbral = std::pair<uint8_t, uint8_t>;

// Función hash para usar pair en unordered_map
namespace std {
    template<>
    struct hash<ClaveUmbral> {
        size_t operator()(const ClaveUmbral& k) const {
            return boost::hash_value(k);
        }
    };
}

// API del gestor de umbrales
void inicializar_umbral_global();
float obtener_umbral(uint8_t municipio, uint8_t franja);
void actualizar_umbral(uint8_t municipio, uint8_t franja, float nuevo);
std::vector<UmbralPorPar> obtener_umbral_para_envio();
