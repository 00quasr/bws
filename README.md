# BWS C++ Projects

Monorepo containing Qt/C++ projects developed during my apprenticeship as an Application Developer  (2025-2028).

Each project focuses on specific programming concepts and demonstrates practical applications of algorithms and GUI development.

## Projects

| Project | Description | Key Concepts |
|---------|-------------|--------------|
| [Notenversetzungsprogramm](./Kapitalverdopplung) | Capital doubling calculator with compound interest | While loops, financial math |
| [Fahrscheinautomaten](./Fahrscheinautomaten) | Ticket machine with change calculation | Greedy algorithm, arrays |
| [QuadratwurzelHeron](./QuadratwurzelHeron) | Square root using Heron's method | Iterative approximation |
| [KapitalBerechnung](./KapitalBerechnung) | Interest calculator template | Qt Designer basics |

## Build Requirements

- Qt 5.x or Qt 6.x
- C++ compiler with C++17 support
- Qt Creator (recommended) or qmake

## Building a Project

```bash
cd ProjectName
qmake ProjectName.pro
make
```

Or open the `.pro` file directly in Qt Creator.

## Project Structure

```
bws/
├── Fahrscheinautomaten/     # Ticket vending machine
├── KapitalBerechnung/       # Interest calculator (UI only)
├── Kapitalverdopplung/ # Capital doubling calculator
├── QuadratwurzelHeron/      # Square root calculator
└── README.md
```

Each project contains:
- `main.cpp` - Application entry point
- `mainwindow.cpp/.h` - Main window logic
- `mainwindow.ui` - Qt Designer UI file
- `*.pro` - Qt project configuration

## License

Educational projects developed during vocational training.
