import json
import sys

# Get the input string from the program arguments
if len(sys.argv) < 4:
    print('Three arguments required: config file name, variable and selector')
    exit()
file_str = sys.argv[1]
variable_str = sys.argv[2]
value_str = sys.argv[3]

# Load the JSON file containing key-value pairs
with open(file_str) as f:
    config_dict = json.load(f)

# Check if the input string exists in the dictionary
if variable_str in config_dict:
    var_config_dict = config_dict[variable_str]
    if value_str in var_config_dict:
        return_str = var_config_dict[value_str]
    else:
        return_str = 'No corresponding selector found for seletor argument.'
else:
    return_str = 'No corresponding variable found for input argument.'

# Print the resulting string
print(return_str)
