#include <stdlib.h>

#include "mylist.h"

struct Node *addFront(struct List *list, void *data) {
	struct Node *n = malloc(sizeof(struct Node));
	if (n == NULL) {
		return n;
	}
	n->data = data;
	n->next = list->head;
	list->head = n;
	return n;
}

void traverseList(struct List *list, void (*f)(void *)) {
	struct Node *t = list->head;
	while (t) {
		f(t->data);
		t = t->next;
	}
}

struct Node *findNode(struct List *list, const void *dataSought, int (*compar)(const void *, const void *)) {
        struct Node *t = list->head;
        while (t && compar(t->data, dataSought)) {
                t = t->next;
        }
        return t;
}

void *popFront(struct List *list) {
	if (isEmptyList(list)) {
		return NULL;
	}
	struct Node *n = list->head;
	list->head = n->next;
	void *d = n->data;
	free(n);
	return d;
}

void *peekFront(struct List *list) {
	if (isEmptyList(list)) { return NULL; }
	return list->head->data;	
}

void *peekLast(struct List *list) {
	if (isEmptyList(list)) { return NULL; }
	struct Node *n = list->head;
	while (n->next) { n = n->next; }
	return n->data;
}

void removeAllNodes(struct List *list, bool freeData) {
	while (list->head) {
		void *d = popFront(list);
		if (freeData && d) { free(d); }
	}
}

struct Node *addAfter(struct List *list, struct Node *prevNode, void *data) {
	if (prevNode == NULL) {
		return addFront(list, data);
	}

	struct Node *n = malloc(sizeof(struct Node));
	if (n == NULL) {
		return n;
	}
	n->data = data;
	n->next = prevNode->next;
	prevNode->next = n;
	return n;
}

void reverseList(struct List *list) {
	struct Node *c = list->head;
	struct Node *n;

	while (c && c->next) {
		n = c->next;
		c->next = n->next;
		n->next = list->head;
		list->head = n;
	}
}
