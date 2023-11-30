import sys

def remove_bytes(input_file, output_file):
    try:
        with open(input_file, 'rb') as file:
            # Read the content of the input binary file
            content = file.read()
            
            # Remove the first 2048 bytes
            modified_content = content[2048:]
            
            # Write the modified content to the output file
            with open(output_file, 'wb') as output_file:
                output_file.write(modified_content)
            print(f"Bytes removed successfully. Output written to {output_file}")
    except FileNotFoundError:
        print("Error: Input file not found.")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script_name.py input_file output_file")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        remove_bytes(input_file, output_file)
 
