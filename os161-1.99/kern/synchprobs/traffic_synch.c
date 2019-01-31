#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */
//static struct semaphore *intersectionSem;

typedef struct Vehicle
{
  Direction origin;
  Direction destination;
} Vehicle;

#define MAX_THREADS 10
static int NumThreads = 10;      // number of concurrent simulation threads
volatile int threadsFull = 0;
static struct cv *intercv;
static struct lock *interlk;
static Vehicle * volatile vehicles[MAX_THREADS];
//previous prototype
static bool right_turn(Vehicle *v);
static bool check_constraints(Vehicle *v);

/*
 * bool right_turn()
 * 
 * Purpose:
 *   predicate that checks whether a vehicle is making a right turn
 *
 * Arguments:
 *   a pointer to a Vehicle
 *
 * Returns:
 *   true if the vehicle is making a right turn, else false
 *
 * Note: written this way to avoid a dependency on the specific
 *  assignment of numeric values to Directions
 */
bool
right_turn(Vehicle *v) {
  KASSERT(v != NULL);
  if (((v->origin == west) && (v->destination == south)) ||
      ((v->origin == south) && (v->destination == east)) ||
      ((v->origin == east) && (v->destination == north)) ||
      ((v->origin == north) && (v->destination == west))) {
    return true;
  } else {
    return false;
  }
}



/*
 * check_constraints()
 * 
 * Purpose:
 *   checks whether the entry of a vehicle into the intersection violates
 *   any synchronization constraints.   Causes a kernel panic if so, otherwise
 *   returns silently
 *
 * Arguments:
 *   number of the simulation thread that moved the vehicle into the intersection 
 *
 * Returns:
 *   nothing
 */

bool
check_constraints(Vehicle *v) {

  /* compare newly-added vehicle to each other vehicles in in the intersection */
  for(int i=0;i<NumThreads;i++) {
    if (vehicles[i] == NULL) continue;
    if (vehicles[i]->origin == v->origin) continue;
    if ((vehicles[i]->origin == v->destination) &&
        (vehicles[i]->destination == v->origin)) continue;
    if ((right_turn(vehicles[i]) || right_turn(v)) &&
    (v->destination != vehicles[i]->destination)) continue;

    return false;
  }
  return true;
}

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */


  intercv = cv_create("intercv");
  interlk = lock_create("interlk");

  for(int i=0;i<MAX_THREADS;i++) {    
    vehicles[i] = (Vehicle * volatile)NULL;
  }
  if (intercv == NULL) {
    panic("could not create intersection cv");
  }
  if (interlk == NULL) {
    panic("could not create intersection lock");
  }

  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(intercv != NULL);
  cv_destroy(intercv);
  KASSERT(interlk !=NULL);
  lock_destroy(interlk);

  //sem_destroy(intersectionSem);
}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  KASSERT(interlk != NULL);
  KASSERT(intercv != NULL);
  Vehicle *v = kmalloc(sizeof(struct Vehicle));
  v->origin = origin;
  v->destination = destination;
  lock_acquire(interlk);
  while (check_constraints(v) == false || threadsFull ==NumThreads){
    cv_wait(intercv, interlk);
  }
  threadsFull++;
  for(int i = 0; i<NumThreads;i++){
    if(vehicles[i] == NULL){
        vehicles[i] = v;
        break;
      }
  }
  lock_release(interlk);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  /* replace this default implementation with your own implementation */
  //(void)origin;  /* avoid compiler complaint about unused parameter */
  //(void)destination; /* avoid compiler complaint about unused parameter */
  KASSERT(interlk != NULL);
  KASSERT(intercv != NULL);
  lock_acquire(interlk);

  for (int i =0; i< NumThreads;i++){
    if(vehicles[i]!=(Vehicle * volatile)NULL){
      if(vehicles[i]->origin == origin && vehicles[i]->destination == destination){
        vehicles[i]=(Vehicle * volatile)NULL;
        threadsFull--;
        cv_broadcast(intercv,interlk);
        break;
      }
    }
  }

  lock_release(interlk);
}
