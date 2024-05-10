# Specify the name of the executable.
add_executable(lab08)

# Specify the source files to be compiled.
target_sources(lab08 PRIVATE lab08.c lab08.S)

# Generate the PIO header file from the PIO source file.
pico_generate_pio_header(lab08 ${CMAKE_CURRENT_LIST_DIR}/lab08.pio)

# Pull in commonly used features.
target_link_libraries(lab08 PRIVATE pico_stdlib hardware_pio hardware_adc)

# Create map/bin/hex file etc.
pico_add_extra_outputs(lab08)

# Add the URL via pico_set_program_url.
apps_auto_set_url(lab08)
