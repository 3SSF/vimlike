#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>

#define INS_STR "\33[93m--INSERT--"
#define NOR_STR "\33[91m--NORMAL--"

struct termios oldt, newt;
struct winsize ws;
int result;
char mode[63] = NOR_STR;
int ln; // TODO

void set_background_color(int r, int g, int b) {
    printf("\033[48;2;%d;%d;%dm", r, g, b);  // Set true-color background
}

void fill_screen_with_background() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);  // Get terminal dimensions

    printf("\033[H");  // Move cursor to top-left
    for (int i = 0; i < ws.ws_row; i++) {
        for (int j = 0; j < ws.ws_col; j++) {
            putchar(' ');  // Fill each cell with a space to apply background color
        }
        putchar('\n');
    }
    printf("\033[H");  // Move cursor back to top-left
}

void clear() {
    fill_screen_with_background();
}

void init() {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void normalTerm() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\033[0m");  // Reset color to default
}

void outputString(char* out) {
    set_background_color(30, 30, 30);  // Set a dark gray background color
    clear();
    printf("\33[%d;0H\33[107m", ws.ws_row-1);
    for (int i = 0; i < ws.ws_col; i++) {
        putchar(' ');  // Fill each cell with a space to apply background color
    }
    printf("\33[%d;0H\33[107m", ws.ws_row-1);
    printf(" %s | %d\33[0;0H\33[0m", mode, ln);
    printf("\n\033[48;2;30;30;30m\033[38;2;50;50;50m ~ \033[0m\033[48;2;30;30;30m");
    for (int i = 0; i < strlen(out); i++) {
        if (out[i] == '\n') {
            // Set both the foreground and background colors for the "~ " part
            printf("\n\033[48;2;30;30;30m\033[38;2;50;50;50m ~ \033[0m\033[48;2;30;30;30m");
        } else {
            putchar(out[i]);
        }
    }
    
    // Reset to avoid affecting any output after this function
    printf("\033[0m");
}

char* program(char* fileContents) {
    char broken = 0;
    size_t initialSize = strlen(fileContents);
    char* inp = (char*)malloc(initialSize + 2);
    if (inp == NULL) {
        result = 255;
        return NULL;
    }

    strcpy(inp, fileContents);
    int i = initialSize; 

    char buf = 0;
    outputString(inp);
    while (1) {
        buf = getchar();
        if (!strcmp(mode, INS_STR)){
            if (i > 0 && buf == 127) {
                inp[--i] = '\0';
            } else if (buf == 27) { 
                strcpy(mode, NOR_STR);
            } else if (buf != 127) {
                inp[i++] = buf;
            }
        } else {
            if (buf == 'i') strcpy(mode, INS_STR);
            if (buf == ':'){
                printf("\33[%d;0H", ws.ws_row);
                printf(":");
                char buf2;
                int i2 = 0;
                char inp2[255] = {'\0'};
                while (1){
                    buf2 = getchar();
                    if (buf2 == 27) break;
                    else if (buf2 == 127 && i2 > 0){
                        inp2[--i2] = '\0';
                    } else if (buf2 == '\n'){
                        if (!strcmp(inp2, "wq")){
                            broken = 1;
                            break;
                        }
                        if (!strcmp(inp2, "x")){
                            broken = 1;
                            break;
                        }
                        if (inp2[0] == '!'){
                            inp2[0] = '\0';
                            for (i = 0; i < 256; i++){
                                inp2[i] = inp2[i] ^ inp2[i+1];
                                inp2[i+1] = inp2[i+1] ^ inp2[i];
                                inp2[i] = inp2[i] ^ inp2[i+1];
                            }
                            system(inp2);
                            break;
                        }
                    } else inp2[i2++] = buf2;
                    outputString(inp);
                    printf("\33[%d;0H\033[0m\033[48;2;30;30;30m:%s", ws.ws_row, inp2);
                }
            }
        }

        inp[i] = '\0';
        char* tmp = realloc(inp, i + 2);
        if (tmp == NULL) {
            result = 16;
            free(inp);
            return NULL;
        }
        inp = tmp;
        outputString(inp);
        if (broken) break;
    }

    return inp;
}

int main(int argc, char* argv[]) {
    if (argv[1] == NULL) return 5;

    int fd = open(argv[1], O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        perror("Error opening file");
        return 23;
    }

    init();
    set_background_color(30, 30, 30);  // Set a dark gray background color
    fill_screen_with_background();

    off_t fileSize = lseek(fd, 0, SEEK_END);
    if (fileSize == -1) {
        perror("Error getting file size");
        close(fd);
        return 125;
    }

    lseek(fd, 0, SEEK_SET);

    char* fC = (char*)malloc(fileSize + 1);
    if (fC == NULL) {
        perror("Unable to allocate memory for file contents");
        close(fd);
        return 132;
    }

    ssize_t buf = read(fd, fC, fileSize);
    if (buf == -1) {
        perror("Unable to read file contents");
        close(fd);
        free(fC);
        return 22;
    }
    
    fC[buf] = '\0';

    lseek(fd, 0, SEEK_SET);

    char* dat = program(fC);
    free(fC);
    if (dat == NULL) {
        close(fd);
        normalTerm();
        return result;
    }

    if (ftruncate(fd, 0) == -1) {
        perror("Error truncating file");
        close(fd);
        free(dat);
        normalTerm();
        return 1;
    }

    ssize_t bytesWritten = write(fd, dat, strlen(dat));
    if (bytesWritten == -1) {
        perror("Error writing to file");
        close(fd);
        free(dat);
        normalTerm();
        return 1;
    }

    close(fd);
    normalTerm();
    free(dat);

    return result;
}

