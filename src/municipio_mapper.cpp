#include "municipio_mapper.hpp"
#include <utility>

std::mutex mtx;

MunicipioMapper::MunicipioMapper(const std::string& shapefile_path)
    : loader_(shapefile_path)
{}

uint8_t MunicipioMapper::codificar(Punto punto) const {
    auto it = m_cache.find(punto);
    uint8_t municipio_id;
    if (it == m_cache.end())
    {
        // OGRGeometry::Contains() no es thread safe
        mtx.lock();
        municipio_id = loader_.get_municipio(punto);
        mtx.unlock();
        m_cache.insert(std::make_pair(punto, municipio_id));
    }
    else
    {
        municipio_id = it->second;
    }

    return municipio_id;
}

std::string MunicipioMapper::decodificar(uint8_t id) const {
    return loader_.nombre_municipio(id);
}

size_t MunicipioMapper::cantidad() const {
    return loader_.cantidad();
    //return static_cast<size_t>(255); // Por convención, dejamos 255 como "ninguno"
}
