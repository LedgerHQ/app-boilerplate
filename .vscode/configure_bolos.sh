#!/bin/bash

# Split the input string into an array of NAME=VALUE pairs
IFS=' ' read -ra name_value_pairs <<< "$@"

# Define the output file name
output_file=".vscode/bolos_config.h"

# Open the output file for writing
echo "#ifndef BOLOS_CONFIG_H" > "$output_file"
echo "#define BOLOS_CONFIG_H" >> "$output_file"

# Loop through the name_value_pairs array and write #define statements to the output file
for pair in "${name_value_pairs[@]}"
do
    # Split each pair into NAME and VALUE variables
    IFS='=' read -ra pair_parts <<< "$pair"
    name="${pair_parts[0]}"
    value="${pair_parts[1]}"
    
    # Write the #define statement to the output file
    echo "#define $name $value" >> "$output_file"
done

# Close the output file
echo "#endif // BOLOS_CONFIG_H" >> "$output_file"

# Print a confirmation message
echo "Configuration file '$output_file' generated successfully."
