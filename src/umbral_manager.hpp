#pragma once

#include <map>
#include <utility>
#include <cstdint>

struct Umbral {
    float promedio;
    float desvio;
};

class UmbralManager {
public:
    // Devuelve el umbral actual para (municipio, franja)
    Umbral get(uint8_t municipio_id, uint8_t franja) const;

    // Establece/actualiza el umbral para (municipio, franja)
    void set(uint8_t municipio_id, uint8_t franja, float promedio, float desvio);

    // Actualiza el umbral suavizado usando nuevos datos (promedio y desvío)
    void actualizar_suavizado(uint8_t municipio_id, uint8_t franja, float nuevo_promedio, float nuevo_desvio, float alpha = 0.2f);

private:
    // clave: (municipio_id, franja), valor: umbral histórico
    std::map<std::pair<uint8_t, uint8_t>, Umbral> umbrales_;
};
