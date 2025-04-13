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

typedef struct 
{
    const char *Name;
    unsigned Start, End;
    location Location;
} variable;

typedef struct 
{
    variable *Ptr;
    unsigned Count, Capacity;
} variable_list;

typedef struct 
{
    variable_list Active;
    variable_list LiveIntervalByStart;
} register_allocation_result;



variable_list VarList_Init(void)
{
    return (variable_list) { 0 };
}

variable_list VarList_CopySortedByStartPoint(const variable_list Copy)
{
    if (0 == Copy.Count)
    {
        return VarList_Init();
    }

    variable_list New = {
        .Ptr = malloc(Copy.Count * sizeof(Copy.Ptr[0])),
        .Count = Copy.Count,
        .Capacity = Copy.Count,
    };
    assert(NULL != New.Ptr);

    for (unsigned i = 0; i < Copy.Count; i++)
    {
        New.Ptr[i] = Copy.Ptr[i];
        unsigned j = i;
        while (j > 0 
        && New.Ptr[j - 1].Start > New.Ptr[j].Start)
        {
            SWAP(variable, New.Ptr[j - 1], New.Ptr[j]);
            j--;
        }
    }
    return New;
}

#define VarList_EnsurePush(p_list) \
    if ((p_list)->Count + 1 >= (p_list)->Capacity) \
        VarList_EnsureCapacity(p_list, ((p_list)->Count + 1) * 2)
void VarList_EnsureCapacity(variable_list *List, unsigned NewCapacity)
{
    variable *Tmp = realloc(List->Ptr, NewCapacity * sizeof(Tmp[0]));
    assert(NULL != Tmp);
    List->Ptr = Tmp;
    List->Capacity = NewCapacity;
}

void VarList_Push(variable_list *List, const variable *Var)
{
    VarList_EnsurePush(List);
    List->Ptr[List->Count] = *Var;
    List->Count++;
}

void VarList_Pop(variable_list *List)
{
    List->Count -= (List->Count > 0);
}

void VarList_InsertInEndPointOrder(variable_list *List, const variable *Var)
{
    VarList_EnsurePush(List);
    variable *Ptr = List->Ptr;
    if (List->Count == 0)
    {
        Ptr[0] = *Var;
        List->Count++;
        return;
    }
    unsigned Start = 0;
    unsigned End = List->Count;
    unsigned Mid = End/2;

    while (Start + 1 < End)
    {
        if (Var->End < Ptr[Mid].End)
        {
            End = Mid;
        }
        else if (Var->End > Ptr[Mid].End)
        {
            Start = Mid;
        }
        else break;
        Mid = Start + (End - Start)/2;
    }

    List->Count++;
    memmove(Ptr + Start + 1, Ptr + Start, sizeof(*Ptr) * (List->Count - End));
    Ptr[Start] = *Var;
}

void VarList_RemovePreserveOrder(variable_list *List, unsigned Index)
{
    if (Index >= List->Count)
        return;

    memmove(List->Ptr + Index, List->Ptr + Index + 1, (List->Count - Index - 1) * sizeof(List->Ptr[0]));
    List->Count--;
}

void VarList_Destroy(variable_list *List)
{
    free(List->Ptr);
    *List = VarList_Init();
}


static void PrintInterval(const variable_list Vars, unsigned Start, unsigned End)
{
    /* print names */
    for (unsigned i = 0; i < Vars.Count; i++)
    {
        printf("%s ", Vars.Ptr[i].Name);
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
                printf("%c ", Var->Location.Type == LOCTYPE_REG? 'r' : 'm');
            }
            else
            {
                printf("  ");
            }
        }
        printf("\n");
    }
}

/* linear scan, implemented using Wikipedia's pseudocode, 
 * but with ExpireOldInterval(i) and SpillAtInterval(i) inlined */
static register_allocation_result LinearScanRegAlloc(register_allocator *RegAlloc, program_stack_allocator *ProgStack, const variable_list Vars)
{
    /* TODO: ActiveInReg is not a deep copy, it should be a list of references to elements of LiveIntervalByStart */

    /* variables that are being used in register and is currently active, sorted by increasing end point */
    variable_list ActiveInReg = VarList_Init(); 

    /* iterate through variables sorted by start point */
    variable_list LiveIntervalByStart = VarList_CopySortedByStartPoint(Vars);
    VARLIST_FOREACH(LiveIntervalByStart, i)
    {
        /* bookkeeping ActiveInReg, removing any residuals that have passed their end point */
        for (unsigned j = 0; j < ActiveInReg.Count; j++)
        {
            if (ActiveInReg.Ptr[j].End >= i->Start)
                break;

            VarList_RemovePreserveOrder(&ActiveInReg, j);
            RegAlloc_Dealloc(RegAlloc, ActiveInReg.Ptr[j].Location.As.Register);
        }

        /* after removing residual end points, try to allocate a register for i */
        /* if there are still registers left, use it */
        if (ActiveInReg.Count != RegAlloc_RegisterCount(RegAlloc))
        {
            i->Location = LOC_REGISTER(RegAlloc_Allocate(RegAlloc));
            VarList_InsertInEndPointOrder(&ActiveInReg, i);
        }
        /* no register available, but if i has a shorter lifetime than the longest living active variable, 
         * we prefer i to be in a register instead of such variable */
        else if (ActiveInReg.Ptr[ActiveInReg.Count - 1].End > i->End)
        {
            /* spill the undesirable variable into memory */
            variable *Spill = ActiveInReg.Ptr + ActiveInReg.Count - 1;
            Spill->Location = LOC_MEMORY(ProgStack_Allocate(ProgStack));
            VarList_Pop(&ActiveInReg);

            /* make i a register variable and append it to the active variable in register list */
            i->Location = LOC_REGISTER(Spill->Location.As.Register);
            VarList_InsertInEndPointOrder(&ActiveInReg, i);
        }
        /* as a last resort, i will reside in memory */
        else
        {
            i->Location = LOC_MEMORY(ProgStack_Allocate(ProgStack));
        }
    }

    return (register_allocation_result) {
        .Active = ActiveInReg,
        .LiveIntervalByStart = LiveIntervalByStart,
    };
}


int main(void)
{
    variable Vars[] = {
        { .Name = "A", .Start = 0, .End = 10 },
        { .Name = "B", .Start = 3, .End = 4 },
        { .Name = "C", .Start = 6, .End = 7 },
        { .Name = "D", .Start = 2, .End = 8 },
    };
    variable_list VarList = {
        .Ptr = Vars,
        .Count = STATIC_ARRAY_SIZE(Vars),
        .Capacity = STATIC_ARRAY_SIZE(Vars)
    };

    register_allocator RegAlloc = RegAlloc_Init();
    program_stack_allocator ProgStack = ProgStack_Init();
    register_allocation_result Result = LinearScanRegAlloc(&RegAlloc, &ProgStack, VarList);

    PrintInterval(VarList, 0, 10);
    printf("reg: %d, stack: %d\n", RegAlloc.FreeReg[0] + RegAlloc.FreeReg[1], ProgStack);
    printf("\nActive: \n");
    PrintInterval(Result.Active, 0, 10);
    printf("\nLive: \n");
    PrintInterval(Result.LiveIntervalByStart, 0, 10);
    return 0;
}

