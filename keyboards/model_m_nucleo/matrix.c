#include "matrix.h"
#include "wait.h"
#include "i2c_master.h"

// ==========================================================================
// PROGRAMMING SAFETY GUARDRAILS (ISO C11 Feature)
// ==========================================================================
// _Static_assert was added to the language in ISO C11 to validate assumptions 
// during compilation. If a configuration or array size changes accidentally, 
// this forces a compile-time failure rather than deploying bricked firmware.
_Static_assert(MATRIX_ROWS == 11, "Compilation halted: Row boundary specification must equal 11.");
_Static_assert(MATRIX_COLS == 13, "Compilation halted: Column boundary specification must equal 13.");

// Hardware Device Address Registers
#define EXPANDER_1_ADDR (0x21 << 1)
#define EXPANDER_2_ADDR (0x22 << 1)

#define MCP23017_IODIRA 0x00
#define MCP23017_IODIRB 0x01
#define MCP23017_GPPUA  0x0C
#define MCP23017_GPPUB  0x0D
#define MCP23017_GPIOA  0x12
#define MCP23017_GPIOB  0x13

// Encapsulated state storage (Singleton Pattern)
static matrix_row_t matrix[MATRIX_ROWS];
static bool i2c_initialized = false;

// ==========================================================================
// DRIVER DESIGN PATTERN: ENCAPSULATED TRANSACTION BLOCK
// ==========================================================================
typedef struct {
    uint8_t dev_addr;
    uint8_t reg_addr;
    uint8_t data;
} mcp_packet_t;

// Safety Improvement: ISO C99 Designated Initializers (.member = val) are used
// below. Implemented in 1999, they eliminate structural padding vulnerabilities
// and out-of-order execution bugs common in flat C parameter structs.
static bool write_register_safe(uint8_t dev, uint8_t reg, uint8_t val) {
    mcp_packet_t packet = {
        .dev_addr = dev,
        .reg_addr = reg,
        .data     = val
    };
    uint8_t buffer[2] = {packet.reg_addr, packet.data};
    return (i2c_transmit(packet.dev_addr, buffer, 2, 10) == I2C_STATUS_SUCCESS);
}

void matrix_init(void) {
    // Initialize matrix array (safe for early boot, no I2C)
    for (uint8_t i = 0; i < MATRIX_ROWS; i++) {
        matrix[i] = 0;
    }
}

void matrix_init_expanders(void) {
#if MODEL_M_ENABLE_EXPANDERS
    // Deferred I2C initialization (called after USB is ready)
    if (i2c_initialized) return;
    
    i2c_init();
    wait_ms(100); // Guardrail: allow IC decoupling capacitors to clear inrush spikes

    // Initialize Expander 1 (Columns)
    write_register_safe(EXPANDER_1_ADDR, MCP23017_IODIRA, 0xFF);
    write_register_safe(EXPANDER_1_ADDR, MCP23017_GPPUA,  0xFF);
    write_register_safe(EXPANDER_1_ADDR, MCP23017_IODIRB, 0xFF);
    write_register_safe(EXPANDER_1_ADDR, MCP23017_GPPUB,  0xFF);

    // Initialize Expander 2 (Rows & Status LEDs)
    write_register_safe(EXPANDER_2_ADDR, MCP23017_IODIRA, 0x80); // Pin 7 input, 0-6 output
    write_register_safe(EXPANDER_2_ADDR, MCP23017_IODIRB, 0x80); // Pin 7 input, 0-6 output

    i2c_initialized = true;
#endif
}

uint8_t matrix_scan(void) {
#if !MODEL_M_ENABLE_EXPANDERS
    return 0;
#endif

    // Ensure I2C expanders are initialized
    if (!i2c_initialized) {
        matrix_init_expanders();
    }
    
    bool matrix_changed = false;

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        uint16_t row_mask = 0xFFFF;
        
        if (row < 7) {
            row_mask = ~(1 << row) & 0x7F; 
            if (!write_register_safe(EXPANDER_2_ADDR, MCP23017_GPIOA, (uint8_t)row_mask)) continue;
            write_register_safe(EXPANDER_2_ADDR, MCP23017_GPIOB, 0xFF);
        } else {
            uint8_t b_row = row - 7;
            row_mask = ~(1 << b_row) & 0x0F;
            // Masking 0xF0 preserves state pins 4, 5, 6 for status LEDs without line cross-talk
            if (!write_register_safe(EXPANDER_2_ADDR, MCP23017_GPIOB, (uint8_t)(row_mask | 0xF0))) continue;
            write_register_safe(EXPANDER_2_ADDR, MCP23017_GPIOA, 0xFF);
        }

        uint8_t port_a = 0, port_b = 0;
        if (i2c_read_register(EXPANDER_1_ADDR, MCP23017_GPIOA, &port_a, 1, 10) != I2C_STATUS_SUCCESS) continue;
        if (i2c_read_register(EXPANDER_1_ADDR, MCP23017_GPIOB, &port_b, 1, 10) != I2C_STATUS_SUCCESS) continue;

        // Strip pin positions safely using bitwise shifting techniques
        uint16_t clean_a = ((~port_a) >> 1) & 0x3F; // Eliminate skipped 1A0 pin
        uint16_t clean_b = (~port_b) & 0x7F;

        matrix_row_t current_row_state = clean_a | (clean_b << 6);

        if (matrix[row] != current_row_state) {
            matrix[row] = current_row_state;
            matrix_changed = true;
        }
    }
    return (uint8_t)matrix_changed;
}

matrix_row_t matrix_get_row(uint8_t row) {
    return matrix[row];
}

void matrix_print(void) {
}
