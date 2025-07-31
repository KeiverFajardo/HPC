#include "algoritmo_a.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mpi.h>
#include <ostream>
#include <print>
#include <ranges>
#include <set>
#include <string>
#include <utility>

#include "algoritmo_b.hpp"
#include "csv_reader.hpp"
#include "mpi_datatypes.hpp"
#include "common_constants.hpp"
#include "umbral.hpp"

#include "gnuplot-iostream.h"

#include <unordered_map>
#include <boost/functional/hash.hpp>

constexpr int BLOCK_SIZE = 1000000;
constexpr int NON_RESPONSIVE_THRESHOLD = 3000; // in milliseconds

constexpr const char *CHECKPOINT_FILE = "checkpoint.bin";
constexpr const char *NEW_CHECKPOINT_FILE = "checkpoint.bin.new";
constexpr const char *OLD_CHECKPOINT_FILE = "checkpoint.bin.old";

AlgoritmoA::AlgoritmoA(const std::string &shapefile_path, std::vector<const char*> csv_files)
    : m_files(csv_files), m_mapper(shapefile_path)
{
    for (uint8_t umbral_id = 0; umbral_id < MAX_UMBRAL_ID; umbral_id++)
    {
        m_umbrales[umbral_id] = 15.0f;
        m_datos_estadisticos_mes[umbral_id] = { 0.0f, 0, 0 };
    }
}

void AlgoritmoA::enviar_umbrales(int dst)
{
    MPI_Send(m_umbrales.data(), m_umbrales.size(), MPI_FLOAT, dst, UMBRALES_TAG, MPI_COMM_WORLD);

    // No podemos usar mas esto porque ahora tenemos que tolerar que algun nodo se caiga
    // MPI_Bcast(umbrales_buffer.data(), umbrales_buffer.size(), MPI_Umbral, MASTER_RANK, MPI_COMM_WORLD);
}

void AlgoritmoA::recalcular_umbrales()
{
    for (size_t umbral_id = 0; umbral_id < m_datos_estadisticos_mes.size(); umbral_id++)
    {
        auto &datos = m_datos_estadisticos_mes[umbral_id];
        if (datos.cantidad_registros != 0)
            m_umbrales[umbral_id] = (datos.suma_velocidades / datos.cantidad_registros) * 0.5;
        datos.suma_velocidades = 0.0f;
        datos.cantidad_registros = 0;
    }
}

const auto franjas_horarias_names = std::to_array({
    "Mañana",
    "Mediodia",
    "Tarde"
});

void graph_bars(
    std::array<Gnuplot, 3> &gps,
    std::array<std::array<std::pair<std::string, int>, 8>, 3> barras,
    MunicipioMapper &mapper,
    Date date
) {
    for (const auto &[franja_horaria, gp] : std::views::enumerate(gps))
    {
        for (int municipio = 0; municipio < 8; municipio++)
        {
            barras[franja_horaria][municipio].first
                = '"' + mapper.decodificar(municipio) + '"';
        }
        std::array<const char *, DIA_SEMANA_COUNT> nombres_dias = {
            "Domingo",
            "Lunes",
            "Martes",
            "Miercoles",
            "Jueves",
            "Viernes",
            "Sabado"
        };
        uint8_t dia_semana = day_of_week(date.day, date.month, date.year);
        
        gp << "set title 'Anomalias en " << nombres_dias[dia_semana] << " " << franjas_horarias_names[franja_horaria]
           << " - " << (int)date.day << "/" << (int)date.month << "/" << date.year << "'\n";
        gp << "plot '-' using 2:xtic(1) with boxes\n";
        // std::print("\t");
        // for (auto &[name, val] : barras.at(franja_horaria))
        // {
        //     std::print("{} ", val);
        // }
        // std::println("");
        gp.send1d(barras.at(franja_horaria));
    }
    // std::cin.get();
}

void AlgoritmoA::recover()
{
    // RECOVER
    std::array<
        std::array<std::pair<std::string, int>, 8>,
        3
    > barras;

    std::ifstream ifs(CHECKPOINT_FILE, std::ios::binary);
    // 1. Cargar umbrales
    ifs.read(reinterpret_cast<char*>(m_umbrales.data()), m_umbrales.size() * sizeof(m_umbrales[0]));
    
    // 2. Cargar anomalias totales
    for (size_t franja = 0; franja < m_anomalias_totales_por_franja_horaria.size(); franja++)
    {
        ifs.read(
            reinterpret_cast<char*>(m_anomalias_totales_por_franja_horaria[franja].data()),
            m_anomalias_totales_por_franja_horaria[franja].size() * sizeof(m_anomalias_totales_por_franja_horaria[franja][0])
        );
    }

    // 3. Cargar valores de grafica viejos
    size_t file_count = 0;
    ifs.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));
    m_history.resize(file_count);
    m_current_file_index = file_count;
    std::println("RECOVERING at {}", m_current_file_index);
    for (size_t file_i = 0; file_i < file_count; file_i++)
    {
        size_t day_graph_count = 0;
        ifs.read(reinterpret_cast<char*>(&day_graph_count), sizeof(day_graph_count));
        m_history[file_i].resize(day_graph_count);
        
        for (size_t i = 0; i < day_graph_count; i++)
        {
            Date date;
            ifs.read(reinterpret_cast<char*>(&date), sizeof(date));
            m_history[file_i][i].first = date;
            for (int franja = 0; franja < FRANJA_HORARIA_COUNT; franja++)
            {
                for (int municipio = 0; municipio < MUNICIPIO_COUNT; municipio++)
                {
                    int count = -1;
                    ifs.read(reinterpret_cast<char*>(&count), sizeof(count));
                    barras[franja][municipio].second = count;
                    m_history[file_i][i].second[franja][municipio].second = count;
                }
            }
            graph_bars(m_gps, barras, m_mapper, date);
        }
    }
}

void AlgoritmoA::save_checkpoint()
{
    std::ofstream ofs(NEW_CHECKPOINT_FILE, std::ios::binary);

    // 1. Guardar umbrales
    ofs.write(reinterpret_cast<const char*>(m_umbrales.data()), m_umbrales.size() * sizeof(m_umbrales[0]));

    // 2. Guardar anomalias totales
    for (size_t franja = 0; franja < m_anomalias_totales_por_franja_horaria.size(); franja++)
    {
        ofs.write(
            reinterpret_cast<const char*>(m_anomalias_totales_por_franja_horaria[franja].data()),
            m_anomalias_totales_por_franja_horaria[franja].size() * sizeof(m_anomalias_totales_por_franja_horaria[franja][0])
        );
    }

    size_t hist_size = m_history.size();
    ofs.write(reinterpret_cast<const char*>(&hist_size), sizeof(hist_size));
    for (size_t file_i = 0; file_i < m_history.size(); file_i++)
    {
        size_t day_graph_count = m_history[file_i].size();
        ofs.write(reinterpret_cast<const char*>(&day_graph_count), sizeof(day_graph_count));

        for (size_t i = 0; i < day_graph_count; i++)
        {
            Date &date = m_history[file_i][i].first;
            ofs.write(reinterpret_cast<const char*>(&date), sizeof(date));
            auto &barras = m_history[file_i][i].second;
            for (int franja = 0; franja < FRANJA_HORARIA_COUNT; franja++)
            {
                for (int municipio = 0; municipio < MUNICIPIO_COUNT; municipio++)
                {
                    ofs.write(
                        reinterpret_cast<const char*>(&barras[franja][municipio].second),
                        sizeof(barras[franja][municipio].second)
                    );
                }
            }
        }
    }

    ofs.close();

    if (std::filesystem::exists(CHECKPOINT_FILE))
        std::filesystem::rename(CHECKPOINT_FILE, OLD_CHECKPOINT_FILE);

    std::filesystem::rename(NEW_CHECKPOINT_FILE, CHECKPOINT_FILE);

    if (std::filesystem::exists(OLD_CHECKPOINT_FILE))
        std::filesystem::remove(OLD_CHECKPOINT_FILE);
}

void AlgoritmoA::procesar()
{
    for (const auto &[i, gp] : std::views::enumerate(m_gps))
    {
        gp << "set terminal gif size 1280,960 animate delay 16.66\n";
        gp << "set output 'image" << i << ".gif'\n";
        gp << "set style fill solid\n";
        gp << "set boxwidth 0.5\n";
        gp << "set yrange [0:2000]\n";
        gp << "set xlabel 'Municipios'\n";
        gp << "set ylabel 'Cantidad de anomalias'\n";
    }

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::set<int> free_slaves;
    for (int i = 1; i < world_size; i++) free_slaves.emplace(i);
    std::set<int> unresponsive_slaves;

    std::vector<ResultadoEstadistico> resultados;
    
    if (std::filesystem::exists(CHECKPOINT_FILE))
        recover();

    while(m_current_file_index < m_files.size())
    {
        m_history.emplace_back();

        int bloque = 0;
        auto &filename = m_files[m_current_file_index];

        std::ifstream ifs(filename);
        ifs.seekg(0, std::ios::end);
        const size_t filesize = ifs.tellg();
        ifs.close();
        size_t cursor = 0;
        //info("Comienzo archivo {} ({} bytes)", filename, filesize);

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
            std::array<DatosEstadisticos, MAX_UMBRAL_ID>
        > graph_jobs;

        std::unordered_map<
            uint8_t,
            std::array<DatosEstadisticos, MAX_UMBRAL_ID>
        > graph_jobs_en_construccion;
        uint8_t max_received_day = 0;

        // TODO: Podemos hacer como una especie de free list para checkear que se recibieron hasta
        // cierto bloque
        std::vector<bool> received_blocks(block_count, false);
        std::unordered_map<int, uint8_t> min_day_for_block;
        using time_point = std::chrono::time_point<std::chrono::system_clock>;
        std::unordered_map<int, std::tuple<int, time_point, int, int>> assigned_jobs;
        size_t old_first_non_received_block = 0;

        int int_file_index = m_current_file_index;
        for (int slave : free_slaves)
        {
            // Cambiar archivo
            MPI_Send(&int_file_index, 1, MPI_INT, slave, CHANGE_FILE_TAG, MPI_COMM_WORLD);

            enviar_umbrales(slave);
        }
        // ---- Envío de bloques a esclavos
        bool file_ended = false;
        while (!file_ended
            || !graph_jobs.empty()
            || !graph_jobs_en_construccion.empty()
            || free_slaves.size() + unresponsive_slaves.size() < static_cast<size_t>(world_size - 1))
        {
            // Si ya recibimos resultados de todos los bloques del archivo o todos
            // los nodos slave estan ocupados
            if (file_ended || free_slaves.empty())
            {
                if (file_ended && free_slaves.size() + unresponsive_slaves.size() == static_cast<size_t>(world_size - 1))
                {
                    // Pasar trabajos de graficado en construcccion a pendientes de graficar
                    while (!graph_jobs_en_construccion.empty())
                    {
                        auto job = graph_jobs_en_construccion.extract(graph_jobs_en_construccion.begin());
                        graph_jobs.insert(std::move(job));
                    }
                }
                else
                {
                    // Pasar trabajos de graficado en construccion para los que se recibieron
                    // todos los bloques de ese dia a trabajos pendientes
                    int first_non_received_block = 0;
                    for (size_t i = old_first_non_received_block; i < received_blocks.size(); i++)
                    {
                        if (!received_blocks[i])
                        {
                            first_non_received_block = i;
                            break;
                        }
                    }
                    old_first_non_received_block = first_non_received_block;

                    if (first_non_received_block > 0)
                    {
                        int day_limit = min_day_for_block.at(first_non_received_block - 1);

                        for (
                            auto it = graph_jobs_en_construccion.begin();
                            it != graph_jobs_en_construccion.end();
                        ) {
                            if (it->first < day_limit)
                            {
                                graph_jobs.insert({it->first, std::move(it->second)});
                                it = graph_jobs_en_construccion.erase(it);
                            }
                            else
                                it++;
                        }
                    }
                }

                // Graficar trabajos pendientes
                if (!graph_jobs.empty())
                {
                    auto job = graph_jobs.extract(graph_jobs.begin());
                    int day = job.key();
                    std::array<
                        std::array<std::pair<std::string, int>, 8>,
                        3
                    > barras;

                    for (uint8_t municipio = 0; municipio < MUNICIPIO_COUNT; municipio++)
                    {
                        for (uint8_t franja_horaria = 0; franja_horaria < FRANJA_HORARIA_COUNT; franja_horaria++)
                        {
                            barras.at(franja_horaria).at(municipio).second = 0;
                            for (uint8_t dia_semana = 0; dia_semana < DIA_SEMANA_COUNT; dia_semana++)
                            {
                                uint8_t umbral_id = get_umbral_id(municipio, franja_horaria, dia_semana);
                                // CAMBIAR ACA SI QUEREMOS CAMBIAR QUE SE GRAFICA
                                barras.at(franja_horaria).at(municipio).second
                                    += job.mapped()[umbral_id].cantidad_anomalias;
                                    // += job.mapped()[umbral_id].suma_velocidades
                                    //     / job.mapped()[umbral_id].cantidad_registros;
                            }
                            //barras.at(franja_horaria).at(municipio).second /= DIA_SEMANA_COUNT;
                        }
                    }

                    graph_bars(
                        m_gps,
                        barras,
                        m_mapper,
                        Date {
                            first_day_date.year,
                            first_day_date.month,
                            static_cast<uint8_t>(day),
                        }
                    );

                    m_history.back().emplace_back(
                        Date {
                            first_day_date.year,
                            first_day_date.month,
                            static_cast<uint8_t>(day),
                        },
                        barras
                    );
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
                    //std::print("\r\033[K");
                    std::println("BLOQUE {}/{} DE ARCHIVO {}/{} for {} ({})", bloque+1, block_count, m_current_file_index+1, m_files.size(), slave, filename);
                    MPI_Send(range, 2, MPI_UINT64_T, slave, BLOCKS_TAG, MPI_COMM_WORLD);
                    assigned_jobs.insert({slave, std::make_tuple(bloque, std::chrono::system_clock::now(), range[0], range[1])});
                    bloque++;
                }
                else
                {
                    file_ended = true;
                }
            }

            // Check if a slave is not responding
            if (!free_slaves.empty())
            {
                std::vector<int> ranks_to_remove;
                for (auto &[slave_rank, tuple] : assigned_jobs)
                {
                    auto &[block, time_stamp, range_start, range_end] = tuple;
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - time_stamp);

                    if (duration.count() > NON_RESPONSIVE_THRESHOLD)
                        ranks_to_remove.emplace_back(slave_rank);
                }

                for (int rank_to_remove : ranks_to_remove)
                {
                    if (free_slaves.empty()) break;
                    // RESEND
                    unresponsive_slaves.insert(rank_to_remove);

                    auto tuple = assigned_jobs.extract(rank_to_remove).mapped();
                    auto &[block, time_stamp, range_start, range_end] = tuple;
                    int slave = free_slaves.extract(free_slaves.begin()).value();
                    uint64_t range[2];
                    range[0] = range_start;
                    range[1] = range_end;
                    //std::print("\r\033[K");
                    std::println("RESEND BLOQUE {}/{} DE ARCHIVO {}/{} {}->{} ({})", block+1, block_count, m_current_file_index+1, m_files.size(), rank_to_remove, slave, filename);
                    MPI_Send(range, 2, MPI_UINT64_T, slave, BLOCKS_TAG, MPI_COMM_WORLD);
                    assigned_jobs.insert({slave, std::make_tuple(block, std::chrono::system_clock::now(), range[0], range[1])});
                }
            }

            int flag = 0;
            MPI_Status status;
            MPI_Iprobe(MPI_ANY_SOURCE, BLOCKS_TAG, MPI_COMM_WORLD, &flag, &status);
            while (flag) { 
                // ---- Recepción de resultados estadísticos
                int count;
                MPI_Get_count(&status, MPI_ResultadoEstadistico, &count);
                resultados.clear();
                resultados.resize(count);
                // info("RECV {} {}", count, status.MPI_SOURCE);
                MPI_Recv(resultados.data(), count, MPI_ResultadoEstadistico, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                // Only happens if the job was sent to another slave
                if (!assigned_jobs.contains(status.MPI_SOURCE))
                {
                    // TODO: Recover a node if it comes back

                    int int_file_index = m_current_file_index;
                    MPI_Send(&int_file_index, 1, MPI_INT, status.MPI_SOURCE, CHANGE_FILE_TAG, MPI_COMM_WORLD);
                    enviar_umbrales(status.MPI_SOURCE);
                    unresponsive_slaves.extract(status.MPI_SOURCE);
                    free_slaves.emplace(status.MPI_SOURCE);
                    MPI_Iprobe(MPI_ANY_SOURCE, BLOCKS_TAG, MPI_COMM_WORLD, &flag, &status);
                    continue;
                }

                // info("BEGIN RESPONSE");
                uint8_t min_day = std::numeric_limits<uint8_t>::max();
                for (const auto &r : resultados)
                {
                    DatosEstadisticos &datos = graph_jobs_en_construccion[r.dia][r.umbral_id];

                    uint8_t municipio, franja, dia_sem;
                    reverse_umbral_id(r.umbral_id, municipio, franja, dia_sem);

                    // info("\tFRANJA {} COUNT {} SUM {} ANOM", franja, r.cantidad_registros, r.suma_velocidades, r.cantidad_anomalias);

                    if (r.dia < min_day) min_day = r.dia;
                    if (r.dia > max_received_day) max_received_day = r.dia;

                    datos.suma_velocidades += r.suma_velocidades;
                    datos.cantidad_registros += r.cantidad_registros;
                    datos.cantidad_anomalias += r.cantidad_anomalias;

                    DatosEstadisticos &datos_mes = m_datos_estadisticos_mes[r.umbral_id];
                    datos_mes.suma_velocidades += r.suma_velocidades;
                    datos_mes.cantidad_registros += r.cantidad_registros;
                    datos_mes.cantidad_anomalias += r.cantidad_anomalias;

                    m_anomalias_totales_por_franja_horaria.at(franja).at(municipio) += r.cantidad_anomalias;
                }

                free_slaves.emplace(status.MPI_SOURCE);
                auto tuple = assigned_jobs.extract(status.MPI_SOURCE).mapped();
                int block_index = std::get<0>(tuple);
                received_blocks.at(block_index) = true;
                min_day_for_block.insert({block_index, min_day});

                //Mostrar resultados
                // for (const auto& r : resultados)
                // {
                //     std::string nombre_municipio = m_mapper.decodificar(r.municipio_id);
                //     info("Municipio {}, Franja {} => Suma: {:.2f}, N: {}, Rank origen: {}",
                //         nombre_municipio, (int)r.franja_horaria, r.suma_velocidades, r.cantidad, status.MPI_SOURCE);
                // }

                MPI_Iprobe(MPI_ANY_SOURCE, BLOCKS_TAG, MPI_COMM_WORLD, &flag, &status);
            }
        }

        recalcular_umbrales();

        save_checkpoint();

        m_current_file_index++;
    }
    
    std::filesystem::remove(CHECKPOINT_FILE);

    for (int i = 1; i < world_size; ++i)
    {
        MPI_Send(nullptr, 0, MPI_Register, i, EXIT_MESSAGE_TAG, MPI_COMM_WORLD);
    }

    std::print("Franja Horaria");
    for (size_t municipio = 0; municipio < MUNICIPIO_COUNT; municipio++)
    {
        std::print(",{}", m_mapper.decodificar(municipio));
    }
    std::println();
    for (size_t franja = 0; franja < m_anomalias_totales_por_franja_horaria.size(); franja++)
    {
        std::print("{}", franja);
        for (size_t municipio = 0; municipio < m_anomalias_totales_por_franja_horaria.at(franja).size(); municipio++)
        {
            std::print(",{}", m_anomalias_totales_por_franja_horaria.at(franja).at(municipio));
        }
        std::println();
    }
}
