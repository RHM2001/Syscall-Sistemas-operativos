#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#define MIN_NUMPROC 1
#define MAX_NUMPROC 8
#define MAX_BYTES 128

int read_all(int fd, char *buf, ssize_t size)
{
    ssize_t num_r = 0;
    ssize_t num_left = size;
    char *buf_left = buf;
    int nums_read = 0;

    while ((num_left > 0) && (num_r = read(fd, buf_left, num_left)) != 0)
    {
        buf_left += num_r;
        num_left -= num_r;
        nums_read += num_r;
    }

    return nums_read;
}

/////////////////////////////////////////////////////////////////////////
//                                                                     //
//  Función para la extracción de argumentos de una lénea de comandos  //
//                                                                     //
/////////////////////////////////////////////////////////////////////////

char **extract_arguments(char *l)
{
    char delimitador[] = " ";

    char **argumentos = malloc(sizeof(char) * MAX_BYTES);

    if (argumentos == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    char *linea = strtok(l, delimitador);
    if (linea != NULL)
    {
        int j = 0;
        while (linea != NULL)
        {
            // Sólo en la primera pasamos la cadena; en las siguientes pasamos NULL
            argumentos[j] = linea;
            linea = strtok(NULL, delimitador);
            j++;
        }
        argumentos[j + 1] = NULL;
    }
    return argumentos;
}

//////////////////////////////////////////////////////////////////////////////////////////
//                                                                                      //
//  Función para llevar a cabo la creación de un proceso con los argumentos indicados.  //
//                                                                                      //
//////////////////////////////////////////////////////////////////////////////////////////

pid_t execvp_process(char **args)
{
    pid_t pid; /* Usado en el proceso padre para guardar el PID del proceso hijo */

    switch (pid = fork())
    {
    case -1:             /* fork() falló */
        perror("fork()");
        exit(EXIT_FAILURE);
        break;
    case 0:             /* Ejecución del proceso hijo tras fork() con éxito */
        execvp(args[0], args);                
        fprintf(stderr, "execlp() failed\n");
        exit(EXIT_FAILURE);
        break;
    }

    return pid;
}

int main(int argc, char **argv)
{

    int opt;
    int NUMPROC = MIN_NUMPROC;

    while ((opt = getopt(argc, argv, "p:h")) != -1)
    {
        switch (opt)
        {
        case 'p':
            if (atoi(optarg) >= MIN_NUMPROC && atoi(optarg) <= MAX_NUMPROC)
            {
                NUMPROC = atoi(optarg);
            }
            else
            {
                fprintf(stderr, "Uso: %s [-p NUMPROC]\n Lee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n -p NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
                exit(EXIT_FAILURE);
            }

            break;
        case 'h':
        default:
            fprintf(stderr, "Uso: %s [-p NUMPROC]\n Lee de la entrada estándar una secuencia de líneas conteniendo órdenes para ser ejecutadas y lanza cada una de dichas órdenes en un proceso diferente.\n -p NUMPROC Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    // Buffer
    char buffer[16];

    ////////////////////////////////////////////////////////////////
    //                                                            //
    //  Array bidimensional donde se van a guardar los comandos.  //
    //                                                            //
    ////////////////////////////////////////////////////////////////

    char **array_lines = malloc(sizeof(char *) * NUMPROC);
    if (array_lines == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < NUMPROC; i++)
    {
        array_lines[i] = malloc(sizeof(char) * 128);
        if (array_lines[i] == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
    }

    int num_comandos = 0;
    int fila = 0;
    int colum = 0;
    int num_leidos = 1;

    while (num_leidos > 0)
    {
        num_leidos = read_all(STDIN_FILENO, buffer, 16);

        if (num_leidos == -1)
        {
            perror("read()");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < num_leidos; i++)
        {
            if (buffer[i] != '\n')
            {
                array_lines[fila][colum] = buffer[i];
                colum++;

                if (colum > MAX_BYTES)
                {
                    fprintf(stderr, "Error: La línea no puede contener más de 128 bytes.\n");

                    // Libero memorias dinámicas

                    for (int i = 0; i < NUMPROC; i++)
                    {
                        free(array_lines[i]);
                    }
                    free(array_lines);

                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                array_lines[fila][colum] = '\0';

                if (num_comandos == NUMPROC)
                {
                    wait(NULL);
                    num_comandos--;
                }

                num_comandos++;
                char **arguments = extract_arguments(array_lines[fila]);
                execvp_process(arguments);

                fila++;

                colum = 0;

                if (fila == NUMPROC)
                {
                    fila = 0;
                }
            }
        }
    }

    for (int i = 0; i < num_comandos; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait()");
            exit(EXIT_FAILURE);
        }
    }

    // Libero memorias dinámicas

    for (int i = 0; i < NUMPROC; i++)
    {
        free(array_lines[i]);
    }
    free(array_lines);

    return EXIT_SUCCESS;
}
