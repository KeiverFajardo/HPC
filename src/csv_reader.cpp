#include "csv_reader.hpp"
#include <sstream>
#include <string>

// cod_detector,id_carril,fecha,hora,dsc_avenida,dsc_int_anterior,dsc_int_siguiente,latitud,longitud,velocidad
// 206,1,2025-05-01,23:55:00.0,Bv Batlle y Ordonez,Joanico,8 de Octubre,-34.878960,-56.148300,36.0

Date CsvReader::parse_date(std::stringstream &date_string)
{
    Date result;

    std::getline(date_string, m_aux_buf, '-');
    result.year = std::stoul(m_aux_buf);
    std::getline(date_string, m_aux_buf, '-');
    result.month = std::stoul(m_aux_buf);
    std::getline(date_string, m_aux_buf, '-');
    result.day = std::stoul(m_aux_buf);

    return result;
}

Hour CsvReader::parse_hour(std::stringstream &hour_string)
{
    Hour result;

    std::getline(hour_string, m_aux_buf, ':');
    result.hour = std::stoul(m_aux_buf);
    std::getline(hour_string, m_aux_buf, ':');
    result.minute = std::stoul(m_aux_buf);
    std::getline(hour_string, m_aux_buf, '.');
    result.second = std::stoul(m_aux_buf);

    return result;
}

bool CsvReader::get(Register &reg)
{
    if (m_file.tellg() >= m_end) {
        m_file.close();
        return false;
    }
    m_value.clear();
    while (m_file.good() && m_value.empty()) std::getline(m_file, m_value);
    if (m_file.eof()) return false;
    m_line.clear();
    m_line.str(m_value);

    std::getline(m_line, m_value, ',');
    reg.cod_detector = std::stoul(m_value);
    
    std::getline(m_line, m_value, ',');
    reg.id_carril = std::stoul(m_value);
   
    std::getline(m_line, m_value, ',');
    m_aux_ss.clear();
    m_aux_ss.str(m_value);
    reg.fecha = parse_date(m_aux_ss);
    
    std::getline(m_line, m_value, ',');
    m_aux_ss.clear();
    m_aux_ss.str(m_value);
    reg.hora = parse_hour(m_aux_ss);

    m_line.ignore(std::numeric_limits<std::streamsize>::max(), ','); // dsc_avenida
    m_line.ignore(std::numeric_limits<std::streamsize>::max(), ','); // dsc_int_anterior
    m_line.ignore(std::numeric_limits<std::streamsize>::max(), ','); // dsc_int_siguiente

    std::getline(m_line, m_value, ',');
    reg.latitud = std::stof(m_value);
    
    std::getline(m_line, m_value, ',');
    reg.longitud = std::stof(m_value);
    
    std::getline(m_line, m_value, ',');
    reg.velocidad = std::stof(m_value);

    return true;
}
