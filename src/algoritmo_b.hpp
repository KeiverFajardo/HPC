#pragma once

#include <cstdint>

struct ResultadoEstadistico {
    uint8_t municipio_id;
    uint8_t franja_horaria;
    float promedio;
    float desvio;
    std::size_t cantidad;
    bool es_anomalia;
};

void procesar_b();
