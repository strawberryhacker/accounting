#include "date.h"
#include "basic.h"
#include <time.h>

char* month_names[12] = {"jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"};
char* day_names[7]    = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

bool date_is_smaller(Date* a, Date* b) {
  if (a->year == b->year) {
    if (a->month == b->month) {
      return a->day < b->day;
    }

    return a->month < b->month;
  }

  return a->year < b->year;
}

bool date_is_equal(Date* a, Date* b) {
  return (a->day == b->day) && (a->month == b->month) && (a->year == b->year);
}

bool date_is_bigger(Date* a, Date* b) {
  return !date_is_smaller(a, b) && !date_is_equal(a, b);
}

int date_to_weekday(int d, int m, int y) {
    return (d += m < 3 ? y-- : y - 2, 23 * m / 9 + d + 4 + y / 4- y / 100 + y / 400) % 7;
}

static int get_day_or_month_index(char* data, char** list, int count) {
  for (int i = 0; i < count; i++) {
    debug("Comparing %.3s and %s\n", data, list[i]);
    if (strncasecmp(data, list[i], 3) == 0) return i + 1;
  }
  return 0;
}

int get_month(char* data) {
  debug("Date : %s\n", data);
  int r = get_day_or_month_index(data, month_names, 12);
  debug("Date-: %s\n", data);
  return r;
}

int get_day(char* data) {
  return get_day_or_month_index(data, day_names, 7);
}

static void fix_incomplete_date(OptionsDate* date, bool end) {
  int day, month, year;
  time_t t = time(NULL);
  struct tm tm  = *localtime(&t);

  day   = tm.tm_mday;
  month = tm.tm_mon + 1;
  year  = tm.tm_year + 1900;

  int month_limit = (end) ? 12 : 1;
  int day_limit = (end) ? 31 : 1;

  if (date->wild) {
    if (end) {
      date->day = day;
      date->month = month;
      date->year = year;
    } else {
      date->day = 1;
      date->month = 1;
      date->year = 1;
    }
  }

  if (date->count == 0) return;

  if (date->count == 1) {
    if (1000 <= date->day && date->day <= 3000) {
      date->year = date->day;
      date->month = month_limit;
      date->day = day_limit;
    } else {
      date->month = date->day;
      date->year = (date->month <= month) ? year : year - 1;
      date->day = day_limit;
    }
  } else if (date->count == 2) {
    if (1000 <= date->month && date->month <= 3000) {
      date->year = date->month;
      date->month = date->day;
      date->day = day_limit;
    } else {
      date->year = year;
      if (month == date->month) {
        if (day < date->day) {
          date->year--;
        }
      } else if (month < date->month) {
        date->year--;
      }
    }
  }
}

void clean_up_dates(OptionsDate* from, OptionsDate* to) {
  bool from_valid = from->count || from->wild;
  bool to_valid   = to->count   || to->wild;

  if (from_valid) {
    if (!to_valid) {
      *to = *from;
    }

    fix_incomplete_date(from, false);
    fix_incomplete_date(to,   true);

    if (date_is_bigger(&from->date, &to->date)) {
      OptionsDate tmp = *from;
      *from = *to;
      *to = tmp;
    }
  }
}
