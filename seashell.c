#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
const char * sysname = "seashell";

enum return_codes {
    SUCCESS = 0,
    EXIT = 1,
    UNKNOWN = 2,
};
struct command_t {
    char *name;
    bool background;
    bool auto_complete;
    int arg_count;
    char **args;
    char *redirects[3]; // in/out redirection
    struct command_t *next; // for piping
};
/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t * command)
{
    int i=0;
    printf("Command: <%s>\n", command->name);
    printf("\tIs Background: %s\n", command->background?"yes":"no");
    printf("\tNeeds Auto-complete: %s\n", command->auto_complete?"yes":"no");
    printf("\tRedirects:\n");
    for (i=0;i<3;i++)
        printf("\t\t%d: %s\n", i, command->redirects[i]?command->redirects[i]:"N/A");
    printf("\tArguments (%d):\n", command->arg_count);
    for (i=0;i<command->arg_count;++i)
        printf("\t\tArg %d: %s\n", i, command->args[i]);
    if (command->next)
    {
        printf("\tPiped to:\n");
        print_command(command->next);
    }


}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
    if (command->arg_count)
    {
        for (int i=0; i<command->arg_count; ++i)
            free(command->args[i]);
        free(command->args);
    }
    for (int i=0;i<3;++i)
        if (command->redirects[i])
            free(command->redirects[i]);
    if (command->next)
    {
        free_command(command->next);
        command->next=NULL;
    }
    free(command->name);
    free(command);
    return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
    char cwd[1024], hostname[1024];
    gethostname(hostname, sizeof(hostname));
    getcwd(cwd, sizeof(cwd));
    printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
    return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
    const char *splitters=" \t"; // split at whitespace
    int index, len;
    len=strlen(buf);
    while (len>0 && strchr(splitters, buf[0])!=NULL) // trim left whitespace
    {
        buf++;
        len--;
    }
    while (len>0 && strchr(splitters, buf[len-1])!=NULL)
        buf[--len]=0; // trim right whitespace

    if (len>0 && buf[len-1]=='?') // auto-complete
        command->auto_complete=true;
    if (len>0 && buf[len-1]=='&') // background
        command->background=true;

    char *pch = strtok(buf, splitters);
    command->name=(char *)malloc(strlen(pch)+1);
    if (pch==NULL)
        command->name[0]=0;
    else
        strcpy(command->name, pch);

    command->args=(char **)malloc(sizeof(char *));

    int redirect_index;
    int arg_index=0;
    char temp_buf[1024], *arg;
    while (1)
    {
        // tokenize input on splitters
        pch = strtok(NULL, splitters);
        if (!pch) break;
        arg=temp_buf;
        strcpy(arg, pch);
        len=strlen(arg);

        if (len==0) continue; // empty arg, go for next
        while (len>0 && strchr(splitters, arg[0])!=NULL) // trim left whitespace
        {
            arg++;
            len--;
        }
        while (len>0 && strchr(splitters, arg[len-1])!=NULL) arg[--len]=0; // trim right whitespace
        if (len==0) continue; // empty arg, go for next

        // piping to another command
        if (strcmp(arg, "|")==0)
        {
            struct command_t *c=malloc(sizeof(struct command_t));
            int l=strlen(pch);
            pch[l]=splitters[0]; // restore strtok termination
            index=1;
            while (pch[index]==' ' || pch[index]=='\t') index++; // skip whitespaces

            parse_command(pch+index, c);
            pch[l]=0; // put back strtok termination
            command->next=c;
            continue;
        }

        // background process
        if (strcmp(arg, "&")==0)
            continue; // handled before

        // handle input redirection
        redirect_index=-1;
        if (arg[0]=='<')
            redirect_index=0;
        if (arg[0]=='>')
        {
            if (len>1 && arg[1]=='>')
            {
                redirect_index=2;
                arg++;
                len--;
            }
            else redirect_index=1;
        }
        if (redirect_index != -1)
        {
            command->redirects[redirect_index]=malloc(len);
            strcpy(command->redirects[redirect_index], arg+1);
            continue;
        }

        // normal arguments
        if (len>2 && ((arg[0]=='"' && arg[len-1]=='"')
            || (arg[0]=='\'' && arg[len-1]=='\''))) // quote wrapped arg
        {
            arg[--len]=0;
            arg++;
        }
        command->args=(char **)realloc(command->args, sizeof(char *)*(arg_index+1));
        command->args[arg_index]=(char *)malloc(len+1);
        strcpy(command->args[arg_index++], arg);
    }
    command->arg_count=arg_index;
    return 0;
}
void prompt_backspace()
{
    putchar(8); // go back 1
    putchar(' '); // write empty over
    putchar(8); // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
    int index=0;
    char c;
    char buf[4096];
    static char oldbuf[4096];

    // tcgetattr gets the parameters of the current terminal
    // STDIN_FILENO will tell tcgetattr that it should write the settings
    // of stdin to oldt
    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    // ICANON normally takes care that one line at a time will be processed
    // that means it will return if it sees a "\n" or an EOF or an EOL
    new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
    // Those new settings will be set to STDIN
    // TCSANOW tells tcsetattr to change attributes immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);


    //FIXME: backspace is applied before printing chars
    show_prompt();
    int multicode_state=0;
    buf[0]=0;
      while (1)
      {
        c=getchar();
        // printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

        if (c==9) // handle tab
        {
            buf[index++]='?'; // autocomplete
            break;
        }

        if (c==127) // handle backspace
        {
            if (index>0)
            {
                prompt_backspace();
                index--;
            }
            continue;
        }
        if (c==27 && multicode_state==0) // handle multi-code keys
        {
            multicode_state=1;
            continue;
        }
        if (c==91 && multicode_state==1)
        {
            multicode_state=2;
            continue;
        }
        if (c==65 && multicode_state==2) // up arrow
        {
            int i;
            while (index>0)
            {
                prompt_backspace();
                index--;
            }
            for (i=0;oldbuf[i];++i)
            {
                putchar(oldbuf[i]);
                buf[i]=oldbuf[i];
            }
            index=i;
            continue;
        }
        else
            multicode_state=0;

        putchar(c); // echo the character
        buf[index++]=c;
        if (index>=sizeof(buf)-1) break;
        if (c=='\n') // enter key
            break;
        if (c==4) // Ctrl+D
            return EXIT;
      }
      if (index>0 && buf[index-1]=='\n') // trim newline from the end
          index--;
      buf[index++]=0; // null terminate string

      strcpy(oldbuf, buf);

      parse_command(buf, command);

     //  print_command(command); // DEBUG: uncomment for debugging

    // restore the old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
      return SUCCESS;
}
int process_command(struct command_t *command);
int main()
{
    while (1)
    {
        struct command_t *command=malloc(sizeof(struct command_t));
        memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

        int code;
        code = prompt(command);
        if (code==EXIT) break;

        code = process_command(command);
        if (code==EXIT) break;

        free_command(command);
    }

    printf("\n");
    return 0;
}
void init_shell_session(){
    
    char shortdirfilepath[256];
    strcat(strcpy(shortdirfilepath, getenv("HOME")), "/.shortdir");

    FILE *fptr;
    fptr = fopen(shortdirfilepath, "rb+");
    if(fptr == NULL)
    {
        fptr = fopen(shortdirfilepath, "wb");
    }
    fclose(fptr);
    
    char crontabfilepath[256];
    strcat(strcpy(crontabfilepath, getenv("HOME")), "/.crontab_music");

    FILE *fiptr;
    fiptr = fopen(crontabfilepath, "rb+");
    if(fiptr == NULL)
    {
        fiptr = fopen(crontabfilepath, "wb");
    }
    fclose(fiptr);
}

void delete_line(char* fname, int del){
    if(del==-1)
        return;
    FILE *fptr1, *fptr2;
    char tmp[256];
    strcat(strcpy(tmp, fname), "b");
    char curr;
    int line_number = 0;
    fptr1 = fopen(fname,"r");
    fptr2 = fopen(tmp, "w");
    curr = getc(fptr1);
    if(curr!=EOF) {line_number =1;}
    while(1){
      if(del != line_number)
        putc(curr, fptr2);
        curr = getc(fptr1);
        if(curr =='\n') line_number++;
        if(curr == EOF) break;
    }
    fclose(fptr1);
    fclose(fptr2);
    remove(fname);
    rename(tmp, fname);
}

int find_line_number(char* filename, char* text){
    FILE* filePointer;
    int bufferLength = 512;
    char buffer[bufferLength];
    filePointer = fopen(filename, "r");
    int lineNumber = 0;
    while(fgets(buffer, bufferLength, filePointer)) {
        if(strstr(buffer, text)!=0){
            return lineNumber;
        }
        lineNumber++;
    }
    fclose(filePointer);
    return -1;
}

int process_command(struct command_t *command)
{
    int r;
    if (strcmp(command->name, "")==0) return SUCCESS;

    if (strcmp(command->name, "exit")==0)
        return EXIT;

    if (strcmp(command->name, "cd")==0)
    {
        if (command->arg_count > 0)
        {
            r=chdir(command->args[0]);
            if (r==-1)
                printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
            
            return SUCCESS;
        }
    }
    if (strcmp(command->name, "highlight")==0)   //TODO PUNCTIOATION CASELERE BAK
    {
        if (command->arg_count > 0)
        {
            char word[20000];
            FILE *fptr = fopen(command->args[2], "r");
     

            if (fptr == NULL)
            {
                printf("Error file missing\n");
                exit(-1);
            }
            
                while(!feof(fptr))
                {
                    fscanf(fptr,"%s", word);

                    if((strcmp(command->args[0], word))==0){//if match found

                        if(strcmp(command->args[1],"r")){
                            //fseek(file_ptr, -1L, SEEK_CUR);
                            printf("\033[1;31m");
                        }
                        if(strcmp(command->args[1],"g")){
                           // fseek(file_ptr, -1L, SEEK_CUR);
                            printf("\033[0;32m");
                        }
                        if(strcmp(command->args[1],"b")){
                          //  fseek(file_ptr, -1L, SEEK_CUR);
                            printf("\033[0;34m");
                        }
                    }
                    
                    printf("%s ",word);
                    printf("\x1B[0m");
               
                    
                }

            
            return SUCCESS;
        }
    }
    if (strcmp(command->name, "goodMorning")==0)
    {
        char crontabfilepath[256];
        strcat(strcpy(crontabfilepath, getenv("HOME")), "/.crontab_music");
        if (command->arg_count > 0)
        {
            char hour[100];
            char min[100];
            char musicTime[256];
            char delim[] = ".";
            strcpy(musicTime,command->args[0]);
            char *ptr = strtok(musicTime,delim);
            strcpy(hour,ptr);
            ptr = strtok(NULL,"\n");
            strcpy(min,ptr);
            
            FILE *fptr;
            
            fptr = fopen(crontabfilepath, "a+");

            fprintf(fptr, min);
            fprintf(fptr, " ");
            fprintf(fptr, hour);
            fprintf(fptr, " *");
            fprintf(fptr, " *");
            fprintf(fptr, " * ");
            fprintf(fptr, command->args[1]);

            fclose(fptr);
            
            return SUCCESS;
        }
    }
    if (strcmp(command->name, "shortdir")==0)
        {
        char shortdirfilepath[256];
        strcat(strcpy(shortdirfilepath, getenv("HOME")), "/.shortdir");

            if (command->arg_count > 0)
            {
                if(strcmp(command->args[0], "set")==0){
                    char cwd[512];
                    getcwd(cwd, sizeof(cwd)) ;

                    char tmp[256];
                    strcat(strcpy(tmp, command->args[1]), "=");
                    delete_line(shortdirfilepath, find_line_number(shortdirfilepath, tmp));
                    FILE *fptr;
                    fptr = fopen(shortdirfilepath, "a+");

                    fprintf(fptr, command->args[1]);
                    fprintf(fptr, "=");
                    fprintf(fptr, cwd);
                    fprintf(fptr, "\n");

                    fclose(fptr);
                    return SUCCESS;
                }
                else if(strcmp(command->args[0], "jump")==0){
                    FILE* filePointer;
                    int bufferLength = 512;
                    char buffer[bufferLength];
                    filePointer = fopen(shortdirfilepath, "r");
                    while(fgets(buffer, bufferLength, filePointer)) {
                        char *ptr = strtok(buffer, "=");
                        if(strcmp(command->args[1], ptr)==0){
                            ptr = strtok(NULL,  "=");
                            ptr[strcspn(ptr, "\n")] = 0;
                            r=chdir(ptr);
                            if (r==-1)
                                printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
                            return SUCCESS;
                        }
                    }
                    fclose(filePointer);
                    return SUCCESS;
                }
                else if(strcmp(command->args[0], "del")==0){
                    char tmp[256];
                    strcat(strcpy(tmp, command->args[1]), "=");
                    delete_line(shortdirfilepath, find_line_number(shortdirfilepath,  tmp));
                    return SUCCESS;
                }
                else if(strcmp(command->args[0], "clear")==0){
                    FILE* filePointer;
                    filePointer = fopen(shortdirfilepath, "w");
                    fclose(filePointer);
                    return SUCCESS;
                }
                else if(strcmp(command->args[0], "list")==0){
                    FILE* filePointer;
                    filePointer = fopen(shortdirfilepath, "r");
                    char c;
                    c = fgetc(filePointer);
                    while (c != EOF)
                    {
                        printf ("%c", c);
                        c = fgetc(filePointer);
                    }
                    fclose(filePointer);

                    return SUCCESS;
                }

            }
        }
        if (strcmp(command->name, "kdiff")==0){
        
        if (command->arg_count > 0)
        {
            if (strcmp(command->args[0],"-b")==0){
                FILE *fp1, *fp2;
                int isSame=1;
                int differenceCharCount=0;

                if ((fp1 = fopen(command->args[1],"r")) == NULL){
                    printf("Can't open file: %s\n", command->args[1]);
                    exit(1);
                }

                if ((fp2 = fopen(command->args[2],"r")) == NULL){
                    printf("Can't open file: %s\n", command->args[2]);
                    exit(1);
                }
                char ch1, ch2;

             
                while (((ch1 = fgetc(fp1)) != EOF) && ((ch2 = fgetc(fp2)) != EOF))
                {

                    if (ch1 == ch2) //Go next char
                    {
                        continue;
                    }
                    else
                    {
                        differenceCharCount++;
                        isSame=0;
                    }
                }
                if (isSame == 0)
                    {
                        printf("The two files are different in %d bytes\n", differenceCharCount*sizeof(char));
                    }else
                    {
                        printf("The two files are identical\n ");
                    }

            }
                return SUCCESS;
            } else {
                FILE *fp1, *fp2;
                
                    int nLine = 1;
                    char l1[500], l2[500];



                    if ((fp1 = fopen(command->args[1],"r")) == NULL){
                        printf("Can't open file: %s\n", command->args[1]);
                        exit(1);
                    }

                    if ((fp2 = fopen(command->args[2],"r")) == NULL){
                        printf("Can't open file: %s\n", command->args[2]);
                        exit(1);
                    }
                
                    fgets(l1,500,fp1);
                    fgets(l2,500,fp2);
                
                while ((fgets(l1,500,fp1)) ||  fgets(l2,500,fp2)){
                        
                
                        if(strcmp(l1,l2) != 0){
                            printf("%s:Line%d: %s \n",command->args[1],nLine,l1);
                            printf("%s:Line%d: %s \n",command->args[2],nLine,l2);

                           // nLine++;
                           
                        } else {
                            fgets(l1,500,fp1);
                            fgets(l2,500,fp2);
                            nLine++;
                        }
                    
                    }
                
                return SUCCESS;
            }
    }
        
    

   // print_command(command);
    
    pid_t pid=fork();
   // envList = { "HOME=/root", PATH="/bin:/sbin", NULL };
    if (pid==0) // child
    {
        /// This shows how to do exec with environ (but is not available on MacOs)
        // extern char** environ; // environment variables
        // execvpe(command->name, command->args, environ); // exec+args+path+environ

        /// This shows how to do exec with auto-path resolve
        // add a NULL argument to the end of args, and the name to the beginning
        // as required by exec
        
        // increase args size by 2
        command->args=(char **)realloc(
            command->args, sizeof(char *)*(command->arg_count+=2));

        // shift everything forward by 1
        for (int i=command->arg_count-2;i>0;--i)
            command->args[i]=command->args[i-1];

        // set args[0] as a copy of name
        command->args[0]=strdup(command->name);
        // set args[arg_count-1] (last) to NULL
        command->args[command->arg_count-1]=NULL;
        
        //char *pathCom= getenv(command->name);
        //printf("-%s:\n",command->name);

        execvp(command->name, command->args); // exec+args+path
        exit(0);
        /// TODO: do your own exec with path resolving using execv()
        
    }
    else
    {
        if (!command->background)
            wait(0); // wait for child process to finish
        return SUCCESS;
    }

    // TODO: your implementation here

    printf("-%s: %s: command not found\n", sysname, command->name);
    return UNKNOWN;
}

