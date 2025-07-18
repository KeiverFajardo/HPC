#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <tuple>

// Calcula la media (promedio) de un conjunto de valores
float calcular_media(const std::vector<float>& valores);

// Calcula el desvío estándar de un conjunto de valores, dado el promedio
float calcular_desvio(const std::vector<float>& valores, float media);

// Determina si un valor es una anomalía dada la media y la desviación estándar
bool es_anomalia(float valor, float media, float desviacion, float umbral = 2.0f);
