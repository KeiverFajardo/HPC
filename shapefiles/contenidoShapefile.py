import geopandas as gpd
import sys

shp = gpd.read_file(sys.argv[1])
shp = shp.to_crs(epsg=4326)

shp.to_file('shapefiles/procesado.shp')

for index, row in shp.iterrows():
    print(f"Municipio {row['MUNICIPIO']} con ID {row['GID']}")

# Muestra las columnas / campos del shapefile
print(shp.columns)

# Opcional: muestra las primeras filas con los datos
print(shp.head())
