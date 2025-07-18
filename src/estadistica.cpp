#include "estadistica.hpp"

float calcular_media(const std::vector<float>& valores) {
    if (valores.empty()) return 0.0f;

    float suma = 0.0f;
    for (float v : valores)
        suma += v;

    return suma / valores.size();
}

float calcular_desvio(const std::vector<float>& valores, float media) {
    if (valores.size() < 2) return 0.0f;

    float suma = 0.0f;
    for (float v : valores)
        suma += (v - media) * (v - media);

    return std::sqrt(suma / (valores.size() - 1));
}

bool es_anomalia(float valor, float media, float desviacion, float umbral)
{
    if (desviacion == 0.0f) return false;
    return std::abs(valor - media) > umbral * desviacion;
}
