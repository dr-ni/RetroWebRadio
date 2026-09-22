/*
 * RetroWebRadio - station list loader
 *
 * Reads stations.xml once at startup:
 *
 *   <global>
 *     <pages>N</pages>
 *     <p1>
 *       <count>M</count>
 *       <s1><name>...</name><url>...</url></s1>
 *       ...
 *     </p1>
 *     ...
 *   </global>
 *
 * Pages and stations are ordered by the number in their element name
 * (p1, p2, ... / s1, s2, ...). <pages> and <count> are only used for a
 * consistency warning; the actual content of the file is authoritative.
 *
 * GNU GENERAL PUBLIC LICENSE Version 3
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libxml/parser.h>
#include "stations.h"

/* "p12" with prefix 'p' -> 12, anything else -> 0 */
static int indexed_name(const xmlChar *name, char prefix)
{
  const char *s = (const char *)name;
  char *end;
  long n;

  if (s[0] != prefix || !isdigit((unsigned char)s[1]))
    return 0;
  n = strtol(s + 1, &end, 10);
  if (*end != '\0' || n < 1 || n > 10000)
    return 0;
  return (int)n;
}

static int is_elem(xmlNodePtr n, const char *name)
{
  return n->type == XML_ELEMENT_NODE && !xmlStrcmp(n->name, (const xmlChar *)name);
}

/* copy the text content of child element <tag> into dst (always terminated) */
static int child_text(xmlNodePtr parent, const char *tag, char *dst, size_t size)
{
  xmlNodePtr c;
  xmlChar *txt;
  size_t len;

  dst[0] = '\0';
  for (c = parent->children; c != NULL; c = c->next) {
    if (!is_elem(c, tag))
      continue;
    txt = xmlNodeGetContent(c);
    if (txt == NULL)
      return 0;
    /* trim surrounding whitespace */
    {
      const char *b = (const char *)txt;
      while (isspace((unsigned char)*b))
        b++;
      len = strlen(b);
      while (len > 0 && isspace((unsigned char)b[len - 1]))
        len--;
      if (len >= size) {
        fprintf(stderr, "stations: <%s> too long, truncated: %.40s...\n", tag, b);
        len = size - 1;
      }
      memcpy(dst, b, len);
      dst[len] = '\0';
    }
    xmlFree(txt);
    return 1;
  }
  return 0;
}

static int max_index(xmlNodePtr parent, char prefix)
{
  xmlNodePtr c;
  int n, max = 0;

  for (c = parent->children; c != NULL; c = c->next)
    if (c->type == XML_ELEMENT_NODE && (n = indexed_name(c->name, prefix)) > max)
      max = n;
  return max;
}

static long child_long(xmlNodePtr parent, const char *tag)
{
  char buf[32];
  if (!child_text(parent, tag, buf, sizeof(buf)))
    return -1;
  return strtol(buf, NULL, 10);
}

static int load_page(xmlNodePtr p, int pidx, struct page *page)
{
  int nmax = max_index(p, 's');
  struct station *tmp;
  char *used;
  xmlNodePtr c;
  int i, n, count = 0;
  long declared;

  page->count = 0;
  page->stations = NULL;
  if (nmax == 0)
    return 0;

  tmp = calloc((size_t)nmax, sizeof(*tmp));
  used = calloc((size_t)nmax, 1);
  if (tmp == NULL || used == NULL) {
    free(tmp);
    free(used);
    return -1;
  }

  for (c = p->children; c != NULL; c = c->next) {
    if (c->type != XML_ELEMENT_NODE || (n = indexed_name(c->name, 's')) == 0)
      continue;
    if (used[n - 1]) {
      fprintf(stderr, "stations: p%d/s%d defined twice, ignoring second\n", pidx, n);
      continue;
    }
    child_text(c, "name", tmp[n - 1].name, sizeof(tmp[n - 1].name));
    child_text(c, "url", tmp[n - 1].url, sizeof(tmp[n - 1].url));
    if (tmp[n - 1].url[0] == '\0') {
      fprintf(stderr, "stations: p%d/s%d has no <url>, skipped\n", pidx, n);
      continue;
    }
    if (tmp[n - 1].name[0] == '\0')
      snprintf(tmp[n - 1].name, sizeof(tmp[n - 1].name), "%d-%d", pidx, n);
    used[n - 1] = 1;
  }

  /* compact, keeping the numeric order */
  for (i = 0; i < nmax; i++)
    if (used[i])
      tmp[count++] = tmp[i];
  free(used);

  declared = child_long(p, "count");
  if (declared >= 0 && declared != count)
    fprintf(stderr, "stations: p%d declares <count>%ld but has %d stations\n",
            pidx, declared, count);

  page->count = count;
  page->stations = tmp;
  return 0;
}

int stations_load(const char *filename, struct stationlist *list)
{
  xmlDocPtr doc;
  xmlNodePtr root, c;
  struct page *pages;
  int pmax, n, np = 0;
  long declared;

  list->pages = 0;
  list->page = NULL;

  doc = xmlReadFile(filename, NULL, XML_PARSE_NONET);
  if (doc == NULL) {
    fprintf(stderr, "stations: cannot parse %s\n", filename);
    return -1;
  }
  root = xmlDocGetRootElement(doc);
  if (root == NULL) {
    fprintf(stderr, "stations: %s is empty\n", filename);
    xmlFreeDoc(doc);
    return -1;
  }

  pmax = max_index(root, 'p');
  pages = calloc(pmax > 0 ? (size_t)pmax : 1, sizeof(*pages));
  if (pages == NULL) {
    xmlFreeDoc(doc);
    return -1;
  }

  /* load in numeric order p1..pN, independent of element order in file */
  for (n = 1; n <= pmax; n++) {
    for (c = root->children; c != NULL; c = c->next) {
      if (c->type != XML_ELEMENT_NODE || indexed_name(c->name, 'p') != n)
        continue;
      if (load_page(c, n, &pages[np]) < 0) {
        list->page = pages;
        list->pages = np;
        stations_free(list);
        xmlFreeDoc(doc);
        return -1;
      }
      if (pages[np].count > 0)
        np++;               /* drop empty pages */
      break;                /* first <pN> wins */
    }
  }

  declared = child_long(root, "pages");
  if (declared >= 0 && declared != np)
    fprintf(stderr, "stations: <pages>%ld</pages> but %d non-empty pages found\n",
            declared, np);

  xmlFreeDoc(doc);

  if (np == 0) {
    fprintf(stderr, "stations: no stations in %s\n", filename);
    free(pages);
    return -1;
  }
  list->pages = np;
  list->page = pages;
  return 0;
}

void stations_free(struct stationlist *list)
{
  int i;

  for (i = 0; i < list->pages; i++)
    free(list->page[i].stations);
  free(list->page);
  list->page = NULL;
  list->pages = 0;
}
