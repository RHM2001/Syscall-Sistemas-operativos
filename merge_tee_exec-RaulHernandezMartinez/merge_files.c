#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define MAX_BUF_LENGTH 134217728
#define MIN_BUF_LENGTH 1
#define MAX_FILES 16
#define DEFAULT_BUFSIZE 1024

void mostrarSalida()
{
    fprintf(stderr, "No se admite lectura de la entrada standar\n");
    fprintf(stderr, "-t BUFSIZE Tamañoo de buffer donde 1 <= BUFSIZE <= 128MB\n");
    fprintf(stderr, "-o FILEOUT Usa FILEOUT en lugar de la salida estandar\n");
}

////////////////////////////
//                        //
//  Escrituras parciales  //
//                        //
////////////////////////////

int write_all(int fd, char *buf, ssize_t size)
{
    ssize_t num_written = 0;
    ssize_t num_left = size;
    char *buf_left = buf;

    while ((num_left > 0) && (num_written = write(fd, buf_left, num_left)) != -1)
    {
        buf_left += num_written;
        num_left -= num_written;
    }

    return num_written == -1 ? -1 : size;
}

//////////////////////////
//                      //
//  Lecturas parciales  //
//                      //
//////////////////////////

int read_all(int fd, char *buf, ssize_t size, int *nums_read, int i)
{
    ssize_t num_r = 0;
    ssize_t num_left = size;
    char *buf_left = buf;
    nums_read[i] = 0;

    while ((num_left > 0) && (num_r = read(fd, buf_left, num_left)) != 0)
    {
        buf_left += num_r;
        num_left -= num_r;
        nums_read[i] += num_r;
    }

    return nums_read[i];
}

/* FUNCION QUE ME DICE SI TENGO QUE SALIR SALIR DEL BUCLE DE MEZCLA */
int salir(int *nums_read, int n)
{
    for (int i = 0; i < n; i++)
    {

        if (nums_read[i] != 0)
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int opt;
    char *fileout = NULL;
    int fdout;

    int buf_size = DEFAULT_BUFSIZE;

    optind = 1;

    while ((opt = getopt(argc, argv, "t:o:h")) != -1)
    {
        switch (opt)
        {
        case 't':
            buf_size = atoi(optarg);
            break;
        case 'o':
            fileout = optarg;
            break;
        case 'h':
        default:
            fprintf(stderr, "Uso: %s [-t BUF_SIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
            mostrarSalida();
            exit(EXIT_FAILURE);
        }
    }

    int nfiles = argc - optind;

    /* COMPROBACION DEL CORRECTO TAMAÑO DEL BUFFER */
    if (buf_size > MAX_BUF_LENGTH || buf_size < MIN_BUF_LENGTH)
    {
        fprintf(stderr, "ERROR: Wrong buffer size.\n");
        fprintf(stderr, "Uso: %s [-t BUF_SIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    // Se abre el fichero de salida.
    if (fileout != NULL)
    {
        fdout = open(fileout, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (fdout == -1)
        {
            perror("open(fileout)");
            exit(EXIT_FAILURE);
        }
    }
    // Si es NULL, la salida es la estandar.
    else
    {
        fdout = STDOUT_FILENO;
    }

    // Se comprueba que haya al menos un fichero de entrada.
    if (optind == argc)
    {
        fprintf(stderr, "ERROR: No hay ficheros de entrada\n");
        fprintf(stderr, "Uso: %s [-t BUF_SIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    // Número máximo de ficheros 16.
    if (nfiles > MAX_FILES)
    {
        fprintf(stderr, "ERROR: Has superado el numero de ficheros maximo. Numero maximo es 16.\n");
        fprintf(stderr, "Uso: %s [-t BUF_SIZE] [-o FILEOUT] FILEIN1 [FILEIN2 ... FILEINn]\n", argv[0]);
        mostrarSalida();
        exit(EXIT_FAILURE);
    }

    // Memoria de los descriptores de fichero de los argumentos.
    int *fdins;
    if ((fdins = malloc(nfiles * sizeof(int))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    // Se inicilaizan los descriptores con la comprobación de los ficheros.
    int err = 0;
    for (int i = 0; i < nfiles; i++)
    {
        int in = open(argv[i + optind], O_RDONLY);
        if (in == -1)
        {
            perror("open(filein)");
            err++;
            continue;
        }
        else
        {
            fdins[i - err] = in;
        }
    }

    nfiles -= err;

    // Memoria dinamica para los buffers de lectura por fichero.
    char **inter_buffers;
    if ((inter_buffers = (char **)malloc(nfiles * sizeof(char *))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < nfiles; i++)
    {
        if ((inter_buffers[i] = (char *)malloc(buf_size * sizeof(char))) == NULL)
        {
            perror("malloc()");
            exit(EXIT_FAILURE);
        }
    }

    // Reservo memoría para saber los bytes leidos.
    int *nums_read;
    int num_written;
    if ((nums_read = (int *)malloc(nfiles * sizeof(int))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    // Inicializo a -1.

    for (int i = 0; i < nfiles; i++)
    {
        nums_read[i] = -1;
    }

    // Buffer de salida.
    char *buf_salida;
    if ((buf_salida = (char *)malloc(buf_size * sizeof(char))) == NULL)
    {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < nfiles; i++)
    {
        if (fdins[i] > 0)
        {
            memset(inter_buffers[i], 0, buf_size);
            nums_read[i] = read_all(fdins[i], inter_buffers[i], buf_size, nums_read, i);
            if (nums_read[i] == -1)
            {
                perror("read(fdin)");
                exit(EXIT_FAILURE);
            }
        }
    }

    int writeind = 0;
    int ind_buf = 0;
    int ind_byte = 0;
    int *indices = calloc(nfiles, sizeof(int));

    int i = 0;
    while (!salir(nums_read, nfiles))
    {
        if (nums_read[i] > 0)
        {
            for (int j = indices[i]; j < nums_read[i]; j++)
            {
                if (writeind < buf_size)
                {
                    buf_salida[writeind] = inter_buffers[i][j];
                    writeind++;
                    if (inter_buffers[i][j] == '\n')
                    {

                        if (j + 1 < nums_read[i])
                        {
                            indices[i] = j + 1;
                        }
                        else
                        {
                            memset(inter_buffers[i], 0, buf_size);
                            nums_read[i] = read_all(fdins[i], inter_buffers[i], buf_size, nums_read, i);
                            indices[i] = 0;
                        }
                        i = (i + 1) % nfiles;

                        break;
                    }
                    else
                    {
                        if (j == nums_read[i] - 1 && nums_read[i] != buf_size)
                        {
                            memset(inter_buffers[i], 0, buf_size);
                            nums_read[i] = read_all(fdins[i], inter_buffers[i], buf_size, nums_read, i);
                            i = (i + 1) % nfiles;
                            break;
                        }
                        if (j == nums_read[i] - 1)
                        {
                            memset(inter_buffers[i], 0, buf_size);
                            nums_read[i] = read_all(fdins[i], inter_buffers[i], buf_size, nums_read, i);
                            indices[i] = 0;
                            break;
                        }
                    }
                }
                else
                {
                    if (j < buf_size)
                    {
                        indices[i] = j;
                    }
                    else
                    {
                        memset(inter_buffers[i], 0, buf_size);
                        nums_read[i] = read_all(fdins[i], inter_buffers[i], buf_size, nums_read, i);
                        indices[i] = 0;
                    }

                    break;
                }
            }
        }
        else
        {
            i = (i + 1) % nfiles;
        }

        if (writeind == buf_size || salir(nums_read, nfiles))
        {
            num_written = write_all(fdout, buf_salida, writeind);
            if (num_written == -1)
            {
                perror("write(fdin)");
                exit(EXIT_FAILURE);
            }
            memset(buf_salida, 0, buf_size);
            writeind = 0;
        }
    }

    for (int i = 0; i < nfiles; i++)
    {
        if (close(fdins[i]) == -1)
        {
            perror("close(fdins[i])");
            exit(EXIT_FAILURE);
        }
    }

    if (fdout != STDOUT_FILENO)
    {
        if (close(fdout) == -1)
        {
            perror("close(fdout)");
            exit(EXIT_FAILURE);
        }
    }

    free(fdins);
    for (int i = 0; i < nfiles; i++)
    {
        free(inter_buffers[i]);
    }
    free(inter_buffers);
    free(nums_read);
    free(buf_salida);
    free(indices);

    exit(EXIT_SUCCESS);
}