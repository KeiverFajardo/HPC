# Compilar

```
> mkdir build
> cd build
> cmake .. && cmake --build .
```

# Ejecutar

Parado el directorio build:

```
> export OMP_NUM_THREADS=numero_threads_por_nodo
> mpirun -n numero_nodos -hosts lista_hosts src/hpc_project [archivos_csv_en_orden_cronologico...]
```
