#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>

#define INS_STR "\33[93m--INSERT--"
#define NOR_STR "\33[91m--NORMAL--"

struct termios oldTerm, newTerm;
struct winsize winSize;
int result = 0;
char mode[63] = NOR_STR;
int lineNum = 0;
int fileDescriptor;

void setBackgroundColor(int r, int g, int b) {
    printf("\033[48;2;%d;%d;%dm", r, g, b);  // Set true-color background
}

void fillScreenWithBackground() {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &winSize);

    printf("\033[H");  // Move cursor to top-left
    for (int i = 0; i < winSize.ws_row; i++) {
        for (int j = 0; j < winSize.ws_col; j++) {
            putchar(' ');  // Fill each cell with a space to apply background color
        }
        putchar('\n');
    }
    printf("\033[H");  // Move cursor back to top-left
}

void clearScreen() {
    fillScreenWithBackground();
}

void initTerminal() {
    tcgetattr(STDIN_FILENO, &oldTerm);
    newTerm = oldTerm;
    newTerm.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);
}

void restoreTerminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
    printf("\033[0m");  // Reset color to default
}

void writeToFile(const char *buffer) {
    if (fileDescriptor == -1 || buffer == NULL) return;

    if (lseek(fileDescriptor, 0, SEEK_SET) == -1) {
        perror("Error seeking to start of file");
        return;
    }

    if (ftruncate(fileDescriptor, 0) == -1) {
        perror("Error truncating file");
        return;
    }

    ssize_t bytesWritten = write(fileDescriptor, buffer, strlen(buffer));
    if (bytesWritten == -1) {
        perror("Error writing to file");
    }
}

void outputString(const char *out) {
    setBackgroundColor(30, 30, 30);
    clearScreen();
    printf("\33[%d;0H\33[107m", winSize.ws_row-1);

    for (int i = 0; i < winSize.ws_col; i++) {
        putchar(' ');
    }
    printf("\33[%d;0H\33[107m", winSize.ws_row-1);
    printf(" %s | %d\33[0;0H\33[0m", mode, lineNum);
    printf("\n\033[48;2;30;30;30m\033[38;2;50;50;50m ~ \033[0m\033[48;2;30;30;30m");

    for (size_t i = 0; i < strlen(out); i++) {
        if (out[i] == '\n') {
            printf("\n\033[48;2;30;30;30m\033[38;2;50;50;50m ~ \033[0m\033[48;2;30;30;30m");
        } else {
            if (out[i] != '\t') putchar(out[i]);
            else printf("    ");
        }
    }
    printf("\033[0m");
}

void programLoop(char *fileContents) {
    size_t initialSize = strlen(fileContents);
    char *inputBuffer = (char *)malloc(initialSize + 2);
    if (inputBuffer == NULL) {
        result = 255;
        return;
    }

    strcpy(inputBuffer, fileContents);
    int index = initialSize;
    int breakLoop = 0;

    outputString(inputBuffer);

    while (1) {
        char buf = getchar();

        if (!strcmp(mode, INS_STR)) {
            if (index > 0 && buf == 127) {
                inputBuffer[--index] = '\0';
            } else if (buf == 27) { 
                strcpy(mode, NOR_STR);
            } else if (buf != 127 && index < (initialSize + 10000)) {  // Prevent buffer overflow
                inputBuffer[index++] = buf;
                inputBuffer[index] = '\0';  // Null-terminate the string
            }
        } else {
            if (buf == 'i') {
                strcpy(mode, INS_STR);
            }
            if (buf == ':') {
                printf("\33[%d;0H:", winSize.ws_row);
                fflush(stdout);  // Ensure prompt is displayed
                char buf2;
                int i2 = 0;
                char inputCommand[255] = {'\0'};

                while (1) {
                    buf2 = getchar();
                    if (buf2 == 27) break;
                    else if (buf2 == 127 && i2 > 0) {
                        inputCommand[--i2] = '\0';
                    }
                    else if (buf2 == '\n' || buf2 == '\r') {
                        inputCommand[i2] = '\0';
                        if (!strcmp(inputCommand, "wq") || !strcmp(inputCommand, "x")) {
                            writeToFile(inputBuffer);
                            breakLoop = 1;
                            break;
                        }
                        if (!strcmp(inputCommand, "q")) {
                            breakLoop = 1;
                            break;
                        }
                        break;
                    }
                    else {
                        if (i2 < sizeof(inputCommand) - 1) {  // Prevent buffer overflow
                            inputCommand[i2++] = buf2;
                            inputCommand[i2] = '\0';
                        }
                    }

                    outputString(inputBuffer);
                    printf("\33[%d;0H\033[0m\033[48;2;30;30;30m:%s", winSize.ws_row, inputCommand);
                    fflush(stdout);  // Ensure command input is displayed
                }
            }
        }

        // Reallocate buffer to accommodate new characters
        inputBuffer[index] = '\0';  // Ensure null-termination
        char *tmp = realloc(inputBuffer, index + 2);
        if (tmp == NULL) {
            result = 16;
            free(inputBuffer);
            return;
        }
        inputBuffer = tmp;
        outputString(inputBuffer);
        if (breakLoop) break;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) return 5;

    fileDescriptor = open(argv[1], O_RDWR | O_CREAT, 0644);
    if (fileDescriptor == -1) {
        perror("Error opening file");
        return 23;
    }

    initTerminal();
    setBackgroundColor(30, 30, 30);
    fillScreenWithBackground();

    off_t fileSize = lseek(fileDescriptor, 0, SEEK_END);
    if (fileSize == -1) {
        perror("Error getting file size");
        close(fileDescriptor);
        return 125;
    }

    if (lseek(fileDescriptor, 0, SEEK_SET) == -1) {
        perror("Error resetting file offset");
        close(fileDescriptor);
        return 125;
    }

    char *fileContents = (char *)malloc(fileSize + 1);
    if (fileContents == NULL) {
        perror("Unable to allocate memory for file contents");
        close(fileDescriptor);
        return 132;
    }

    ssize_t bytesRead = read(fileDescriptor, fileContents, fileSize);
    if (bytesRead == -1) {
        perror("Unable to read file contents");
        close(fileDescriptor);
        free(fileContents);
        return 22;
    }

    fileContents[bytesRead] = '\0';
    programLoop(fileContents);
    free(fileContents);

    close(fileDescriptor);
    restoreTerminal();

    return result;
}

