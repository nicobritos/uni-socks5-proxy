/**
 * Implementacion de un Sorted Hashmap
 * NO ES THREAD SAFE
 */
#include "sorted_hashmap.h"

typedef struct hashmapCDT {
    int8_t (*cmp)(void *e1, void *e2);
    uint64_t (*hasher)(void *e);

    sorted_hashmap_node *overflow_nodes;
    uint64_t overflow_nodes_length;
    uint64_t total_nodes;
} hashmapCDT;

typedef struct hashmap_nodeCDT {
    void *element;
    sorted_hashmap_node next;
    sorted_hashmap_node previous;
} hashmap_nodeCDT;


/** ----------------- DECLARATIONS ----------------- */
/**
 * Busca un elemento a partir de un starting node
 * @param hashmap
 * @param element
 * @return el nodo | NULL
 */
static sorted_hashmap_node sorted_hashmap_find_from_node_(sorted_hashmap_t hashmap, sorted_hashmap_node node, void *element);
/**
 * Busca un nodo para el cual insertar (sorted). Devuelve NULL si el nodo es NULL.
 * @param hashmap
 * @param element
 * @return el nodo | NULL
 */
static sorted_hashmap_node sorted_hashmap_find_previous_inserting_node_(sorted_hashmap_t hashmap, sorted_hashmap_node node, void *element);
/**
 * Elimina todos los nodos siguientes al pasado por parametro
 * @param node
 */
static void sorted_hashmap_free_starting_node_(sorted_hashmap_node node);


/** ----------------- DEFINITIONS ----------------- */
/**
 * Crea un hashmap
 * @param initial_overflow_length el valor inicial del tamano del array principal (accessible por hashing)
 * @param element
 * @return el mapa creado | NULL
 */
sorted_hashmap_t sorted_hashmap_create(uint64_t initial_overflow_length) {
    sorted_hashmap_t hashmap = calloc(sizeof(*hashmap), 1);
    if (hashmap == NULL) return NULL;

    hashmap->overflow_nodes_length = initial_overflow_length;
    hashmap->overflow_nodes = calloc(sizeof(*hashmap->overflow_nodes), initial_overflow_length);
    if (hashmap->overflow_nodes == NULL) {
        free(hashmap);
        hashmap = NULL;
    }

    return hashmap;
}

/**
 * Busca un elemento
 * @param hashmap
 * @param element
 * @return el nodo | NULL
 */
sorted_hashmap_node sorted_hashmap_find(sorted_hashmap_t hashmap, void *element) {
    if (hashmap == NULL || element == NULL) return NULL;
    if (hashmap->hasher == NULL || hashmap->cmp == NULL) return NULL;
    if (hashmap->total_nodes == 0) return NULL;

    uint64_t hash = hashmap->hasher(element);
    uint64_t index = hash % hashmap->overflow_nodes_length;
    return sorted_hashmap_find_from_node_(hashmap, hashmap->overflow_nodes[index], element);
}

/**
 * Busca un elemento a partir de un starting node
 * @param hashmap
 * @param element
 * @return el nodo | NULL
 */
static sorted_hashmap_node sorted_hashmap_find_from_node_(sorted_hashmap_t hashmap, sorted_hashmap_node node, void *element) {
    if (node == NULL) return NULL;
    int8_t cmp;
    do {
        cmp = hashmap->cmp(node->element, element);
        if (cmp == 0) return node;
        if (cmp == 1) return NULL;
        node = node->next;
    } while (node != NULL);

    return NULL;
}

/**
 * Busca un nodo para el cual insertar (sorted). Devuelve NULL si se inserta al inicio
 * @param hashmap
 * @param element
 * @return el nodo | NULL
 */
static sorted_hashmap_node sorted_hashmap_find_previous_inserting_node_(sorted_hashmap_t hashmap, sorted_hashmap_node node, void *element) {
    if (node == NULL) return NULL;
    int8_t cmp;
    sorted_hashmap_node previous_node = NULL;
    do {
        cmp = hashmap->cmp(node->element, element);
        if (cmp == 0) return node;
        if (cmp == 1) return previous_node;
        previous_node = node;
        node = node->next;
    } while (node != NULL);

    return NULL;
}

/**
 * Agrega o reemplaza un nodo
 * @param hashmap
 * @param node
 */
sorted_hashmap_node sorted_hashmap_add(sorted_hashmap_t hashmap, void *element) {
    if (hashmap == NULL || element == NULL) return NULL;
    if (hashmap->hasher == NULL || hashmap->cmp == NULL) return NULL;
    if (hashmap->total_nodes == 0) return NULL;

    uint64_t hash = hashmap->hasher(element);
    uint64_t index = hash % hashmap->overflow_nodes_length;
    sorted_hashmap_node starting_node = hashmap->overflow_nodes[index];
    sorted_hashmap_node new_node;

    if (starting_node == NULL) {
        /** Tenemos que insertar aca */
        new_node = calloc(sizeof(*new_node), 1);
        if (new_node == NULL) return NULL;
        hashmap->overflow_nodes[index] = new_node;
        new_node->next = NULL;
        new_node->previous = NULL;
    } else {
        sorted_hashmap_node previous_node = sorted_hashmap_find_previous_inserting_node_(hashmap, starting_node, element);
        if (previous_node != NULL && hashmap->cmp(previous_node->element, element) == 0) {
            previous_node->element = element;
            return previous_node;
        }
        new_node = calloc(sizeof(*new_node), 1);
        if (new_node == NULL) return NULL;
        if (previous_node == NULL) {
            /** Es el primer node */
            new_node->next = starting_node;
            starting_node->previous = new_node;
            new_node->previous = NULL;
            hashmap->overflow_nodes[index] = new_node;
        } else {
            new_node->previous = previous_node;
            new_node->next = previous_node->next;
            if (previous_node->next != NULL)
                previous_node->next->previous = new_node;
            previous_node->next = new_node;
        }
    }

    hashmap->total_nodes++;
    new_node->element = element;
    return new_node;
}

/**
 * Obtiene el elemento asociado a un nodo
 * @param node
 * @return element
 */
void *sorted_hashmap_get_element(sorted_hashmap_node node) {
    if (node == NULL) return NULL;
    return node->element;
}

/**
 * Remueve un nodo
 * @param hashmap
 * @param node
 */
void sorted_hashmap_remove(sorted_hashmap_t hashmap, sorted_hashmap_node node) {
    if (hashmap == NULL || node == NULL) return;
    if (hashmap->hasher == NULL || hashmap->cmp == NULL) return;
    if (hashmap->total_nodes == 0) return;

    if (node->previous != NULL) {
        node->previous->next = node->next;
    } else {
        /** Es el primer nodo */
        uint64_t hash = hashmap->hasher(node->element);
        hashmap->overflow_nodes[hash % hashmap->overflow_nodes_length] = node->next;
    }
    if (node->next != NULL) {
        node->next->previous = node->previous;
    }

    hashmap->total_nodes--;
    free(node);
}

/**
 * Elimina un hashmap
 * @param hashmap
 */
void sorted_hashmap_free(sorted_hashmap_t hashmap) {
    if (hashmap == NULL) return;
    uint64_t index = 0;
    while (index < hashmap->overflow_nodes_length) {
        sorted_hashmap_free_starting_node_(hashmap->overflow_nodes[index]);
        index++;
    }
}

/**
 * Elimina todos los nodos siguientes al pasado por parametro
 * @param node
 */
static void sorted_hashmap_free_starting_node_(sorted_hashmap_node node) {
    if (node == NULL) return;
    sorted_hashmap_node next_node;
    do {
        next_node = node->next;
        free(node);
    } while (next_node != NULL);
}

/**
 * Setea la funcion de comparacion
 * @param hashmap
 * @param cmp la funcion de comparacion (sigue estandar C)
 * @return false si el hashmap ya tenia una funcion de comparacion seteada.
 */
bool sorted_hashmap_set_cmp(sorted_hashmap_t hashmap, int8_t (cmp)(void *e1, void *e2)) {
    if (hashmap == NULL || cmp == NULL || hashmap->cmp != NULL) return false;
    hashmap->cmp = cmp;
    return true;
}

/**
 * Setea la funcion de hasheo
 * @param hashmap
 * @param hasher la funcion de hasheo
 * @return false si el hashmap ya tenia una funcion de hasheo seteada.
 */
bool sorted_hashmap_set_hasher(sorted_hashmap_t hashmap, uint64_t (hasher)(void *e)) {
    if (hashmap == NULL || hasher == NULL || hashmap->hasher != NULL) return false;
    hashmap->hasher = hasher;
    return true;
}
