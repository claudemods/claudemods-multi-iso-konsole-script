#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PATH 1024

// Function to prompt user input
void prompt(const char *prompt_text, char *buffer, size_t size) {
    printf("%s ", prompt_text);
    fflush(stdout); // Ensure prompt is printed immediately
    if (fgets(buffer, size, stdin)) {
        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = '\0';
    }
}

// Function to simulate an error message box
void error_box(const char *title, const char *message) {
    printf("%s\n", title);
    printf("%s\n", message);
}

// Function to run ISO in QEMU
void run_iso_in_qemu() {
    char iso_path[MAX_PATH];

    prompt("Select ISO File (full path):", iso_path, sizeof(iso_path));

    if (iso_path[0] == '\0') {
        error_box("Input Error", "ISO file cannot be empty.");
        return;
    }

    // Build the QEMU command
    char qemu_command[2048];
    snprintf(qemu_command, sizeof(qemu_command),
             "qemu-system-x86_64 -m 4000 -smp 2 -accel kvm -cdrom \"%s\"", iso_path);

    // Execute the command
    system(qemu_command);
}

// Main function
int main() {
    run_iso_in_qemu();
    return 0;
}
