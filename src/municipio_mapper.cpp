#include "municipio_mapper.hpp"

MunicipioMapper::MunicipioMapper(const std::string& shapefile_path)
    : loader_(shapefile_path)
{}

uint8_t MunicipioMapper::codificar(const Punto& punto) const {
    return loader_.get_municipio(punto);
}

std::string MunicipioMapper::decodificar(uint8_t id) const {
    return loader_.nombre_municipio(id);
}

size_t MunicipioMapper::cantidad() const {
    return static_cast<size_t>(255); // Por convención, dejamos 255 como "ninguno"
}
