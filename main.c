#include <stdio.h>

#define STATIC_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define IN_RANGE(lower, n, upper) ((lower) <= (n) && (n) <= (upper))

typedef struct 
{
    const char *Name;
    unsigned Start, End;
} variable_interval;


static void PrintInterval(const variable_interval *Intervals, unsigned Count, unsigned Start, unsigned End)
{
    for (unsigned i = Start; i <= End; i++)
    {
        for (unsigned k = 0; k < Count; k++)
        {
            if (IN_RANGE(Intervals[k].Start, i, Intervals[k].End))
            {
                printf("%s ", Intervals[k].Name);
            }
            else
            {
                printf("  ");
            }
        }
        printf("\n");
    }
}


int main(void)
{
    const variable_interval VarInterval[] = {
        { .Name = "A", .Start = 0, .End = 10 },
        { .Name = "B", .Start = 1, .End = 4 },
        { .Name = "C", .Start = 6, .End = 9 },
        { .Name = "D", .Start = 2, .End = 8 },
    };
    PrintInterval(VarInterval, STATIC_ARRAY_SIZE(VarInterval), 0, 10);
    return 0;
}

