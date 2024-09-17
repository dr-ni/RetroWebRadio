//gcc -I /usr/include/libxml2/ xmltest.c -lxml2 -o xmltest

//./xmltest test.xml storyinfo author

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "xmlparse.h" 

/*
<?xml version="1.0" encoding="utf-8"?>
<story>
<count>2</count>
  <storyinfo>
    <count>3</count>
    <author>John Fleck</author>
    <datewritten>June 2, 2002</datewritten>
    <keyword>تخیلی</keyword>
  </storyinfo>
  <body>
    <headline>This is the headline</headline>
    <para>This is the body text.</para>
  </body>
  <reference uri="www.w3.org/TR/xslt20/"/>
</story>

*/
int die(char *msg){
  fprintf(stderr, "%s", msg);
  return(-1);
}
 
int parseNode(xmlDocPtr doc, xmlNodePtr cur, char *subchild, char *result){
 
    xmlChar *key;
    cur = cur->xmlChildrenNode;
    while (cur != NULL)
    {
        if ((!xmlStrcmp(cur->name, (const xmlChar *)subchild)))
        {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
 
         //   fprintf(stderr, "%s\n", key);
            memcpy(result, key, strlen((char*)key));
            xmlFree(key);
            return 1;
        }
        cur = cur->next;
    }
    return 0;
}

int parse1(const char *docname, char *child, char *result){
 
    xmlDocPtr doc;
    xmlNodePtr cur;
    int reval=0;

    doc = xmlParseFile(docname);
 
    if (doc == NULL )
        return(die("Document parsing failed. \n"));
    cur = xmlDocGetRootElement(doc); //Gets the root element of the XML Doc
    if (cur == NULL){
        xmlFreeDoc(doc);
        return(die("Document is Empty!!!\n"));
    }
    reval=parseNode (doc, cur, child, result);
    xmlFreeDoc(doc);
    return reval;
}


 

int parse2(const char *docname, char *child, char *subchild, char *result){
 
    xmlDocPtr doc;
    xmlNodePtr cur;
    int reval=0;
 
    doc = xmlParseFile(docname);
 
    if (doc == NULL )
        return(die("Document parsing failed. \n"));
    cur = xmlDocGetRootElement(doc); //Gets the root element of the XML Doc
    if (cur == NULL){
        xmlFreeDoc(doc);
        return(die("Document is Empty!!!\n"));
    }
 
    cur = cur->xmlChildrenNode;
    while (cur != NULL){
        if ((!xmlStrcmp(cur->name, (const xmlChar *)child))){
            reval=parseNode (doc, cur, subchild, result);
            xmlFreeDoc(doc);
            return reval;
        }
        cur = cur->next;
    }
    xmlFreeDoc(doc);
    return 0;
}


int parse3(const char *docname, char *child, char *subchild, char *subsubchild, char *result){
 
    xmlDocPtr doc;
    xmlNodePtr cur;
    int reval=0;
 
    doc = xmlParseFile(docname);
 
    if (doc == NULL ){
        return(die("Document parsing failed. \n"));
    }
    cur = xmlDocGetRootElement(doc); //Gets the root element of the XML Doc
    if (cur == NULL){
        xmlFreeDoc(doc);
        die("Document is Empty!!!\n");
    }
 
    cur = cur->xmlChildrenNode;
    while (cur != NULL){
       if ((!xmlStrcmp(cur->name, (const xmlChar *)child))){
          cur = cur->xmlChildrenNode;
          while (cur != NULL){
             if ((!xmlStrcmp(cur->name, (const xmlChar *)subchild))){
                 reval=parseNode (doc, cur, subsubchild, result);
                 xmlFreeDoc(doc);
                 return reval;
             }
             cur = cur->next;
          }
       }
       if (cur == NULL){
          return 0;
       }
       cur = cur->next;
    }
    xmlFreeDoc(doc);
    return 0;
}


