#pragma once

#include "csv_reader.hpp"
#include <vector>
#include <cstdint>

struct ResultadoEstadistico {
    uint8_t municipio_id;
    uint8_t franja_horaria;
    float promedio;
    float desvio;
    std::size_t cantidad;
};

// Esta función procesa un bloque de registros extendidos
// y devuelve los resultados estadísticos agrupados por (municipio_id, franja_horaria)
std::vector<ResultadoEstadistico> analizar_bloque(const std::vector<Register> &bloque);
