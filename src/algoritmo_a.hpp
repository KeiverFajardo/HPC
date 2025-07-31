#pragma once

#include "gnuplot-iostream.h"
#include "municipio_mapper.hpp"
#include "umbral.hpp"
#include <boost/container_hash/hash.hpp>
#include <vector>
#include <cstddef>

// Clase que encapsula la lógica del Proceso A (maestro)
class AlgoritmoA {
public:
    AlgoritmoA(const std::string &shapefile_path, std::vector<const char*> csv_files);

    void procesar();
    
    using Clave = std::pair<uint8_t, uint8_t>;

private:
    // Cargar un bloque de tamaño `block_size` con registros extendidos
    void enviar_umbrales(int dst);
    void recalcular_umbrales();

    void recover();
    void save_checkpoint();

    std::vector<const char*> m_files;

    size_t m_current_file_index = 0;
    std::array<float, MAX_UMBRAL_ID> m_umbrales;

    struct DatosEstadisticos {
        float suma_velocidades = 0.0f;
        size_t cantidad_registros = 0;
        size_t cantidad_anomalias = 0;
    };

    std::array<Gnuplot, 3> m_gps;
    // {
    //     Gnuplot(">script0.gs"),
    //     Gnuplot(">script1.gs"),
    //     Gnuplot(">script2.gs"),
    // };
    std::array<DatosEstadisticos, MAX_UMBRAL_ID> m_datos_estadisticos_mes;
    MunicipioMapper m_mapper;
    std::array<std::array<int, 8>, 3> m_anomalias_totales_por_franja_horaria={};

    std::vector< // files
        std::vector< // days
            std::pair<
                Date,
                std::array<std::array<std::pair<std::string, int>, 8>, 3>
            >
        >
    > m_history;
};
