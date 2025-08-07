#include "municipio_mapper.hpp"

std::mutex mtx;

MunicipioMapper::MunicipioMapper(const std::string& shapefile_path)
    : loader_(shapefile_path)
{}

uint8_t MunicipioMapper::codificar(Punto punto) const {
    uint8_t municipio_id = loader_.get_municipio(punto);

    return municipio_id;
}

std::string MunicipioMapper::decodificar(uint8_t id) const {
    return loader_.nombre_municipio(id);
}

size_t MunicipioMapper::cantidad() const {
    return loader_.cantidad();
    //return static_cast<size_t>(255); // Por convención, dejamos 255 como "ninguno"
}
