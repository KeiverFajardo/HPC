#include "umbral_manager.hpp"
#include <boost/functional/hash.hpp>

static std::unordered_map<ClaveUmbral, float> umbrales;

void inicializar_umbral_global() {
    for (uint8_t m = 0; m < 10; ++m)
        for (uint8_t f = 0; f < 6; ++f)
            umbrales[{m, f}] = 25.0f;
}

float obtener_umbral(uint8_t municipio, uint8_t franja) {
    return umbrales[{municipio, franja}];
}

void actualizar_umbral(uint8_t municipio, uint8_t franja, float nuevo) {
    umbrales[{municipio, franja}] = nuevo;
}

std::vector<UmbralPorPar> obtener_umbral_para_envio() {
    std::vector<UmbralPorPar> v;
    for (const auto& [clave, valor] : umbrales)
        v.push_back({clave.first, clave.second, valor});
    return v;
}
