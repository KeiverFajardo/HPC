#include "csv_reader.hpp"
#include "log.hpp"

#include <sstream>
#include <string>

// cod_detector,id_carril,fecha,hora,dsc_avenida,dsc_int_anterior,dsc_int_siguiente,latitud,longitud,velocidad
// 206,1,2025-05-01,23:55:00.0,Bv Batlle y Ordonez,Joanico,8 de Octubre,-34.878960,-56.148300,36.0

Date CsvReader::parse_date(const std::string &date_string)
{
    Date result;
    std::stringstream ss(date_string);

    std::getline(ss, m_aux_buf, '-');
    result.year = std::stoul(m_aux_buf);
    std::getline(ss, m_aux_buf, '-');
    result.month = std::stoul(m_aux_buf);
    std::getline(ss, m_aux_buf, '-');
    result.day = std::stoul(m_aux_buf);

    return result;
}

Hour CsvReader::parse_hour(const std::string &hour_string)
{
    Hour result;
    std::stringstream ss(hour_string);

    std::getline(ss, m_aux_buf, ':');
    result.hour = std::stoul(m_aux_buf);
    std::getline(ss, m_aux_buf, ':');
    result.minute = std::stoul(m_aux_buf);
    std::getline(ss, m_aux_buf, '.');
    result.second = std::stoul(m_aux_buf);

    return result;
}

bool CsvReader::get(Register &reg)
{
    m_line.clear();
    while (m_file.good() && m_line.empty()) std::getline(m_file, m_line);
    if (m_file.eof()) return false;

    std::stringstream ss(m_line);

    std::getline(ss, m_value, ',');
    reg.cod_detector = std::stoul(m_value);
    
    std::getline(ss, m_value, ',');
    reg.id_carril = std::stoul(m_value);
    
    std::getline(ss, m_value, ',');
    reg.fecha = parse_date(m_value);
    
    std::getline(ss, m_value, ',');
    reg.hora = parse_hour(m_value);

    ss.ignore(std::numeric_limits<std::streamsize>::max(), ','); // dsc_avenida
    ss.ignore(std::numeric_limits<std::streamsize>::max(), ','); // dsc_int_anterior
    ss.ignore(std::numeric_limits<std::streamsize>::max(), ','); // dsc_int_siguiente

    std::getline(ss, m_value, ',');
    reg.latitud = std::stof(m_value);
    
    std::getline(ss, m_value, ',');
    reg.longitud = std::stof(m_value);
    
    std::getline(ss, m_value, ',');
    reg.velocidad = std::stof(m_value);

    return true;
}
