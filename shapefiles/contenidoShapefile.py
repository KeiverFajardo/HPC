import geopandas as gpd

gdf = gpd.read_file("muni.shp")

for index, row in gdf.iterrows():
    print(f"Municipio {row['MUNICIPIO']} con ID {row['GID']}")


""" import geopandas as gpd

# Cambia esta ruta por la de tu shapefile
shp = gpd.read_file("sig_municipios.shp")

# Muestra las columnas / campos del shapefile
print(shp.columns)

# Opcional: muestra las primeras filas con los datos
print(shp.head()) """
