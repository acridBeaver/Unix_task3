#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define work_dir "/"

struct Task {
    pid_t pid;
    char * finput; char * foutput;
    int args_len; char ** args;
};

struct TaskArray {
    int length; struct Task ** array;
};

int SIGNALRESTART, SIGNALFINISH = 0;
char *config_file, *log_file = NULL;
struct TaskArray * task_array = NULL;

bool is_absolute_path(char * path) {
    return strlen(path) > 0 && path[0] == '/';
}

void add_task(struct TaskArray *tasks, struct Task *task) {
    struct Task ** array = (struct Task **) realloc(
        tasks -> array, 
        sizeof(struct Task *) * ((tasks -> length) + 1)
        );
    if (array == NULL) {
        fprintf(stderr, "ERROR: can't memory allocation\n");
        exit(EXIT_FAILURE);
    }
    tasks -> array = array;
    tasks -> array[tasks -> length++] = task;
}

struct Task * create_task() {
    struct Task * task = (struct Task * ) malloc(sizeof(struct Task));
    if (task == NULL) {
        fprintf(stderr, "ERROR: can't memory allocation\n");
        exit(EXIT_FAILURE);
    }
    task -> pid = 0; task -> finput = NULL; task -> foutput = NULL; task -> args_len = 0; task -> args = NULL;
    return task;
};

void free_task(struct Task * task) {
    if (task -> finput != NULL)
        free(task -> finput);
    if (task -> foutput != NULL)
        free(task -> foutput);
    for (int i = 0; i < task -> args_len; i++) {
        if (task -> args[i] != NULL)
            free(task -> args[i]);
    }
    if (task -> args != NULL)
        free(task -> args);
    free(task);
}

void free_task_array(struct TaskArray * task_array) {
    for (int i = 0; i < task_array -> length; i++)
        if (task_array -> array[i] != NULL)
            free_task(task_array -> array[i]);
    if (task_array -> array != NULL)
        free(task_array -> array);
    task_array -> length = 0;
    free(task_array);
}

struct TaskArray * creat_task_array() {
    struct TaskArray * new_task_array = (struct TaskArray * ) malloc(sizeof(struct TaskArray));
    new_task_array -> length = 0;
    new_task_array -> array = NULL;
    return new_task_array;
}

char * read_line(int fd) {
    int len = 0;
    char * array = NULL;
    char * new_char;
    
    while (true) {
        new_char = malloc(1);
        int r_bytes = read(fd, new_char, 1);
        if (r_bytes == 0)
            break;
        if (array == NULL)
            array = malloc(1);
        else
            array = (char *) realloc(array, len + 1);
        if (new_char[0] == '\n') {
            array[len++] = '\0';
            break;
        }
        else
            array[len++] = new_char[0];
        free(new_char);
    }
    if (new_char != NULL)
        free(new_char);
    return array;
}

void add_string(char * string, char ** * array, int * length) {
    char ** new_arr = (char ** ) realloc( * array, sizeof(char * ) * (( * length) + 1));
    if (new_arr == NULL) {
        fprintf(stderr, "ERROR: can't memory alloc\n");
        exit(1);
    }
    * array = new_arr;
    if (string == NULL)
        ( * array)[( * length) ++] = NULL;
    else
        ( * array)[( * length) ++] = strdup(string);
}

void split_string(char * string, char ** * array, int * length) {
    * array = NULL;
    * length = 0;
    char * arg = string;

    while (true) {
        if ( * arg == '\0')
            break;
        char * next_arg = strchr(arg, ' ');
        if (next_arg == NULL) {
            add_string(arg, array, length);
            break;
        }
        * next_arg = '\0';
        add_string(arg, array, length);
        if (next_arg == NULL)
            break;
        arg = next_arg + 1;
    }
    add_string(NULL, array, length);
}

struct TaskArray * read_config() {
    struct TaskArray * task_array = creat_task_array();
    int fd = open(config_file, O_RDONLY);

    
    while (true) {
        char * line = read_line(fd);
        if (line == NULL)
            break;
        fprintf(stderr, "INFO: myinit find task!: '%s'\n", line);
        struct Task * task = create_task();
        split_string(line, & task -> args, & task -> args_len);
        free(line);
        if (task -> args_len < 3) {
            fprintf(stderr, "ERROR: Task definition error: less than three arguments\n");
            free_task(task);
            continue;
        }
        int indexes[] = { 0, task -> args_len - 3,  task -> args_len - 2 };
        for (int i = 0; i < 3; i++)
        if (!is_absolute_path(task -> args[indexes[i]])) {
            fprintf(
                stderr, 
                "ERROR: '%s' - not absolute path:(\n", 
                task -> args[indexes[i]]
                );
            free_task(task);
            continue;
        }

        task -> finput = strdup(task -> args[task -> args_len - 3]); task -> foutput = strdup(task -> args[task -> args_len - 2]);
        free(task -> args[task -> args_len - 3]); task -> args[task -> args_len - 3] = NULL;
        free(task -> args[task -> args_len - 2]); task -> args[task -> args_len - 2] = NULL;
        add_task(task_array, task);
    }
    
    return task_array;
}

int dup_fd(char * file_name, int flags, mode_t mode, int new_fd) {
    int fd = open(file_name, flags, mode);
    close(fd);
    return 0;
}

void start_task(int task_index, struct Task * task) {

    pid_t pid = fork();


    
    if (pid == -1) {
        fprintf(stderr, "ERROR: Ошибка запуска задачи под номером %d\n", task_index + 1);
        return;
    }
    
    if (pid != 0) {
        task -> pid = pid;
        fprintf(stderr, "INFO: Запущена задача под номером %d\n", task_index + 1);
        return;
    }

    if (
        dup_fd(task -> finput, O_RDONLY, 0600, 0) == -1 ||
        dup_fd(task -> foutput, O_CREAT | O_TRUNC | O_WRONLY, 0600, 1) == -1 ||
        dup_fd("/dev/null", O_CREAT | O_APPEND | O_WRONLY, 0600, 2) == -1
        ) 
        exit(EXIT_FAILURE);

    char ** args = task -> args;
    execv(args[0], args);
    exit(0);
}

void stop_tasks(struct TaskArray * task_array) {

    for (int i = 0; i < task_array -> length; i++) {
        pid_t pid = task_array -> array[i] -> pid;
        if (pid != 0)
            kill(pid, SIGTERM);
    }
    
    int task_count = 0;
    for (int i = 0; i < task_array -> length; i++)
        if (task_array -> array[i] -> pid != 0)
            task_count++;
    
    while (task_count > 0) {
        int task_status = 0;
        pid_t task_pid = waitpid(-1, & task_status, 0);
        if (task_pid == -1) 
            continue;
        
        int task_index = 0;
        while (task_index < task_array -> length)
        {
            if (task_array -> array[task_index] -> pid == task_pid)
                break;
            task_index++;
        }
        if (WIFEXITED(task_status)) {
            fprintf(stderr, "INFO: task number %d finished with code %d\n",
                task_index + 1, WEXITSTATUS(task_status));
        } else if (WIFSIGNALED(task_status)) {
            fprintf(stderr, "INFO: task number %d finished with signal code %d\n",
                task_index + 1, WTERMSIG(task_status));
        }
        task_count--;
    }
}



void run_myinit(struct TaskArray * task_array) {
    while (true) {
        int task_status = 0;
        pid_t task_pid = waitpid(-1, & task_status, 0);
        
        if (task_pid == -1) {
            if (SIGNALRESTART || SIGNALFINISH)
                break;
            continue;
        }

        int task_index = 0;
        while (task_index < task_array -> length)
        {
            if (task_array -> array[task_index] -> pid == task_pid)
                break;
            task_index++;
        }
        if (WIFEXITED(task_status)) {
            fprintf(stderr, "INFO: task number %d finished with code %d\n",
                task_index + 1, WEXITSTATUS(task_status));
        } else if (WIFSIGNALED(task_status)) {
            fprintf(stderr, "INFO: task number %d finished with signal code %d\n",
                task_index + 1, WTERMSIG(task_status));
        }

        
        task_array -> array[task_index] -> pid = 0;
        
        if (SIGNALRESTART || SIGNALFINISH)
            break;
        
        start_task(task_index, task_array -> array[task_index]);
    }
}

void signal_handler(int s) {
    switch (s) {
        case 1:
            SIGNALRESTART = 1;
            fprintf(stderr, "INFO: myinit caught signal SIGHUP\n");
            break;
        case 2:
            SIGNALFINISH = 1;
            fprintf(stderr, "INFO: myinit caught signal SIGINT\n", s);
            break;
        case 15:
            exit(15);
        default:
            SIGNALFINISH = 1;
            fprintf(stderr, "WARN: myinit caught unknown: %d\n", s);
            break;
    }
}

int main(int argc, char * argv[]) {
    if (argc != 3) {
        fprintf(stderr, "ERROR: Run this like: %s <config_file> <log_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    config_file = (char *) malloc(strlen(argv[1]) + 1);
    strcpy(config_file, argv[1]);
    log_file = (char *) malloc(strlen(argv[2]) + 1);
    strcpy(log_file, argv[2]);
        
    pid_t pid = fork();

    if (pid != 0)
        return 0;
    
    pid_t group_id = setsid();

    struct rlimit fd_limit;
    getrlimit(RLIMIT_NOFILE, &fd_limit);
    for (int fd = 0; fd < fd_limit.rlim_max; fd++)
        close(fd);

    int fd = open(log_file, O_CREAT | O_TRUNC | O_WRONLY, 0600);
    if (fd == -1 || dup2(fd, 1) == -1 || dup2(fd, 2) == -1) {
        fprintf(stderr, "ERROR: Can't touch log file:(\n");
        exit(EXIT_FAILURE);
    }
    close(fd);

    if (chdir(work_dir) == -1) {
        fprintf(stderr, "ERROR: Can't change currently directory\n");
        exit(EXIT_FAILURE);
    }

    if (signal(SIGINT, signal_handler) == SIG_ERR || signal(SIGHUP, signal_handler) == SIG_ERR || signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "ERROR: can't make signal SIGINT/SIGHUP\n");
        exit(EXIT_FAILURE);
    }
        
    fprintf(stderr, "INFO: myinit running success!\n");
    
    while (1) {
        task_array = read_config();
        for (int i = 0; i < task_array -> length; i++)
            start_task(i, task_array -> array[i]);
    
        run_myinit(task_array);
        
        if (SIGNALRESTART) {
            stop_tasks(task_array);
            SIGNALRESTART = 0;
            fprintf(stderr, "INFO: myinit re_running success!\n");
        }
        
        free_task_array(task_array);
        
        if (SIGNALFINISH) {
            SIGNALFINISH = 0;
            break;
        }
    }
void free_conf_and_log_file(char* conf_file, char* l_file){
    if (conf_file != NULL)
        free(config_file);
    if (l_file != NULL)
        free(log_file);
}
void ending_success(){
    fprintf(stderr, "INFO: myinit ending success!\n");
}

    free_conf_and_log_file(config_file, log_file);
    ending_success();   
    return 0;
}