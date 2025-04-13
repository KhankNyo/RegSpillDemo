#include <stdio.h>

#define UTILS_IMPL
#include "Utils.h"


/* linear scan, implemented using Wikipedia's pseudocode, 
 * but with ExpireOldInterval(i) and SpillAtInterval(i) inlined, 
 * assuming we're operating on the variable list and it is already sorted by increasing starting point */
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
        { .Name = "D", .Start = 3, .End = 4 },
        { .Name = "E", .Start = 5, .End = 7 },
    };
    variable_array VarArray = {
        .Ptr = Vars,
        .Count = STATIC_ARRAY_SIZE(Vars),
        .Capacity = STATIC_ARRAY_SIZE(Vars)
    };

    register_allocator RegAlloc = RegAlloc_Init();
    program_stack_allocator ProgStack = ProgStack_Init();
    LinearScanRegAlloc(&RegAlloc, &ProgStack, &VarArray);

    printf("free reg left: %d/%d\n"
            "stack mem count: %d\n", 
            RegAlloc_FreeRegisterCount(&RegAlloc), 
            RegAlloc_RegisterCount(&RegAlloc), 
            ProgStack
    );
    printf("Live: \n");
    PrintInterval(VarArray, 0, 10);
    return 0;
}

