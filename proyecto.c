#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_INPUT 1024
#define MAX_ARGS 200
#define MAX_CMDS 50
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"

// Función para dividir la entrada en tokens
void parse_input(char *input, char **args) {
    char *token = strtok(input, " \t\n");
    int i = 0;
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;
}

// Función para dividir la entrada por tuberías
int split_by_pipes(char *input, char *cmds[]) {
    int i = 0;
    char *token = strtok(input, "|");
    while (token != NULL) {
        cmds[i++] = token;
        token = strtok(NULL, "|");
    }
    cmds[i] = NULL;
    return i;
}

// Función para manejar redirecciones
void handle_redirections(char **args) {
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) { // Redirección de entrada
            int fd = open(args[i + 1], O_RDONLY);
            if (fd < 0) {
                perror(RED "Error al abrir el archivo de entrada" RESET);
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
            args[i] = NULL; // Eliminar "<" y el archivo de los argumentos
        } else if (strcmp(args[i], ">") == 0) { // Redirección de salida
            int fd = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) {
                perror(RED "Error al abrir el archivo de salida" RESET);
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            args[i] = NULL; // Eliminar ">" y el archivo de los argumentos
        }
    }
}

// Función para manejar múltiples tuberías
void execute_command(char *input) {
    char *cmds[MAX_CMDS];
    int num_cmds = split_by_pipes(input, cmds);

    if (num_cmds < 1) return;

    int pipefds[2 * (num_cmds - 1)];
    for (int i = 0; i < num_cmds - 1; i++) {
        if (pipe(pipefds + 2 * i) < 0) {
            perror(RED "Error al crear la tubería" RESET);
            exit(1);
        }
    }

    for (int i = 0; i < num_cmds; i++) {
        pid_t pid = fork();

        if (pid == 0) { // Proceso hijo
            char *args[MAX_ARGS];
            parse_input(cmds[i], args);

            // Manejo de redirecciones antes de redirigir tuberías
            handle_redirections(args);

            // Redirecciones de entrada
            if (i > 0) { 
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }
            // Redirecciones de salida
            if (i < num_cmds - 1) { 
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            // Cerrar descriptores no usados
            for (int j = 0; j < 2 * (num_cmds - 1); j++) {
                close(pipefds[j]);
            }

            execvp(args[0], args);
            fprintf(stderr, RED "%s: no se encontró la orden\n" RESET, args[0]);
            exit(1);
        } else if (pid < 0) {
            perror(RED "Error al crear el proceso" RESET);
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