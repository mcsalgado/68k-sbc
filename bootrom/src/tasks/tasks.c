#include "tasks/tasks.h"

uint32_t ExecutingTaskRegisters[16];
uint16_t SFRAME_SR = 0;
uint32_t SFRAME_PC = 0;

uint8_t TASK_SwitchingEnabled = 0;

List *TASK_List;
List *TASK_ReadyList;

extern void LIST_AddHead(__reg("a0") List *list, __reg("a1") Node *node);
extern void LIST_AddTail(__reg("a0") List *list, __reg("a1") Node *node);

extern Node *LIST_RemHead(__reg("a0") List *list);
extern Node *LIST_RemTail(__reg("a0") List *list);
extern Node *LIST_Remove(__reg("a0") List *list, __reg("a1") Node *node);

Task *running_task = NULL;

void TASK_InitSubsystem()
{
  TASK_List = MEMMGR_NewPtr(sizeof(List), H_SYSHEAP);
  LIST_Init(TASK_List, 0);

  TASK_ReadyList = MEMMGR_NewPtr(sizeof(List), H_SYSHEAP);
  LIST_Init(TASK_ReadyList, 0);
  
  //printf("List at %x is initialized\n", TASK_List);
  MMIO8(0x600017) = 0x40; // MFP vectors now begin at number 64
  MMIO8(0x600007) = MMIO8(0x600007) | 0x20; // enable TIMER A interrupt
  
  // Set TIMER A for delay mode, 200 prescaler
  MMIO8(0x600019) = 0x10 | 0x07;
}

void TASK_EnableSwitching(uint8_t enable)
{
  TASK_SwitchingEnabled = enable;
}

void TASK_Add(Task *task,
			  CPTR start_address,   // the task's entry point
			  CPTR cleanup_address, // if the task returns, call this
			  uint32_t stack_size)  // bytes to allocate for the stack
{
  //printf("task == %6X\n", task);
  //printf("task->info == %6X\n", task->info);
  //printf("status register addr is %6X\n", &(task->info->status_register));
  task->info->state = TASK_READY;
  
  //TODO: create a new heap for applications that need one  
  if(task->info->heap == TASK_SYSHEAP)
	task->info->stack_low = MEMMGR_NewPtr(stack_size, H_SYSHEAP);
  else if(task->info->heap == TASK_APPHEAP)
	task->info->stack_low = MEMMGR_NewPtr(stack_size, H_CURHEAP);
  else
	{
	  printf("TASK_Add: Heap selection not recognized!\nLocking system.\n");
	  while(true) {};
	}


  task->info->pc = (uint32_t)start_address;
  task->info->status_register = 0x2300; // TODO

  task->info->stack_high = (CPTR)((uint32_t)task->info->stack_low + stack_size - 8);
  task->info->stack_pointer = task->info->stack_high;
  
  LIST_AddTail(TASK_List, (Node *)task);
  LIST_AddTail(TASK_ReadyList, (Node *)task);
  printf("TASK: Task %x added to task list. Task's SP is %x\n",
		 task,
		 task->info->stack_pointer);

  printf("*** TASK_List ***\n");
  printf("Head: %6X, Tail: %6X\n", TASK_ReadyList->lh_Head, TASK_ReadyList->lh_Tail);

  //MEMMGR_DumpHeapBlocks(&heap_system);
}

void TASK_Wait(Task *task)
{
  // Put a task to sleep until it receives a signal.  
}

void TASK_CleanReadyQueue()
{
  Task *task, *readyTask;

  for(task = TASK_ReadyList->lh_Head; task->node.ln_Succ != NULL; task = task->node.ln_Succ)
	{
	  if(task->info->state != TASK_READY)
    {
      printf("Removing task %06X from the ready list\n", task);
		  LIST_Remove(TASK_ReadyList, task);
    }
	}
}

void TASK_ProcessQuantum()
{
  // The timer expired. See if we need to switch to another task.

  // The scheduler is disabled, don't do anything.
  if(TASK_SwitchingEnabled == 0)
	{
	  return;
	}
  
  //printf("Timeslice expired!\n");

  TASK_ContextSwitch(TASK_FindReadyTask());
}

void TASK_ContextSwitch(Task *new_task)
{
  printf("*** Context Switch to Task %06X ***\n", new_task);

  //Find the running task.
  Task *current_task = running_task;
  if(current_task == NULL)
	{
	  printf("Current task is NULL\n");
	}
  else
	{
	  //printf("Current task is %x\n", current_task);
	  // Back up the current registers to the Task struct.
	  for(int i=0;i<16;i++)
		{
		  current_task->info->registers[i] = ExecutingTaskRegisters[i];
		}
	  current_task->info->stack_pointer = (CPTR)ExecutingTaskRegisters[7];
	  current_task->info->pc = SFRAME_PC;
	  current_task->info->status_register = SFRAME_SR;

    // If something forced this task to wait, then it won't be in RUNNING state and we don't want it to be READY.
    if(current_task->info->state == TASK_RUNNING)
    {
      current_task->info->state = TASK_READY;
      //printf("Adding task %x to ReadyList\n", current_task);
      LIST_AddTail(TASK_ReadyList, current_task);
    }
    else if(current_task->info->state == TASK_READY)
    {
      //printf("Adding task %x to ReadyList\n", current_task);
      LIST_AddTail(TASK_ReadyList, current_task);
    }
	}

  TASK_RestoreRegisters(new_task);
  
  new_task->info->state = TASK_RUNNING;

  // Re-queue the current task.
  LIST_Remove(TASK_ReadyList, new_task);
  running_task = new_task;

  TASK_AllowInterrupts(); // Unmask the interrupt if it's been masked and we got here manually
}

void TASK_RestoreRegisters(Task *new_task)
{
  // Restore the new task's registers.

  for(int i=0;i<16;i++)
	ExecutingTaskRegisters[i] = new_task->info->registers[i];
  
  //printf("New task is %x, new A7 is $%x\n", new_task, new_task->info->stack_pointer);
  
  ExecutingTaskRegisters[7] = (uint32_t)new_task->info->stack_pointer;
  SFRAME_PC = new_task->info->pc;
  SFRAME_SR = new_task->info->status_register;
}

void TASK_ForbidInterrupts()
{
  // Disable the timer interrupt during *sensitive* operations.
  MMIO8(0x600013) = MMIO8(0x600013) & 0xDF;
}

void TASK_AllowInterrupts()
{
  // Enable the timer interrupt, allowing timeslices to be processed.
  MMIO8(0x600013) = MMIO8(0x600013) | 0x20;
}

Task *TASK_FindReadyTask()
{
  Task *task;

  TASK_ProcessSignals();
  //TASK_CleanReadyQueue();
  TASK_PrintTaskList(TASK_ReadyList);

  for(task = TASK_ReadyList->lh_Head; task->node.ln_Succ != NULL; task = task->node.ln_Succ)
	{
	  if(task->info->state == TASK_READY)
		  return task;
	}

  // Nothing else is ready, so just load the current task again.
  return running_task;
}

void TASK_WaitForMessage()
{
  // Set the current task to TASK_WAITING.
  // Set the task's signal block so it's waiting for a SIG_MESSAGE.

  // Atomic operation.
  TASK_ForbidInterrupts();
  
  if(running_task == NULL)
  {
    printf("Running task is NULL\n");
  }
  else
  {
    printf("Task %06X is now waiting for a message\n", running_task);
    running_task->info->state = TASK_WAITING;
    running_task->info->signals.waiting |= SIG_MESSAGE;
    printf("Task %06X is no longer ready\n", running_task);
  }

  MMIO8(0x60000B) |= 0x20; // Trigger a context switch.
}

void TASK_PrintTaskList(List *list)
{
  Task *task;
  printf("*** start task list at %06X ***\n", list);
  for(task = list->lh_Head; task->node.ln_Succ != NULL; task = task->node.ln_Succ)
	{
	  if(task->info->state == TASK_READY)
		  printf("TASK: %06X, state %d\n", task, task->info->state);
	}
  printf("*** end ***\n");
}

void TASK_ProcessSignals()
{
  // Wake up any tasks that have received signals they're waiting for.
  for(Task *task = TASK_List->lh_Head; task->node.ln_Succ != NULL; task = task->node.ln_Succ)
	{
	  if(task->info->state == TASK_WAITING)
    {
      printf("Task %06X is waiting. Signals Waiting: %06X | Signals Received: %06X\n", task, (task->info->signals.waiting), (task->info->signals.received));

      // Check if any received signals match what we're explicitly waiting for.
		  if((task->info->signals.waiting & task->info->signals.received) != 0)
      {
        // If so, this task is now READY.
        task->info->state = TASK_READY;

        printf("Task %06X is ready, adding to list at %06X\n", task, TASK_ReadyList);

        // TODO: Why does this freeze? Is TASK_ReadyList getting corrupted?
        LIST_AddTail(TASK_ReadyList, task);
      }
    }
	}
}

Task *TASK_GetRunningTask()
{
  return running_task;  
}