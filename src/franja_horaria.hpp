#pragma once

#include "csv_reader.hpp"  // Necesita struct Hour

#include <string>
#include <cstdint>

enum class FranjaHoraria : uint8_t {
    Madrugada = 0,   // 00:00 - 07:59
    Diurna     = 1,   // 08:00 - 15:59
    TardeNoche = 2    // 16:00 - 23:59
};

// Retorna el ID numérico (0, 1, 2) correspondiente a la franja
FranjaHoraria get_franja_horaria(const Hour& h);

// Retorna el nombre textual de la franja
std::string to_string(FranjaHoraria franja);
