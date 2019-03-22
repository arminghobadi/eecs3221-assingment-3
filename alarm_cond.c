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
  int seconds;
  int messageType;
  int messageNumber;
  time_t time; /* seconds from EPOCH */
  char message[128];
} alarm_t;

typedef struct type_a
{
  int seconds;
  time_t time;
  int message_type;
  int message_number;
  char message[128];
} type_a;

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
    if (next->time >= alarm->time)
    {
      alarm->link = next;
      *last = alarm;
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

void isValidTypeA(char line[])
{
  //  typedef struct type_a {
  //    int seconds;
  //    int message_type;
  //    int message_number;
  //    char message[128];
  //  } type_a ;
  // 5 Message(2, 13) Visit Grandma on way back home
  bool haveSeenSeconds = false;
  int seconds;
  bool haveSeenMessageString = false;
  bool haveSeenOpenParanthesis = false;
  bool haveSeenMessageType = false;
  int messageType;
  bool haveSeenComma = false;
  bool haveSeenMessageNumber = false;
  int messageNumber;
  bool haveSeenCloseParanthesis = false;
  bool haveSeenMessage = false;
  char message[128];
  char error[200];

  int lastWhiteSpaceIndex = 0;

  for (int i = 0; i < strlen(line); i++){
    if (!haveSeenSeconds && line[i] == ' '){
      
      int res = 0;
      for (int j = lastWhiteSpaceIndex; j < i; j++){
        if (isdigit(line[j])){
          res = (res * 10) + (int)line[j] - (int)'0';
        }
      }
      seconds = res;
      printf("seconds is %d\n", seconds);
      lastWhiteSpaceIndex = i;
      haveSeenSeconds = true;
    }
    if (line[i] == ' ' && haveSeenSeconds && !haveSeenMessageString){
      lastWhiteSpaceIndex = i;
      if (line[i+1] == 'M' && 
        line[i+2] == 'e' && 
        line[i+3] == 's' && 
        line[i+4] == 's' &&
        line[i+5] == 'a' &&
        line[i+6] == 'g' &&
        line[i+7] == 'e'){
          haveSeenMessageString = true;
          printf("saw message string\n");
          i +=6;
      }
    }
    if (haveSeenSeconds && haveSeenMessageString && !haveSeenOpenParanthesis){
      if (line[i] != '('){
        continue;
      }
      haveSeenOpenParanthesis = true;
      printf("saw ( \n");
    }
    if (haveSeenSeconds && haveSeenMessageString && haveSeenOpenParanthesis && !haveSeenMessageType) {
      int res = 0;
      if ( isdigit( line[i] ) ){
        for (int j = i ; j<strlen(line) ; j++){ // will perform a manual break in this for loop
          res = (res * 10) + (int)line[j] - (int)'0';
          if (!isdigit(line[j+1])){
            haveSeenMessageType = true;
            messageType = res;
            printf("message type is %d\n", messageType);
            break;
          }
        }
      }
    }

    if (haveSeenSeconds && haveSeenMessageString && haveSeenOpenParanthesis && haveSeenMessageType && !haveSeenComma){
      if (line[i]==','){
        haveSeenComma = true;
      }
    }
    if (haveSeenSeconds && haveSeenMessageString && haveSeenOpenParanthesis && haveSeenMessageType && haveSeenComma && !haveSeenMessageNumber){
      int res = 0;
      if ( isdigit( line[i] ) ){
        for (int j = i ; j<strlen(line) ; j++){ // will perform a manual break in this for loop
          res = (res * 10) + (int)line[j] - (int)'0';
          if (!isdigit(line[j+1])){
            haveSeenMessageNumber = true;
            messageNumber = res;
            printf("message number is %d\n", messageNumber);
            break;
          }
        }
      }
    }

    if (haveSeenSeconds && haveSeenMessageString && haveSeenOpenParanthesis && haveSeenMessageType && haveSeenComma && haveSeenMessageNumber && !haveSeenCloseParanthesis){
      if (line[i]==')'){
        haveSeenCloseParanthesis = true;
      }
      continue;
    }
    if (haveSeenSeconds && haveSeenMessageString && haveSeenOpenParanthesis && haveSeenMessageType && haveSeenComma && haveSeenMessageNumber && haveSeenCloseParanthesis && !haveSeenMessage){
      if (line[i] == ' '){
        continue;
      }
      for( int j = 0 ; (j < 128 &&  i < strlen(line)) ; j++) {
        // TODO: there might be a problem here!
        // printf("i is %d line[i] is %c and current message[j] is %c \n",i, line[i], message[j]);
        message[j] = line[i];
        i++;
        
      }
      printf(" message is %s\n", message);
      break;
    }

  }
  type_a newAlarm = { seconds, messageType, messageNumber, *message };
  printf("my struct is %s %d\n", &newAlarm.message, newAlarm.seconds);
}

void seperateBySpace(char string[])
{
  int lastWhiteSpaceIndex = 0;
  int lastResultIndex = 0;
  char result[128];
  for (int i = 0; i < strlen(string); i++)
  {
    if (string[i] == ' ')
    {
      int res = 0;

      for (int j = lastWhiteSpaceIndex; j < i; j++)
      {
        if (isdigit(string[j]))
        {
          res = (res * 10) + (int)string[j] - (int)'0';
        }
      }
      printf("int is %d\n", res);
      lastWhiteSpaceIndex = i;
    }
  }
}

void isValidTypeB(char line[]){

}

char typeFinder(char line[])
{
  if (isdigit(line[0]))
  {
    isValidTypeA(line);
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
    line[12] == 'd'
  ){
    return 'B';
  }
  else if (
    line[0] == 'C' &&
    line[1] == 'a' &&
    line[2] == 'n' &&
    line[3] == 'c' &&
    line[4] == 'e' &&
    line[5] == 'l'
  ){
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
    line[11] == 'd'
  ){
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
    line[12] == 'd'
  ){
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
  isValidTypeA("armin:");

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
    alarm = (alarm_t *)malloc(sizeof(alarm_t));
    if (alarm == NULL)
      errno_abort("Allocate alarm");
    printf(" the type is %c\n",typeFinder(line));
    seperateBySpace(line);

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
