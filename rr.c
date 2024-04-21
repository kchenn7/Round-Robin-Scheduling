#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;


  /* Additional fields here */
  u32 remaining_time;
  u32 start_exec_time;
  u32 waiting_time;
  u32 response_time;
  u32 end_time;
  /* End of "Additional fields here" */

};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */

  bool finished = false ;
  int processes_finished = 0;
  int t = 0;
  int quantum_left = quantum_length;

  struct process *p;

  //keep running until finished
  while (!finished)
  {
    //loop through arrival times and see which to add
    //first processes come first
    for (int i = 0; i < size; i++)
    {
      if(data[i].arrival_time == t)
      {

        p = &data[i];
        p->remaining_time = p->burst_time;
        p->start_exec_time =-1;
        p->waiting_time = -1;
        TAILQ_INSERT_TAIL(&list, p, pointers);
      }
    }
    //make sure queue is not empty
    if (!TAILQ_EMPTY(&list))
    {
      //if quantum left is 0, then refresh, and move head out of the queue
      //if head's time left is greater than 0, move it to the tail,
      //otherwise just remove it completely 
      if (quantum_left == 0)
      {
        quantum_left = quantum_length;
        p = TAILQ_FIRST(&list);
        TAILQ_REMOVE(&list, p, pointers);
        if (p-> remaining_time > 0)
        {
          TAILQ_INSERT_TAIL(&list, p, pointers);
        }
        //if it is finished, then increment processes_finished
        //add a waiting time for later
        else if (p-> remaining_time == 0)
        {
          processes_finished++;
          //printf("process %d: end at %d wait %d response %d\n", p->pid,p->end_time, p->waiting_time, p->response_time);
        }
      }
    }
    //make sure the remove head didn't empty queue
    if (!TAILQ_EMPTY(&list))
    {
      //work on first unit, take note of start_exec_time
      p = TAILQ_FIRST(&list);
      if (p->start_exec_time == -1)
        {
          p->start_exec_time = t;
          p->response_time = t - p->arrival_time;
        }

      //reduce quantum_left and remaining_time
      p->remaining_time--;
      quantum_left--;
      //if it becomes 0, then make quantum 0, so the quantum check can handle
      //the head of the linked list on next iteration
      if (p-> remaining_time == 0)
      {
        p->end_time = t+1;
        if (p->waiting_time == -1)
        {
          p->waiting_time = t + 1 - p->arrival_time - p->burst_time;
        }
        quantum_left = 0;
      }
    }

    //if processes are all finished, end
    if (processes_finished == size)
    {
      finished = true;
    }

    t++;
  }

  for (int i = 0; i < size; i++)
  {
    total_waiting_time += data[i].waiting_time;
    total_response_time += data[i].response_time;
  }
  
  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
