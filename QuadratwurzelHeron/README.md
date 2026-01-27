# Quadratwurzel Heron (Square Root Calculator)

A Qt application that calculates square roots using Heron's method (also known as the Babylonian method), showing each iteration step.

## Description

The user enters a number and the desired precision (decimal places). The program iteratively approximates the square root, displaying each step until the result converges to the specified accuracy.

## Programming Concepts

### Heron's Method (Babylonian Method)
An ancient algorithm for approximating square roots. Starting with an initial guess, each iteration gets closer to the true value:

```cpp
xNew = 0.5 * (xOld + zahl / xOld);
```

This formula averages the current guess with `number/guess`, rapidly converging to √number.

### Do-While Loop
The iteration continues until the desired precision is reached:

```cpp
do {
    xNew = 0.5 * (xOld + zahl / xOld);

    double differenz = xNew - xOld;
    if (differenz < 0) differenz = -differenz;  // Absolute value

    if (differenz < toleranz) break;

    xOld = xNew;
} while (true);
```

### Tolerance/Precision Calculation
Converting decimal places to a tolerance value:

```cpp
double toleranz = 1.0;
for (int i = 0; i <= genauigkeit; i++) {
    toleranz = toleranz / 10.0;  // 2 decimal places → 0.001
}
```

### Manual Absolute Value
Calculating absolute difference without using `abs()`:
```cpp
if (differenz < 0) {
    differenz = -differenz;
}
```

### Input Validation
Handling edge cases before calculation:
```cpp
if (zahl < 0) {
    // Error: no square root of negative numbers
}
if (zahl == 0) {
    // Special case: √0 = 0
}
```

## Usage

1. Enter the number to find the square root of
2. Enter the desired number of decimal places
3. Click "Berechnen" to calculate
4. View each iteration step and the final result
5. Use "Clear" to reset all fields

## Example

Number: 2, Precision: 6 decimal places

```
zahl: 2
Iteration 1: x = 2.00000000
Iteration 2: x = 1.50000000
Iteration 3: x = 1.41666667
Iteration 4: x = 1.41421569
Iteration 5: x = 1.41421356
√2 = 1.414214
```

## Why Heron's Method?

- Converges very quickly (quadratic convergence)
- Simple to implement
- Works with just basic arithmetic operations
- Historical significance - used for over 2000 years
