import os

def debug_tx():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Find the data bits loop
    old_code = """    // Data bits
    for (int i = 0; i < len; i++) {"""
    new_code = """    // Return early to test if preamble crashes
    return;
    // Data bits
    for (int i = 0; i < len; i++) {"""
    
    content = content.replace(old_code, new_code)
    
    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    debug_tx()
