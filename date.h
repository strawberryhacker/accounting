#ifndef DATE_H
#define DATE_H

#include <stdbool.h>

typedef struct {
  int day;
  int month;
  int year;
} Date;

typedef struct {
  union {
    Date date;
    struct {
      int day;
      int month;
      int year;
    };
  };
  int count;
  bool wild;
} OptionsDate;

bool date_is_smaller(Date* a, Date* b);
bool date_is_bigger(Date* a, Date* b);
bool date_is_equal(Date* a, Date* b);
int date_to_weekday(int d, int m, int y);
int get_month(char* data);
int get_day(char* data);
void clean_up_dates(OptionsDate* from, OptionsDate* to);

#endif
