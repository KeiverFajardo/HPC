#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct ResultadoEstadistico {
    uint8_t umbral_id;
    uint8_t dia;
    float suma_velocidades;
    size_t cantidad_registros;
    size_t cantidad_anomalias;
};

void procesar_b(const std::string &shapefile_path, std::vector<const char*> files);
