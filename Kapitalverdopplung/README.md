# Kapitalverdopplung (Capital Doubling Calculator)

A Qt application that calculates how many years it takes for an initial capital to double through compound interest.

## Description

The user enters a starting capital and an annual interest rate. The program then calculates year by year how the capital grows until it has doubled, displaying each year's balance.

## Programming Concepts

### While Loop
The core calculation uses a `while` loop that continues as long as the capital hasn't reached the target:

```cpp
while (kapital < zielKapital) {
    jahr = jahr + 1;
    kapital = kapital + (kapital * zinssatz / 100);
}
```

### Compound Interest Formula
Each year, interest is calculated on the current balance (not just the initial amount):
- Simple interest: `interest = principal * rate`
- Applied each year to the growing balance

### QString Number Formatting
Displaying currency values with proper decimal places:
```cpp
QString::number(kapital, 'f', 2)  // 'f' = fixed-point, 2 decimal places
```

### Qt Widgets Used
- `QLineEdit` - Text input for capital and interest rate
- `QListWidget` - Displays the year-by-year calculation results
- `QPushButton` - Triggers calculation, clear, and exit actions

## Usage

1. Enter the starting capital in euros
2. Enter the annual interest rate as a percentage
3. Click "Berechnen" to calculate
4. View the yearly progression until the capital doubles
5. Use "Leeren" to clear all fields
6. Use "Ende" to close the application

## Example

Starting capital: 1000€, Interest rate: 5%

```
Startkapital: 1000.00 Euro

nach 1 Jahr(en): 1050.00 Euro
nach 2 Jahr(en): 1102.50 Euro
...
nach 15 Jahr(en): 2078.93 Euro

Kapital hat sich nach 15 Jahren verdoppelt
```
