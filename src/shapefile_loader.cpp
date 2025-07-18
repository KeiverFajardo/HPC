#include "shapefile_loader.hpp"

#include <ogrsf_frmts.h>
#include <stdexcept>
#include <iostream>

ShapefileLoader::ShapefileLoader(const std::string& shapefile_path) {
    GDALAllRegister();

    std::unique_ptr<GDALDataset> dataset(
        static_cast<GDALDataset*>(GDALOpenEx(shapefile_path.c_str(), GDAL_OF_VECTOR, nullptr, nullptr, nullptr))
    );

    if (!dataset) {
        throw std::runtime_error("No se pudo abrir el shapefile: " + shapefile_path);
    }

    OGRLayer* layer = dataset->GetLayer(0);
    if (!layer) {
        throw std::runtime_error("No se pudo acceder a la capa del shapefile.");
    }

    OGRFeature* feature;
    layer->ResetReading();

    uint8_t next_id = 0;
    while ((feature = layer->GetNextFeature()) != nullptr) {
        OGRGeometry* geometry = feature->GetGeometryRef();
        if (!geometry) {
            OGRFeature::DestroyFeature(feature);
            continue;
        }

        std::string nombre = feature->GetFieldAsString("MUNICIPIO");
        if (nombre.empty()) {
            OGRFeature::DestroyFeature(feature);
            continue;
        }

        auto geom_clone = geometry->clone();
        municipios_.push_back({nombre, geom_clone});
        nombre_to_id_[nombre] = next_id;
        ++next_id;

        OGRFeature::DestroyFeature(feature);
    }
}

uint8_t ShapefileLoader::get_municipio(const Punto& punto) const {
    OGRPoint ogr_point(punto.lon, punto.lat);  // lon primero, luego lat

    for (uint8_t i = 0; i < municipios_.size(); ++i) {
        const Municipio& m = municipios_[i];
        if (static_cast<OGRGeometry*>(m.geometria)->Contains(&ogr_point)) {
            return i;
        }
    }

    return 255;  // ID especial para "no encontrado"
}

std::string ShapefileLoader::nombre_municipio(uint8_t id) const {
    if (id >= municipios_.size()) return "Desconocido";
    return municipios_[id].nombre;
}
