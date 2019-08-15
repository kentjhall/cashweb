#ifndef _MYLIST_H_
#define _MYLIST_H_

#include <stdbool.h>

typedef struct Node {
    void *data;
    struct Node *next;
} Node;

typedef struct List {
    struct Node *head;
} List;

// initialize empty list
static inline void initList(struct List *list)
{
    list->head = 0;
}

// adds a node with specified data to front of list
// returns that node on success, or NULL on failure
struct Node *addFront(struct List *list, void *data);

// traverses the list, calling f() on each data item
void traverseList(struct List *list, void (*f)(void *));

// traverses the list, comparing each data item with specified dataSought using compar()
// compar() returns 0 on match and non-zero value otherwise
struct Node *findNode(struct List *list, const void *dataSought,
	int (*compar)(const void *, const void *));

// returns 1 if list is empty, 0 otherwise
static inline int isEmptyList(struct List *list)
{
    return (list->head == 0);
}

// removes first node from the list, deallocating memory
// returns the data pointer that was stored in the node, or NULL on empty list
void *popFront(struct List *list);

// returns data pointer from first node of list, or NULL on empty list
void *peekFront(struct List *list);

// returns data pointer from last node of list, or NULL on empty list
void *peekLast(struct List *list);

// returns data pointer from 'i'th node of list, or NULL if not there
void *peekAt(struct List *list, size_t i);

// removes all nodes from list, deallocating memory for nodes (and data if freeData is true)
// freeData should only be set true if all data is heap-allocated
void removeAllNodes(struct List *list, bool freeData);

// creates a node holding the given data pointer, adds it right after prevNode
// if prevNode is NULL, is added to the front of the list
// behavior is undefined if prevNode does not belong to given list
struct Node *addAfter(struct List *list, 
	struct Node *prevNode, void *data);

// reverses list
void reverseList(struct List *list);

#endif /* #ifndef _MYLIST_H_ */
