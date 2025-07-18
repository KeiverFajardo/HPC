// registro_extendido.hpp
struct RegisterExt {
    unsigned cod_detector;
    unsigned id_carril;
    Date fecha;
    Hour hora;
    float latitud;
    float longitud;
    float velocidad;

    uint8_t municipio_id;
    uint8_t franja_horaria; // 0 = madrugada, 1 = jornada, 2 = noche
};
