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
    
    int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd < 0) {
        perror("Error creando archivo destino");
        close(src_fd);
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

// Función para copiar un directorio de manera recursiva a otro destino
void cp_dir_to_dir(const char *src, const char *dest) {
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Error abriendo directorio");
        return;
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
            cp_dir_to_dir(src_path, dest_path);
        } else {
            cp_file_to_dir(src_path, dest);
        }
    }
    closedir(dir);
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
    while ((r1 = fread(buf1, 1, sizeof(buf1), f1)) > 0 && 
           (r2 = fread(buf2, 1, sizeof(buf2), f2)) > 0) {
        if (r1 != r2 || memcmp(buf1, buf2, r1) != 0) {
            result = 0;
            break;
        }
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

// Función para sincronizar dos directorios
// Si un archivo existe en d1 pero no en d2, pregunta al usuario si desea copiarlo
void sync_dirs(const char *d1, const char *d2) {
    DIR *dir1 = opendir(d1);
    DIR *dir2 = opendir(d2);
    if (!dir1 || !dir2) {
        perror("Error abriendo directorios");
        return;
    }
    
    struct dirent *entry;
    struct stat st1, st2;
    char path1[1024], path2[1024];
    
    while ((entry = readdir(dir1)) != NULL) {
        snprintf(path1, sizeof(path1), "%s/%s", d1, entry->d_name);
        snprintf(path2, sizeof(path2), "%s/%s", d2, entry->d_name);
        
        if (stat(path1, &st1) == 0) {
            if (stat(path2, &st2) != 0) {
                printf("%s no existe en %s. Copiarlo? (y/n): ", entry->d_name, d2);
                char resp;
                scanf(" %c", &resp);
                if (resp == 'y') {
                    if (S_ISDIR(st1.st_mode)) {
                        cp_dir_to_dir(path1, path2);
                    } else {
                        cp_file_to_dir(path1, d2);
                    }
                }
            }else{
                if (!S_ISDIR(st1.st_mode) && !S_ISDIR(st2.st_mode) && !same_content_file(path1, path2))  {
                        
                    if (difftime(st1.st_mtime, st2.st_mtime) > 0) {
                        printf("%s fue modificado más recientemente que %s. Actualizar %s? (y/n): ", path1, path2, path2);
                        char resp;
                        scanf(" %c", &resp);
                        if (resp == 'y') {
                            if (S_ISDIR(st1.st_mode)) {
                                rm_dir(path2);
                                cp_dir_to_dir(path1, path2);
                            } else {
                                cp_file_to_dir(path1, d2);
                            }
                        }
                    }else if (difftime(st1.st_mtime, st2.st_mtime) < 0) {
                        printf("%s fue modificado más recientemente que %s. Actualizar %s? (y/n): ", path2, path1, path1);
                        char resp;
                        scanf(" %c", &resp);
                        if (resp == 'y') {
                            if (S_ISDIR(st2.st_mode)) {
                                rm_dir(path1);
                                cp_dir_to_dir(path2, path1);
                            } else {
                                cp_file_to_dir(path2, d1);
                            }
                        }
                    }
                }  
            }
        }
    }
    closedir(dir1);
    closedir(dir2);
}

// Función principal que recibe dos directorios como argumentos y sincroniza su contenido
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <directorio1> <directorio2>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    sync_dirs(argv[1], argv[2]);
    sync_dirs(argv[2], argv[1]);

    printf("Sincronización completada.\n");
    return EXIT_SUCCESS;
}
