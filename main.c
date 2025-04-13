#include <stdio.h>
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
#define VARLIST_AT_OR_NULL(varlist, index) ((index) < (varlist).Count ? &(varlist).Ptr[index] : NULL)

#define LOC_REGISTER(index) (location) {.Type = LOCTYPE_REG, .As.Register = index}
#define LOC_MEMORY(index) (location) {.Type = LOCTYPE_MEM, .As.Register = index}


typedef struct {
    unsigned FreeReg[2];
} register_allocator;
#define RegAlloc_RegisterCount(p_regalloc) 2
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
void RegAlloc_Dealloc(register_allocator *Alloc, int Register)
{
    assert(Register < RegAlloc_RegisterCount(Alloc));
    Alloc->FreeReg[Register] = 1;
}

typedef int program_stack_allocator;
#define ProgStack_Init() 0
#define ProgStack_Allocate(p_progstack) (*(p_progstack))++

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

typedef struct 
{
    variable_array Active;
    variable_array LiveIntervalByStart;
} register_allocation_result;


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



static void PrintInterval(const variable_array Vars, unsigned Start, unsigned End)
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

/* linear scan, implemented using Wikipedia's pseudocode, 
 * but with ExpireOldInterval(i) and SpillAtInterval(i) inlined, 
 * assuming we're operating on the variable list and it is already sorted by increasing starting point (reasonable for a normal program) */
static void LinearScanRegAlloc(register_allocator *RegAlloc, program_stack_allocator *ProgStack, variable_array *Vars)
{
    /* variables that are being used in register and is currently active, sorted by increasing end point */
    variable_list Active = VarList_Init();

    VARLIST_FOREACH(*Vars, i)
    {
        /* bookkeeping ActiveInReg, removing any residuals that have passed their end point */
        variable *CurrInActive = Active.Head;
        while (CurrInActive)
        {
            if (CurrInActive->End >= i->Start)
                break;

            /* remove the residual */
            RegAlloc_Dealloc(RegAlloc, CurrInActive->Location.As.Register);
            variable *Next = CurrInActive->Next;
            VarList_Remove(&Active, CurrInActive);

            /* iterate to the next node */
            CurrInActive = Next;
        }

        /* after removing residual end points, try to allocate a register for i */
        /* if there are still registers left, use it */
        if (Active.Count != RegAlloc_RegisterCount(RegAlloc))
        {
            i->Location = LOC_REGISTER(RegAlloc_Allocate(RegAlloc));
            VarList_InsertInEndPointOrder(&Active, i);
        }
        /* no register available, but if i has a shorter lifetime than the longest living active variable, 
         * we prefer i to be in a register instead of such variable */
        else if (Active.Tail->End > i->End)
        {
            /* spill the undesirable variable into memory */
            variable *Spill = Active.Tail;
            Spill->Location = LOC_MEMORY(ProgStack_Allocate(ProgStack));
            VarList_Remove(&Active, Active.Tail);

            /* make i a register variable and append it to the active variable in register list */
            i->Location = LOC_REGISTER(Spill->Location.As.Register);
            VarList_InsertInEndPointOrder(&Active, i);
        }
        /* as a last resort, i will reside in memory */
        else
        {
            i->Location = LOC_MEMORY(ProgStack_Allocate(ProgStack));
        }
    }
}


int main(void)
{
    variable Vars[] = {
        { .Name = "A", .Start = 0, .End = 10 },
        { .Name = "B", .Start = 1, .End = 9 },
        { .Name = "C", .Start = 2, .End = 8 },
        { .Name = "D", .Start = 3, .End = 7 },
    };
    variable_array VarArray = {
        .Ptr = Vars,
        .Count = STATIC_ARRAY_SIZE(Vars),
        .Capacity = STATIC_ARRAY_SIZE(Vars)
    };

    register_allocator RegAlloc = RegAlloc_Init();
    program_stack_allocator ProgStack = ProgStack_Init();
    LinearScanRegAlloc(&RegAlloc, &ProgStack, &VarArray);

    printf("reg: %d, stack: %d\n", RegAlloc.FreeReg[0] + RegAlloc.FreeReg[1], ProgStack);
    printf("Live: \n");
    PrintInterval(VarArray, 0, 10);
    return 0;
}

