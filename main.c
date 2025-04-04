#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <time.h>


/**
 * @struct dir_data
 * @brief Estructura para almacenar informacion sobre un directorio.
 *
 * Esta estructura contiene datos relacionados con un directorio, 
 * incluyendo el numero de archivos y el tamaño total de los mismos.
 *
 * - file_count: Numero total de archivos en el directorio.
 *
 * - total_size: Tamaño total de los archivos en el directorio, en bytes.
 */
struct dir_data {
    int file_count;
    long long total_size;
};

/**
 * @struct sync_data
 * 
 * @brief Esta estructura se utiliza para almacenar información relacionada con la sincronización
 * de datos entre dos directorios.
 * 
 * - weight_from_dir1_to_dir2: Peso total (en bytes) de los archivos transferidos desde 
 *   el directorio 1 al directorio 2.
 *
 * - weight_from_dir2_to_dir1: Peso total (en bytes) de los archivos transferidos desde 
 *   el directorio 2 al directorio 1.
 *
 * - file_count_from_dir1_to_dir2: Cantidad de archivos transferidos desde el directorio 1 
 *   al directorio 2.
 *
 * - file_count_from_dir2_to_dir1: Cantidad de archivos transferidos desde el directorio 2 
 *   al directorio 1.
 */
struct sync_data {
    long long weight_from_dir1_to_dir2;
    long long weight_from_dir2_to_dir1;
    int file_count_from_dir1_to_dir2;
    int file_count_from_dir2_to_dir1;
};


/**
 * @brief Copia un archivo a un directorio especificado.
 *
 * Esta funcion toma un archivo fuente y lo copia a un directorio destino,
 * preservando los permisos del archivo original. Si ocurre algún error durante
 * el proceso, se imprime un mensaje de error en la salida estandar de error.
 *
 * @param file Ruta al archivo fuente que se desea copiar.
 * @param dir Ruta al directorio destino donde se copiara el archivo.
 *
 * @details
 * - La función construye la ruta completa del archivo destino combinando el
 *   directorio destino y el nombre del archivo fuente.
 * - Se abren tanto el archivo fuente como el archivo destino, y se verifica
 *   que las operaciones sean exitosas.
 * - Los permisos del archivo destino se ajustan para que coincidan con los
 *   del archivo fuente.
 * - El contenido del archivo fuente se lee en bloques y se escribe en el
 *   archivo destino.
 *
 */
void cp_file_to_dir(const char *file, const char *dir) {
    char dest_path[1024];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dir, strrchr(file, '/') + 1);
    
    // Abrimos el archivo fuente
    int src_fd = open(file, O_RDONLY);
    if (src_fd < 0) {
        perror("Error abriendo archivo fuente");
        return;
    }
    
    // Obtenemos las caracteristicas del archivo fuente
    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        perror("Error obteniendo información del archivo fuente");
        close(src_fd);
        return;
    }
    
    // Creamos el archivo
    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dest_fd < 0) {
        perror("Error creando archivo destino");
        close(src_fd);
        return;
    }
    
    // Cambiarmos los permisos del archivo destino para que coincidan con el archivo fuente
    if (fchmod(dest_fd, st.st_mode & 0777) < 0) {
        perror("Error cambiando permisos del archivo destino");
        close(src_fd);
        close(dest_fd);
        return;
    }
    
    // Declaramos un buffer para realizar la copia por bloques
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) {
        write(dest_fd, buffer, bytes);
    }
    
    // Cerramos los archivos
    close(src_fd);
    close(dest_fd);
}


/**
 * @brief Copia un directorio de manera recursiva a otro destino.
 *
 * Esta funcion toma un directorio fuente y copia su contenido, incluyendo
 * subdirectorios y archivos, a un directorio de destino. Si el directorio
 * de destino no existe, se crea automaticamente. Ademas, se recopilan
 * estadisticas sobre la cantidad de archivos copiados y el tamaño total
 * de los datos transferidos.
 *
 * @param src Ruta del directorio fuente que se desea copiar.
 * @param dest Ruta del directorio destino donde se copiara el contenido.
 * @return Una estructura `dir_data` que contiene:
 *         - `file_count`: Número total de archivos copiados.
 *         - `total_size`: Tamaño total (en bytes) de los archivos copiados.
 */
struct dir_data cp_dir_to_dir(const char *src, const char *dest) {
    
    // Inicializamos la data
    struct dir_data data;
    data.file_count = 0;
    data.total_size = 0;

    // Abrimos el directorio
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Error abriendo directorio");
        return data;
    }
    
    // Creamos el nuevo directorio
    struct stat st;
    mkdir(dest, 0755);
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char src_path[1024], dest_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);
        
        // Chequeamos si es un archivo o un directorio
        stat(src_path, &st);
        if (S_ISDIR(st.st_mode)) {
            struct dir_data recursiveData = cp_dir_to_dir(src_path, dest_path);
            data.file_count += recursiveData.file_count;
            data.total_size += recursiveData.total_size;
        } else {
            data.file_count++;
            data.total_size += st.st_size;
            cp_file_to_dir(src_path, dest);
        }
    }

    // Cerramos el directorio
    closedir(dir);

    return data;
}

/**
 * @brief Compara si dos archivos tienen el mismo contenido.
 *
 * Esta funcion abre dos archivos en modo binario y compara su contenido
 * para determinar si son idénticos. La comparación se realiza en bloques
 * de 4096 bytes para optimizar el rendimiento en archivos grandes.
 *
 * @param file1 Ruta al primer archivo a comparar.
 * @param file2 Ruta al segundo archivo a comparar.
 * @return int Retorna 1 si los archivos tienen el mismo contenido, 
 *         de lo contrario retorna 0.
 *
 * @note La funcion utiliza `fread` para leer los archivos en bloques
 *       y `memcmp` para comparar los datos leidos. Si alguno de los
 *       archivos es vacío y el otro no, se considera que no son iguales.
 */
int same_content_file(const char *file1, const char *file2) {
    // Abrimos los archivos
    FILE *f1 = fopen(file1, "rb");
    FILE *f2 = fopen(file2, "rb");

    if (!f1 || !f2) {
        perror("Error abriendo archivos para comparación");
        return 0;
    }
    
    int result = 1;
    char buf1[4096], buf2[4096];
    size_t r1, r2;
 
    r1 = fread(buf1, 1, sizeof(buf1), f1);
    r2 = fread(buf2, 1, sizeof(buf2), f2);
    // Chequeamos si alguno de los archivos es vacio
    if((r1 == 0 && r2 != 0) || (r1 != 0 && r2 == 0)) {
        result = 0;
        fclose(f1);
        fclose(f2);
        return result;
    }

    if (r1 != r2 || memcmp(buf1, buf2, r1) != 0) {
        result = 0;
        fclose(f1);
        fclose(f2);
        return result;
    }

    while (r1 > 0 && r2 > 0) {
        if (r1 != r2 || memcmp(buf1, buf2, r1) != 0) {
            result = 0;
            break;
        }

        r1 = fread(buf1, 1, sizeof(buf1), f1);
        r2 = fread(buf2, 1, sizeof(buf2), f2);
    }

    // Cerramos los archivos
    fclose(f1);
    fclose(f2);

    return result;
}

/**
 * @brief Elimina un directorio y todo su contenido de manera recursiva.
 *
 * Esta funcion elimina todos los archivos y subdirectorios dentro del directorio
 * especificado, y luego elimina el directorio en sí. 
 *
 * @param path Ruta del directorio que se desea eliminar.
 *
 * La función realiza las siguientes acciones:
 * - Abre el directorio especificado.
 * - Itera sobre cada entrada en el directorio.
 * - Si la entrada es un subdirectorio, llama recursivamente a rm_dir.
 * - Si la entrada es un archivo, lo elimina.
 * - Finalmente, elimina el directorio una vez que está vacío.
 *
 * @note Si ocurre un error al abrir el directorio, se imprime un mensaje de error
 *       utilizando perror y la función retorna sin realizar ninguna acción.
 */
void rm_dir(const char *path) {
    // Abrimos el directorio
    DIR *dir = opendir(path);
    if (!dir) {
        perror("Error abriendo directorio para eliminación");
        return;
    }
    
    struct dirent *entry;
    char file_path[1024];
    struct stat st;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        snprintf(file_path, sizeof(file_path), "%s/%s", path, entry->d_name);
        stat(file_path, &st);
        // Verificamos si es un directorio o un archivo
        if (S_ISDIR(st.st_mode)) {
            rm_dir(file_path);
        } else {
            remove(file_path);
        }
    }
    // Cerramos el directorio y lo borramos
    closedir(dir);
    rmdir(path);
}

/**
 * @brief Sincroniza dos directorios, copiando o eliminando archivos según las decisiones del usuario.
 * 
 * Esta funcion compara los contenidos de dos directorios y realiza las siguientes acciones:
 * - Si un archivo existe en el primer directorio (d1) pero no en el segundo (d2), 
 *   pregunta al usuario si desea copiarlo al segundo directorio o eliminarlo del primero.
 * - Si un archivo existe en ambos directorios pero su contenido es diferente, 
 *   pregunta al usuario si desea actualizar el archivo mas antiguo con el mas reciente.
 * - Si un archivo es un directorio, la funcion se llama recursivamente para sincronizar su contenido.
 * 
 * @param d1 Ruta del primer directorio.
 * @param d2 Ruta del segundo directorio.
 * @return struct sync_data Estructura que contiene los resultadoss sobre la sincronizacion:
 *         - `weight_from_dir1_to_dir2`: Tamaño total de los archivos copiados de d1 a d2.
 *         - `weight_from_dir2_to_dir1`: Tamaño total de los archivos copiados de d2 a d1.
 *         - `file_count_from_dir1_to_dir2`: Numero de archivos copiados de d1 a d2.
 *         - `file_count_from_dir2_to_dir1`: Numero de archivos copiados de d2 a d1.
 * 
 * @note La función asume que el usuario tiene permisos de lectura y escritura en ambos directorios.
 * @note Si un archivo es un directorio, se sincroniza recursivamente.
 */
struct sync_data sync_dirs(const char *d1, const char *d2) {
    // Inicializamos el struct con los resultados
    struct sync_data data;
    data.weight_from_dir1_to_dir2 = 0;
    data.weight_from_dir2_to_dir1 = 0;
    data.file_count_from_dir1_to_dir2 = 0;
    data.file_count_from_dir2_to_dir1 = 0;

    struct stat st;
    if (stat(d1, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return data;
    }
    
    if (stat(d2, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return data;
    }

    // Abrimos los directorios
    DIR *dir1 = opendir(d1);
    DIR *dir2 = opendir(d2);
    if (!dir1 || !dir2) {
        perror("Error abriendo directorios");
        return data;
    }
    
    struct dirent *entry;
    struct stat st1, st2;
    char path1[1024], path2[1024];

    // Iteramos sobre el directorio1
    while ((entry = readdir(dir1)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(path1, sizeof(path1), "%s/%s", d1, entry->d_name);
        snprintf(path2, sizeof(path2), "%s/%s", d2, entry->d_name);
        
        // Verificamos que el archivo que esta en dir1 este en dir2
        if (stat(path1, &st1) == 0) {
            if (stat(path2, &st2) != 0) {
                printf("%s no existe en %s. Desea copiarlo al directorio que no lo contiene o eliminarlo? (c/e): ", entry->d_name, d2);
                char resp;
                scanf(" %c", &resp);
                // Si la respuesta es 'c' copiamos el archivo
                if (resp == 'c') {
                    
                    printf("Copiando %lld a %s, %s\n", data.weight_from_dir1_to_dir2, d2, path1);

                    // Verificamos si es un archivo o un directorio
                    if (S_ISDIR(st1.st_mode)) {
                        struct dir_data copied_dir_data = cp_dir_to_dir(path1, path2);
                        data.file_count_from_dir1_to_dir2 += copied_dir_data.file_count;
                        data.weight_from_dir1_to_dir2 += copied_dir_data.total_size;

                    } else {
                        data.file_count_from_dir1_to_dir2++;
                        data.weight_from_dir1_to_dir2 += st1.st_size;

                        cp_file_to_dir(path1, d2);
                    }
                // Si la respuesta es 'e' eliminamos el archivo
                } else if (resp == 'e') {
                    if (S_ISDIR(st1.st_mode)) {
                        rm_dir(path1);
                    } else {
                        remove(path1);
                    }
                }
            }else{
                // Hacemos la verificacion de la fecha de modificacion entre dos archivos de mismo
                // nombre y tamaño
                if (!S_ISDIR(st1.st_mode) && !S_ISDIR(st2.st_mode) && !same_content_file(path1, path2))  {
                    
                    if (difftime(st1.st_mtime, st2.st_mtime) > 0) {
                        printf("%s fue modificado más recientemente que %s. Actualizar %s? (y/n): ", path1, path2, path2);
                        char resp;
                        scanf(" %c", &resp);
                        if (resp == 'y') {

                            data.weight_from_dir1_to_dir2 += st1.st_size;
                            data.file_count_from_dir1_to_dir2++;
                            cp_file_to_dir(path1, d2);
                        }else if (resp == 'n') {
                            
                            data.weight_from_dir2_to_dir1 += st2.st_size;
                            data.file_count_from_dir2_to_dir1++;
                            cp_file_to_dir(path2, d1);

                        }
                    }
                }  
            }
        }

        // Sumamos los datos de la sincronizacion recursiva
        struct sync_data recursive_data = sync_dirs(path1, path2);
        data.weight_from_dir1_to_dir2 += recursive_data.weight_from_dir1_to_dir2;
        data.weight_from_dir2_to_dir1 += recursive_data.weight_from_dir2_to_dir1;
        data.file_count_from_dir1_to_dir2 += recursive_data.file_count_from_dir1_to_dir2;
        data.file_count_from_dir2_to_dir1 += recursive_data.file_count_from_dir2_to_dir1;
    }

    // Cerramos los archivos
    closedir(dir1);
    closedir(dir2);
    return data;
}

// Funcion main
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <directorio1> <directorio2>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Hacemos la sincronizacion de manera bidireccional
    struct sync_data data_first_call = sync_dirs(argv[1], argv[2]);
    struct sync_data data_second_call = sync_dirs(argv[2], argv[1]);

    // Definimos variables que guardan los resultados
    long long weight_from_dir1_to_dir2 = data_first_call.weight_from_dir1_to_dir2 + data_second_call.weight_from_dir2_to_dir1;
    long long weight_from_dir2_to_dir1 = data_first_call.weight_from_dir2_to_dir1 + data_second_call.weight_from_dir1_to_dir2;
    int file_count_from_dir1_to_dir2 = data_first_call.file_count_from_dir1_to_dir2 + data_second_call.file_count_from_dir2_to_dir1;
    int file_count_from_dir2_to_dir1 = data_first_call.file_count_from_dir2_to_dir1 + data_second_call.file_count_from_dir1_to_dir2;

    // Se imprimen los resultados
    printf("Sincronización completada. \n");
    printf("Se transfirieron %lld kb y %d archivos desde el primer directorio hacia el segundo directorio\n ", weight_from_dir1_to_dir2/1024, file_count_from_dir1_to_dir2);
    printf("Se transfirieron %lld kb y %d archivos desde el segundo directorio hacia el primer directorio\n ", weight_from_dir2_to_dir1/1024, file_count_from_dir2_to_dir1);
    return EXIT_SUCCESS;
}
