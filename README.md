# Cash

Cash is a small, simple, double entry plain text accounting system, which supports interactivly adding transactions, printing transactions, showing balance, and more. It supports flexible expression based filtering based on accounts, descriptions, amounts, date, and more. It should be trivial to retrive any transaction you are looking for. It also support saving references, receipts, etc. in the transaction by dragging the file into the terminal while adding a transaction.

*Note: error handling is bad*

*Note: date range (-d) could be a lot better*

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

The program will guide you thruogh adding a transaction. It uses the accounts from the journal, so you must add that first. If you want to save a reference together with the transaction, just drag the file into the terminal while filling out the transaction. The reference is saved in the data directory, see add.c (top). Use ESC to go to the previous prompt.

*Note: When dragging files into the terminal the path is just copied. I tried to support both Linux and WSL, but it is not tested well enough.*

## Command line options

Here are the the various command line options

```
-sort (!) date    (sort by date in ascending order)
-sort (!) amount  (sort by amount in ascending order)
-sort (!) from    (sort by source account in ascending order)
-sort (!) to      (sort by destination account in ascending order)

! reverses the output
```

```
-d -date * *                  (all transactions, default)
-d -date * 12.jan             (everything before 12.jan)
-d -date jan feb              (everything between jan and feb)
-d -date 2021 *               (everything between since 1.jan.2021)
-d -date dec                  (everything in december)
-d -date 12.oct.2022  *       (everything in since 12.oct.2022)
-d -date sep.2021 3.feb.2023  (everything between 1.sep.2021 and 3.feb.2023)
-d -date 3.jan                (everything on 3.jan this year or last year)

Note: order are reversed such that the first date is before the last date
```

```
-u -unify               (all account activity are displayed in separate transactions,
                         transfers to accounts are shown in positive numbers,
                         transfers from accounts are shown in negative numbers)
-t -short               (only print the last account name, not the categories)
-m -monthly             (group by month)
-q -quarterly           (group by quarter)
-y -yearl               (group by year)
-z -zero                (prints zeros when showing balance)
-c -flat                (prints zeros when showing balance)
-r -running             (print or use running total, can be used to check real balance)
-l -refs                (show the reference in the transaction list)
-f -filter [filter]     (pass selected transactions though the filter)
-g -nogrid              (remove the grid in the output)
-e -sum                 (unified transaction view only, adds a period based (m/q/y) running sum for the account)
-b -budget              (balance view only, show budget minus the monthly or yearly total)
-p -percent             (shows percent)
```

## Filter expressions

The filters may be modified by the program. If that is the case, the final filter is printed after the command. Parenthesis are preserved. Here is example output.

```
... -filter (day < 23 and day > mon + 2) or amount = 20 * (2 + 3 / 1.3)
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

```
Show adding transaction with autocomplete.

 add
   (ref ...sktop/Capture.PNG)   13.12.2022   NOK 50.00   Assets.Visa  >  Expen
                                                                         Expenses.Food
                                                                         Expenses.Drinks
                                                                         Expenses.Clothing
```

```
Monthly output between dates.

                         nov.2021    dec.2021    mar.2022    apr.2022
   ------------------+-----------+-----------+-----------+-----------
   Assets            |     -23.32|   10000.00|    3000.00|   -1014.00
       Visa          |     -23.32|   10000.00|    3000.00|   -1014.00
       Sparegris     |       0.00|       0.00|       0.00|       0.00
       Cash          |       0.00|       0.00|       0.00|       0.00
   ------------------+-----------+-----------+-----------+-----------
   Expenses          |      23.32|       0.00|       0.00|    1014.00
       Food          |      23.32|       0.00|       0.00|       0.00
       Drinks        |       0.00|       0.00|       0.00|       0.00
       Clothing      |       0.00|       0.00|       0.00|       0.00
       Rent          |       0.00|       0.00|       0.00|     444.00
       Adventures    |       0.00|       0.00|       0.00|       0.00
       Trips         |       0.00|       0.00|       0.00|       0.00
           Abroad    |       0.00|       0.00|       0.00|       0.00
           Home      |       0.00|       0.00|       0.00|       0.00
       School        |       0.00|       0.00|       0.00|       0.00
       Other         |       0.00|       0.00|       0.00|       0.00
       Subscriptions |       0.00|       0.00|       0.00|     570.00
       Interest      |       0.00|       0.00|       0.00|       0.00
   ------------------+-----------+-----------+-----------+-----------
   Income            |       0.00|       0.00|   -3000.00|       0.00
       Salary        |       0.00|       0.00|       0.00|       0.00
       Other         |       0.00|       0.00|   -3000.00|       0.00
   ------------------+-----------+-----------+-----------+-----------
   Equity            |       0.00|  -10000.00|       0.00|       0.00
       OpeningBalance|       0.00|  -10000.00|       0.00|       0.00
   ------------------+-----------+-----------+-----------+-----------
   Liabilities       |       0.00|       0.00|       0.00|       0.00
       CreditCard    |       0.00|       0.00|       0.00|       0.00
       StudentLoan   |       0.00|       0.00|       0.00|       0.00
```

```
Yearly output without zeros.

                             2021        2022        2023
   ------------------+-----------+-----------+-----------
   Assets            |    9976.68|     982.05|  491633.00
       Visa          |    9976.68|     985.05|  491633.00
       Sparegris     |           |   -1594.00|
       Cash          |           |    1591.00|
   ------------------+-----------+-----------+-----------
   Expenses          |      23.32|    2251.95|    3367.00
       Food          |      23.32|    1584.96|    3080.00
       Drinks        |           |    -234.00|
       Clothing      |           |     687.00|
       Rent          |           |    -356.00|    -213.00
       Adventures    |           |      34.00|
       Trips         |           |           |
           Abroad    |           |           |
           Home      |           |           |
       School        |           |    -468.00|
       Other         |           |           |
       Subscriptions |           |     769.99|
       Interest      |           |     234.00|     500.00
   ------------------+-----------+-----------+-----------
   Income            |           |   -3234.00|
       Salary        |           |    -234.00|
       Other         |           |   -3000.00|
   ------------------+-----------+-----------+-----------
   Equity            |  -10000.00|       3.00|
       OpeningBalance|  -10000.00|       3.00|
   ------------------+-----------+-----------+-----------
   Liabilities       |           |      -3.00| -495000.00
       CreditCard    |           |           |
       StudentLoan   |           |      -3.00| -495000.00
```

```
Quarterly output filtered on accounts.

                          q3.2021     q1.2022     q2.2022     q3.2022     q1.2023
   ------------------+-----------+-----------+-----------+-----------+-----------
   Assets            |    9976.68|    1986.00|    -473.32|    -530.63|  491633.00
       Visa          |    9976.68|    1986.00|    -473.32|    -527.63|  491633.00
       Sparegris     |       0.00|       0.00|       0.00|   -1594.00|       0.00
       Cash          |       0.00|       0.00|       0.00|    1591.00|       0.00
   ------------------+-----------+-----------+-----------+-----------+-----------
   Expenses          |      23.32|    1014.00|     473.32|     764.63|    3367.00
       Rent          |       0.00|     444.00|       0.00|    -800.00|    -213.00
```

```
Output without grid and flat account view.

                              nov.2021    dec.2021    mar.2022    apr.2022
   Assets.Visa                  -23.32    10000.00     3000.00    -1014.00
   Assets.Sparegris               0.00        0.00        0.00        0.00
   Assets.Cash                    0.00        0.00        0.00        0.00
   Expenses.Food                 23.32        0.00        0.00        0.00
   Expenses.Drinks                0.00        0.00        0.00        0.00
   Expenses.Clothing              0.00        0.00        0.00        0.00
   Expenses.Rent                  0.00        0.00        0.00      444.00
   Expenses.Adventures            0.00        0.00        0.00        0.00
   Expenses.Trips.Abroad          0.00        0.00        0.00        0.00
   Expenses.Trips.Home            0.00        0.00        0.00        0.00
   Expenses.School                0.00        0.00        0.00        0.00
   Expenses.Other                 0.00        0.00        0.00        0.00
   Expenses.Subscriptions         0.00        0.00        0.00      570.00
   Expenses.Interest              0.00        0.00        0.00        0.00
   Income.Salary                  0.00        0.00        0.00        0.00
   Income.Other                   0.00        0.00    -3000.00        0.00
   Equity.OpeningBalance          0.00   -10000.00        0.00        0.00
   Liabilities.CreditCard         0.00        0.00        0.00        0.00
   Liabilities.StudentLoan        0.00        0.00        0.00        0.00
```

```
Budget view. Only budget account are showed.

                              nov.2021    dec.2021    mar.2022    apr.2022
   Expenses.Food               3976.68     4000.00     4000.00     4000.00
   Expenses.Drinks              500.00      500.00      500.00      500.00
   Expenses.Clothing            500.00      500.00      500.00      500.00
   Expenses.Rent               4736.67     4736.67     4736.67     4292.67
   Expenses.Adventures          300.00      300.00      300.00      300.00
   Expenses.School               41.67       41.67       41.67       41.67
   Expenses.Other               500.00      500.00      500.00      500.00
   Expenses.Subscriptions       400.00      400.00      400.00     -170.00
   Expenses.Interest            250.00      250.00      250.00      250.00
   Income.Salary              10000.00    10000.00    10000.00    10000.00
```

```
Default transaction view.

   12.dec.2022 | Assets.Visa             | Expenses.Food           |        2.00 | This is a description.
   12.dec.2022 | Assets.Sparegris        | Expenses.Clothing       |        3.00 | This is
   12.dec.2022 | Expenses.School         | Expenses.Interest       |      234.00 | Testing
   12.dec.2022 | Liabilities.StudentLoan | Equity.OpeningBalance   |        3.00 | This is a test
   12.dec.2022 | Income.Salary           | Assets.Visa             |      234.00 | Testing
   12.dec.2022 | Assets.Visa             | Expenses.Food           |       23.32 | This is a description.
   12.dec.2022 | Assets.Visa             | Expenses.Food           |       23.32 | This is a description.
   13.dec.2022 | Expenses.Rent           | Assets.Visa             |      234.00 | SiT
   ------------+-------------------------+-------------------------+-------------+--------------
   01.jan.2023 | Assets.Visa             | Expenses.Food           |      234.00 |
   01.jan.2023 | Assets.Visa             | Expenses.Food           |      234.00 | yyyyyyyyyyyyyyyyyyyyy
   01.jan.2023 | Assets.Visa             | Expenses.Food           |      234.00 |
   01.jan.2023 | Assets.Visa             | Expenses.Food           |        1.00 | est
   01.jan.2023 | Assets.Visa             | Expenses.Food           |        1.00 | aoesuth
   01.jan.2023 | Assets.Visa             | Expenses.Food           |        1.00 | Testing
   01.jan.2023 | Expenses.Rent           | Assets.Visa             |      213.00 | Vi kjopte noe fisk
   02.jan.2023 | Assets.Visa             | Expenses.Food           |        2.00 | test
   03.jan.2023 | Liabilities.StudentLoan | Assets.Visa             |   500000.00 | Starting.
   04.jan.2023 | Assets.Visa             | Expenses.Food           |       23.00 |
   04.jan.2023 | Assets.Visa             | Expenses.Interest       |      500.00 | Starting.
   04.jan.2023 | Assets.Visa             | Liabilities.StudentLoan |     5000.00 | Lan
   05.jan.2023 | Assets.Visa             | Expenses.Food           |     2345.00 | Vi kjopte noe fisk
   ------------+-------------------------+-------------------------+-------------+--------------
   02.feb.2023 | Assets.Visa             | Expenses.Food           |        5.00 |
```

```
Transaction view with short names, unified output, with balance and running sum.

   12.dec.2022   Food             Visa                   23.32       1608.28       1561.64   This is a description.
   13.dec.2022   Rent             Visa                 -234.00       -356.00       -800.00   SiT
   13.dec.2022   Visa             Rent                  234.00      10961.73       -293.63   SiT

   01.jan.2023   Visa             Food                 -234.00      10727.73       -234.00
   01.jan.2023   Food             Visa                  234.00       1842.28        234.00
   01.jan.2023   Visa             Food                 -234.00      10493.73       -468.00   yyyyyyyyyyyyyyyyyyyyy
   01.jan.2023   Food             Visa                  234.00       2076.28        468.00   yyyyyyyyyyyyyyyyyyyyy
   01.jan.2023   Visa             Food                 -234.00      10259.73       -702.00
   01.jan.2023   Food             Visa                  234.00       2310.28        702.00
   01.jan.2023   Visa             Food                   -1.00      10258.73       -703.00   est
   01.jan.2023   Food             Visa                    1.00       2311.28        703.00   est
   01.jan.2023   Visa             Food                   -1.00      10257.73       -704.00   aoesuth
   01.jan.2023   Food             Visa                    1.00       2312.28        704.00   aoesuth
   01.jan.2023   Visa             Food                   -1.00      10256.73       -705.00   Testing
   01.jan.2023   Food             Visa                    1.00       2313.28        705.00   Testing
   01.jan.2023   Rent             Visa                 -213.00       -569.00       -213.00   Vi kjopte noe fisk
   01.jan.2023   Visa             Rent                  213.00      10469.73       -492.00   Vi kjopte noe fisk
   02.jan.2023   Visa             Food                   -2.00      10467.73       -494.00   test
   02.jan.2023   Food             Visa                    2.00       2315.28        707.00   test
   03.jan.2023   StudentLoan      Visa              -500000.00    -500003.00    -500000.00   Starting.
   03.jan.2023   Visa             StudentLoan        500000.00     510467.73     499506.00   Starting.
   04.jan.2023   Visa             Food                  -23.00     510444.73     499483.00
   04.jan.2023   Food             Visa                   23.00       2338.28        730.00
   04.jan.2023   Visa             Interest             -500.00     509944.73     498983.00   Starting.
   04.jan.2023   Interest         Visa                  500.00        734.00        500.00   Starting.
   04.jan.2023   Visa             StudentLoan         -5000.00     504944.73     493983.00   Lan
   04.jan.2023   StudentLoan      Visa                 5000.00    -495003.00    -495000.00   Lan
   05.jan.2023   Visa             Food                -2345.00     502599.73     491638.00   Vi kjopte noe fisk
   05.jan.2023   Food             Visa                 2345.00       4683.28       3075.00   Vi kjopte noe fisk

   02.feb.2023   Visa             Food                   -5.00     502594.73         -5.00
   02.feb.2023   Food             Visa                    5.00       4688.28          5.00
```

