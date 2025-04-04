#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <time.h>

// Función para copiar un archivo a un directorio
// Toma la ruta de un archivo y un directorio de destino y copia el archivo allí
void cp_file_to_dir(const char *file, const char *dir) {
    char dest_path[1024];
    snprintf(dest_path, sizeof(dest_path), "%s/%s", dir, strrchr(file, '/') + 1);
    
    int src_fd = open(file, O_RDONLY);
    if (src_fd < 0) {
        perror("Error abriendo archivo fuente");
        return;
    }
    
    struct stat st;
    if (fstat(src_fd, &st) < 0) {
        perror("Error obteniendo información del archivo fuente");
        close(src_fd);
        return;
    }
    
    
    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
    if (dest_fd < 0) {
        perror("Error creando archivo destino");
        close(src_fd);
        return;
    }
    
    // Cambiar los permisos del archivo destino para que coincidan con el archivo fuente
    if (fchmod(dest_fd, st.st_mode & 0777) < 0) {
        perror("Error cambiando permisos del archivo destino");
        close(src_fd);
        close(dest_fd);
        return;
    }
    
    char buffer[4096];
    ssize_t bytes;
    while ((bytes = read(src_fd, buffer, sizeof(buffer))) > 0) {
        write(dest_fd, buffer, bytes);
    }
    
    close(src_fd);
    close(dest_fd);
}

struct dir_data {
    int file_count;
    long long total_size;
};
// Función para copiar un directorio de manera recursiva a otro destino
struct dir_data cp_dir_to_dir(const char *src, const char *dest) {
    
    struct dir_data data;
    data.file_count = 0;
    data.total_size = 0;

    DIR *dir = opendir(src);
    if (!dir) {
        perror("Error abriendo directorio");
        return data;
    }
    
    struct stat st;
    mkdir(dest, 0755);
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        
        char src_path[1024], dest_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, entry->d_name);
        
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
    closedir(dir);

    return data;
}

// Función para comparar si dos archivos tienen el mismo contenido
int same_content_file(const char *file1, const char *file2) {
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
    
    fclose(f1);
    fclose(f2);

    return result;
}

// Función para eliminar un directorio y su contenido de manera recursiva
void rm_dir(const char *path) {
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
        if (S_ISDIR(st.st_mode)) {
            rm_dir(file_path);
        } else {
            remove(file_path);
        }
    }
    closedir(dir);
    rmdir(path);
}

struct sync_data {
    long long weight_from_dir1_to_dir2;
    long long weight_from_dir2_to_dir1;
    int file_count_from_dir1_to_dir2;
    int file_count_from_dir2_to_dir1;
};

// Función para sincronizar dos directorios
// Si un archivo existe en d1 pero no en d2, pregunta al usuario si desea copiarlo
struct sync_data sync_dirs(const char *d1, const char *d2) {
    struct sync_data data;
    data.weight_from_dir1_to_dir2 = 0;
    data.weight_from_dir2_to_dir1 = 0;
    data.file_count_from_dir1_to_dir2 = 0;
    data.file_count_from_dir2_to_dir1 = 0;

    struct stat st;
    if (stat(d1, &st) != 0 || !S_ISDIR(st.st_mode)) {
        // fprintf(stderr, "%s no es un directorio válido.\n", d1);
        return data;
    }
    
    if (stat(d2, &st) != 0 || !S_ISDIR(st.st_mode)) {
        // fprintf(stderr, "%s no es un directorio válido.\n", d2);
        return data;
    }

    DIR *dir1 = opendir(d1);
    DIR *dir2 = opendir(d2);
    if (!dir1 || !dir2) {
        perror("Error abriendo directorios");
        return data;
    }
    
    struct dirent *entry;
    struct stat st1, st2;
    char path1[1024], path2[1024];


    while ((entry = readdir(dir1)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(path1, sizeof(path1), "%s/%s", d1, entry->d_name);
        snprintf(path2, sizeof(path2), "%s/%s", d2, entry->d_name);
        
        if (stat(path1, &st1) == 0) {
            if (stat(path2, &st2) != 0) {
                printf("%s no existe en %s. Desea copiarlo al directorio que no lo contiene o eliminarlo? (c/e): ", entry->d_name, d2);
                char resp;
                scanf(" %c", &resp);
                if (resp == 'c') {
                    // data.weight_from_dir1_to_dir2 += st1.st_size;
                    printf("Copiando %lld a %s, %s\n", data.weight_from_dir1_to_dir2, d2, path1);
                    if (S_ISDIR(st1.st_mode)) {
                        struct dir_data copied_dir_data = cp_dir_to_dir(path1, path2);
                        data.file_count_from_dir1_to_dir2 += copied_dir_data.file_count;
                        data.weight_from_dir1_to_dir2 += copied_dir_data.total_size;

                    } else {
                        data.file_count_from_dir1_to_dir2++;
                        data.weight_from_dir1_to_dir2 += st1.st_size;

                        cp_file_to_dir(path1, d2);
                    }
                } else if (resp == 'e') {
                    if (S_ISDIR(st1.st_mode)) {
                        rm_dir(path1);
                    } else {
                        remove(path1);
                    }
                }
            }else{
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

        struct sync_data recursive_data = sync_dirs(path1, path2);
        data.weight_from_dir1_to_dir2 += recursive_data.weight_from_dir1_to_dir2;
        data.weight_from_dir2_to_dir1 += recursive_data.weight_from_dir2_to_dir1;
        data.file_count_from_dir1_to_dir2 += recursive_data.file_count_from_dir1_to_dir2;
        data.file_count_from_dir2_to_dir1 += recursive_data.file_count_from_dir2_to_dir1;
    }

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
    
    struct sync_data data_first_call = sync_dirs(argv[1], argv[2]);
    struct sync_data data_second_call = sync_dirs(argv[2], argv[1]);

    long long weight_from_dir1_to_dir2 = data_first_call.weight_from_dir1_to_dir2 + data_second_call.weight_from_dir2_to_dir1;
    long long weight_from_dir2_to_dir1 = data_first_call.weight_from_dir2_to_dir1 + data_second_call.weight_from_dir1_to_dir2;
    int file_count_from_dir1_to_dir2 = data_first_call.file_count_from_dir1_to_dir2 + data_second_call.file_count_from_dir2_to_dir1;
    int file_count_from_dir2_to_dir1 = data_first_call.file_count_from_dir2_to_dir1 + data_second_call.file_count_from_dir1_to_dir2;

    printf("Sincronización completada. \n");
    printf("Se transfirieron %lld kb y %d archivos desde el primer directorio hacia el segundo directorio\n ", weight_from_dir1_to_dir2/1024, file_count_from_dir1_to_dir2);
    printf("Se transfirieron %lld kb y %d archivos desde el segundo directorio hacia el primer directorio\n ", weight_from_dir2_to_dir1/1024, file_count_from_dir2_to_dir1);
    return EXIT_SUCCESS;
}
