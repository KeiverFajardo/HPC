#pragma once

#include "csv_reader.hpp"
#include <cassert>
#include <cstdint>

struct UmbralPorPar {
    uint8_t id;
    float umbral;
};

constexpr uint8_t MUNICIPIO_COUNT = 8;
constexpr uint8_t FRANJA_HORARIA_COUNT = 3;
constexpr uint8_t DIA_SEMANA_COUNT = 7;

constexpr uint8_t MAX_UMBRAL_ID = MUNICIPIO_COUNT * FRANJA_HORARIA_COUNT * DIA_SEMANA_COUNT;

inline uint8_t get_umbral_id(uint8_t municipio, uint8_t franja_horaria, uint8_t dia_semana)
{
    return
        + dia_semana * MUNICIPIO_COUNT * FRANJA_HORARIA_COUNT
        + franja_horaria * MUNICIPIO_COUNT
        + municipio;
}

inline void reverse_umbral_id(uint8_t umbral_id, uint8_t &municipio, uint8_t &franja_horaria, uint8_t &dia_semana)
{
    municipio = umbral_id % MUNICIPIO_COUNT;
    franja_horaria = (umbral_id / MUNICIPIO_COUNT) % FRANJA_HORARIA_COUNT;
    dia_semana = umbral_id / (MUNICIPIO_COUNT * FRANJA_HORARIA_COUNT);
}

inline uint8_t day_of_week(uint8_t d, uint8_t m, uint8_t y)
{
    // Predefined month codes for each month
    static uint8_t monthCode[] = {6, 2, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

    // Adjust year for January and February
    if (m < 3) {
        y -= 1;  // If month is January or February, treat them as part of the previous year
    }

    // Calculate the year code
    int yearCode = (y % 100) + (y % 100) / 4;

    // Adjust year code for the century
    yearCode = (yearCode + (y / 100) / 4 + 5 * (y / 100)) % 7;

    // Calculate the day of the week and return the value as an integer
    return (d + monthCode[m - 1] + yearCode) % 7;
}

inline uint8_t get_umbral_id(uint8_t municipio, uint8_t franja_horaria, Date fecha)
{
    return
        + day_of_week(fecha.day, fecha.month, fecha.year) * MUNICIPIO_COUNT * FRANJA_HORARIA_COUNT
        + franja_horaria * MUNICIPIO_COUNT
        + municipio;
}

