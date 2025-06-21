#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <pthread.h>
#include <termios.h>
#include <sys/stat.h>

char* detected_distro = NULL;
char* executable_name = NULL;
bool commands_completed = false;

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

bool verify_extraction(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return false;
    }
    return true;
}

void* execute_commands_thread(void* arg) {
    const char* password = (const char*)arg;
    char command[1024];
    int ret;

    printf("\033[38;2;0;255;255mCreating config directory...\033[0m\n");
    snprintf(command, sizeof(command), "mkdir -p /home/$USER/.config/cmi >/dev/null 2>&1");
    ret = system(command);

    printf("\033[38;2;0;255;255mCloning repository...\033[0m\n");
    ret = system("git clone https://github.com/claudemods/claudemods-multi-iso-konsole-script.git /home/$USER/claudemods-multi-iso-creator >/dev/null 2>&1");

    if (strcmp(detected_distro, "arch") == 0) {
        printf("\033[38;2;0;255;255mBuilding Arch version...\033[0m\n");
        snprintf(command, sizeof(command), "cd /home/$USER/claudemods-multi-iso-creator/advancedcscript/arch && make >/dev/null 2>&1");
        ret = system(command);
        if (ret == 0) {
            snprintf(command, sizeof(command), "echo '%s' | sudo -S cp -f /home/$USER/claudemods-multi-iso-creator/advancedcscript/arch/archisocreator.bin /usr/bin/ >/dev/null 2>&1", password);
            system(command);
        }

        printf("\033[38;2;0;255;255mExtracting Arch build images...\033[0m\n");
        snprintf(command, sizeof(command), "echo '%s' | sudo -S unzip -o /home/$USER/claudemods-multi-iso-creator/advancedcscript/buildimages/build-image-arch.zip -d /home/$USER/.config/cmi/ >/dev/null 2>&1", password);
        ret = system(command);
        if (ret == 0 && verify_extraction("/home/$USER/.config/cmi/arch")) {
            printf("\033[38;2;0;255;0mArch build images extracted successfully!\033[0m\n");
        }
    }
    else if (strcmp(detected_distro, "ubuntu") == 0) {
        printf("\033[38;2;0;255;255mBuilding Ubuntu version...\033[0m\n");
        snprintf(command, sizeof(command), "cd /home/$USER/claudemods-multi-iso-creator/advancedcscript/ubuntu && make >/dev/null 2>&1");
        ret = system(command);
        if (ret == 0) {
            snprintf(command, sizeof(command), "echo '%s' | sudo -S cp -f /home/$USER/claudemods-multi-iso-creator/advancedcscript/ubuntu/ubuntuisocreator.bin /usr/bin/ >/dev/null 2>&1", password);
            system(command);
        }

        printf("\033[38;2;0;255;255mExtracting Ubuntu build images...\033[0m\n");
        snprintf(command, sizeof(command), "echo '%s' | sudo -S unzip -o /home/$USER/claudemods-multi-iso-creator/advancedcscript/buildimages/build-image-ubuntu.zip -d /home/$USER/.config/cmi/ >/dev/null 2>&1", password);
        ret = system(command);
        if (ret == 0 && verify_extraction("/home/$USER/.config/cmi/ubuntu")) {
            printf("\033[38;2;0;255;0mUbuntu build images extracted successfully!\033[0m\n");
        }
    }
    else if (strcmp(detected_distro, "debian") == 0) {
        printf("\033[38;2;0;255;255mBuilding Debian version...\033[0m\n");
        snprintf(command, sizeof(command), "cd /home/$USER/claudemods-multi-iso-creator/advancedcscript/debian && make >/dev/null 2>&1");
        ret = system(command);
        if (ret == 0) {
            snprintf(command, sizeof(command), "echo '%s' | sudo -S cp -f /home/$USER/claudemods-multi-iso-creator/advancedcscript/debian/debianisocreator.bin /usr/bin/ >/dev/null 2>&1", password);
            system(command);
        }

        printf("\033[38;2;0;255;255mExtracting Debian build images...\033[0m\n");
        snprintf(command, sizeof(command), "echo '%s' | sudo -S unzip -o /home/$USER/claudemods-multi-iso-creator/advancedcscript/buildimages/build-image-debian.zip -d /home/$USER/.config/cmi/ >/dev/null 2>&1", password);
        ret = system(command);
        if (ret == 0 && verify_extraction("/home/$USER/.config/cmi/debian")) {
            printf("\033[38;2;0;255;0mDebian build images extracted successfully!\033[0m\n");
        }
    }

    system("rm -rf /home/$USER/claudemods-multi-iso-creator/.git >/dev/null 2>&1");
    commands_completed = true;
    return NULL;
}

void show_loading_bar() {
    const int width = 50;
    const float total_time = 0.8f;
    const int steps = 100;
    const float step_time = total_time / steps;

    printf("\033[38;2;0;255;0mClaudeMods Multi ISO Creator v2.0 21-06-2025\033[0m\n");
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
    char* password = get_password();

    if (detect_distro() == NULL) {
        printf("\033[38;2;255;0;0mUnsupported distribution!\033[0m\n");
        free(password);
        return EXIT_FAILURE;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, execute_commands_thread, (void*)password);

    show_loading_bar();

    while (!commands_completed) {
        usleep(10000);
    }
    pthread_join(thread, NULL);

    printf("\033[38;2;0;255;0mInstallation complete!\033[0m\n");
    printf("\033[38;2;0;255;0mThe executable has been installed to /usr/bin/\033[0m\n");
    printf("\033[38;2;0;255;0mConfiguration files placed in ~/.config/cmi/\033[0m\n");
    printf("\033[38;2;0;255;0mStart with command: %s\033[0m\n", executable_name);

    free(password);
    return EXIT_SUCCESS;
}
