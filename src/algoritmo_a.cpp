#include "algoritmo_a.hpp"

#include <mpi.h>
#include <ranges>
#include <set>
#include <utility>

#include "algoritmo_b.hpp"
#include "csv_reader.hpp"
#include "log.hpp"
#include "mpi_datatypes.hpp"
#include "common_constants.hpp"
#include "umbral.hpp"

#include "gnuplot-iostream.h"

#include <unordered_map>
#include <boost/functional/hash.hpp>

constexpr int BLOCK_SIZE = 1000000;

AlgoritmoA::AlgoritmoA(const std::string &shapefile_path)
    : m_mapper(shapefile_path)
{
    info("MUNICIPIOS {}", m_mapper.cantidad());
    for (uint8_t municipio = 0; municipio < 8; ++municipio) {
        for (uint8_t franja_horaria = 0; franja_horaria < 3; ++franja_horaria) {
             m_umbrales[{municipio, franja_horaria}] = 15.0f;
             m_datos_estadisticos_mes[{municipio, franja_horaria}] = { 0.0f, 0, 0 };
        }
    }
}

void AlgoritmoA::enviar_umbrales()
{
    std::vector<UmbralPorPar> umbrales_buffer;
    for (auto &[clave, umbral] : m_umbrales)
        umbrales_buffer.emplace_back(UmbralPorPar {
            clave.first,
            clave.second,
            umbral,
        });

    info("UMBRASLES_BUFFER {}", umbrales_buffer.size());
    MPI_Bcast(umbrales_buffer.data(), umbrales_buffer.size(), MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);
}

void AlgoritmoA::recalcular_umbrales()
{
    for (auto &[clave, datos] : m_datos_estadisticos_mes)
    {
        if (datos.cantidad_registros != 0)
            m_umbrales[clave] = (datos.suma_velocidades / datos.cantidad_registros) * 0.75;
        datos.suma_velocidades = 0.0f;
        datos.cantidad_registros = 0;
    }
}

const auto franjas_horarias_names = std::to_array({
    "Mañana",
    "Mediodia",
    "Tarde"
});

void AlgoritmoA::procesar(std::vector<const char*> files)
{
    std::array<Gnuplot, 3> gps;
    // {
    //     Gnuplot(">script0.gs"),
    //     Gnuplot(">script1.gs"),
    //     Gnuplot(">script2.gs"),
    // };
    for (const auto &[i, gp] : std::views::enumerate(gps))
    {
        gp << "set terminal gif size 1280,960 animate delay 66.66\n";
        gp << "set output 'image" << i << ".gif'\n";
        gp << "set style fill solid\n";
        gp << "set boxwidth 0.5\n";
        // gp << "set yrange [0:500]\n";
        gp << "set xlabel 'Municipios'\n";
        gp << "set ylabel 'Cantidad de anomalias'\n";
    }

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::set<int> free_slaves;
    for (int i = 1; i < world_size; i++)
        free_slaves.emplace(i);

    std::vector<ResultadoEstadistico> resultados;

    for (size_t file_index = 0; file_index < files.size(); file_index++)
    {
        int bloque = 0;
        auto &filename = files[file_index];

        std::ifstream ifs(filename);
        ifs.seekg(0, std::ios::end);
        const size_t filesize = ifs.tellg();
        ifs.close();
        size_t cursor = 0;
        info("Comienzo archivo {} ({} bytes)", filename, filesize);

        size_t block_count = (filesize + BLOCK_SIZE - 1 ) / BLOCK_SIZE;

        Date first_day_date;
        {
            CsvReader csv_reader(filename, 0, filesize);
            Register r;
            if (csv_reader.get(r))
                first_day_date = r.fecha;
            else
                exit(1);
        }

        std::unordered_map<
            uint8_t,
            std::unordered_map<Clave, DatosEstadisticos, boost::hash<Clave>>
        > graph_jobs;

        std::unordered_map<
            uint8_t,
            std::unordered_map<Clave, DatosEstadisticos, boost::hash<Clave>>
        > recolecting_graph_jobs;
        uint8_t max_received_day = 0;

        // TODO: Podemos hacer como una especie de free list para checkear que se recibieron hasta
        // cierto bloque
        std::vector<bool> received_blocks(block_count, false);
        std::unordered_map<int, uint8_t> min_day_for_block;
        std::unordered_map<int, int> assigned_jobs;

        enviar_umbrales();
        // ---- Envío de bloques a esclavos
        bool file_ended = false;
        while (!file_ended
            || !graph_jobs.empty()
            || !recolecting_graph_jobs.empty()
            || free_slaves.size() < static_cast<size_t>(world_size - 1))
        {
            if (file_ended || free_slaves.empty())
            {
                if (file_ended && free_slaves.size() == static_cast<size_t>(world_size - 1))
                {
                    while (!recolecting_graph_jobs.empty())
                    {
                        auto job = recolecting_graph_jobs.extract(recolecting_graph_jobs.begin());
                        graph_jobs.insert(std::move(job));
                    }
                }
                else
                {
                    int first_non_received_block = 0;
                    for (size_t i = 0; i < received_blocks.size(); i++)
                    {
                        if (!received_blocks[i])
                        {
                            first_non_received_block = i;
                            break;
                        }
                    }

                    if (first_non_received_block > 0)
                    {
                        int day_limit = min_day_for_block.at(first_non_received_block - 1);

                        for (
                            auto it = recolecting_graph_jobs.begin();
                            it != recolecting_graph_jobs.end();
                        ) {
                            if (it->first < day_limit)
                            {
                                graph_jobs.insert({it->first, std::move(it->second)});
                                it = recolecting_graph_jobs.erase(it);
                            }
                            else
                                it++;
                        }
                    }
                }

                if (!graph_jobs.empty())
                {
                    auto job = graph_jobs.extract(graph_jobs.begin());
                    int day = job.key();
                    std::array<
                        std::array<std::pair<std::string, int>, 8>,
                        3
                    > barras;

                    for (auto &[clave, datos] : job.mapped())
                    {
                        barras.at(clave.second).at(clave.first).second = datos.cantidad_anomalias;
                    }

                    for (const auto &[franja_horaria, gp] : std::views::enumerate(gps))
                    {
                        for (int municipio = 0; municipio < 8; municipio++)
                        {
                            barras[franja_horaria][municipio].first
                                = '"' + m_mapper.decodificar(municipio) + "\\n"
                                  "(" + std::to_string(m_umbrales[{municipio, franja_horaria}]) + ")\"";
                        }
                        gp << "set title 'Anomalias en " << franjas_horarias_names[franja_horaria]
                           << " - " << day << "/" << (int)first_day_date.month << "/" << first_day_date.year << "'\n";
                        gp << "plot '-' using 2:xtic(1) with boxes\n";
                        gp.send1d(barras.at(franja_horaria));
                    }
                }
            }
            else if (!file_ended)
            {
                if (cursor < filesize)
                {
                    int slave = free_slaves.extract(free_slaves.begin()).value();
                    uint64_t range[2];
                    range[0] = cursor;
                    cursor += BLOCK_SIZE;
                    range[1] = cursor;
                    info("BLOQUE {}/{} ({}:{}) for {}", bloque, block_count, range[0], range[1], slave);
                    MPI_Send(range, 2, MPI_UINT64_T, slave, TAG_DATA, MPI_COMM_WORLD);
                    assigned_jobs.insert({slave, bloque});
                    bloque++;
                }
                else
                {
                    file_ended = true;
                }
            }

            int flag = 0;
            MPI_Status status;
            MPI_Iprobe(MPI_ANY_SOURCE, TAG_DATA, MPI_COMM_WORLD, &flag, &status);
            while (flag) { 
                // ---- Recepción de resultados estadísticos
                int count;
                MPI_Get_count(&status, MPI_ResultadoEstadistico, &count);
                resultados.clear();
                resultados.resize(count);
                MPI_Recv(resultados.data(), count, MPI_ResultadoEstadistico, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

                uint8_t min_day = std::numeric_limits<uint8_t>::max();
                for (const auto &r : resultados)
                {
                    Clave clave = {r.municipio_id, r.franja_horaria};
                    DatosEstadisticos &datos = recolecting_graph_jobs[r.dia][clave];

                    if (r.dia < min_day) min_day = r.dia;
                    if (r.dia > max_received_day) max_received_day = r.dia;

                    datos.suma_velocidades += r.suma_velocidades;
                    datos.cantidad_registros += r.cantidad_registros;
                    datos.cantidad_anomalias += r.cantidad_anomalias;

                    DatosEstadisticos &datos_mes = m_datos_estadisticos_mes[clave];
                    datos_mes.suma_velocidades += r.suma_velocidades;
                    datos_mes.cantidad_registros += r.cantidad_registros;
                    datos_mes.cantidad_anomalias += r.cantidad_anomalias;
                }

                free_slaves.emplace(status.MPI_SOURCE);
                int block_index = assigned_jobs.extract(status.MPI_SOURCE).mapped();
                received_blocks.at(block_index) = true;
                min_day_for_block.insert({block_index, min_day});

                //Mostrar resultados
                // for (const auto& r : resultados)
                // {
                //     std::string nombre_municipio = m_mapper.decodificar(r.municipio_id);
                //     info("Municipio {}, Franja {} => Suma: {:.2f}, N: {}, Rank origen: {}",
                //         nombre_municipio, (int)r.franja_horaria, r.suma_velocidades, r.cantidad, status.MPI_SOURCE);
                // }

                MPI_Iprobe(MPI_ANY_SOURCE, TAG_DATA, MPI_COMM_WORLD, &flag, &status);
            }
        }

        if (static_cast<size_t>(file_index) != files.size() - 1)
        {
            for (int i = 1; i < world_size; ++i)
            {
                MPI_Send(nullptr, 0, MPI_Register, i, END_OF_FILE_TAG, MPI_COMM_WORLD);
            }
        }

        recalcular_umbrales();
    }

    for (int i = 1; i < world_size; ++i)
    {
        MPI_Send(nullptr, 0, MPI_Register, i, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);
    }
}
