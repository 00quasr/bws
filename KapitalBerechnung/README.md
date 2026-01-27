# KapitalBerechnung (Interest Calculator)

A Qt application template for calculating interest and capital growth.

## Status

**Work in Progress** - The UI has been designed but the calculation logic is not yet implemented.

## Description

This project provides the graphical interface for an interest calculator. The UI includes input fields for capital and interest rate, action buttons, and a display area for results.

## Programming Concepts

### Qt Designer UI Files
The interface is defined in `mainwindow.ui`, an XML file created with Qt Designer:
- Visual layout without writing code
- Widgets defined declaratively
- Automatically generates `ui_mainwindow.h`

### Qt Project Structure
Standard Qt application setup:
```
KapitalBerechnung/
├── main.cpp           # Entry point, creates QApplication
├── mainwindow.cpp     # Window logic (to be implemented)
├── mainwindow.h       # Header with slot declarations
├── mainwindow.ui      # Designer UI file
└── KapitalBerechnung.pro  # Project configuration
```

### Signal/Slot Pattern
Qt's mechanism for connecting UI events to code:
```cpp
// In the .ui file, buttons are named btnBerechnen, btnClear, btnEnde
// Qt auto-connects to slots named on_<widget>_<signal>()

void MainWindow::on_btnBerechnen_clicked() {
    // Called when Berechnen button is clicked
}
```

### UI Components Used
- `QLabel` - Static text labels
- `QLineEdit` - Input fields for capital and interest rate
- `QPushButton` - Berechnen, Clear, Ende buttons
- `QListWidget` - Output display area

## Building

```bash
qmake KapitalBerechnung.pro
make
```
