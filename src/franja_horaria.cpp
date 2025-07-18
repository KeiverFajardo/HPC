#include "franja_horaria.hpp"

FranjaHoraria get_franja_horaria(const Hour& h) {
    if (h.hour < 8)
        return FranjaHoraria::Madrugada;
    else if (h.hour < 16)
        return FranjaHoraria::Diurna;
    else
        return FranjaHoraria::TardeNoche;
}

std::string to_string(FranjaHoraria franja) {
    switch (franja) {
        case FranjaHoraria::Madrugada:  return "Madrugada";
        case FranjaHoraria::Diurna:     return "Diurna";
        case FranjaHoraria::TardeNoche: return "Tarde-Noche";
        default:                        return "Desconocida";
    }
}
