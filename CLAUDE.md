# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses a Makefile-based build system:

```bash
# Standard build
make

# Build with debug output
make DEBUG=1

# Build with memory checking (AddressSanitizer)
make MEMCHECK=1

# Build with test configurations
make TEST_NORMAL_FRAME_DROP=1
make TEST_TIMEOUT_DROP=1

# Build with WDE (Write Data Enable) mode
make WDE=1

# Clean build artifacts
make clean

# Generate ctags for navigation
make tags
```

## Testing

The project includes a network simulator for testing:

```bash
# Start network simulator
./tloe_ns -p <queue_name>

# Start master endpoint
./tloe_endpoint -p <queue_name> -m

# Start slave endpoint  
./tloe_endpoint -p <queue_name> -s
```

For Ethernet mode testing:
```bash
# Master endpoint
./tloe_endpoint -i <interface> -d <destination_mac> -m

# Slave endpoint
./tloe_endpoint -i <interface> -d <destination_mac> -s
```

## Architecture Overview

This is a C implementation of the OmniXtend protocol (TileLink over Ethernet). The architecture consists of:

### Core Components
- **tloe_endpoint**: Main endpoint implementation supporting both master and slave roles
- **tloe_connection**: Connection management and state handling
- **tloe_frame**: Frame formatting, parsing, and protocol handling
- **tloe_fabric**: Transport abstraction layer supporting Ethernet and Message Queue modes

### Communication Layers
- **tloe_ether**: Raw Ethernet transport implementation
- **tloe_mq**: Message Queue transport for inter-process communication
- **tloe_transmitter/tloe_receiver**: Asynchronous transmission and reception logic
- **tloe_seq_mgr**: Sequence number management for reliable communication

### Protocol Support
- **tilelink_handler/tilelink_msg**: TileLink protocol implementation
- **flowcontrol**: Credit-based flow control mechanism
- **retransmission**: Reliable delivery with timeout and retransmission
- **timeout**: Timeout management for connections and operations

### Utilities
- **util/circular_queue**: Thread-safe circular buffer implementation
- **util/util**: General utility functions

## Key Design Patterns

### Multi-threaded Architecture
The system uses dedicated threads for transmission, reception, and protocol handling. Thread synchronization is handled through mutexes and condition variables.

### Transport Abstraction
The fabric layer abstracts different transport mechanisms (Ethernet vs Message Queue) allowing the same protocol logic to work over different underlying transports.

### Channel-based Communication
TileLink channels (A, B, C, D, E) are implemented with separate queues and flow control, following the TileLink specification.

### Credit-based Flow Control
Flow control prevents buffer overflow by exchanging credits during connection establishment and managing them throughout the communication session.

## Development Notes

### Debug Support
Use `DEBUG=1` to enable comprehensive debug output via `DEBUG_PRINT` macros defined in `tloe_common.h`.

### Memory Safety
The codebase includes `BUG_ON` macros for runtime assertions and supports AddressSanitizer via `MEMCHECK=1`.

### Code Organization
- Header files contain structure definitions and function prototypes
- Implementation files are organized by functional area
- Common definitions are centralized in `tloe_common.h`

## Interactive Commands

When running endpoints, these commands are available:
- `e` - Start Arty demo mode
- `c` - Open connection
- `d` - Close connection  
- `s` - Show status and statistics
- `r <addr> [count]` - Read memory operations
- `w <addr> <value> [count]` - Write memory operations
- `t <count>` - Run test sequence
- `q` - Quit

Network simulator commands:
- `s` - Toggle simulator thread
- `flush/f` - Flush message queues
- `a/b/w <count>` - Drop packets for testing
- `quit/q` - Quit simulator