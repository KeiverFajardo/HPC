#include "umbral_manager.hpp"

Umbral UmbralManager::get(uint8_t municipio_id, uint8_t franja) const {
    auto it = umbrales_.find({municipio_id, franja});
    if (it != umbrales_.end()) {
        return it->second;
    }
    return {0.0f, 0.0f};  // Umbral por defecto si no hay datos
}

void UmbralManager::set(uint8_t municipio_id, uint8_t franja, float promedio, float desvio) {
    umbrales_[{municipio_id, franja}] = {promedio, desvio};
}

void UmbralManager::actualizar_suavizado(uint8_t municipio_id, uint8_t franja, float nuevo_promedio, float nuevo_desvio, float alpha) {
    Umbral anterior = get(municipio_id, franja);
    float promedio_suavizado = alpha * nuevo_promedio + (1 - alpha) * anterior.promedio;
    float desvio_suavizado = alpha * nuevo_desvio + (1 - alpha) * anterior.desvio;
    set(municipio_id, franja, promedio_suavizado, desvio_suavizado);
}
