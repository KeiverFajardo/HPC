#pragma once

#include <fstream>
#include <string>
#include <sstream>

struct Date {
    uint16_t year;
    uint8_t month;
    uint8_t day;
};

struct Hour {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

struct Register {
    unsigned int cod_detector;
    unsigned int id_carril;
    Date fecha;
    Hour hora;  // HHMMSS
    float latitud, longitud;
    float velocidad;
    uint8_t municipio_id; // id municipio
    uint8_t franja_horaria; // 0 = madrugada, 1 = jornada, 2 = noche
};

class CsvReader {
public:
    CsvReader(const char *filename, size_t begin, size_t end)
        : m_end(end), m_file(filename)
    {
        m_file.seekg(begin, std::ios::beg);
        m_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    /// Returns false if there isn't more registers
    bool get(Register &reg);
private:
    Date parse_date(std::stringstream &date_string);
    Hour parse_hour(std::stringstream &date_string);

    size_t m_end;
    std::stringstream m_line, m_aux_ss;
    std::string m_value, m_aux_buf;
    std::ifstream m_file;
};

template <>
struct std::formatter<Register> {
    bool multiline = false;
 
    template<class ParseContext>
    constexpr ParseContext::iterator parse(ParseContext& ctx)
    {
        auto it = ctx.begin();
        if (it == ctx.end())
            return it;

        if (*it == '@')
        {
            multiline = true;
            it++;
        }

        if (it != ctx.end() && *it != '}')
            throw std::format_error("Invalid format args for Register.");

        return it;
    }

    template<class FmtContext>
    constexpr typename FmtContext::iterator format(const Register& reg, FmtContext& ctx) const {
        // Format the Person as a string and pass to formatter<string>
        if (multiline)
        {
            return std::format_to(
                ctx.out(),
                "(\n"
                "\tcod_detector: {},\n"
                "\tid_carril: {},\n"
                "\tfecha: {}-{}-{},\n"
                "\thora: {}:{}:{},\n"
                "\tlatitud: {},\n"
                "\tlongitud: {},\n"
                "\tvelocidad: {},\n"
                "\tmunicipio_id: {},\n"
                "\tfranja_horaria: {}\n"
                ")",
                reg.cod_detector,
                reg.id_carril,
                reg.fecha.year, reg.fecha.month, reg.fecha.day,
                reg.hora.hour, reg.hora.minute, reg.hora.second,
                reg.latitud,
                reg.longitud,
                reg.velocidad,
                reg.municipio_id,
                reg.franja_horaria
            );
        }
        else
        {
            return std::format_to(
                ctx.out(),
                "(cod_detector: {}, id_carril: {}, fecha: {}-{}-{}, hora: {}:{}:{}, latitud: {}, longitud: {}, velocidad: {}, municipio_id: {}, franja_horaria: {})",
                reg.cod_detector,
                reg.id_carril,
                reg.fecha.year, reg.fecha.month, reg.fecha.day,
                reg.hora.hour, reg.hora.minute, reg.hora.second,
                reg.latitud,
                reg.longitud,
                reg.velocidad,
                reg.municipio_id,
                reg.franja_horaria
            );
        }
    }
};
