#!/usr/bin/env ruby

# Function to prompt for user input
def prompt(prompt_text)
    print "#{prompt_text} "
    gets.chomp
end

# Function to display an error message box (simulated)
def error_box(title, message)
    puts "#{title}\n"
    puts "#{message}\n"
end

# Function to run the ISO in QEMU
def run_iso_in_qemu
    iso_file_path = prompt("Select ISO File (full path):")
    if iso_file_path.empty?
        error_box("Input Error", "ISO file cannot be empty.")
        return
    end

    qemu_command = "qemu-system-x86_64 -m 4000 -smp 2 -accel kvm -cdrom #{iso_file_path}"
    system(qemu_command)
end

# Main execution
run_iso_in_qemu
