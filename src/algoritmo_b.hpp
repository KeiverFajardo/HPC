#pragma once

#include <cstdint>

struct ResultadoEstadistico {
    uint8_t municipio_id;
    uint8_t franja_horaria;
    float suma_velocidades;
    std::size_t cantidad;
};

void procesar_b();
