#include <types.h>
#include <lib.h>
#include <synch.h>
#include <test.h>
#include <thread.h>

#include "paintshop.h"
#include "paintshop_driver.h"


/*
 * **********************************************************************
 * YOU ARE FREE TO CHANGE THIS FILE BELOW THIS POINT AS YOU SEE FIT
 *
 */

/* Declare any globals you need here (e.g. locks, etc...) */
struct paintorder * order_buffer[NCUSTOMERS];
struct semaphore * mutex;
struct semaphore * empty;
struct semaphore * full;
int next_write;
int next_read;


struct cv * tint_container_counter_cv;
struct lock * tint_container_counter_lock;
struct lock * tint_locks[NCOLOURS];
int tint_container_flag_counter[NCOLOURS];
/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY CUSTOMER THREADS
 * **********************************************************************
 */

/*
 * order_paint()
 *
 * Takes one argument referring to the order to be filled. The
 * function makes the order available to staff threads and then blocks
 * until the staff have filled the can with the appropriately tinted
 * paint.
 */

void order_paint(struct paintorder *order)
{
    order->finished = sem_create("finished", 0);

    P(empty);
    P(mutex);

    order_buffer[next_write] = order;

    next_write++;
    if(next_write >= NCUSTOMERS) next_write = 0;

    V(mutex);
    V(full);

    // Has customer wait to be processed.
    if(order->finished->sem_count>0){
        panic("semaphore wrong!!!");
    }

    P(order->finished);
    // after activate consumer, cleanup the semaphore
    sem_destroy(order->finished);
}



/*
 * **********************************************************************
 * FUNCTIONS EXECUTED BY PAINT SHOP STAFF THREADS
 * **********************************************************************
 */

/*
 * take_order()
 *
 * This function waits for a new order to be submitted by
 * customers. When submitted, it returns a pointer to the order.
 *
 */

struct paintorder *take_order(void)
{
        struct paintorder *ret = NULL;

        P(full);
        P(mutex);

        ret = order_buffer[next_read];

        next_read++;
        if(next_read >= NCUSTOMERS) next_read = 0;

        V(mutex);
        V(empty);

        return ret;
}


/*
 * fill_order()
 *
 * This function takes an order provided by take_order and fills the
 * order using the mix() function to tint the paint.
 *
 * NOTE: IT NEEDS TO ENSURE THAT MIX HAS EXCLUSIVE ACCESS TO THE
 * REQUIRED TINTS (AND, IDEALLY, ONLY THE TINTS) IT NEEDS TO USE TO
 * FILL THE ORDER.
 */

void fill_order(struct paintorder *order)
{
    lock_acquire(tint_container_counter_lock);
    int tint_available_flag = 0;
    int i;
    while(tint_available_flag == 0){
        tint_available_flag = 1;
        for(i=0;i<PAINT_COMPLEXITY;i++){
            if(order->requested_tints[i] == 0){
                continue;
            }
            if(tint_container_flag_counter[order->requested_tints[i]-1] > 0){
                tint_available_flag = 0;
                break;
            }
        }
        // kprintf("order number, %d\n",order->test);
        if(tint_available_flag == 0){
            // kprintf("cv_wait, %d\n",order->test);
            cv_wait(tint_container_counter_cv, tint_container_counter_lock);
        }
    }

    // kprintf("out of while loop\n");
    // Set the counter and lock the corresponding tint lock.
    // should be able to acquire lock here
    for(i=0;i<PAINT_COMPLEXITY;i++){
        if(order->requested_tints[i] ==0 || !lock_do_i_hold(tint_locks[order->requested_tints[i]-1])){
            continue;
        }
        tint_container_flag_counter[order->requested_tints[i]-1]++;
        lock_acquire(tint_locks[order->requested_tints[i]-1]);
    }

    // release counter lock here, so that other could deal with other colors 
    // at the same time
    lock_release(tint_container_counter_lock);

    mix(order);

 // modify counter, reset corresponding bit to 0
    lock_acquire(tint_container_counter_lock);

    for(i=0;i<PAINT_COMPLEXITY;i++){
        if(order->requested_tints[i] ==0 || !lock_do_i_hold(tint_locks[order->requested_tints[i]-1])){
            continue;
        }
        tint_container_flag_counter[order->requested_tints[i]-1] = 0;
        lock_release(tint_locks[order->requested_tints[i]-1]);
    }

    cv_signal(tint_container_counter_cv, tint_container_counter_lock);
    lock_release(tint_container_counter_lock);   
}

/*
 * serve_order()
 *
 * Takes a filled order and makes it available to (unblocks) the
 * waiting customer.
 */

void serve_order(struct paintorder *order)
{
    // if(order->go_home_flag == 0){
    //     int i;

    //     // modify counter, reset corresponding bit to 0
    //     lock_acquire(tint_container_counter_lock);

    //     for(i=0;i<PAINT_COMPLEXITY;i++){
    //         if(order->requested_tints[i] ==0 || !lock_do_i_hold(tint_locks[order->requested_tints[i]-1])){
    //             continue;
    //         }
    //         tint_container_flag_counter[order->requested_tints[i]-1] = 0;
    //         lock_release(tint_locks[order->requested_tints[i]-1]);
    //     }

    //     cv_signal(tint_container_counter_cv, tint_container_counter_lock);
    //     lock_release(tint_container_counter_lock);
    // }
    
    // kprintf("check the semaphore of order, %d\n",order->finished->sem_count);

    V(order->finished);

    // sem_destroy(order->finished);
}



/*
 * **********************************************************************
 * INITIALISATION AND CLEANUP FUNCTIONS
 * **********************************************************************
 */


/*
 * paintshop_open()
 *
 * Perform any initialisation you need prior to opening the paint shop
 * to staff and customers. Typically, allocation and initialisation of
 * synch primitive and variable.
 */

// struct cv * tint_container_counter_cv;
// struct lock * tint_container_counter_lock;
// struct lock * tint_locks[NCOLOURS];
// int tint_container_flag_counter[NCOLOURS];

void paintshop_open(void)
{
    mutex = sem_create("mutex", 1);
    empty = sem_create("empty", NCUSTOMERS);
    full = sem_create("full", 0);
    next_write = 0;
    next_read = 0;

    int i;
    for(i=0; i<NCOLOURS; i++){
        tint_container_flag_counter[i] = 0;
    }

    tint_container_counter_lock = lock_create("tint_container_counter_lock");
    tint_container_counter_cv = cv_create("tint_container_counter_cv");

    for(i=0; i<NCOLOURS; i++){
        tint_locks[i] = lock_create("tint_lock");
    }
}

/*
 * paintshop_close()
 *
 * Perform any cleanup after the paint shop has closed and everybody
 * has gone home.
 */

void paintshop_close(void)
{
    sem_destroy(mutex);
    sem_destroy(empty);
    sem_destroy(full);
    lock_destroy(tint_container_counter_lock);
    int i;
    for(i=0; i<NCOLOURS; i++){
        lock_destroy(tint_locks[i]);
    }
    cv_destroy(tint_container_counter_cv);
}

