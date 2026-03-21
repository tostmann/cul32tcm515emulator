import os

def fix_radio_transmit():
    with open('src/radio_hal.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Replace malloc with static buffer
    old_malloc = """    rmt_symbol_word_t *tx_symbols = malloc(1024 * sizeof(rmt_symbol_word_t));
    if (!tx_symbols) return;"""
    
    new_malloc = """    static rmt_symbol_word_t tx_symbols[1024];"""
    
    content = content.replace(old_malloc, new_malloc)
    
    with open('src/radio_hal.c', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Fixed radio_transmit malloc!")

if __name__ == "__main__":
    fix_radio_transmit()
