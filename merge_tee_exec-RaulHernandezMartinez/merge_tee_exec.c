#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>


#define MAX_BUF_LENGTH 134217728
#define MIN_BUF_LENGTH 1
#define DEFAULT_BUFSIZE 1024
#define MIN_NUMPROC 1
#define MAX_NUMPROC 8
#define MAX_FILES 16

void mostrarSalida()
{
    fprintf(stderr, "No se admite lectura de la entrada standar\n");
    fprintf(stderr, "-t BUFSIZE Tamaño de buffer donde 1 <= BUFSIZE <= 128MB\n");
    fprintf(stderr, "-l Nombre del archivo de log\n");
    fprintf(stderr, "-p Número de procesos en ejecución de forma simultánea (1 <= NUMPROC <= 8)\n");
}

int main(int argc, char *argv[]){

    int opt;
    char * logfile = NULL;
    char * buf_size = NULL;
    char * NUMPROC = NULL;
    int fdout;

    while ((opt = getopt(argc, argv, "l:t:p:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            buf_size = optarg;
            break;
        case 'l':
            logfile = optarg;
            break;
        case 'p':
            NUMPROC = optarg;
            break;
        case 'h':
        default:
            fprintf(stderr, "Uso: %s -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
            mostrarSalida();
            exit(EXIT_FAILURE);
        }
    }

    int nfiles = argc - optind;

    if (buf_size != NULL && (atoi(buf_size) > MAX_BUF_LENGTH || atoi(buf_size) < MIN_BUF_LENGTH))
    {
        fprintf(stderr, "ERROR: Wrong buffer size.\n");
        fprintf(stderr, "Uso: %s -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    if (NUMPROC != NULL && (atoi(NUMPROC) > MAX_NUMPROC || atoi(NUMPROC) < MIN_NUMPROC))
    {
        fprintf(stderr, "ERROR: Número de NUMPROC incorrecto.\n");
        fprintf(stderr, "Uso: %s -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    if (logfile != NULL)
    {
        fdout = open(logfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fdout == -1)
        {
            perror("open(logfile)");
            exit(EXIT_FAILURE);
        }
    }
    else{
        fprintf(stderr, "ERROR: No hay fichero de log\n");
        fprintf(stderr, "Uso: %s -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    if (optind == argc)
    {
        fprintf(stderr, "ERROR: No hay ficheros de entrada\n");
        fprintf(stderr, "Uso: %s -l LOGFILE [-t BUFSIZE] [-p NUMPROC] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    if (nfiles > MAX_FILES)
    {
        fprintf(stderr, "ERROR: Has superado el numero de ficheros maximo. Numero maximo es 16.\n");
        fprintf(stderr, "Uso: %s [-t BUF_SIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    /* merge_files | tee | exec_lines */

    pid_t pid;

    int tuberia_izq[2];

    if (pipe(tuberia_izq) == -1) 
    {
        perror("pipe()");
        exit(EXIT_FAILURE);
    }

    switch (fork())
    {
    case -1:
        perror("fork(1)");
        exit(EXIT_FAILURE);
        break;
    case 0: /* Hijo izquierdo de la tubería */
        if (close(tuberia_izq[0]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }

        if (dup2(tuberia_izq[1], STDOUT_FILENO) == -1)
        {
            perror("dup2(1)");
            exit(EXIT_FAILURE);
        }
    
        if (close(tuberia_izq[1]) == -1)
        {
            perror("close(2)");
            exit(EXIT_FAILURE);
        }
        
        char ** argumentos;
        if (buf_size == NULL)
        {
            if ((argumentos = (char **)malloc((nfiles + 3) * sizeof(char *))) == NULL)
            {
                perror("malloc()");
                exit(EXIT_FAILURE);
            }

            argumentos[0] = "./merge_files";
            for (int i = 0 ; i < nfiles ; i++)
            {
                argumentos[i+1] = argv[i+optind];
            }
            argumentos[nfiles+2] = NULL;

        }
        else
        {
            if ((argumentos = (char **)malloc((5 + nfiles) * sizeof(char *))) == NULL)
            {
                perror("malloc()");
                exit(EXIT_FAILURE);
            }

            argumentos[0] = "./merge_files";
            argumentos[1] = "-t";
            argumentos[2] = buf_size;

            for (int i = 0; i < nfiles; i++)
            {
                printf("Fichero: %s\n", argv[i+optind]);
                argumentos[i+3] = argv[i+optind];
            }
            argumentos[3+nfiles] = NULL;
        }

        execvp("./merge_files", argumentos);
        perror("execlp(merge_files)");
        exit(EXIT_FAILURE);
        break;
    default:
        break;
    }

    int tuberia_der[2];

    if (pipe(tuberia_der) == -1)
    {
        perror("pipe(der)");
        exit(EXIT_FAILURE);
    }
    
    switch (fork())
    {
    case -1:
        perror("fork(2)");
        exit(EXIT_FAILURE);
        break;
    case 0: /* Hijo derecho de la tubería  */
        if (close(tuberia_izq[1]) == -1)
        {
            perror("close(3)");
            exit(EXIT_FAILURE);
        }
    
        if (dup2(tuberia_izq[0], STDIN_FILENO) == -1)
        {
            perror("dup2(2)");
            exit(EXIT_FAILURE);
        }
        
        if (close(tuberia_izq[0]) == -1)
        {
            perror("close(4)");
            exit(EXIT_FAILURE);
        }
        
        if (close(tuberia_der[0]) == -1)
        {
            perror("close(1)");
            exit(EXIT_FAILURE);
        }
        
        if (dup2(tuberia_der[1], STDOUT_FILENO) == -1)
        {
            perror("dup2(1)");
            exit(EXIT_FAILURE);
        }
        
        if (close(tuberia_der[1]) == -1)
        {
            perror("close(2)");
            exit(EXIT_FAILURE);
        }
        
        execlp("tee", "tee", logfile, NULL);
        perror("execlp(tee)");
        exit(EXIT_FAILURE);
        break;
    default: /* El proceso padre continúa... */
        break;
    }

    if (close(tuberia_izq[0]) == -1)
    {
        perror("close(pipefds[0])");
        exit(EXIT_FAILURE);
    }
    if (close(tuberia_izq[1]) == -1)
    {
        perror("close(pipefds[1])");
        exit(EXIT_FAILURE);
    }

    switch (fork())
    {
    case -1:
        perror("fork(3)");
        exit(EXIT_FAILURE);
        break;
    case 0: /* Hijo derecho de la tubería DERECHA */
        if (close(tuberia_der[1]) == -1)
        {
            perror("close(3)");
            exit(EXIT_FAILURE);
        }
        
        if (dup2(tuberia_der[0], STDIN_FILENO) == -1)
        {
            perror("dup2(2)");
            exit(EXIT_FAILURE);
        }
        
        if (close(tuberia_der[0]) == -1)
        {
            perror("close(4)");
            exit(EXIT_FAILURE);
        }

        char **argumentos;
        if (NUMPROC == NULL)
        {
            if ((argumentos = (char **)malloc(2 * sizeof(char *))) == NULL)
            {
                perror("malloc()");
                exit(EXIT_FAILURE);
            }

            argumentos[0] = "./exec_lines";
            argumentos[1] = NULL;
            
        }
        else
        {
            if ((argumentos = (char **)malloc(4 * sizeof(char *))) == NULL)
            {
                perror("malloc()");
                exit(EXIT_FAILURE);
            }

            argumentos[0] = "./exec_lines";
            argumentos[1] = "-p";
            argumentos[2] = NUMPROC;
            argumentos[3] = NULL;
        }

        execvp("./exec_lines", argumentos);
        perror("execlp(mystrings)");
        exit(EXIT_FAILURE);
        break;
    default:
        break;
    }

    if (close(tuberia_der[0]) == -1)
    {
        perror("close(pipefds_der[0])");
        exit(EXIT_FAILURE);
    }
    if (close(tuberia_der[1]) == -1)
    {
        perror("close(pipefds_der[1])");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < 3; i++)
    {
        if (wait(NULL) == -1)
        {
            perror("wait(i)");
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);

}