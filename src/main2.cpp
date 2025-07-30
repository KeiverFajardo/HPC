#include "csv_reader.hpp"
#include "franja_horaria.hpp"
#include "gnuplot-iostream.h"
#include "municipio_mapper.hpp"
#include "umbral.hpp"

#include <filesystem>
#include <ranges>
#include <print>

constexpr int BLOCK_SIZE = 1000000;

struct ResultadoEstadistico {
    uint8_t umbral_id;
    uint8_t dia;
    float suma_velocidades;
    size_t cantidad_registros;
    size_t cantidad_anomalias;
};

std::vector<ResultadoEstadistico> analizar_bloque(
    CsvReader &csv_reader,
    MunicipioMapper &mapper,
    const std::array<float, MAX_UMBRAL_ID> &umbrales
) {
    // Calcular estadística por grupo
    std::vector<ResultadoEstadistico> resultados;

    struct Accumulador {
        float suma_velocidades = 0.0f;
        size_t cantidad_registros = 0;
        size_t cantidad_anomalias = 0;
    };

    std::unordered_map<uint8_t, std::array<Accumulador, MAX_UMBRAL_ID>> accumuladores;

    Register reg;

    while (csv_reader.get(reg))
    {
        if (reg.velocidad < 0.1f) continue;

        // Asignar municipio
        reg.municipio_id = mapper.codificar(Punto { reg.latitud, reg.longitud });
        // Asignar franja horaria
        reg.franja_horaria = std::to_underlying(get_franja_horaria(reg.hora));

        uint8_t dia_semana = day_of_week(reg.fecha.day, reg.fecha.month, reg.fecha.year);
        uint8_t umbral_id = get_umbral_id(reg.municipio_id, reg.franja_horaria, dia_semana);
        auto &[suma, cantidad_registros, cantidad_anomalias]
            = accumuladores[reg.fecha.day][umbral_id];
        suma += reg.velocidad;
        cantidad_registros++;
        if (reg.velocidad < umbrales.at(umbral_id))
            cantidad_anomalias++;
    }

    for (const auto &[day, aux2] : accumuladores)
    {
        for (int umbral_id = 0; umbral_id < MAX_UMBRAL_ID; umbral_id++)
        {
            auto &[suma, cantidad_registros, cantidad_anomalias] = accumuladores[day][umbral_id];
            if (cantidad_registros <= 0) continue;
            ResultadoEstadistico res;
            res.umbral_id = umbral_id;
            res.dia = day;
            res.suma_velocidades = suma;
            res.cantidad_registros = cantidad_registros;
            res.cantidad_anomalias = cantidad_anomalias;
            resultados.push_back(res);
        }
    }

    return resultados;
}

class AlgoritmoA {
public:
    AlgoritmoA(const std::string &shapefile_path);

    void procesar(std::vector<const char*> files);
    
    using Clave = std::pair<uint8_t, uint8_t>;

private:
    // Cargar un bloque de tamaño `block_size` con registros extendidos
    void recalcular_umbrales();

    std::array<float, MAX_UMBRAL_ID> m_umbrales;

    struct DatosEstadisticos {
        float suma_velocidades = 0.0f;
        size_t cantidad_registros = 0;
        size_t cantidad_anomalias = 0;
    };

    std::array<DatosEstadisticos, MAX_UMBRAL_ID> m_datos_estadisticos_mes;
    MunicipioMapper m_mapper;
};

AlgoritmoA::AlgoritmoA(const std::string &shapefile_path)
    : m_mapper(shapefile_path)
{
    for (uint8_t umbral_id = 0; umbral_id < MAX_UMBRAL_ID; umbral_id++)
    {
        m_umbrales[umbral_id] = 15.0f;
        m_datos_estadisticos_mes[umbral_id] = { 0.0f, 0, 0 };
    }
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
        gp << "set terminal gif size 1280,960 animate delay 16.66\n";
        gp << "set output 'image" << i << ".gif'\n";
        gp << "set style fill solid\n";
        gp << "set boxwidth 0.5\n";
        gp << "set yrange [0:2000]\n";
        gp << "set xlabel 'Municipios'\n";
        gp << "set ylabel 'Cantidad de anomalias'\n";
    }

    std::vector<ResultadoEstadistico> resultados;

    size_t start_from_file_idx = 0;
    constexpr const char *checkpoint_file = "checkpoint.csv";
    if (std::filesystem::exists(checkpoint_file))
    {
        std::println("RECOVERING");
        // RECOVER
        std::array<
            std::array<std::pair<std::string, int>, 8>,
            3
        > barras;

        std::ifstream ifs(checkpoint_file, std::ios::binary);
        ifs.read(reinterpret_cast<char*>(m_umbrales.data()), m_umbrales.size() * sizeof(m_umbrales[0]));

        size_t file_count = 0;
        ifs.read(reinterpret_cast<char*>(&file_count), sizeof(file_count));
        start_from_file_idx = file_count + 1;
        for (size_t file_i = 0; file_i < file_count; file_i++)
        {
            size_t day_graph_count = 0;
            ifs.read(reinterpret_cast<char*>(&day_graph_count), sizeof(day_graph_count));
            
            for (size_t i = 0; i < day_graph_count; i++)
            {
                Date date;
                ifs.read(reinterpret_cast<char*>(&date), sizeof(date));
                for (int franja = 0; franja < FRANJA_HORARIA_COUNT; franja++)
                {
                    for (int municipio = 0; municipio < MUNICIPIO_COUNT; municipio++)
                    {
                        ifs.read(
                            reinterpret_cast<char*>(&barras[franja][municipio].second),
                            sizeof(barras[franja][municipio].second)
                        );
                    }
                }
                graph_bars(gps, barras, m_mapper, date);
            }
        }
    }

    std::vector< // files
        std::vector< // days
            std::pair<
                Date,
                std::array<std::array<std::pair<std::string, int>, 8>, 3>
            >
        >
    > history;

    std::array<std::array<int, 8>, 3> anomalias_totales_por_franja_horaria{};

    for (size_t file_index = start_from_file_idx; file_index < files.size(); file_index++)
    {
        history.emplace_back();

        int bloque = 0;
        auto &filename = files[file_index];

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
        > recolecting_graph_jobs;
        uint8_t max_received_day = 0;

        // TODO: Podemos hacer como una especie de free list para checkear que se recibieron hasta
        // cierto bloque
        std::unordered_map<int, uint8_t> min_day_for_block;
        // ---- Envío de bloques a esclavos
        bool file_ended = false;
        while (!file_ended
            || !graph_jobs.empty()
            || !recolecting_graph_jobs.empty())
        {
            if (file_ended)
            {
                while (!recolecting_graph_jobs.empty())
                {
                    auto job = recolecting_graph_jobs.extract(recolecting_graph_jobs.begin());
                    graph_jobs.insert(std::move(job));
                }

                while (!graph_jobs.empty())
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
                        gps,
                        barras,
                        m_mapper,
                        Date {
                            first_day_date.year,
                            first_day_date.month,
                            static_cast<uint8_t>(day),
                        }
                    );

                    history.back().emplace_back(
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
                    uint64_t range[2];
                    range[0] = cursor;
                    cursor += BLOCK_SIZE;
                    range[1] = cursor;
                    //std::print("\r\033[K");
                    std::println("BLOQUE {}/{} DE ARCHIVO {}/{} ({})", bloque+1, block_count, file_index+1, files.size(), filename);

                    CsvReader csv_reader(filename, range[0], range[1]);
                    std::vector<ResultadoEstadistico> resultados
                        = analizar_bloque(csv_reader, m_mapper, m_umbrales);

                    uint8_t min_day = std::numeric_limits<uint8_t>::max();
                    for (const auto &r : resultados)
                    {
                        DatosEstadisticos &datos = recolecting_graph_jobs[r.dia][r.umbral_id];

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

                        anomalias_totales_por_franja_horaria.at(franja).at(municipio) += r.cantidad_anomalias;
                    }

                    bloque++;
                }
                else
                {
                    file_ended = true;
                }
            }
        }

        recalcular_umbrales();

        // checkpoint
        {
            std::ofstream ofs("checkpoint.new", std::ios::binary);

            ofs.write(reinterpret_cast<const char*>(m_umbrales.data()), m_umbrales.size() * sizeof(m_umbrales[0]));

            ofs.write(reinterpret_cast<const char*>(&file_index), sizeof(file_index));
            for (size_t file_i = 0; file_i < history.size(); file_i++)
            {
                size_t day_graph_count = history[file_i].size();
                ofs.write(reinterpret_cast<const char*>(&day_graph_count), sizeof(day_graph_count));

                for (size_t i = 0; i < day_graph_count; i++)
                {
                    Date &date = history[file_i][i].first;
                    ofs.write(reinterpret_cast<const char*>(&date), sizeof(date));
                    auto &barras = history[file_i][i].second;
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

            if (std::filesystem::exists(checkpoint_file))
                std::filesystem::rename(checkpoint_file, "checkpoint.old");

            std::filesystem::rename("checkpoint.new", checkpoint_file);

            if (std::filesystem::exists("checkpoint.old"))
                std::filesystem::remove("checkpoint.old");
        }
    }
    
    std::filesystem::remove(checkpoint_file);

    std::print("Franja Horaria");
    for (size_t municipio = 0; municipio < MUNICIPIO_COUNT; municipio++)
    {
        std::print(",{}", m_mapper.decodificar(municipio));
    }
    std::println();
    for (size_t franja = 0; franja < anomalias_totales_por_franja_horaria.size(); franja++)
    {
        std::print("{}", franja);
        for (size_t municipio = 0; municipio < anomalias_totales_por_franja_horaria.at(franja).size(); municipio++)
        {
            std::print(",{}", anomalias_totales_por_franja_horaria.at(franja).at(municipio));
        }
        std::println();
    }
}

int main(int argc, char *argv[])
{
    std::vector<const char *> files;

    for (int i = 1; i < argc; i++)
        files.emplace_back(argv[i]);

    AlgoritmoA algoritmo("../shapefiles/procesado.shp");
    
    algoritmo.procesar(std::move(files));
    return 0;
}
