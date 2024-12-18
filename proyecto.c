#include <stdio.h> // funciones de entradas y salidas
#include <stdlib.h> // funciones generales 
#include <string.h> // manipulacion de cadenas
#include <unistd.h> // funciones del sistema operativo (fork,exevcp, dup2)
#include <sys/types.h> // manejo de procesos
#include <sys/wait.h>
#include <fcntl.h> // operaciones sobre archivoc

#define MAX_INPUT 1024 // Tamaño maximo de la cadena a introducir de comando
#define MAX_ARGS 200 // Numero maximo de argumentos por comando
#define MAX_CMDS 50 // Numero maximo de comandos en la tuberia
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

// Función para dividir la entrada en tokens y argumentos
void parse_input(char *input, char **args) {
    char *token = strtok(input, " \t\n"); // Empieza a tokenizar la cadena separado por espacios, tabulaciones o saltos de linea
    int i = 0;  // Inicia el contador de tokens
    while (token != NULL) { // Mientras la cadena no se termina sigue separando por tokens
        args[i++] = token;  
        token = strtok(NULL, " \t\n");
        
    }
    args[i] = NULL; // Al llegar al final de la cadena se almacena NULL en el arreglo 
}

// Función para dividir la entrada por tuberías
int split_by_pipes(char *input, char *cmds[]) { 
    int i = 0;
    char *token = strtok(input, "|"); // Empieza a tokenizar la cadena separada por | 
    while (token != NULL) { // Sigue buscando hasta que no encuentre mas | 
        cmds[i++] = token; // Almacena en cmds el comando del |
        token = strtok(NULL, "|");  // Sigue buscando en la cadena en la posicion donde se quedo
    }
    cmds[i] = NULL; // Al llegar al final de la cadena y no encuentra mas | almacena NULL en el ultimo elemento del arreglo
    return i;
}

// Función para manejar redirecciones
void handle_redirections(char **args) {
    for (int i = 0; args[i] != NULL; i++) { // Recorre todos los argumentos hasta encontrar NULL
        if (strcmp(args[i], "<") == 0) { // Compara la cadena si se trata de "<" y devuelve 0 si son iguales
            int fd = open(args[i + 1], O_RDONLY);  // El archivo donde se leera la entrada esta en i + 1
            if (fd < 0) { 
                perror(RED "Error al abrir el archivo de entrada" RESET);
                exit(1);
            }
            dup2(fd, STDIN_FILENO); // Redirige el descriptor de archivo fd hacia la entrada estandar
            close(fd); // Se cierra el descriptor de archivo 
            args[i] = NULL; // Eliminar "<" y el archivo de los argumentos
        } else if (strcmp(args[i], ">") == 0) { // Compara la cadena si se trata de ">" y devuelve 0 si son iguales
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644); // El archivo donde se leera la entrada esta en i + 1, 0644: Permisos del archivo (lectura/escritura para el propietario, solo lectura para otros)
            if (fd < 0) {
                perror(RED "Error al abrir el archivo de salida" RESET);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO); // Redirige el descriptor de archivo hacia la salida estandar
            close(fd); // Cierra el descriptor de archivo
            args[i] = NULL; // Eliminar ">" y el archivo de los argumentos
        }
    }
}

// Función para manejar múltiples tuberías
void execute_command(char *input) {
    char *cmds[MAX_CMDS]; // Arreglo para almacenar comandos separados por tuberias
    int num_cmds = split_by_pipes(input, cmds); // Divide la entrada por |

    if (num_cmds < 1) return; // Si el numero de comandos es menor a uno osea cero se sale de la funcion

    int pipefds[2 * (num_cmds - 1)]; // Crea los descriptores de archivos para cada tuberia 
    for (int i = 0; i < num_cmds - 1; i++) { // Crea una tuberia por cada comando
        if (pipe(pipefds + 2 * i) < 0) {  // Crea la tuberia con su descriptores de archivo (pipefds + 2 * 0) = pipefds[0], pipefds[1], (pipefds + 2 * 1) = pipefds[2] , pipefds[3]
            perror(RED "Error al crear la tubería" RESET);
            exit(1);
        }
    }

    for (int i = 0; i < num_cmds; i++) { // Se crea un proceso por cada comando
        pid_t pid = fork(); 

        if (pid == 0) { // Proceso hijo
            char *args[MAX_ARGS];
            parse_input(cmds[i], args); // Divide cada comando en argumentos

            // Manejo de redirecciones antes de redirigir tuberías
            handle_redirections(args); 

            // Redirecciones de entrada
            if (i > 0) { 
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO); // Si no es el primer comando conecta la salida de la tuberia anterior al comando actual
            }
            // Redirecciones de salida
            if (i < num_cmds - 1) { 
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO); // Si no es el ultimo comando conecta la salida del comando actual a la entrada de la siguiente ruberia
            }

            // Cerrar descriptores no usados
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }

            execvp(args[0], args); // ejecuta el comando por cada argumento
            fprintf(stderr, RED "%s: no se encontró la orden\n" RESET, args[0]);
            exit(1);
        } else if (pid < 0) {
            fprintf(stderr, RED ": Error al crear el proceso \n" RESET);
            exit(1);
        }
    }

    // Cerrar descriptores de las tuberías en el proceso padre
    for (int i = 0; i < 2 * (num_cmds - 1); i++) {
        close(pipefds[i]);
    }

    // Esperar a todos los hijos
    for (int i = 0; i < num_cmds; i++) {
        wait(NULL);
    }
}

int main() {
    system("clear");
    char input[MAX_INPUT];
    const char *username = getenv("USER");

    while (1) {
        printf(YELLOW "%s> " RESET, username);
        fflush(stdout);
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            break;
        }
        if (strcmp(input, "exit\n") == 0) {
            break;
        }
        execute_command(input);
    }

    return 0;
}