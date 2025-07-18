#include "algoritmo_a.hpp"
#include <utility>

AlgoritmoA::AlgoritmoA(const std::string &csv_path, const std::string &shapefile_path)
    : lector_csv(csv_path), loader(shapefile_path)
{
    loader.inicializar();
}

bool AlgoritmoA::cargar_bloque(std::vector<RegisterExt> &bloque, std::size_t block_size)
{
    bloque.clear();
    Register reg;

    while (bloque.size() < block_size && lector_csv.get(reg))
    {
        Register r_ext;
        r_ext.cod_detector = reg.cod_detector;
        r_ext.id_carril = reg.id_carril;
        r_ext.fecha = reg.fecha;
        r_ext.hora = reg.hora;
        r_ext.latitud = reg.latitud;
        r_ext.longitud = reg.longitud;
        r_ext.velocidad = reg.velocidad;

        // Asignar municipio
        r_ext.municipio_id = loader.get_municipio(Punto { r_ext.latitud, r_ext.longitud });

        // Asignar franja horaria
        r_ext.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));

        bloque.push_back(r_ext);
        registros_procesados++;
    }

    return !bloque.empty();
}
