# Cash

Cash is a small, simple, double entry plain text accounting system, which supports interactivly adding transactions, printing transactions, showing balance, and more. It supports flexible expression based filtering based on accounts, descriptions, amounts, date, and more. It should be trivial to retrive any transaction you are looking for. It also support saving references, receipts, etc. in the transaction by dragging the file into the terminal while adding a transaction.

*Note: error handling is bad!*

## Memory

Dynamic memory only used to read the journal from file and parse it. This memory is freed after parsing. Transactions and accounts are statically allocated. If not sufficient, increase MAX_TRANSACTIONS or MAX_ACCOUNTS  in journal.h. 

## File format

The accounts entry start with @ and must be the first entry. When accounts are referenced, a dot is used to separate categories.

```
@ {
  Assets {
    Visa
  }
  Expenses {
    Food
    Trips {
      Abroad
      Other
    }
  }
}
```

Budget entries starts with ? followed by m (monthly) or y (yearly) followed by an account and amount

```
? m Expenses.Food         100.00
? y Expenses.Trips.Other   20.99
```

Transaction entries starts with $ and has the following format

```
$ [date xx.xx.xxxx] [from account] [to account] [amount] '[description]' [(reference)]

$ 12.12.2022 Assets.Visa Expenses.Trips.Other 2100.00 'Went on the beach' 5
$ 12.12.2022 Assets.Visa Expenses.Trips.Other 2100.00 '' 
```

## Commands

```
clear             (clears the terminal and moves the cursor to 0,0)
print             (prints transactions)
balance           (prints balance)
add               (start interactively adding a transaction)
```

## Adding transactions

The program will guide you thruogh adding a transaction. It uses the accounts from the journal, so you must add that first. If you want to save a reference together with the transaction, just drag the file into the terminal while filling out the transaction. The reference is saved in the data directory, see add.c (top).

*Note: When dragging files into the terminal the path is just copied. I tried to support both Linux and WSL, but it is not tested well enough.*

## Command line options

Here are the the various command line options

```
-sort (!) date             (sort by date in ascending order)
-sort (!) amount           (sort by amount in ascending order)
-sort (!) from             (sort by source account in ascending order)
-sort (!) to               (sort by destination account in ascending order)

! reverses the output
```

```
-date * *                  (all transactions, default)
-date * 12.jan             (everything before 12.jan)
-date jan feb              (everything between jan and feb)
-date 2021 *               (everything between since 1.jan.2021)
-date dec                  (everything in december)
-date 12.oct.2022  *       (everything in since 12.oct.2022)
-date sep.2021 3.feb.2023  (everything between 1.sep.2021 and 3.feb.2023)
-date 3.jan                (everything on 3.jan this year or last year)

Note: order are reversed such that the first date is before the last date
```

```
-unify                     (all account activity are displayed in separate transactions,
                           transfers to accounts are shown in positive numbers,
                           transfers from accounts are shown in negative numbers)
```

```
-short                     (only print the last account name, not the categories)
```

```
-monthly                   (group by month)
-quarterly                 (group by quarter)
-yearl                     (group by year)
```

```
-zero                      (prints zeros when showing balance)
```

```
-flat                      (prints zeros when showing balance)
```

```
-running                   (print or use running total, can be used to check real balance)
```

```
-ref                       (show the reference in the transaction list)
```

```
-filter [filter]           (pass selected transactions though the filter)
```

## Filter expressions

The filters may be modified by the program. If that is the case, the final filter is printed after the command. Parenthesis are preserved. Here is example output.

```
command -filter (day < 23 and day > mon + 2) or amount = 20 * (2 + 3 / 1.3)
  Using filter: (day < 23 and weekday > wed) or amount = 86.15
```

Primary expressions are evaluated as follows:

```
to [account name]       (true if account name matches the transaction destination account)
from [account name]     (true if account name matches the transaction source account)
account [account name]  (true if account name matches any of the transaction accounts)
desc '[description]'    (true if the transaction decsription includes the quoted string)
amount                  (amount of the transaction)
day                     (day or weekday of the transaction based on the left hand side)
month                   (month of the transaction)
year                    (year of the transaction)
ref                     (reference present or reference number depending on left hand side)
[number|float|double]   (the given number)
[jan|feb|mar|...]       (jan = 1, feb = 2, ...)
[mon|tue|wed|...]       (mon = 1, tue = 2, ...)
```

Any logical expresion consisting of primary expression can be evaluated. Parenthesized expressions are supported. The following operators are supported.

```
(not) [expression]
```

```
[expression] (+|-|*|/|=|!=|<|>|<=|>=|or|and) [expression]
```

## Autocomplete

Autocomplete can suggest accounts and descriptions when interactively adding transactions based on the journal data. It can also autocomplete accounts when writing filter expressions based on the user input.

## Examples

```
print -date 2023 -sort amount -short -unify -filter account Assets.Visa and amount > (20 * (199 - 64))

Prints all transactions including the Visa account and maching the amount, 
unifies the output such that Visa appears on the left side with the amount sign
showing the direction of the transfer, prints only the last account name and not the path, 
and sorts the output by amount.
```
