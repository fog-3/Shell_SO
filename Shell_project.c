/**
UNIX Shell Project
Fernando Osuna Granados
Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */

#include <dirent.h>
#include <stdio.h>

void traverse_proc(void) {
    DIR *d; 
    struct dirent *dir;
    char buff[2048];
    d = opendir("/proc");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            sprintf(buff, "/proc/%s/stat", dir->d_name); 
            FILE *fd = fopen(buff, "r");
            if (fd){
                long pid;     // pid
                long ppid;    // ppid
                char state;   // estado: R (runnable), S (sleeping), T(stopped), Z (zombie)

                // La siguiente línea lee pid, state y ppid de /proc/<pid>/stat
                fscanf(fd, "%ld %s %c %ld", &pid, buff, &state, &ppid);
                fclose(fd);
				if (state == 'Z'){
					printf("%ld\n", pid);
				}
            }
        }
        closedir(d);
    }
}

static void parse_redirections(char **args,  char **file_in, char **file_out){
    *file_in = NULL;
    *file_out = NULL;
    char **args_start = args;
    while (*args) {
        int is_in = !strcmp(*args, "<");
        int is_out = !strcmp(*args, ">");
        if (is_in || is_out) {
            args++;
            if (*args){
                if (is_in)  *file_in = *args;
                if (is_out) *file_out = *args;
                char **aux = args + 1;
                while (*aux) {
                   *(aux-2) = *aux;
                   aux++;    
                }
                *(aux-2) = NULL;
                args--;
            } else {
                /* Syntax error */
                fprintf(stderr, "syntax error in redirection\n");
                args_start[0] = NULL; // Do nothing
            }
        } else {
            args++; 
        }
    }
    // Debug:
    // *file_in && fprintf(stderr, "[parse_redirections] file_in='%s'\n", *file_in);
    // *file_out && fprintf(stderr, "[parse_redirections] file_out='%s'\n", *file_out);
}

job *tareas;

void manejador2 (int signal){
	FILE *fp = fopen("hup.txt", "a");// abre un fichero en modo 'append'
	if (!fp)
		perror("Error abriendo fichero");
	else 
	{
		fprintf(fp, "SIGHUP recibido.\n"); // escribe en el fichero
		fclose(fp);
	}
}

void manejador (int signal){
	// Recorra la lista -> waitpid NO BLOQUEANTE y procederm
	// MANEJADOR DE SIGCHLD ->
	// recorrer todos los jobs en bg y suspendidos a ver
	// qué les ha pasado: 
	// SI MUERTOS-> quitar de la lista
	// SI CAMBIAN DE ESTADO -> cambiar el job correspondienteç

	job * item;
	int status;
	int info;
	int pid_wait = 0;
	enum status status_res;

	for (int i = 1; i <= list_size(tareas); i++)
	{
		item = get_item_bypos(tareas, i);
		pid_wait = waitpid(item->pgid, &status, WUNTRACED | WNOHANG);
		if (pid_wait == item->pgid) // A este jobs le ha pasado algo
		{
			status_res = analyze_status(status, &info);
			// - EXITED
             // - SIGNALED
             // - SUSPENDED
             // - CONTINUED
			printf("[SIGCHLD] Signal: %s, info: %d, background job: %s, pid=%i\n", 
			status_strings[status_res], info, item->command, pid_wait);
			if (status_res == EXITED || status_res == SIGNALED){
				delete_job(tareas, item);
				i--;
			}
			else if (status_res == SUSPENDED){
				item->state = STOPPED;
			}
			else if (status_res == CONTINUED)
				item->state = BACKGROUND;
		}
	}
}

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */

	job * item; // Para almacenar un nuevo job

	ignore_terminal_signals(); // Ignora ^Z ^C
	
	signal(SIGHUP, manejador2);
	signal(SIGCHLD, manejador); // Instalamos manejador SIGCHLD
	tareas = new_list("Tareas");

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		
		char *file_in = NULL;
		char *file_out = NULL;
		parse_redirections(args, &file_in, &file_out);

		if (file_in)
			printf("Redirection <: '%s'\n", file_in);
		if (file_out)
			printf("Redirection >: '%s'\n", file_out);

		if(args[0]==NULL) continue;   // if empty command

		if (! strcmp(args[0], "hola")){
			printf("Hello world!!!\n");
			continue;
		}
		/*** comandos internos ***/
		/* cd */ /* logout */ /* jobs */ /* fg */ /* bg */

		if(! strcmp(args[0], "cd")){
			int err;
			if (args[1] == NULL)
				err = chdir(getenv("HOME"));
			else
				err = chdir(args[1]);
			if (err)
				fprintf(stderr, "Error en chdir\n");
			continue;
		}
		
		if (! strcmp(args[0], "jobs")){
			block_SIGCHLD();
			print_job_list(tareas);
			unblock_SIGCHLD();
			continue;
		}

		if(! strcmp(args[0], "fg")){
			// Obtenemos la posición del job
			int pos = 1;
			if (args[1] != NULL){
				pos = atoi(args[1]);
				if (pos <= 0){
					fprintf(stderr, "Error in the argument of fg\n");
					continue;
				}
			}

			// Accedemos a la lista
			block_SIGCHLD();
			job *job = get_item_bypos(tareas, pos);
			unblock_SIGCHLD();

			if (job == NULL){
				fprintf(stderr, "Job not found\n");
				continue;
			}

			// Cedemos la terminal
			set_terminal(job->pgid);

			if (job->state == STOPPED){
				// Mandamos una señal
				killpg(job->pgid, SIGCONT);
			}

			pid_wait = waitpid(job->pgid, &status, WUNTRACED);
			status_res = analyze_status(status, &info);

			if (status_res == EXITED || status_res == SIGNALED){
				printf("The child ended with the signal: %d\n", info);
				block_SIGCHLD();
				delete_job(tareas, job);
				unblock_SIGCHLD();
			}

			if (status_res == SUSPENDED){
				printf("Child in fg suspended\n");
				block_SIGCHLD();
				job->state = STOPPED;
				unblock_SIGCHLD();
			}

			set_terminal(getpid()); // Shell recupera el terminal
			continue;
		}

		if (! strcmp(args[0], "bg")){
			
			// Obtenemos la posición del job
			int pos = 1;
			if (args[1] != NULL) {
				pos = atoi(args[1]);
				if (pos <= 0) {
					fprintf(stderr, "Error int the argument of bg\n");
					continue;
				}
			}
			
			// Accedemos a la lista
			block_SIGCHLD();
			job *job = get_item_bypos(tareas, pos);
			unblock_SIGCHLD();
			
			if (job == NULL) {
				fprintf(stderr, "Job not found\n");
				continue;
			}
			
			block_SIGCHLD();
			if (job->state == STOPPED) {
				job->state = BACKGROUND;
				// Mandamos una señal de grupo para reanudar
				killpg(job->pgid, SIGCONT);
			}
			unblock_SIGCHLD();
			
			continue;
		}

		if (!strcmp(args[0], "deljob")){
			job *item = get_item_bypos(tareas, 1);
			if (!item){
				printf("No hay trabajo actual\n");
			}
			else if (item->state == STOPPED){
				printf("No se permiten borrar trabajos en segundo plano suspendidos\n");
			}
			else if (item->state == BACKGROUND){
				printf("Borrando trabajo actual de la lista de jobs: PID=%d command=%s\n", item->pgid, item->command);
				delete_job(tareas, item);
			}
			continue;
		}

		if (!strcmp(args[0], "currjob")){
			job *item = get_item_bypos(tareas, 1);
			if (!item){
				fprintf(stderr, "No hay trabajo actual");
			}else{
				printf("Trabajo actual: PID=%d command=%s\n", item->pgid, item->command);
			}
			continue;
		}

		if (!strcmp(args[0], "zjobs")){
			traverse_proc();
			continue;
		}

		if (!strcmp(args[0], "logout")){
			exit(0);
		}

		/*** comandos externos ***/

		pid_fork = fork();
		
		if (pid_fork > 0){ //Proceso Padre
			// PADRE -> Shell
			new_process_group(pid_fork); // Hijo -> fuera

			if (background == 0){
				set_terminal(pid_fork);
				pid_wait = waitpid(pid_fork, &status, WUNTRACED);
				
				status_res = analyze_status(status, &info);
				if (status_res == SUSPENDED){
					item = new_job(pid_fork, args[0], STOPPED);
					block_SIGCHLD(); 
					add_job(tareas, item);
					unblock_SIGCHLD(); 
					printf("\nForeground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0], status_strings[status_res], info);

				} else {
					if (info != 255){
						printf("\nForeground pid: %d, command: %s, %s, info: %d\n", pid_fork, args[0], status_strings[status_res], info);
					}
				}

				set_terminal(getpid());
			} else {
				// Nuevo nodo job -> nuevo job BACKGROUND
				item = new_job(pid_fork, args[0], BACKGROUND);
				block_SIGCHLD(); // enmascarar sigchld (sección libre de sigchld)
				add_job(tareas, item);
				unblock_SIGCHLD();
				printf("\nBackground job running... pid: %d, command: %s\n", pid_fork, args[0]);
			}
			set_terminal(getpid());
		} else if (pid_fork == 0){
			// HIJO
			new_process_group(getpid()); // hijo -> me voy
			if (!background){
				set_terminal(getpid()); // ceder el terminal al hijo (solo la entrada)
			}
			restore_terminal_signals();
			
			if (file_in){
				FILE *f = fopen(file_in, "r");
				if (!f){
					perror("Error abriendo el archivo de entrada");
					exit(EXIT_FAILURE);
				}
				dup2(fileno(f), STDIN_FILENO);
				fclose(f);
			}

			if (file_out){
				FILE *f = fopen(file_out, "w");
				if (!f){
					perror("Error abriendo el archivo de salida");
					exit(EXIT_FAILURE);
				}
				dup2(fileno(f), STDOUT_FILENO);
				fclose(f);
			}
			
			execvp(args[0], args);
			fprintf(stderr, "Error, command not found: %s\n", args[0]);
			exit(-1);
		} else {
			fprintf(stderr, "Error in fork\n");
			continue;
		}
		/* the steps are:
			 (1) fork a child process using fork()
			 (2) the child process will invoke execvp()
			 (3) if background == 0, the parent will wait, otherwise continue 
			 (4) Shell shows a status message for processed command 
			 (5) loop returns to get_commnad() function
		*/

	} // end while
}
