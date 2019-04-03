/*
 * alarm_cond.c
 *
 * This is an enhancement to the alarm_mutex.c program, which
 * used only a mutex to synchronize access to the shared alarm
 * list. This version adds a condition variable. The alarm
 * thread waits on this condition variable, with a timeout that
 * corresponds to the earliest timer request. If the main thread
 * enters an earlier timeout, it signals the condition variable
 * so that the alarm thread will wake up and process the earlier
 * timeout first, requeueing the later request.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <ctype.h>
#include <stdbool.h>

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag
{
  struct alarm_tag *link;
  char type;
  int seconds;
  int messageType;
  int messageNumber;
  time_t time; /* seconds from EPOCH */
  char message[128];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
alarm_t *alarm_list = NULL;
time_t current_alarm = 0;

/*
 * Insert alarm entry on list, in order.
 */
void alarm_insert(alarm_t *alarm)
{
  int status;
  alarm_t **last, *next;

  /*
   * LOCKING PROTOCOL:
   *
   * This routine requires that the caller have locked the
   * alarm_mutex!
   */
  last = &alarm_list;
  next = *last;
  while (next != NULL)
  {
    if (next->type == 'A' && next->messageNumber == alarm->messageNumber){
      printf("Type A Replacement Alarm Request With Message Number %d Inserted Into Alarm List at %lld: %c\n", alarm->messageNumber, (long long) time(NULL), alarm->type);
      alarm->link = next;
      next = alarm;
      break;
    }
    if (next->time >= alarm->time)
    {
      alarm->link = next;
      *last = alarm;
      printf("Type A Alarm Request With Message Number %d Inserted Into Alarm List at %lld: %c\n", alarm->messageNumber, (long long) time(NULL), alarm->type);
      break;
    }
    last = &next->link;
    next = next->link;
  }
  /*
   * If we reached the end of the list, insert the new alarm
   * there.  ("next" is NULL, and "last" points to the link
   * field of the last item, or to the list header.)
   */
  if (next == NULL)
  {
    *last = alarm;
    alarm->link = NULL;
  }
#ifdef DEBUG
  printf("[list: ");
  for (next = alarm_list; next != NULL; next = next->link)
    printf("%d(%d)[\"%s\"] ", next->time,
           next->time - time(NULL), next->message);
  printf("]\n");
#endif
  /*
   * Wake the alarm thread if it is not busy (that is, if
   * current_alarm is 0, signifying that it's waiting for
   * work), or if the new alarm comes before the one on
   * which the alarm thread is waiting.
   */
  if (current_alarm == 0 || alarm->time < current_alarm)
  {
    current_alarm = alarm->time;
    status = pthread_cond_signal(&alarm_cond);
    if (status != 0)
      err_abort(status, "Signal cond");
  }
}

/*
 * The alarm thread's start routine.
 */
void *alarm_thread(void *arg)
{
  alarm_t *alarm;
  struct timespec cond_time;
  time_t now;
  int status, expired;

  /*
   * Loop forever, processing commands. The alarm thread will
   * be disintegrated when the process exits. Lock the mutex
   * at the start -- it will be unlocked during condition
   * waits, so the main thread can insert alarms.
   */
  status = pthread_mutex_lock(&alarm_mutex);
  if (status != 0)
    err_abort(status, "Lock mutex");
  while (1)
  {
    /*
     * If the alarm list is empty, wait until an alarm is
     * added. Setting current_alarm to 0 informs the insert
     * routine that the thread is not busy.
     */
    current_alarm = 0;
    while (alarm_list == NULL)
    {
      status = pthread_cond_wait(&alarm_cond, &alarm_mutex);
      if (status != 0)
        err_abort(status, "Wait on cond");
    }
    alarm = alarm_list;
    alarm_list = alarm->link;
    now = time(NULL);
    expired = 0;
    if (alarm->time > now)
    {
#ifdef DEBUG
      printf("[waiting: %d(%d)\"%s\"]\n", alarm->time,
             alarm->time - time(NULL), alarm->message);
#endif
      cond_time.tv_sec = alarm->time;
      cond_time.tv_nsec = 0;
      current_alarm = alarm->time;
      while (current_alarm == alarm->time)
      {
        status = pthread_cond_timedwait(
            &alarm_cond, &alarm_mutex, &cond_time);
        if (status == ETIMEDOUT)
        {
          expired = 1;
          break;
        }
        if (status != 0)
          err_abort(status, "Cond timedwait");
      }
      if (!expired)
        alarm_insert(alarm);
    }
    else
      expired = 1;
    if (expired)
    {
      printf("(%d) %s\n", alarm->seconds, alarm->message);
      free(alarm);
    }
  }
}

char typeFinder(char line[])
{
  if (isdigit(line[0]))
  {
    // isValidTypeA(line);
    return 'A';
  }
  else if (
      line[0] == 'C' &&
      line[1] == 'r' &&
      line[2] == 'e' &&
      line[3] == 'a' &&
      line[4] == 't' &&
      line[5] == 'e' &&
      line[6] == '_' &&
      line[7] == 'T' &&
      line[8] == 'h' &&
      line[9] == 'r' &&
      line[10] == 'e' &&
      line[11] == 'a' &&
      line[12] == 'd')
  {
    return 'B';
  }
  else if (
      line[0] == 'C' &&
      line[1] == 'a' &&
      line[2] == 'n' &&
      line[3] == 'c' &&
      line[4] == 'e' &&
      line[5] == 'l')
  {
    return 'C';
  }
  else if (
      line[0] == 'P' &&
      line[1] == 'a' &&
      line[2] == 'u' &&
      line[3] == 's' &&
      line[4] == 'e' &&
      line[5] == '_' &&
      line[6] == 'T' &&
      line[7] == 'h' &&
      line[8] == 'r' &&
      line[9] == 'e' &&
      line[10] == 'a' &&
      line[11] == 'd')
  {
    return 'D';
  }
  else if (
      line[0] == 'R' &&
      line[1] == 'e' &&
      line[2] == 's' &&
      line[3] == 'u' &&
      line[4] == 'm' &&
      line[5] == 'e' &&
      line[6] == '_' &&
      line[7] == 'T' &&
      line[8] == 'h' &&
      line[9] == 'r' &&
      line[10] == 'e' &&
      line[11] == 'a' &&
      line[12] == 'd')
  {
    return 'E';
  }
  return 'F';
}

int main(int argc, char *argv[])
{
  int status;
  char line[128];
  alarm_t *alarm;
  pthread_t thread;

  status = pthread_create(
      &thread, NULL, alarm_thread, NULL);
  if (status != 0)
    err_abort(status, "Create alarm thread");
  while (1)
  {
    printf("Alarm> ");
    if (fgets(line, sizeof(line), stdin) == NULL)
      exit(0);
    if (strlen(line) <= 1)
      continue;

    int message_type;
    alarm = (alarm_t *)malloc(sizeof(alarm_t));
 
    switch (typeFinder(line))
    {
    case 'A':
      
      if (alarm == NULL)
        errno_abort("Allocate alarm");
      else if (sscanf(line, "%d Message(%d, %d) %128[^\n]", &alarm->seconds, &alarm->messageType, &alarm->messageNumber, alarm->message) < 4)
      {
        fprintf(stderr, "Bad command\n");
        free(alarm);
      }
      else
      {
        alarm->type = 'A';
        status = pthread_mutex_lock(&alarm_mutex);
        if (status != 0)
          err_abort(status, "Lock mutex");
        alarm->time = time(NULL) + alarm->seconds;
        alarm_insert(alarm);
        status = pthread_mutex_unlock(&alarm_mutex);
        if (status != 0)
          err_abort(status, "Unlock mutex");
      }
      break;
    case 'B':
      if (sscanf(line, "Create_Thread: MessageType(%d)", &alarm->messageType) < 1)
      {
        alarm->type = 'B';
        alarm_t **last, *next;
        bool shouldProceed = true;

        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'A' && next->messageType != alarm->messageType){
            printf("Type B Alarm Request Error: No Alarm Request With Message Type %d!", alarm->messageType);
            shouldProceed = false;
            break;
          }
        }
        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'B' && next->messageType == alarm->messageType){
            printf("Error: More Than One Type B Alarm Request With Message Type %d!", alarm->messageType);
            shouldProceed = false;
            break;
          }
        }
        if (shouldProceed){
          status = pthread_mutex_lock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Lock mutex");
          alarm->time = time(NULL) + alarm->seconds;
          alarm_insert(alarm);
          status = pthread_mutex_unlock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Unlock mutex");
        }
        else{
          free(alarm);
        }
      }
      break;
    case 'C':
      if (sscanf(line, "Cancel: Message(%d)", &alarm->messageNumber) < 1)
      {
        alarm->type = 'C';
        alarm_t **last, *next;
        bool shouldProceed = true;

        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'A' && next->messageNumber != alarm->messageNumber){
            printf("Error: No Alarm Request With Message Number %d to Cancel!", alarm->messageNumber);
            shouldProceed = false;
            break;
          }
        }
        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'B' && next->messageNumber == alarm->messageNumber){
            printf("Error: More Than One Request to Cancel Alarm Request With Message Number %d!", alarm->messageNumber);
            shouldProceed = false;
            break;
          }
        }
        if (shouldProceed){
          status = pthread_mutex_lock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Lock mutex");
          alarm->time = time(NULL) + alarm->seconds;
          alarm_insert(alarm);
          status = pthread_mutex_unlock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Unlock mutex");
          printf("Type C Cancel Alarm Request With Message Number %d Inserted Into Alarm List at %lld: %c", alarm->messageNumber, (long long)time(NULL), alarm->type);
        }
        else{
          free(alarm);
        }
      }
      break;
    case 'D':
      if (sscanf(line, "Pause_Thread: MessageType(%d)", &alarm->messageType) < 1)
      {
        alarm->type = 'D';
        alarm_t **last, *next;
        bool shouldProceed = true;

        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'A' && next->messageType != alarm->messageType){
            printf("Type D Alarm Request Error: No Alarm Request With Message Type %d!", alarm->messageType);
            shouldProceed = false;
            break;
          }
        }
        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'B' && next->messageNumber == alarm->messageNumber){
            printf("Error: More Than One Type D Alarm Request With Message Type %d!", alarm->messageType);
            shouldProceed = false;
            break;
          }
        }
        if (shouldProceed){
          status = pthread_mutex_lock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Lock mutex");
          alarm->time = time(NULL) + alarm->seconds;
          alarm_insert(alarm);
          status = pthread_mutex_unlock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Unlock mutex");
          printf("Type D Pause Thread Alarm Request For Message Type %d Inserted Into Alarm List at %lld!", alarm->messageType, (long long)time(NULL));
        }
        else{
          free(alarm);
        }
      }
      break;
    case 'E':
      if (sscanf(line, "Resume_Thread: MessageType(%d)", &alarm->messageType) < 1)
      {
        alarm->type = 'E';
        alarm_t **last, *next;
        bool shouldProceed = true;

        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'D' && next->messageType != alarm->messageType){
            printf("Type E Alarm Request Error: No Type D Pause Alarm Request With Message Type %d!", alarm->messageType);
            shouldProceed = false;
            break;
          }
        }
        last = &alarm_list;
        next = *last;
        while(next != NULL){
          if ( next->type == 'E' && next->messageNumber == alarm->messageNumber){
            printf("Error: More Than One Type E Alarm Request With Message Type %d!", alarm->messageType);
            shouldProceed = false;
            break;
          }
        }
        if (shouldProceed){
          status = pthread_mutex_lock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Lock mutex");
          alarm->time = time(NULL) + alarm->seconds;
          alarm_insert(alarm);
          status = pthread_mutex_unlock(&alarm_mutex);
          if (status != 0)
            err_abort(status, "Unlock mutex");
          printf("Type E Resume Thread Alarm Request For Message Type %d Inserted Into Alarm List at %lld!", alarm->messageType, (long long)time(NULL));
        }
        else{
          free(alarm);
        }
      }
      break;
    default:
      printf("bad command\n");
      break;
    }

    //    /*
    //     * Parse input line into seconds (%d) and a message
    //     * (%64[^\n]), consisting of up to 64 characters
    //     * separated from the seconds by whitespace.
    //     */
    //    if (sscanf (line, "%d %64[^\n]",
    //      &alarm->seconds, alarm->message) < 2) {
    //      fprintf (stderr, "Bad command\n");
    //      free (alarm);
    //    }
    //    else {
    //      status = pthread_mutex_lock (&alarm_mutex);
    //      if (status != 0)
    //        err_abort (status, "Lock mutex");
    //      alarm->time = time (NULL) + alarm->seconds;
    //      /*
    //       * Insert the new alarm into the list of alarms,
    //       * sorted by expiration time.
    //       */
    //      alarm_insert (alarm);
    //      status = pthread_mutex_unlock (&alarm_mutex);
    //      if (status != 0)
    //          err_abort (status, "Unlock mutex");
    //    }
  }
}
