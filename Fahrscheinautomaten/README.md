# Fahrscheinautomat (Ticket Vending Machine)

A Qt application that simulates a ticket vending machine, calculating the optimal change to return using the fewest coins possible.

## Description

The user enters a ticket price and the amount of money inserted. The program calculates the change and displays exactly which coins should be returned, using a greedy algorithm to minimize the number of coins.

## Programming Concepts

### Greedy Algorithm
The change calculation uses a greedy approach - always selecting the largest possible coin first:

```cpp
int muenzen[] = {200, 100, 50, 20, 10, 5, 2, 1};  // Coin values in cents

for (int i = 0; i < 8; i++) {
    int anzahl = rueckgeld / muenzen[i];  // How many of this coin?
    if (anzahl > 0) {
        rueckgeld = rueckgeld % muenzen[i];  // Remaining change
    }
}
```

### Parallel Arrays
Two arrays work together - one for values, one for display names:
```cpp
int muenzen[] = {200, 100, 50, 20, 10, 5, 2, 1};
QString Name[] = {"2 Euro", "1 Euro", "50 Cent", "20 Cent", ...};
```

### Integer Arithmetic for Currency
Converting euros to cents avoids floating-point precision errors:
```cpp
int preis = euro * 100;      // 3.50€ becomes 350 cents
int einwurf = einwurfEuro * 100;
```

### Modulo Operator
The `%` operator calculates the remainder after dividing:
```cpp
rueckgeld = rueckgeld % muenzen[i];  // What's left after this coin
```

### Qt Signal Connection
Connecting the Enter key to trigger calculation:
```cpp
connect(ui->leEinwurf, &QLineEdit::returnPressed,
        this, &MainWindow::on_leEinwurf_eingabe);
```

## Usage

1. Enter the ticket price in euros
2. Enter the amount of money inserted
3. Press Enter to calculate
4. View the change breakdown by coin type

## Example

Ticket price: 2.80€, Payment: 5.00€

```
Rückgeld: 2.20 Euro
1 x 2 Euro
1 x 20 Cent
```

## Edge Cases Handled

- **Exact payment**: Displays "kein rückgeld" (no change)
- **Insufficient payment**: Shows how much more is needed
