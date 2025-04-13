#ifndef UTILS_H
#define UTILS_H

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define IN_RANGE(lower, n, upper) ((lower) <= (n) && (n) <= (upper))
#define SWAP(type, a, b) do {\
    type t = a;\
    a = b;\
    b = t;\
} while (0)

#define VARLIST_FOREACH(varlist, iter_name) for (variable *iter_name = (varlist).Ptr; iter_name < (varlist).Ptr + (varlist).Count; iter_name++)

#define LOC_REGISTER(index) (location) {.Type = LOCTYPE_REG, .As.Register = index}
#define LOC_MEMORY(index) (location) {.Type = LOCTYPE_MEM, .As.Memory = index}

#define RegAlloc_RegisterCount(p_regalloc) 2

#define ProgStack_Init() 0
#define ProgStack_Allocate(p_progstack) (*(p_progstack))++



typedef int program_stack_allocator;

typedef struct {
    unsigned FreeReg[RegAlloc_RegisterCount(NULL)];
} register_allocator;

typedef enum {
    LOCTYPE_MEM,
    LOCTYPE_REG,
} location_type;
typedef struct 
{
    location_type Type;
    union {
        unsigned Register;
        unsigned Memory;
    } As;
} location;

typedef struct variable
{
    const char *Name;
    unsigned Start, End;
    location Location;

    struct variable *Next, *Prev;
} variable;

typedef struct 
{
    variable *Ptr;
    unsigned Count, Capacity;
} variable_array;

typedef struct 
{
    /* non owning pointers */
    variable *Head;
    variable *Tail;
    unsigned Count;
} variable_list;



register_allocator RegAlloc_Init(void);
int RegAlloc_Allocate(register_allocator *Alloc);
int RegAlloc_FreeRegisterCount(const register_allocator *Alloc);
void RegAlloc_Dealloc(register_allocator *Alloc, int Register);

variable_list VarList_Init(void);
void VarList_InsertInEndPointOrder(variable_list *List, variable *Var);
void VarList_Remove(variable_list *List, variable *Node);

void PrintInterval(const variable_array Vars, unsigned Start, unsigned End);


#ifdef UTILS_IMPL

register_allocator RegAlloc_Init(void) 
{
    register_allocator This = { 0 };
    for (unsigned i = 0; i < RegAlloc_RegisterCount(&This); i++)
        This.FreeReg[i] = 1;
    return This;
}

int RegAlloc_Allocate(register_allocator *Alloc)
{
    for (unsigned i = 0; i < RegAlloc_RegisterCount(Alloc); i++)
    {
        if (Alloc->FreeReg[i])
        {
            Alloc->FreeReg[i] = 0;
            return i;
        }
    }
    assert(0 && "unreachable");
    return -1;
}

int RegAlloc_FreeRegisterCount(const register_allocator *Alloc)
{
    int Sum = 0;
    for (unsigned i = 0; i < RegAlloc_RegisterCount(Alloc); i++)
        Sum += Alloc->FreeReg[0];
    return Sum;
}

void RegAlloc_Dealloc(register_allocator *Alloc, int Register)
{
    assert(Register < RegAlloc_RegisterCount(Alloc));
    Alloc->FreeReg[Register] = 1;
}



variable_list VarList_Init(void)
{
    return (variable_list) { 0 };
}

void VarList_InsertInEndPointOrder(variable_list *List, variable *Var)
{
    assert(Var && "VarList_InsertInEndPointOrder() unreachable");
    Var->Prev = NULL;
    Var->Next = NULL;

    List->Count++;
    if (!List->Head)
    {
        List->Head = Var;
        List->Tail = Var;
        return;
    }
    assert(List->Tail && "VarList_InsertInEndPointOrder() unreachable");
    if (List->Head->End > Var->End)
    {
        Var->Next = List->Head;
        List->Head->Prev = Var;
        return;
    }
    if (List->Tail->End < Var->End)
    {
        Var->Prev = List->Tail;
        List->Tail->Next = Var;
        List->Tail = Var;
        return;
    }
    
    variable *Curr = List->Head;
    while (Curr && Curr->End < Var->End)
    {
        Curr = Curr->Next;
    }

    assert(Curr->Prev && Curr->Next && "VarList_InsertInEndPointOrder() unreachable");
    variable *Prev = Curr->Prev;
    Prev->Next = Var;
    Var->Prev = Prev;
    Var->Next = Curr;
    Curr->Prev = Var;
}

void VarList_Remove(variable_list *List, variable *Node)
{
    assert(NULL != Node && "VarList_Remove");
    if (0 == List->Count)
    {
        return;
    }
    List->Count--;

    variable *Prev = Node->Prev;
    variable *Next = Node->Next;
    if (Prev)
        Prev->Next = Next;
    else
        List->Head = Next;
    if (Next)
        Next->Prev = Prev;
    else 
        List->Tail = Prev;

    Node->Next = NULL;
    Node->Prev = NULL;
}


void PrintInterval(const variable_array Vars, unsigned Start, unsigned End)
{
    /* print names */
    for (unsigned i = 0; i < Vars.Count; i++)
    {
        printf("%s  ", Vars.Ptr[i].Name);
    }
    printf("\n");

    /* print when the variables are active */
    for (unsigned i = Start; i <= End; i++)
    {
        for (const variable *Var = Vars.Ptr; 
            Var < Vars.Ptr + Vars.Count; 
            Var++)
        {
            if (IN_RANGE(Var->Start, i, Var->End))
            {
                int Location = 'r', 
                    Value = Var->Location.As.Register;
                if (Var->Location.Type == LOCTYPE_MEM)
                {
                    Location = 'm';
                    Value = Var->Location.As.Memory;
                }

                printf("%c%d ", Location, Value);
            }
            else
            {
                printf("   ");
            }
        }
        printf("\n");
    }
}

#endif /* UTILS_IMPL */
#endif /* UTILS_H */


