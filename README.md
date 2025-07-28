# ft_nmap


A Network Mapper (nmap) implementation with NixOS development environment.

## Development Environment

This project uses Nix flakes for reproducible development environments and VM creation.

### Quick Start

1. **Build and run the VM:**
   ```bash
   make vm-up
   ```

2. **Or use the new flake commands:**
   ```bash
   # Build the VM
   make vm-build
   
   # Run the VM directly  
   make vm-run
   
   # Enter development shell
   make dev-shell
   ```

3. **Build the project:**
   ```bash
   make all
   ```

### Available Make Targets

#### Build Targets
- `make all` - Build all targets (app, debug, test)
- `make app` - Build the main application
- `make debug` - Build debug version
- `make test` - Build and run tests
- `make clean` - Clean build artifacts

#### VM Targets
- `make vm-up` - Start the development VM
- `make vm-down` - Stop the VM
- `make vm-restart` - Restart the VM
- `make vm-build` - Build VM using flake
- `make vm-run` - Run VM directly using flake

#### Flake Targets
- `make flake-check` - Validate flake configuration
- `make flake-update` - Update flake dependencies
- `make flake-show` - Show flake outputs
- `make dev-shell` - Enter development shell

## Project Structure

```
ft_nmap/
├── src/           # Main source code
├── include/       # Header files
├── test/          # Test files (using munit)
├── debug/         # Debug utilities
├── .vm/           # VM configuration and scripts
├── flake.nix      # Nix flake configuration
└── Makefile       # Build configuration
```

## Flake-based Development

This project uses Nix flakes for:
- **Reproducible builds**: Exact dependency versions
- **Development environment**: All tools included
- **VM creation**: Consistent development environment
- **Easy setup**: One command to get started

See [FLAKE.md](FLAKE.md) for detailed flake documentation.

## Testing

The project uses the munit testing framework:

```bash
make test        # Build and run tests
make run-test    # Run existing test binary
make debug-test  # Debug tests with gdb
```

## Contributing

1. Start the development environment: `make vm-up`
2. Make your changes
3. Run tests: `make test`
4. Build: `make all` 