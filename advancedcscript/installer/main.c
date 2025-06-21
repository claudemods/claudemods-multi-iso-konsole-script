#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <termios.h>

char* detected_distro = NULL;
char* executable_name = NULL;
bool commands_completed = false;

// Function to get password without echo
char* get_password() {
    struct termios old_term, new_term;
    char* password = malloc(128);

    tcgetattr(STDIN_FILENO, &old_term);
    new_term = old_term;
    new_term.c_lflag &= ~(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_term);

    printf("\033[38;2;0;255;0mEnter sudo password: \033[0m");
    fgets(password, 128, stdin);
    password[strcspn(password, "\n")] = '\0';

    tcsetattr(STDIN_FILENO, TCSANOW, &old_term);
    printf("\n");
    return password;
}

// Function to detect Linux distribution
char* detect_distro() {
    FILE *fp;
    char buffer[256];

    fp = popen("cat /etc/os-release | grep -E '^ID=' | cut -d'=' -f2", "r");
    if (fp == NULL) {
        perror("Failed to detect distribution");
        exit(EXIT_FAILURE);
    }

    if (fgets(buffer, sizeof(buffer), fp) != NULL) {
        buffer[strcspn(buffer, "\n")] = 0;
        buffer[strcspn(buffer, "\"")] = 0;

        if (strcmp(buffer, "arch") == 0 || strcmp(buffer, "cachyos") == 0) {
            detected_distro = "arch";
            executable_name = "archisocreator.bin";
        } else if (strcmp(buffer, "ubuntu") == 0) {
            detected_distro = "ubuntu";
            executable_name = "ubuntuisocreator.bin";
        } else if (strcmp(buffer, "debian") == 0) {
            detected_distro = "debian";
            executable_name = "debianisocreator.bin";
        }
    }

    pclose(fp);
    return detected_distro;
}

// Thread function to execute commands
void* execute_commands_thread(void* arg) {
    const char* password = (const char*)arg;
    char command[512];

    // Clone repository
    system("git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git /home/$USER/claudemods-multi-iso-creator 2>/dev/null");

    // Build and install based on distro
    if (strcmp(detected_distro, "arch") == 0) {
        snprintf(command, sizeof(command), "cd /home/$USER/claudemods-multi-iso-creator/advancedcscript/arch && make -s && echo '%s' | sudo -S cp -f archisocreator.bin /usr/bin/ 2>/dev/null", password);
    } else if (strcmp(detected_distro, "ubuntu") == 0) {
        snprintf(command, sizeof(command), "cd /home/$USER/claudemods-multi-iso-creator/advancedcscript/ubuntu && make -s && echo '%s' | sudo -S cp -f ubuntuisocreator.bin /usr/bin/ 2>/dev/null", password);
    } else if (strcmp(detected_distro, "debian") == 0) {
        snprintf(command, sizeof(command), "cd /home/$USER/claudemods-multi-iso-creator/advancedcscript/debian && make -s && echo '%s' | sudo -S cp -f debianisocreator.bin /usr/bin/ 2>/dev/null", password);
    }

    system(command);

    // Clean up
    system("rm -rf /home/$USER/claudemods-multi-iso-creator/.git 2>/dev/null");

    commands_completed = true;
    return NULL;
}

// Function to display loading bar in green
void show_loading_bar() {
    const int width = 50;
    const float total_time = 0.8f;
    const int steps = 100;
    const float step_time = total_time / steps;

    printf("\033[38;2;0;255;0mClaudeMods Multi ISO Creator v2.0\033[0m\n");
    printf("\033[38;2;0;255;0mLoading: \033[0m\n");
    printf("[");

    for (int i = 0; i < width; i++) printf(" ");
    printf("] 0%%");
    printf("\r[");

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = step_time * 1000000000;

    for (int i = 0; i <= steps; i++) {
        float progress = (float)i / steps;
        int pos = progress * width;

        // Print green loading bar
        for (int j = 0; j < pos; j++) {
            printf("\033[38;2;0;255;0m█\033[0m");
        }

        printf("\033[%dC", width - pos + 1);
        printf("%d%%", i);
        printf("\r[");

        fflush(stdout);
        nanosleep(&ts, NULL);
    }
    printf("\n");
}

int main() {
    // Ask for password first
    char* password = get_password();

    // Detect distribution
    if (detect_distro() == NULL) {
        printf("\033[38;2;255;0;0mUnsupported distribution!\033[0m\n");
        free(password);
        return EXIT_FAILURE;
    }

    // Create thread for commands
    pthread_t thread;
    pthread_create(&thread, NULL, execute_commands_thread, (void*)password);

    // Show loading bar while commands execute
    show_loading_bar();

    // Wait for commands to complete
    while (!commands_completed) {
        usleep(10000);
    }
    pthread_join(thread, NULL);

    // Final message in green
    printf("\033[38;2;0;255;0mInstallation complete!\033[0m\n");
    printf("\033[38;2;0;255;0mThe executable has been installed to /usr/bin/\033[0m\n");
    printf("\033[38;2;0;255;0mStart with command: %s\033[0m\n", executable_name);

    free(password);
    return EXIT_SUCCESS;
}
