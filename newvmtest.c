#include "param.h"
#include "types.h"
#include "stat.h"
#include "user.h"
#include "syscall.h"
#include "traps.h"
#include "memlayout.h"

int data_int;

int main(void)
{
	int stack_int;
	int* heap_int = malloc(sizeof(int));

	printf(1, "main: 0x%p\n", main);
	printf(1, "data_int: 0x%p\n", &data_int);
	printf(1, "stack_int: 0x%p\n", &stack_int);
	printf(1, "heap_int: 0x%p\n", heap_int);

	exit();
}




