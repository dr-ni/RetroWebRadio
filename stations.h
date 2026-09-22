/*
 * RetroWebRadio - station list loader
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#ifndef STATIONS_H
#define STATIONS_H

#define STATION_NAME_MAX 128
#define STATION_URL_MAX  512

struct station {
  char name[STATION_NAME_MAX];
  char url[STATION_URL_MAX];
};

struct page {
  int count;                 /* number of stations on this page */
  struct station *stations;  /* count entries */
};

struct stationlist {
  int pages;                 /* number of pages */
  struct page *page;         /* pages entries, index 0 == <p1> */
};

/*
 * Parse the stations XML file once. Returns 0 on success, -1 on error
 * (an error message has been printed to stderr in that case).
 */
int stations_load(const char *filename, struct stationlist *list);
void stations_free(struct stationlist *list);

#endif
